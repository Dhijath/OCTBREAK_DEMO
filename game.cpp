/*==============================================================================

   ゲーム制御 [game.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

   ・ゲーム全体の初期化 / 更新 / 描画 / 終了
   ・プレイヤー / カメラ / マップ / 弾 / エフェクト / エネミーを統括
   ・ダンジョン再生成（Rキー）を追加
   ・　エネミーとの衝突でダメージ処理

==============================================================================*/

#include "game.h"
#include "shader.h"
#include "Sampler.h"
#include "Meshfield.h"
#include "Light.h"
#include <DirectXMath.h>
using namespace DirectX;

#include "model.h"
#include "Player.h"
#include "Player_Camera.h"
#include "map.h"
#include "billboard.h"
#include "texture.h"
#include "sprite_anim.h"
#include "bullet.h"
#include "bullet_hit_effect.h"
#include "particle_spark.h"
#include "direct3d.h"
#include "cube.h"
#include "sprite.h"
#include "blob_shadow.h"
#include "FloorRegistry.h"
#include "key_logger.h"
#include "collision.h"

#include <cfloat>
#include <cstdint>
#include "shader3d.h"
#include "EnemyAPI.h"
#include "WallShader.h"
#include "Minimap.h"
#include "HUD.h"
#include "ItemManager.h"
#include "EnemyBullet.h"
#include "EnemyManager.h"
#include "EnemyAI.h"
#include "EnemyParts.h"
#include "EnemyDex.h"
#include "DamagePopup.h"
#include "BossIntro.h"
#include "BossDefeat.h"
#include "Shadow_Map.h"
#include "pad_logger.h"
#include "Option.h"
#include "Score.h"
#include "Skybox.h"
#include "Shop.h"
#include <cstdlib>   // rand（ダメージポップアップ位置のランダム化）




//==============================================================================
// 足元の「支持面Y」を探す（XZが重なっていて、上面が足元以下の中で最大のY）
// Map 構造に依存するため game.cpp 内部関数として定義
//==============================================================================
static bool GetSupportY_FromMapAABBs(
    const AABB& actor,
    float* outY)
{
    const float eps = 0.02f;

    float bestY = -FLT_MAX;
    bool found = false;

    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* objPtr = Map_GetObject(i);
        if (objPtr->KindId != 1) continue; // KIND_FLOOR だけ
        const AABB& obj = objPtr->Aabb;


        // XZ が重なっているか（簡易）
        const bool overlapX =
            actor.min.x <= obj.max.x && actor.max.x >= obj.min.x;
        const bool overlapZ =
            actor.min.z <= obj.max.z && actor.max.z >= obj.min.z;

        if (!overlapX || !overlapZ) continue;

        // 「上に乗れる面」条件
        const float topY = obj.max.y;
        if (topY <= actor.min.y + eps)
        {
            if (!found || topY > bestY)
            {
                bestY = topY;
                found = true;
            }
        }
    }

    if (found && outY) *outY = bestY;
    return found;
}

namespace
{
    // フィールド描画用のワールド行列
    XMMATRIX g_mtxWorld_Field;

    // HUD表示フラグ（F3でトグル、デバッグ用）
    bool g_HudVisible = true;

    // テスト用テクスチャID
    int g_TexTest = -1;

    // ダンジョン生成用シード（Rで更新）

    std::uint32_t g_DungeonSeed = 12345u;
    static EnemyManager g_EnemyManager;

    // ボス管理
    static Enemy* g_pBossEnemy = nullptr; // ボスへの生ポインタ（生存中のみ有効）
    static bool   g_BossDefeated = false;       // ボスが撃破されたか
    static bool   g_IsBossRoom    = false;
    static bool   g_IsSurvival    = false;  // サバイバルモード中フラグ
    static int    g_TexLockon     = -1;
    static EnemyMix g_EnemyMix    = EnemyMix::Balanced;  // 敵編成（ミッションごとに設定）
    static float    g_EnemyHpScale = 1.0f;               // 通常エネミーの耐久倍率（ミッションごとに設定）
    static EnemyType g_BossType   = EnemyType::Boss;     // ボス部屋で出すボスの種類（ミッションごとに設定）
    static float    g_BossHpScale  = 1.0f;               // ボスの耐久倍率（ミッションごとに設定）
    static int      g_KillCount   = 0;                   // 撃破数（クリア画面の集計用）

    // 更新中に出された出現要求（ボスの召喚など）
    struct SpawnRequest { XMFLOAT3 pos; int type; };
    static std::vector<SpawnRequest> g_SpawnRequests;
}

// ボス（撃破演出・ボス戦の対象）になる種別か
static bool IsBossType(EnemyType type)
{
    return type == EnemyType::Boss || type >= EnemyType::BossArgus;
}

// このフレームで倒れたエネミーの数（エネミー図鑑に種別ごとの撃破を記録する）
static int CountDeadEnemies()
{
    int dead = 0;
    for (int i = 0; i < g_EnemyManager.GetCount(); ++i)
    {
        const Enemy& e = g_EnemyManager.GetEnemy(i);
        if (e.IsAlive()) continue;
        ++dead;
        EnemyDex_RecordKill(e.GetTypeId());
    }
    return dead;
}

