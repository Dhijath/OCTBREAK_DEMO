/*==============================================================================

   ゲーム制御 [game.cpp]
                                                         Author : 51106
                                                         Date   : 2025/01/16
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
#include "DamagePopup.h"
#include "BossIntro.h"
#include "Shadow_Map.h"
#include "pad_logger.h"
#include "Option.h"




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

    // テスト用テクスチャID
    int g_TexTest = -1;

    // ダンジョン生成用シード（Rで更新）

    std::uint32_t g_DungeonSeed = 12345u;
    static EnemyManager g_EnemyManager;

    // ボス管理
    static Enemy* g_pBossEnemy = nullptr; // ボスへの生ポインタ（生存中のみ有効）
    static bool   g_BossDefeated = false;   // ボスが撃破されたか
    static bool   g_IsBossRoom = false;   // ボス部屋フェーズ中フラグ
    static int g_TexLockon = -1;
}

//==============================================================================
// ゲーム全体の初期化
//==============================================================================
void Game_Initialize()
{

    g_TexLockon = Texture_Load(L"Resource/Texture/Lockon.png");
    // 3D描画用ステートに戻す

    Shader3d_Begin();             // ← 3D用シェーダ・行列
    Direct3D_ConfigureOffScreenBuffer();

    EnemyBullet_Initialize();
    // 弾・被弾エフェクトの初期化
    Bullet_Initialize();
    BulletHitEffect_Initialize();
    SparkEffect_Initialize();

    // マップ初期化（ここでスポーン位置が確定する想定）
    Map_Initialize();


    // 念のため床登録を更新（支持面判定に使う場合がある）
    Map_RegisterFloors();

    // エネミー初期化（マネージャ初期化）
    g_pBossEnemy = nullptr;
    g_BossDefeated = false;
    g_EnemyManager.Initialize();
    const auto& spawns = Map_GetEnemySpawnPositions();
    for (int i = 0; i < static_cast<int>(spawns.size()); ++i)
    {
        // スポーン順番で種別を割り振る（3体に1体Sniper、5体に1体Tank、残りSpeed/Normal）
        EnemyType type = EnemyType::Normal;
        if (i % 5 == 0) type = EnemyType::Tank;
        else if (i % 3 == 0) type = EnemyType::Sniper;
        else if (i % 2 == 0) type = EnemyType::Speed;

        g_EnemyManager.Spawn(spawns[i], type);
    }

    // ボス部屋フェーズのときのみボスをスポーン
    if (g_IsBossRoom)
    {
        const int bossIdx = g_EnemyManager.Spawn(Map_GetBossSpawnPosition(), EnemyType::Boss);
        g_pBossEnemy = &g_EnemyManager.GetEnemy(bossIdx);
    }

    // プレイヤー追従カメラ
    Player_Camera_Initialize();

    // ビルボード
    Billboard_Initialize();

    HUD_Initialize();

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
    g_pBossEnemy = nullptr;   // 旧ポインタを先に無効化
    g_BossDefeated = false;
    g_EnemyManager.Initialize(); // 既存エネミーを全削除

    const auto& spawns = Map_GetEnemySpawnPositions();
    for (int i = 0; i < static_cast<int>(spawns.size()); ++i)
    {
        EnemyType type = EnemyType::Normal;
        if (i % 5 == 0) type = EnemyType::Tank;
        else if (i % 3 == 0) type = EnemyType::Sniper;
        else if (i % 2 == 0) type = EnemyType::Speed;

        g_EnemyManager.Spawn(spawns[i], type);
    }

    // ボス部屋フェーズのときのみボスをスポーン
    if (g_IsBossRoom)
    {
        const int bossIdx = g_EnemyManager.Spawn(Map_GetBossSpawnPosition(), EnemyType::Boss);
        g_pBossEnemy = &g_EnemyManager.GetEnemy(bossIdx);
    }
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
// ボスの向き（正面ベクトル）を直接セット
// BossIntro_Start から呼ばれ、演出開始時にボスをプレイヤー方向へ向ける
//==============================================================================
void Game_SetBossLookDir(const XMFLOAT3& dir)
{
    if (g_pBossEnemy) g_pBossEnemy->SetFront(dir);
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

        // Bボタンでモードトグル
        if (PadLogger_IsTrigger(PAD_B))
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

    // 追尾エネミー更新
    // エネミー側でプレイヤー衝突判定（ダメージ＋ノックバック）を実施
    //複数種版に変更
    g_EnemyManager.Update(elapsed_time);

    // ボス撃破チェック（RemoveDead の前に実施：削除後はポインタが無効になるため）
    if (!g_BossDefeated && g_pBossEnemy && !g_pBossEnemy->IsAlive())
    {
        g_BossDefeated = true;
        g_pBossEnemy = nullptr;
    }

    g_EnemyManager.RemoveDead();

    // ミサイル爆発エリアダメージ（BulletManager に蓄積された爆発を消費）
    {
        const int ec = Bullet_GetPendingExplosionCount();
        for (int i = 0; i < ec; ++i)
        {
            const ExplosionEvent exp = Bullet_GetPendingExplosion(i);
            const XMVECTOR vCenter = XMLoadFloat3(&exp.center);
            const int enemyCnt = g_EnemyManager.GetCount();
            for (int j = 0; j < enemyCnt; ++j)
            {
                Enemy& e = g_EnemyManager.GetEnemy(j);
                if (!e.IsAlive()) continue;
                const float dist = XMVectorGetX(XMVector3Length(
                    XMLoadFloat3(&e.GetPosition()) - vCenter));
                if (dist <= exp.radius)
                {
                    e.Damage(exp.damage);
                    // 爆発ダメージもポップアップ表示
                    XMFLOAT3 popupPos = e.GetPosition();
                    popupPos.y += 1.2f;
                    DamagePopup_Add(popupPos, exp.damage);
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

    MiniMap_Draw2D();   // 画面に貼る
    //HUD描画
    HUD_Draw();
    // HUD_Draw が内部で SetDepthEnable(true) に戻すため、2Dパスのために再度無効化
    Direct3D_SetDepthEnable(false);

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
// ゲーム終了処理（リソース解放）
//==============================================================================
void Game_Finalize()
{

    DamagePopup_Finalize();
    ItemManager_Finalize();
    HUD_Finalize();

    Billboard_Finalize();

    // マップ関連
    Map_Finalize();

    // フィールドメッシュ
    MeshField_Finalize();

    // カメラ・プレイヤー
    Player_Camera_Finalize();
    Player_Finalize();

    // エネミー
    g_EnemyManager.Finalize();

    // エフェクト／弾
    SparkEffect_Finalize();
    BulletHitEffect_Finalize();
    Bullet_Finalize();

    // エネミーバレット
    EnemyBullet_Finalize();

}


