/*==============================================================================

   エネミー弾管理 [EnemyBullet.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/24
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyBullet.h"
#include "Trail.h"
#include "model.h"
#include "map.h"
#include "Player.h"
#include "collision.h"
#include "collision_obb.h"
#include "bullet_hit_effect.h"
#include "particle_spark.h"
#include <cmath>
#include <algorithm>
#include <utility>
#include "light.h"

using namespace DirectX;

//==============================================================================
// 定数
//==============================================================================
static constexpr int    MAX_ENEMY_BULLET = 128;   // 弾の最大発射数
static constexpr float  BULLET_SPEED = 8.0f;  // 移動速度（m/s）
static constexpr double BULLET_LIFE_TIME = 3.0;   // 寿命（秒）
static constexpr float  BULLET_SIZE = 0.05f; // モデルスケール
static constexpr float  BULLET_HALF_W = 0.1f;  // 衝突判定用の半径（X/Z）
static constexpr float  BULLET_HALF_H = 0.1f;  // 衝突判定用の半径（Y）

//==============================================================================
// 弾1発分のデータ構造体
//==============================================================================
struct EnemyBulletData
{
    XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };        // 現在位置（ワールド座標）
    XMFLOAT3 prevPosition = { 0.0f, 0.0f, 0.0f };    // 前フレーム位置
    XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };        // 速度ベクトル
    double   accumulatedTime = 0.0;                   // 経過時間
    int      damage = 0;                              // ダメージ量
    bool     destroyed = false;                      // 消滅フラグ
    MODEL* pModel = nullptr;                          // 使用するモデル
    Trail    trail = {};                             //   リボントレイル
};
//==============================================================================
// モジュールローカルグローバル変数
//==============================================================================
namespace
{
    EnemyBulletData g_Bullets[MAX_ENEMY_BULLET]; // 弾配列
    int             g_Count = 0;                 // 現在の弾数
    MODEL* g_pModel = nullptr;           // 弾のモデル

}

//==============================================================================
// 前方宣言（内部ヘルパー関数）
//==============================================================================
static void CheckWallCollision(EnemyBulletData& b);
static void CheckPlayerCollision(EnemyBulletData& b);

//==============================================================================
// システム初期化
//==============================================================================
void EnemyBullet_Initialize()
{
    g_Count = 0;
    g_pModel = ModelLoad("resource/Models/bullet.fbx", BULLET_SIZE);
}

//==============================================================================
// システム終了処理
//==============================================================================
void EnemyBullet_Finalize()
{
    for (int i = 0; i < g_Count; ++i)
        g_Bullets[i].trail.Finalize();
    g_Count = 0;
    ModelRelease(g_pModel);
    g_pModel = nullptr;
}

// アセットを解放せず弾だけ全消去（ルーム遷移時用）
void EnemyBullet_ClearAll()
{
    for (int i = 0; i < g_Count; ++i)
        g_Bullets[i].trail.Finalize();
    g_Count = 0;
}

//==============================================================================
// 全弾の更新
//==============================================================================
void EnemyBullet_Update(double elapsed_time)
{
    for (int i = 0; i < g_Count; ++i)
    {
        EnemyBulletData& b = g_Bullets[i];
        if (b.destroyed) continue;

        // 寿命チェック
        b.accumulatedTime += elapsed_time;
        if (b.accumulatedTime >= BULLET_LIFE_TIME)
        {
            b.destroyed = true;
            continue;
        }

        // 前フレーム位置を保存してから位置を更新する
        b.prevPosition = b.position;

        XMVECTOR pos = XMLoadFloat3(&b.position);
        XMVECTOR vel = XMLoadFloat3(&b.velocity);
        pos += vel * static_cast<float>(elapsed_time);
        XMStoreFloat3(&b.position, pos);

        // トレイル更新
        b.trail.Update(elapsed_time, b.position);

        // 壁との衝突チェック
        CheckWallCollision(b);
        if (b.destroyed) continue;

        // プレイヤーとの衝突チェック
        CheckPlayerCollision(b);
    }

    // 消えた弾をSwap & Popで詰める
    for (int i = 0; i < g_Count; )
    {
        if (g_Bullets[i].destroyed)
        {
            // Trail リソースを解放してから最後の要素とムーブ交換
            g_Bullets[i].trail.Finalize();
            std::swap(g_Bullets[i], g_Bullets[g_Count - 1]);
            g_Count--;
        }
        else
        {
            ++i;
        }
    }
}

//==============================================================================
// 全弾の描画
//==============================================================================
void EnemyBullet_Draw()
{
    if (!g_pModel) return;

    Light_SetAmbient({ 10.0, 1.0, 1.0 });
    for (int i = 0; i < g_Count; ++i)
    {
        const EnemyBulletData& b = g_Bullets[i];

        // 速度方向にXY両軸で向かせる（銃弾の挙動）
        XMVECTOR front = XMLoadFloat3(&b.velocity);
        if (XMVectorGetX(XMVector3LengthSq(front)) > 0.0001f)
            front = XMVector3Normalize(front);
        else
            front = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX viewLike = XMMatrixLookToLH(XMVectorZero(), front, up);
        const XMMATRIX rot = XMMatrixInverse(nullptr, viewLike);

        XMMATRIX trans = XMMatrixTranslation(b.position.x, b.position.y, b.position.z);

        // 弾ごとのモデルを優先、なければ共通モデルを使用
        MODEL* pDrawModel = b.pModel ? b.pModel : g_pModel;
        ModelDraw(pDrawModel, rot * trans);
    }

    Light_SetAmbient({ 1.0, 1.0, 1.0 });

    // トレイル描画（加算ブレンド・不透明物の後）
    for (int i = 0; i < g_Count; ++i)
        g_Bullets[i].trail.Draw();
}

//==============================================================================
// エネミー弾生成
//==============================================================================
void EnemyBullet_Create(const XMFLOAT3& position, const XMFLOAT3& velocity, int damage, float speed, MODEL* pModel)
{
    // 上限チェック
    if (g_Count >= MAX_ENEMY_BULLET) return;

    EnemyBulletData& b = g_Bullets[g_Count];

    b.position        = position;
    b.prevPosition    = position;
    b.accumulatedTime = 0.0;
    b.damage          = damage;
    b.destroyed       = false;
    b.pModel          = pModel; // nullptr なら Draw 時に g_pModel にフォールバック
    b.trail.Initialize(24, 0.08f, 0.20f, { 1.0f, 0.15f, 0.15f, 1.0f }); // 赤系トレイル

    // 速度を指定スピードに正規化する
    XMVECTOR v = XMLoadFloat3(&velocity);
    float len = XMVectorGetX(XMVector3Length(v));
    if (len > 0.0001f)
        v = v / len * speed;
    XMStoreFloat3(&b.velocity, v);

    ++g_Count;
}

//==============================================================================
// 弾数取得
//==============================================================================
int EnemyBullet_GetCount()
{
    return g_Count;
}

//==============================================================================
// 壁衝突チェック（内部ヘルパー）
//
// ■概要
// ・弾の現在位置からAABBと重なっていれば消滅フラグを立てる
// ・消滅時には前フレーム位置でヒットエフェクトを生成する
//
// ■引数
// ・b : 処理対象の弾データ（参照渡し）
//==============================================================================
static void CheckWallCollision(EnemyBulletData& b)
{
    // 弾の簡易AABBを生成する
    AABB bulletAABB =
    {
        { b.position.x - BULLET_HALF_W, b.position.y - BULLET_HALF_H, b.position.z - BULLET_HALF_W },
        { b.position.x + BULLET_HALF_W, b.position.y + BULLET_HALF_H, b.position.z + BULLET_HALF_W }
    };

    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* mo = Map_GetObject(i);
        if (!mo)           continue;
        if (mo->KindId != 2) continue; // 壁のみ判定する（KindId 2 = 壁）

        if (Collision_IsHitAABB(bulletAABB, mo->Aabb).isHit)
        {
            SparkEffect_Create(b.prevPosition, 1.0f); // エフェクトを前フレーム位置で生成する
            b.destroyed = true;
            return;
        }
    }
}

//==============================================================================
// プレイヤー衝突チェック（内部ヘルパー）
//
// ■概要
// ・プレイヤーOBBと弾AABBが重なっていればダメージを与えて消滅する
// ・プレイヤーが死亡または無敵中はスキップする
//
// ■引数
// ・b : 処理対象の弾データ（参照渡し）
//==============================================================================
static void CheckPlayerCollision(EnemyBulletData& b)
{
    // プレイヤーが死亡または無敵中はスキップする
    if (!Player_IsEnable())     return;
    if (Player_IsInvincible())  return;

    // 弾の簡易AABBを生成する
    AABB bulletAABB =
    {
        { b.position.x - BULLET_HALF_W, b.position.y - BULLET_HALF_H, b.position.z - BULLET_HALF_W },
        { b.position.x + BULLET_HALF_W, b.position.y + BULLET_HALF_H, b.position.z + BULLET_HALF_W }
    };

    OBB playerOBB = Player_GetOBB();

    if (Collision_IsHitOBB_AABB(playerOBB, bulletAABB).isHit)
    {
        Player_TakeDamage(b.damage);            // プレイヤーにダメージを与える
        SparkEffect_Create(b.position, 1.0f);   // ヒットエフェクトを生成する
        b.destroyed = true;
    }
}