void Game_SpawnEnemy(const XMFLOAT3& pos, int type);

static void FlushSpawnRequests()
{
    if (g_SpawnRequests.empty()) return;
    std::vector<SpawnRequest> pending;
    pending.swap(g_SpawnRequests);
    for (const SpawnRequest& r : pending)
        Game_SpawnEnemy(r.pos, r.type);
}

// 通常エネミーの耐久をミッションの倍率に合わせる（ボスは対象外）
static void ApplyEnemyHpScale(int index)
{
    if (g_EnemyHpScale == 1.0f) return;
    Enemy& e = g_EnemyManager.GetEnemy(index);
    const int maxHp = static_cast<int>(e.GetMaxHP() * g_EnemyHpScale);
    e.SetHP(maxHp, maxHp);
}

// ボスの耐久をミッションの倍率に合わせる（第二作戦区域の再戦用）
static void ApplyBossHpScale(Enemy& boss)
{
    if (g_BossHpScale == 1.0f) return;
    const int maxHp = static_cast<int>(boss.GetMaxHP() * g_BossHpScale);
    boss.SetHP(maxHp, maxHp);
}

// i 番目のスポーン位置に出す敵の種別。
// ブロックステージが部隊として種別を指定していればそれを、なければ編成の比率で決める
static EnemyType PickEnemyType(int i);
static EnemyType SpawnTypeAt(int i)
{
    const int designated = BlockStage_IsActive() ? BlockStage_GetEnemyType(i) : -1;
    return (designated >= 0) ? static_cast<EnemyType>(designated) : PickEnemyType(i);
}

// スポーン順番（i）から敵種別を割り振る。編成ごとに比率を変える。
static EnemyType PickEnemyTypeFor(EnemyMix mix, int i);
static EnemyType PickEnemyType(int i)
{
    return PickEnemyTypeFor(g_EnemyMix, i);
}

// 増援など、任意の編成で種別を決めたいとき用（Game_Manager から呼ぶ）
int Game_GetEnemyTypeForMix(EnemyMix mix, int index)
{
    return static_cast<int>(PickEnemyTypeFor(mix, index));
}

static EnemyType PickEnemyTypeFor(EnemyMix mix, int i)
{
    switch (mix)
    {
    case EnemyMix::Swarm:   // 高機動型主体（自爆型・突撃型・翼型を混ぜる。残りSpeed）
        if (i % 8 == 1) return EnemyType::Wing;
        if (i % 7 == 3) return EnemyType::Bomber;
        if (i % 5 == 2) return EnemyType::Gunner;
        if (i % 6 == 0) return EnemyType::Sniper;
        if (i % 3 == 0) return EnemyType::Normal;
        return EnemyType::Speed;

    case EnemyMix::Heavy:   // 重装型主体（砲撃型・回転砲型・脚型を混ぜる。2体に1体Tank、残りNormal）
        if (i % 7 == 3) return EnemyType::Artillery;
        if (i % 9 == 5) return EnemyType::Gatling;
        if (i % 11 == 7) return EnemyType::Walker;
        if (i % 2 == 0) return EnemyType::Tank;
        if (i % 3 == 0) return EnemyType::Sniper;
        return EnemyType::Normal;

    case EnemyMix::Sniper:  // 狙撃型主体（砲撃型・幻影型を混ぜる。2体に1体Sniper、残りSpeed）
        if (i % 6 == 3) return EnemyType::Artillery;
        if (i % 7 == 5) return EnemyType::Phantom;
        if (i % 9 == 1) return EnemyType::Halo;
        if (i % 2 == 0) return EnemyType::Sniper;
        if (i % 5 == 0) return EnemyType::Tank;
        return EnemyType::Speed;

    default:                // 混成（突撃型・幻影型・自爆型を少し混ぜる）
        if (i % 7 == 4)  return EnemyType::Gunner;
        if (i % 11 == 6) return EnemyType::Phantom;
        if (i % 13 == 9) return EnemyType::Bomber;
        if (i % 10 == 3) return EnemyType::Orbiter;
        if (i % 17 == 8) return EnemyType::Walker;
        if (i % 5 == 0) return EnemyType::Tank;
        if (i % 3 == 0) return EnemyType::Sniper;
        if (i % 2 == 0) return EnemyType::Speed;
        return EnemyType::Normal;
    }
}

// ボス消滅コールバック（BossDefeat演出のHOLD終了時に呼ばれる）
static void OnBossVanish()
{
    const int cnt = g_EnemyManager.GetCount();
    for (int i = 0; i < cnt; ++i)
    {
        Enemy& e = g_EnemyManager.GetEnemy(i);
        if (e.IsDead() && e.IsAlive())
            e.ConfirmDeath();
    }
    g_EnemyManager.RemoveDead();
}

//==============================================================================
// ゲーム全体の初期化
//==============================================================================
//==============================================================================
// D3Dリソース初期化（アプリ起動時に1回だけ呼ぶ）
//==============================================================================
void Game_InitializeD3D()
{
    Shader3d_Begin();
    Direct3D_ConfigureOffScreenBuffer();
    MeshField_Initialize(Direct3D_GetDevice(), Direct3D_GetContext());
}

