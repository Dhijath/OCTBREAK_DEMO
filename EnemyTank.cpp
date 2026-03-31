/*==============================================================================

   タンク型エネミー [EnemyTank.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/24
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyTank.h"
#include "EnemyAI.h"
#include "model.h"
#include "map.h"
#include "Player.h"
#include "bullet.h"
#include "Light.h"
#include "Player_Camera.h"
#include "collision_obb.h"
#include "MapPatrolAI.h"
#include "score.h"
#include "ItemManager.h"
#include <algorithm>
#include <cmath>
#include "ModelToon.h"

using namespace DirectX;

//==============================================================================
// 初期化処理
//==============================================================================
void EnemyTank::Initialize(const XMFLOAT3& position)
{
    m_Position = position;
    m_Velocity = { 0,0,0 };
    m_Front = { 0,0,1 };
    m_Destination = MapPatrolAI_GetReachableDestination(m_Position);
    m_IsAlive = true;
    m_IsGround = false;
    m_WasChasing = false;

    SetHP(HP, HP);

    // タンク用モデルをロードする
    m_pModel  = ModelLoad("resource/Models/enemy_tank.fbx", ENEMY_SIZE * 2.5f);
    m_pShield = ModelLoad("resource/Models/Shield.fbx",     ENEMY_SIZE * 2.5f);
    m_pBarrel = ModelLoad("resource/Models/Barrel.fbx",     ENEMY_SIZE * 1.0f);
    ComputeLockOnOffsetFromModel();

    // 盾の状態を初期化する
    m_ShieldHp    = SHIELD_MAX_HP;
    m_ShieldAlive = true;

    Enemy_LoadSE();
}

//==============================================================================
// 更新処理
//==============================================================================
void EnemyTank::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    if (elapsed_time > 1.0 / 30.0)
        elapsed_time = 1.0 / 30.0;

    float dt = static_cast<float>(elapsed_time);

    XMVECTOR pos = XMLoadFloat3(&m_Position);
    XMVECTOR vel = XMLoadFloat3(&m_Velocity);

    EnemyAI_Update(
        &m_Position, &m_Velocity,
        &m_Front, &m_Destination,
        &m_WasChasing,
        &m_LastSeenPos, &m_InvestigateTimer,
        dt,
        CHASE_SPD,   // タンクの追跡速度
        PATROL_SPD,  // タンクの巡回速度
        SIGHT_DIST); // 視野距離

    vel = XMLoadFloat3(&m_Velocity);

    vel += XMVectorSet(0, -9.8f * GRAVITY_MUL * dt, 0, 0);
    vel = ClampXZSpeed(vel, MAX_SPEED * MAX_SPEED_MULT); // タンクは最大速度を半分に制限する
    vel += -vel * (FRICTION * dt);

    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);

    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    if (m_ContactDamageCooldown > 0.0f) m_ContactDamageCooldown -= dt;
    CheckShieldBulletHits(); // 盾判定（盾で消費した弾は本体に当たらない）
    ResolveBulletHits();

    if (IsDead() && IsAlive())
    {
        m_IsAlive = false;
        Score_Addscore(GetKillScore());
        ItemManager_SpawnRandom(m_Position);
    }
}

//==============================================================================
// 描画処理
//==============================================================================
void EnemyTank::Draw()
{
    if (!m_IsAlive) return;
    if (!m_pModel) return;
    Light_SetAmbient({ 1.0, 1.0, 1.0 });

    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        4.0f,
        { 0.2f, 0.2f, 4.4f, 1.0f }  // 青い光でタンクを識別する
    );

    float angle =
        -atan2f(m_Front.z, m_Front.x)
        + XMConvertToRadians(0.0f);

    XMMATRIX rot =
        XMMatrixRotationX(XMConvertToRadians(0.0f)) *
        XMMatrixRotationY(angle) *
        XMMatrixRotationY(XMConvertToRadians(-90.0f));

    XMMATRIX trans =
        XMMatrixTranslation(
            m_Position.x,
            m_Position.y + ENEMY_HEIGHT * 1.0f,
            m_Position.z);

    ModelDrawToon(m_pModel, rot * trans);

    // ---- 盾パーツ（生存中のみ描画）----
    if (m_pShield && m_ShieldAlive)
    {
        XMMATRIX shieldRot =
            XMMatrixRotationY(angle + XMConvertToRadians(m_ShieldRotY)) *
            XMMatrixRotationY(XMConvertToRadians(-90.0f));

        // m_Front（実際の向きベクトル）を直接使ってオフセットを計算
        XMMATRIX shieldTrans = XMMatrixTranslation(
            m_Position.x + m_Front.x * SHIELD_DIST,
            m_Position.y + ENEMY_HEIGHT,
            m_Position.z + m_Front.z * SHIELD_DIST);

        ModelDrawToon(m_pShield, shieldRot * shieldTrans);
    }

    // ---- 銃パーツ ----
    if (m_pBarrel)
    {
        XMMATRIX barrelRot =
            XMMatrixRotationX(XMConvertToRadians(m_BarrelRotX)) *
            XMMatrixRotationY(angle) *
            XMMatrixRotationY(XMConvertToRadians(-90.0f));

        // タンク本体の頂点 Y（ワールド空間）
        const XMFLOAT3 bodyOrigin = { m_Position.x, m_Position.y + ENEMY_HEIGHT, m_Position.z };
        const AABB bodyAABB  = ModelGetAABB(m_pModel,  bodyOrigin);

        // バレルの Y 中心（モデル空間）
        const AABB barrelLocal = ModelGetAABB(m_pBarrel, { 0.0f, 0.0f, 0.0f });
        const float barrelCenterY = (barrelLocal.max.y + barrelLocal.min.y) * 0.5f;

        constexpr float BARREL_DIST = -0.1f;  // 後ろ寄りに調整
        XMMATRIX barrelTrans = XMMatrixTranslation(
            m_Position.x + m_Front.x * BARREL_DIST,
            bodyAABB.max.y - barrelCenterY,    // 本体頂点にバレル中心を合わせる
            m_Position.z + m_Front.z * BARREL_DIST);

        ModelDraw(m_pBarrel, barrelRot * barrelTrans);
    }

    Light_SetAmbient({ 1.0, 1.0, 1.0 });
}

//==============================================================================
// 盾への弾当たり判定
//==============================================================================
void EnemyTank::CheckShieldBulletHits()
{
    if (!m_ShieldAlive || !m_pShield) return;

    // 盾のワールド AABB を計算する
    const XMFLOAT3 shieldPos = {
        m_Position.x + m_Front.x * SHIELD_DIST,
        m_Position.y + ENEMY_HEIGHT,
        m_Position.z + m_Front.z * SHIELD_DIST
    };
    const AABB shieldAABB = ModelGetAABB(m_pShield, shieldPos);

    for (int i = 0; i < Bullet_GetCount(); ++i)
    {
        const OBB bulletOBB = Bullet_GetOBB(i);
        if (!Collision_IsHitOBB_AABB(bulletOBB, shieldAABB).isHit) continue;

        const int dmg = Bullet_GetDamage(i);

        // ビーム弾は貫通するため消滅させない（ダメージのみ与える）
        if (!Bullet_IsBeam(i))
        {
            Bullet_Destroy(i);
            --i;
        }

        m_ShieldHp -= dmg;
        if (m_ShieldHp <= 0)
        {
            m_ShieldHp    = 0;
            m_ShieldAlive = false;
            break; // 盾が壊れたら以降の弾は本体 ResolveBulletHits() で処理
        }
    }
}
