/*==============================================================================

   スナイパー型エネミー [EnemySniper.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/24
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemySniper.h"
#include "EnemyAI.h"
#include "EnemyBullet.h"
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

using namespace DirectX;

//==============================================================================
// 初期化
//==============================================================================
void EnemySniper::Initialize(const XMFLOAT3& position)
{
    m_Position = position;
    m_Velocity = { 0,0,0 };
    m_Front = { 0,0,1 };
    m_Destination = MapPatrolAI_GetReachableDestination(m_Position);
    m_IsAlive = true;
    m_IsGround = false;
    m_WasChasing = false;
    m_shootTimer = 0.0f;

    SetHP(HP, HP);

    m_pModel = ModelLoad("resource/Models/enemy_sniper.fbx", ENEMY_SIZE * 0.85f);
    ComputeLockOnOffsetFromModel();

    Enemy_LoadSE();
}

//==============================================================================
// 更新処理
//==============================================================================
void EnemySniper::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    if (elapsed_time > 1.0 / 30.0)
        elapsed_time = 1.0 / 30.0;

    float dt = static_cast<float>(elapsed_time);

    XMVECTOR pos = XMLoadFloat3(&m_Position);
    XMVECTOR vel = XMLoadFloat3(&m_Velocity);

    float distToPlayer = GetDistanceToPlayer();

    // 射程内かつ視線が通る場合はその場で射撃する
    bool canShoot = (distToPlayer <= SHOOT_RANGE)
        && MapPatrolAI_HasLineOfSight(m_Position, Player_GetPosition());

    if (canShoot)
    {
        // プレイヤーの方向を向く
        XMFLOAT3 playerPos = Player_GetPosition();
        XMVECTOR toPlayer = XMLoadFloat3(&playerPos) - XMLoadFloat3(&m_Position);
        toPlayer = XMVectorSetY(toPlayer, 0.0f);
        if (XMVectorGetX(XMVector3LengthSq(toPlayer)) > 0.0001f)
        {
            XMFLOAT3 front;
            XMStoreFloat3(&front, XMVector3Normalize(toPlayer));
            m_Front = front;
        }

        // 移動速度をゼロにしてその場に留まる
        vel = XMVectorSetX(vel, 0.0f);
        vel = XMVectorSetZ(vel, 0.0f);

        // 射撃タイマーを進める
        m_shootTimer += dt;
        if (m_shootTimer >= SHOOT_INTERVAL)
        {
            Shoot();
            m_shootTimer = 0.0f;
        }
    }
    else
    {
        // 射程外では通常AI（追跡・巡回）を動かす
        EnemyAI_Update(
            &m_Position, &m_Velocity,
            &m_Front, &m_Destination,
            &m_WasChasing,
            &m_LastSeenPos, &m_InvestigateTimer,
            dt,
            CHASE_SPD,   // スナイパーの追跡速度
            PATROL_SPD,  // スナイパーの巡回速度
            SIGHT_DIST); // 視野距離

        vel = XMLoadFloat3(&m_Velocity);
        m_shootTimer = 0.0f;
    }

    vel += XMVectorSet(0, -9.8f * GRAVITY_MUL * dt, 0, 0);
    vel = ClampXZSpeed(vel, MAX_SPEED);
    vel += -vel * (FRICTION * dt);

    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);

    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    if (m_ContactDamageCooldown > 0.0f) m_ContactDamageCooldown -= dt;
    ResolveBulletHits();

    // 死亡判定：スコア・アイテムは GetKillScore() + Enemy 基底の Update 経由で処理しないため
    // ここで Enemy::Update() 相当の死亡処理を行う
    // （IsAlive() チェックで爆発 Kill() 済みの二重加算を防止）
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
void EnemySniper::Draw()
{
    Light_SetAmbient({ 1.0, 40.0, 1.0 });
    if (!m_IsAlive) return;
    if (!m_pModel) return;

    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        4.0f,
        { 0.1f, 4.0f, 0.1f, 1.0f }  // 緑みがかった光でスナイパーを識別する
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

    ModelDraw(m_pModel, rot * trans);
    Light_SetAmbient({ 1.0, 1.0, 1.0 });
}

//==============================================================================
// 射撃処理
//==============================================================================
void EnemySniper::Shoot()
{
    XMFLOAT3 playerPos = Player_GetPosition();

    // 発射位置をエネミーの正面・やや上方の位置にする
    XMFLOAT3 spawnPos = {
        m_Position.x + m_Front.x * 0.3f,
        m_Position.y + ENEMY_HEIGHT * 0.8f,
        m_Position.z + m_Front.z * 0.3f
    };

    // プレイヤーの胴体（中央部）を狙う
    XMFLOAT3 target = {
        playerPos.x,
        playerPos.y + 0.3f,
        playerPos.z
    };

    XMVECTOR dir = XMLoadFloat3(&target) - XMLoadFloat3(&spawnPos);
    XMFLOAT3 vel;
    XMStoreFloat3(&vel, XMVector3Normalize(dir));

    EnemyBullet_Create(spawnPos, vel, SHOOT_DAMAGE);
}

//==============================================================================
// プレイヤーまでの距離を取得する
//==============================================================================
float EnemySniper::GetDistanceToPlayer() const
{
    XMFLOAT3 playerPos = Player_GetPosition();
    float dx = playerPos.x - m_Position.x;
    float dz = playerPos.z - m_Position.z;
    return sqrtf(dx * dx + dz * dz);
}