//==============================================================================
// D3Dリソース解放（アプリ終了時に1回だけ呼ぶ）
//==============================================================================
void Game_FinalizeD3D()
{
    MeshField_Finalize();
}

//==============================================================================
// ゲームプレイ初期化（プレイ開始のたびに呼ぶ）
//==============================================================================
void Game_Initialize()
{
    g_TexLockon = Texture_Load(L"Resource/Texture/Lockon.png");

    EnemyBullet_Initialize();
    // 弾・被弾エフェクトの初期化
    Bullet_Initialize();
    BulletHitEffect_Initialize();
    SparkEffect_Initialize();

    // マップ初期化（ここでスポーン位置が確定する想定）
    Map_Initialize();
    Skybox_Initialize();

    // 天井あり（屋外マップを使うサバイバルは GameManager 側で false にする）
    Map_SetCeilingVisible(true);

    // 念のため床登録を更新（支持面判定に使う場合がある）
    Map_RegisterFloors();

    // エネミー初期化（マネージャ初期化）
    g_pBossEnemy = nullptr;
    g_BossDefeated = false;
    g_KillCount = 0;
    g_SpawnRequests.clear();
    g_EnemyManager.Initialize();
    const auto& spawns = Map_GetEnemySpawnPositions();
    for (int i = 0; i < static_cast<int>(spawns.size()); ++i)
    {
        // スポーン順番で種別を割り振る（比率は敵編成 g_EnemyMix による）
        ApplyEnemyHpScale(g_EnemyManager.Spawn(spawns[i], PickEnemyType(i)));
    }

    // ボス部屋フェーズのときのみボスをスポーン
    if (g_IsBossRoom)
    {
        const int bossIdx = g_EnemyManager.Spawn(Map_GetBossSpawnPosition(), g_BossType);
        g_pBossEnemy = &g_EnemyManager.GetEnemy(bossIdx);
        ApplyBossHpScale(*g_pBossEnemy);
    }

    // プレイヤー追従カメラ
    Player_Camera_Initialize();

    // ビルボード
    Billboard_Initialize();

    HUD_Initialize();
    Minimap_Initialize();

    // プレイヤー初期位置と進行方向（生成マップのスポーンへ）
    Player_Initialize(Map_GetSpawnPosition(), { 0.0f, 0.0f, 1.0f });


    ItemManager_Initialize();

    // ダメージポップアップ初期化
    DamagePopup_Initialize();
}

//==============================================================================
// エネミー再スポーン
//
// ■役割
// ・ダンジョン再生成後に EnemyManager を使って敵を再配置する
// ・Game_Initialize と同じ種別割り振りロジックで生成する
// ・Game_Manager.cpp のゴール到達処理から呼ばれる
//==============================================================================
void Game_RespawnEnemies()
{
    g_pBossEnemy = nullptr;
    g_BossDefeated = false;
    g_SpawnRequests.clear();   // 前のステージで出された召喚要求を持ち越さない
    g_EnemyManager.Initialize();

    const auto& spawns = Map_GetEnemySpawnPositions();
    for (int i = 0; i < static_cast<int>(spawns.size()); ++i)
    {
        ApplyEnemyHpScale(g_EnemyManager.Spawn(spawns[i], SpawnTypeAt(i)));
    }

    // ボス部屋フェーズのときのみボスをスポーン
    if (g_IsBossRoom)
    {
        const int bossIdx = g_EnemyManager.Spawn(Map_GetBossSpawnPosition(), g_BossType);
        g_pBossEnemy = &g_EnemyManager.GetEnemy(bossIdx);
        ApplyBossHpScale(*g_pBossEnemy);
    }
}

//==============================================================================
// ボスの種類設定
//==============================================================================
void Game_SetBossType(int type)
{
    g_BossType = IsBossType(static_cast<EnemyType>(type)) ? static_cast<EnemyType>(type) : EnemyType::Boss;
}

//==============================================================================
// ボスの状態（HUD のボス体力表示用）
//==============================================================================
bool Game_GetBossStatus(int* outHp, int* outMaxHp, const wchar_t** outName)
{
    if (!g_IsBossRoom || !g_pBossEnemy || g_BossDefeated) return false;
    if (outHp)    *outHp    = std::max(0, g_pBossEnemy->GetHP());
    if (outMaxHp) *outMaxHp = g_pBossEnemy->GetMaxHP();
    if (outName)  *outName  = g_pBossEnemy->GetDisplayName();
    return true;
}

//==============================================================================
// 撃破数
//==============================================================================
int  Game_GetKillCount()   { return g_KillCount; }
void Game_ResetKillCount() { g_KillCount = 0; }

//==============================================================================
// 出現要求（エネミーの更新中から呼んでよい。次の更新の後に出現する）
//==============================================================================
void Game_RequestEnemySpawn(const XMFLOAT3& pos, int type)
{
    g_SpawnRequests.push_back({ pos, type });
}

