/*==============================================================================

   スピード型エネミー [EnemySpeed.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/24
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemySpeed.h"
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

using namespace DirectX;

//==============================================================================
// 初期化
//==============================================================================
void EnemySpeed::Initialize(const XMFLOAT3& position)
{
    m_Position = position;
    m_Velocity = { 0,0,0 };
    m_Front = { 0,0,1 };
    m_Destination = MapPatrolAI_GetReachableDestination(m_Position);
    m_IsAlive = true;
    m_IsGround = false;
    m_WasChasing = false;

    SetHP(HP, HP);

    // スピード用モデルをロードする（小さめのスケール）
    m_pModel = ModelLoad("resource/Models/robomodel.fbx", ENEMY_SIZE * 0.7f);
    ComputeLockOnOffsetFromModel();

    Enemy_LoadSE();
}

//==============================================================================
// 更新処理
//==============================================================================
void EnemySpeed::Update(double elapsed_time)
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
        CHASE_SPD,   // スピードの追跡速度
        PATROL_SPD,  // スピードの巡回速度
        SIGHT_DIST); // 視野距離

    vel = XMLoadFloat3(&m_Velocity);

    vel += XMVectorSet(0, -9.8f * GRAVITY_MUL * dt, 0, 0);
    vel = ClampXZSpeed(vel, MAX_SPEED * MAX_SPEED_MULT); // スピード型は最大速度をMAX_SPEED_MULT倍に拡大する
    vel += -vel * (FRICTION * dt);

    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);

    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    ResolveBulletHits();

    if (IsDead())
    {
        m_IsAlive = false;
        Score_Addscore(500);
        ItemManager_SpawnRandom(m_Position);
    }
}

//==============================================================================
// 描画処理
//==============================================================================
void EnemySpeed::Draw()
{
    Light_SetAmbient({ 4.0, 1.0, 1.0 });
    if (!m_IsAlive) return;
    if (!m_pModel) return;

    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        4.0f,
        { 4.4f, 0.1f, 0.1f, 1.0f }  // 赤みがかった光でスピード型を識別する
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