//==============================================================================
// 通常エネミーの耐久倍率設定
//==============================================================================
void Game_SetEnemyHpScale(float scale)
{
    g_EnemyHpScale = std::max(0.1f, scale);
}

void Game_SetBossHpScale(float scale)
{
    g_BossHpScale = std::max(0.1f, scale);
}

//==============================================================================
// ボス生存判定
//
// ■役割
// ・ボスが撃破済みかどうかを返す
// ・Game_Manager.cpp のゴール到達条件チェックに使用
//
// ■戻り値
// ・true  : ボスが生存中（ゴール無効）
// ・false : ボスが撃破済み（ゴール有効）
//==============================================================================
bool Game_IsBossAlive()
{
    return !g_BossDefeated;
}

//==============================================================================
// ボス部屋モード設定
//
// ■役割
// ・true のとき Game_RespawnEnemies / Game_Initialize でボスをスポーンする
// ・false のとき通常ダンジョンではボスを出現させない
//==============================================================================
void Game_SetBossRoomMode(bool isBossRoom)
{
    g_IsBossRoom = isBossRoom;
}

//==============================================================================
// 敵編成設定
//
// ■役割
// ・以降の Game_Initialize / Game_RespawnEnemies で使う敵種別の比率を切り替える
//==============================================================================
void Game_SetEnemyMix(EnemyMix mix)
{
    g_EnemyMix = mix;
}

//==============================================================================
// ボスの向き（正面ベクトル）を直接セット
// BossIntro_Start から呼ばれ、演出開始時にボスをプレイヤー方向へ向ける
//==============================================================================
void Game_SetBossLookDir(const XMFLOAT3& dir)
{
    if (g_pBossEnemy) g_pBossEnemy->SetFront(dir);
}

void Game_DrawEnemyMarkers()
{
    g_EnemyManager.DrawMarkers();
}

int Game_GetAliveEnemyCount()
{
    return g_EnemyManager.GetCount();
}

bool Game_GetEnemyPosition(int index, XMFLOAT3* outPos)
{
    if (!outPos || index < 0 || index >= g_EnemyManager.GetCount()) return false;
    *outPos = g_EnemyManager.GetPositionAt(index);
    return true;
}

bool Game_IsSurvivalMode()
{
    return g_IsSurvival;
}

void Game_SetSurvivalMode(bool val)
{
    g_IsSurvival = val;
}

void Game_SpawnEnemy(const XMFLOAT3& pos, int type)
{
    const int index = g_EnemyManager.Spawn(pos, static_cast<EnemyType>(type));
    if (!IsBossType(static_cast<EnemyType>(type)))
        ApplyEnemyHpScale(index);
}

void Game_ClearEnemies()
{
    g_pBossEnemy   = nullptr;
    g_BossDefeated = false;
    g_EnemyManager.Initialize();
}

//==============================================================================
// ロックオン：カメラ中心レイに最も近いエネミーのワールド位置を返す
// ・毎フレーム再計算（最もレイ中心に近い敵を返す）
// ・視線チェックは壁のみ（床・天井は無視）
//==============================================================================
bool Game_GetLockOnWorldPos(XMFLOAT3* outPos)
{
    const int count = g_EnemyManager.GetCount();
    if (count == 0) return false;

    XMFLOAT3 camPosF   = Player_Camera_GetPosition();
    XMFLOAT3 camFrontF = Player_Camera_GetFront();
    XMFLOAT3 rayStartF = camPosF;
    rayStartF.y += 0.1f;

    XMVECTOR rayOrigin = XMLoadFloat3(&camPosF);
    XMVECTOR rayDir    = XMVector3Normalize(XMLoadFloat3(&camFrontF));

    constexpr float MAX_LOCK_DIST = 50.0f;
    constexpr float MAX_ANGLE_DEG = 15.0f;
    const float cosMaxAngle = cosf(XMConvertToRadians(MAX_ANGLE_DEG));

    float bestCos = cosMaxAngle;
    int   bestIdx = -1;

    for (int i = 0; i < count; ++i)
    {
        const Enemy& e = g_EnemyManager.GetEnemy(i);
        if (!e.IsAlive()) continue;

        XMFLOAT3 enemyPosF = e.GetPosition();
        enemyPosF.y += e.GetLockOnCenterOffset();

        XMVECTOR toEnemy = XMLoadFloat3(&enemyPosF) - rayOrigin;
        float    dist    = XMVectorGetX(XMVector3Length(toEnemy));
        if (dist <= 0.0f || dist > MAX_LOCK_DIST) continue;

        float cosAngle = XMVectorGetX(XMVector3Dot(rayDir, toEnemy / dist));
        if (cosAngle <= bestCos) continue;

        if (!Map_HasLineOfSight(rayStartF, enemyPosF, true)) continue;

        bestCos = cosAngle;
        bestIdx = i;
    }

    if (bestIdx < 0) return false;

    const Enemy& best = g_EnemyManager.GetEnemy(bestIdx);
    *outPos = best.GetPosition();
    outPos->y += best.GetLockOnCenterOffset();
    return true;
}

//==============================================================================
// 毎フレームの更新処理
//==============================================================================
void Game_Update(double elapsed_time)
{
    // 起動直後・再生成直後の dt 暴走対策
    const double MAX_DT = 1.0 / 30.0; // 33ms
    if (elapsed_time > MAX_DT)
        elapsed_time = MAX_DT;

    //--------------------------------------------------------------------------
    // デバッグ：F3 キーで HUD の表示/非表示トグル
    //--------------------------------------------------------------------------
    if (KeyLogger_IsTrigger(KK_F3))
        g_HudVisible = !g_HudVisible;

    //--------------------------------------------------------------------------
    // デバッグ：F9 キーで全種類アイテムを足元にスポーン（長押しで連射）
    //--------------------------------------------------------------------------
    {
        static double s_ItemSpawnTimer = 0.0;
        const double  SPAWN_INTERVAL   = 0.15; // 連射間隔（秒）

        if (KeyLogger_IsPressed(KK_F9))
        {
            s_ItemSpawnTimer -= elapsed_time;
            if (s_ItemSpawnTimer <= 0.0)
            {
                const XMFLOAT3 pos = Player_GetPosition();
                for (int i = 0; i < 12; ++i)
                    ItemManager_Spawn(ItemType::ATK_UP, pos);
                s_ItemSpawnTimer = SPAWN_INTERVAL;
            }
        }
        else
        {
            s_ItemSpawnTimer = 0.0; // 離したらリセット（次押し即発動）
        }
    }
    //--------------------------------------------------------------------------

    //--------------------------------------------------------------------------
    // ボス登場演出中はゲームロジックをスキップ
    // ただしボス自身の Update はバレルアニメ（BossPhase::INTRO）のために呼ぶ
    // 仕様：イントロ開始→ボスアニメ開始→ボスアニメ終了→イントロ終了
    //--------------------------------------------------------------------------
    if (BossIntro_IsPlaying())
    {
        BossIntro_Update(elapsed_time);
        if (g_pBossEnemy) g_pBossEnemy->Update(elapsed_time);
        Player_Update(elapsed_time); // 入力無効中でも重力・物理だけ動かす（Player.cpp 684行の分岐で処理）
        return;
    }

    if (BossDefeat_IsPlaying())
    {
        BossDefeat_Update(elapsed_time);
        SparkEffect_Update(elapsed_time);
        Player_Update(elapsed_time);

        return;
    }

    //--------------------------------------------------------------------------
    // HUDデザイン切り替え（F2キー）
    if (KeyLogger_IsTrigger(KK_F2))
        HUD_SetUseNewDesign(!HUD_GetUseNewDesign());

    // ダンジョン再生成（Rキー）
    // ・マップを再生成して、プレイヤーを安全スポーン位置へ移動する
    //--------------------------------------------------------------------------
    if (KeyLogger_IsTrigger(KK_R))
    {
        Map_GenerateDungeon(++g_DungeonSeed);
        Map_RegisterFloors();

        Player_SetPosition(Map_GetSpawnPosition(), true);
        Player_ResetHP(); // 　HP回復

        Enemy_Finalize();        // 一旦全削除

        ItemManager_Finalize();   // 既存アイテムを全削除
        ItemManager_Initialize(); // 新しいマップ用に再配置
        const auto& spawns = Map_GetEnemySpawnPositions();
        for (const auto& p : spawns)
        {
            Enemy_Spawn(p);
        }
    }

    // プレイヤーとカメラ、弾、被弾エフェクトの更新
    Player_Update(elapsed_time);

    // ロックオンカメラアシスト
    // ・Bボタン（トリガー）でON/OFFトグル（モード状態を記憶）
    // ・右スティック入力中だけ一時抑制（モード状態は変わらない）
    //   → 離したら ON モードなら自動で再ロックオン
    {
        static bool s_LockOnCamMode = false;

        // Xボタンでモードトグル（Bはダッシュ専用のため競合回避）
        if (PadLogger_IsTrigger(PAD_X))
            s_LockOnCamMode = !s_LockOnCamMode;

        // 右スティック入力中だけ一時抑制（モードは変えない）
        float rx = 0.0f, ry = 0.0f;
        PadLogger_GetRightStick(&rx, &ry);
        const bool rightStickMoved = sqrtf(rx * rx + ry * ry) > 0.2f;

        // ON かつスティック非入力のときだけアシスト有効
        if (s_LockOnCamMode && !rightStickMoved)
        {
            XMFLOAT3 lockPos;
            if (Game_GetLockOnWorldPos(&lockPos))
                Player_Camera_SetLockOnAssist(&lockPos);
            else
                Player_Camera_SetLockOnAssist(nullptr);
        }
        else
        {
            Player_Camera_SetLockOnAssist(nullptr);
        }
    }

    Player_Camera_Update(elapsed_time);

    EnemyBullet_Update(elapsed_time);
    Bullet_Update(elapsed_time);
    BulletHitEffect_Update();
    SparkEffect_Update(elapsed_time);

    HUD_Update(elapsed_time);

    // ボス撃破チェック（Update より前に実施：演出開始後に Update を走らせることで
    // BossDefeat_IsPlaying()==true の状態で死亡フラグのセットを抑制できる）
    if (!g_BossDefeated && g_pBossEnemy && g_pBossEnemy->IsDead())
    {
        g_BossDefeated = true;
        EnemyDex_RecordKill(g_pBossEnemy->GetTypeId());   // ボスは撃破演出の後に消えるのでここで記録する
        const XMFLOAT3 bossPos = g_pBossEnemy->GetPosition();
        g_pBossEnemy = nullptr;
        BossDefeat_Start(bossPos, OnBossVanish);
    }

    // 追尾エネミー更新
    EnemyAI_UpdateAlert(static_cast<float>(elapsed_time));
    g_EnemyManager.Update(elapsed_time);

    // 更新中にエネミー（ボスの召喚など）が出した出現要求をここでまとめて処理する
    //（更新ループの途中で配列に追加すると走査中の要素が壊れるため）
    FlushSpawnRequests();

    if (!BossDefeat_IsPlaying())
    {
        g_KillCount += CountDeadEnemies();
        g_EnemyManager.RemoveDead();
    }

    // ミサイル爆発エリアダメージ（BulletManager に蓄積された爆発を消費）
    {
        const int ec = Bullet_GetPendingExplosionCount();
        const int enemyCnt = g_EnemyManager.GetCount();

        // ① 全爆発のダメージを先に適用する（この時点では Kill しない）。
        //    先に Kill すると、同フレームの2発目（両手同時振り等）が !IsAlive でスキップされ、
        //    片方のダメージしか入らなくなるため。撃破処理は②でまとめて1回だけ行う。
        for (int i = 0; i < ec; ++i)
        {
            const ExplosionEvent exp = Bullet_GetPendingExplosion(i);
            const XMVECTOR vCenter = XMLoadFloat3(&exp.center);
            for (int j = 0; j < enemyCnt; ++j)
            {
                Enemy& e = g_EnemyManager.GetEnemy(j);
                if (!e.IsAlive()) continue;   // 前フレームまでに死亡した敵は対象外
                // 縦(Y)は vScale を掛けて距離を割り引く（vScale<1 で縦に広い楕円判定）
                XMVECTOR d = XMLoadFloat3(&e.GetPosition()) - vCenter;
                d = XMVectorSetY(d, XMVectorGetY(d) * exp.vScale);
                const float dist = XMVectorGetX(XMVector3Length(d));
                if (dist <= exp.radius)
                {
                    e.Damage(exp.damage);
                    if (exp.knockback > 0.0f)
                        e.ApplyKnockback(exp.center, exp.knockback);
                    // ポップアップはヒット箇所（敵位置）を中心に少しランダムにばらす
                    // （同じ敵に複数ヒットしても重ならないように）。
                    // 横は「画面の横＝カメラ右方向」でばらす（ワールドXだとカメラ向きでズレるため）。
                    auto rnd = [](float a) {
                        return (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * a;
                    };
                    XMFLOAT3 camF = Player_Camera_GetFront();
                    XMVECTOR camFwd = XMVector3Normalize(XMVectorSet(camF.x, 0.0f, camF.z, 0.0f));
                    XMVECTOR camRight = XMVector3Normalize(
                        XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), camFwd));
                    XMVECTOR popup = XMLoadFloat3(&e.GetPosition())
                        + camRight * rnd(0.2f)                                  // 画面横
                        + XMVectorSet(0.0f, 1.2f + rnd(0.2f), 0.0f, 0.0f);      // 上方向
                    XMFLOAT3 popupPos;
                    XMStoreFloat3(&popupPos, popup);
                    DamagePopup_Add(popupPos, exp.damage);
                }
            }
        }

        // ② このフレームの爆発で HP0 になった敵の撃破処理（各敵1回だけ）。
        if (ec > 0)
        {
            for (int j = 0; j < enemyCnt; ++j)
            {
                Enemy& e = g_EnemyManager.GetEnemy(j);
                if (e.IsAlive() && e.IsDead())
                {
                    e.Kill();
                    Enemy_PlayDeathSE();
                    Score_Addscore(e.GetKillScore());
                    ItemManager_SpawnRandom(e.GetPosition());
                }
            }
        }
        Bullet_ClearPendingExplosions();
    }

    ItemManager_Update();

    // ダメージポップアップ更新
    DamagePopup_Update(static_cast<float>(elapsed_time));

    // マップオブジェクトと弾の AABB 当たり判定
    for (int j = 0; j < Map_GetObjectsCount(); j++)
    {
        const MapObject* mo = Map_GetObject(j);

        // 床(KindId=0,1)は無視、壁(KindId=2)だけ判定
        if (mo->KindId == 0 || mo->KindId == 1) continue;

        for (int i = 0; i < Bullet_GetCount(); i++)
        {
            AABB bullet = Bullet_GetAABB(i);
            AABB mapObj = mo->Aabb;

            if (Collision_IsOverLapAABB(bullet, mapObj))
            {
                Bullet_Destroy(i);
                i--;  // インデックス調整
                break;
            }
        }
    }
}

//==============================================================================
// 描画処理
//==============================================================================
void Game_Draw()
{
    // ---------------------------------------------------------------------
    // ライティング設定
    // ---------------------------------------------------------------------
    //Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
    //
    //Light_SetDirectionalWorld(
    //    { 0.0f, -1.0f, 0.0f, 0.0f },
    //    { 0.3f,  0.3f,  0.3f, 1.0f }
    //);
    //
    //Light_SetSpecularWorld(
    //    Player_Camera_GetPosition(),
    //    10.0f,
    //    { 0.4f, 0.4f, 0.4f, 1.0f }
    //);

    // ---------------------------------------------------------------------
    // シャドウモード取得（オプション画面で変更可能）
    //   0 = なし
    //   1 = シャドウマップのみ
    //   2 = マル影のみ
    //   3 = 両方
    // ---------------------------------------------------------------------
    const int  shadowMode    = Option_GetShadowMode();
    // 0=なし / 1=低（マル影） / 2=中（シャドウマップ・ハード） / 3=高（シャドウマップ・PCF）
    const bool useShadowMap  = (shadowMode == 2 || shadowMode == 3);
    const bool useBlobShadow = (shadowMode == 1);
    const bool usePCF        = (shadowMode == 3);

    // ---------------------------------------------------------------------
    // シャドウパス（深度のみ書き込み）
    // ・ライト視点からエネミー・プレイヤーを描画してシャドウマップを生成
    // ・EndPass 後に ShadowMap::BindForMainPass で通常 PS(t7/b5/b8) にバインド
    // ---------------------------------------------------------------------
    // シャドウパス前に深度書き込みを有効にする（前フレームの2Dパスで無効化されている場合の対策）
    Direct3D_SetDepthEnable(true);

    if (useShadowMap && ShadowMap::IsEnabled())
    {
        const XMFLOAT3 lightDir  = { 1.0f, -0.8f, 0.5f }; // 斜め上から照射
        const XMFLOAT3 focusPos  = Player_GetPosition();

        ShadowMap::BeginPass(lightDir, focusPos, 25.0f, 0.5f, 80.0f);
        g_EnemyManager.DrawShadow();
        Player_DrawShadow();
        ShadowMap::EndPass();

        // メインRTV+DSV+ビューポートを復元
        Direct3D_BindMainRenderTarget();

        // シャドウ SRV / サンプラー / パラメータを PS にバインド（高=PCF / 中=ハード）
        ShadowMap::BindForMainPass(usePCF);
    }
    else
    {
        // シャドウマップ不使用：t7/b5/b8/s1 の残留バインドを解除
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        ID3D11ShaderResourceView* nullSRV = nullptr;
        ctx->PSSetShaderResources(7, 1, &nullSRV);
        ID3D11Buffer* nullCB = nullptr;
        ctx->PSSetConstantBuffers(5, 1, &nullCB);
        ctx->PSSetConstantBuffers(8, 1, &nullCB);
        ID3D11SamplerState* nullSS = nullptr;
        ctx->PSSetSamplers(1, 1, &nullSS);
    }

    // 3Dパス：前フレームの2Dパスで無効化した深度テスト+書き込みを有効化
    Direct3D_SetDepthEnable(true);

    // ---------------------------------------------------------------------
    // サンプラー設定
    // ---------------------------------------------------------------------
    Sampler_SetFilterAnisotropic();

    // ---------------------------------------------------------------------
    // Map 描画（マル影を使う場合は b6 をセット、使わない場合は null）
    // ---------------------------------------------------------------------
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();

        if (useBlobShadow && BlobShadow::IsReady())
        {
            // マル影：プレイヤー足元を中心に投影
            // radius  = 0.20f（キャラ幅の半径・約20cm・CELL_SIZE=1mに対して適正）
            // softness = 0.15f（ぼかし幅）
            BlobShadow::SetToPixelShader(ctx, Player_GetPosition(), 0.20f, 0.15f, 0.55f);
        }
        else
        {
            // マル影無効：b6 を null にしてシェーダー内の影計算をスキップ
            ID3D11Buffer* nullCB = nullptr;
            ctx->PSSetConstantBuffers(6, 1, &nullCB);
        }

        // スカイボックス（Map_Draw より前に描く）
        {
            const XMMATRIX view = XMLoadFloat4x4(&Player_Camera_GetViewMatrix());
            const XMMATRIX proj = XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix());
            Skybox_Draw(view, proj);
        }

        Map_Draw();

        // b6 クリア（次の描画への影響防止）
        ID3D11Buffer* nullCB = nullptr;
        ctx->PSSetConstantBuffers(6, 1, &nullCB);
    }

    // ---------------------------------------------------------------------
    // 3Dオブジェクト描画（モデル類）
    // ---------------------------------------------------------------------


    // エネミー描画（モデル扱いなので丸影なし側）
    //複数種版に変更
    g_EnemyManager.Draw();




    ItemManager_Draw();

    Bullet_Draw();
    EnemyBullet_Draw();
    BulletHitEffect_Draw();
    SparkEffect_Draw();

    Player_Draw();

    // サバイバル：ショップの目印（body＋シールド球）を 3D ワールドパス内で描画する。
    // ここ（壁の深度が残っている・HUD の ClearDepth より前）で描くことで
    // 壁に正しく隠れ、かつ後続の 2D/ポーズ描画のブレンド状態を壊さない。
    if (g_IsSurvival)
        Shop_DrawWorld();

    // ゴールビルボード（半透明）はモデルを全部描いた後に描画
    // → 透過部分越しにエネミー・プレイヤーが正しく見える
    Map_DrawGoal();

    //==========================================================
    // デバッグ: AABBを可視化
    //==========================================================

    // 壁AABBを赤色で表示
    for (int i = 0; i < Map_GetObjectsCount(); i++)
    {
        const MapObject* mo = Map_GetObject(i);
        if (mo->KindId == 2) // KIND_WALL
        {
            Collision_DebugDraw(mo->Aabb, { 1.0f, 0.0f, 0.0f, 1.0f }); // 赤
        }
    }

    // 弾AABBを黄色で表示
    for (int i = 0; i < Bullet_GetCount(); i++)
    {
        AABB bulletAABB = Bullet_GetAABB(i);
        Collision_DebugDraw(bulletAABB, { 1.0f, 1.0f, 0.0f, 1.0f }); // 黄色
    }

    //========================
    // ミニマップ描画

    //========================
    MiniMap_Render3D(); // オフスクリーンに3D描画

    // 2Dパス開始：深度テスト無効化
    // スプライトが z=0 を深度バッファに書き込むと後続スプライト（フェード等）が
    // 深度テストで落ちて描画されなくなる問題を防ぐ
    Direct3D_SetDepthEnable(false);

    //HUD・ミニマップ描画（F3で一括表示/非表示切替可能）
    if (g_HudVisible && !BossDefeat_IsPlaying())
    {
        MiniMap_Draw2D();   // 画面に貼る
        HUD_Draw();
        // HUD_Draw が内部で SetDepthEnable(true) に戻すため、2Dパスのために再度無効化
        Direct3D_SetDepthEnable(false);
    }

    // ---- ロックオンサイト（2Dスプライト・スクリーン投影）----
    {
        XMFLOAT3 lockOnPos;
        if (Game_GetLockOnWorldPos(&lockOnPos))
        {
            const float W = static_cast<float>(Direct3D_GetBackBufferWidth());
            const float H = static_cast<float>(Direct3D_GetBackBufferHeight());

            XMMATRIX view = XMLoadFloat4x4(&Player_Camera_GetViewMatrix());
            XMMATRIX proj = XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix());

            XMVECTOR camPos = XMLoadFloat3(&Player_Camera_GetPosition());
            XMVECTOR ePos = XMLoadFloat3(&lockOnPos);
            float dist = XMVectorGetX(XMVector3Length(ePos - camPos));

            // ワールド座標 → スクリーン座標（実解像度で投影）
            XMVECTOR sc = XMVector3Project(
                ePos, 0.0f, 0.0f, W, H, 0.0f, 1.0f,
                proj, view, XMMatrixIdentity()
            );
            const float sx = XMVectorGetX(sc);
            const float sy = XMVectorGetY(sc);
            const float sz = XMVectorGetZ(sc);

            // 仮想座標系に変換（スプライトは1600×900空間で描画）
            const float vsx = sx * (static_cast<float>(SPRITE_SCREEN_W) / W);
            const float vsy = sy * (static_cast<float>(SPRITE_SCREEN_H) / H);

            // カメラ前方かつ画面内のみ描画
            if (sz > 0.0f && sz < 1.0f &&
                sx > 0.0f && sx < W &&
                sy > 0.0f && sy < H)
            {
                const int sightTex = g_TexLockon;//HUD_GetSightTexture();
                if (sightTex >= 0)
                {
                    // 距離でスケール（近い=大きい、遠い=小さい）
                    float drawSize = 600.0f / dist;
                    if (drawSize > 64.0f) drawSize = 64.0f;
                    if (drawSize < 8.0f) drawSize = 8.0f;

                    Sprite_Draw(
                        sightTex,
                        vsx - drawSize * 0.5f,
                        vsy - drawSize * 0.5f,
                        drawSize,
                        drawSize,
                        { 1.0f, 1.0f, 1.0f, 0.9f }
                    );
                }
            }
        }
    }

    // ---- ダメージポップアップ描画（最前面）----
    DamagePopup_Draw();
}

//==============================================================================
// ゲームプレイ終了処理（プレイ終了のたびに呼ぶ）
//==============================================================================
void Game_Finalize()
{
    DamagePopup_Finalize();
    ItemManager_Finalize();
    HUD_Finalize();
    Billboard_Finalize();

    // マップ関連
    Skybox_Finalize();
    Map_Finalize();

    // カメラ・プレイヤー
    Player_Camera_Finalize();
    Player_Finalize();

    // エネミー
    g_EnemyManager.Finalize();
    EnemyParts_Release();   // 共有パーツモデル（エネミーを解放した後に）

    // エフェクト／弾
    SparkEffect_Finalize();
    BulletHitEffect_Finalize();
    Bullet_Finalize();

    // エネミーバレット
    EnemyBullet_Finalize();
}


