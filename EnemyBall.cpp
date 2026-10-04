/*==============================================================================

   球体型エネミー（複数パーツ・アニメーション付き） [EnemyBall.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   概要は EnemyBall.h を参照。

   ■パーツの組み立て（ボディ中心が原点。モデルの正面は +Z）
     Wing    : 翼 L/R をボディ左右 (±0.28, 0.02, 0) に付け、Z 軸まわりに羽ばたく
     Gatling : 回転砲を右側 (0.42, -0.06, 0.06) に付け、砲身の軸（Z）まわりに回す
     Orbiter : 子機3つが半径 0.62m で周回する（上下にも揺れる）
     Walker  : 脚4本を腰 (±0.2, -0.2, ±0.2) に付け、対角の2本ずつ交互に振る
     Halo    : リングがボディまわりで回転しながらゆっくり傾く

   ■ダメージの目安（プレイヤーHP 8000）
     Wing 急降下 500 / 射撃 180      Gatling 70 × 18 連射
     Orbiter 子機 160（一斉射 140×3×3） Walker 踏みつけ 700・衝撃波 180
     Halo 螺旋弾 150
==============================================================================*/
#include "EnemyBall.h"
#include "EnemyParts.h"
#include "EnemyUtil.h"
#include "EnemyAI.h"
#include "MapPatrolAI.h"
#include "model.h"
#include "light.h"
#include "player_camera.h"
#include "Shadow_Map.h"
#include "particle_spark.h"
#include "score.h"
#include "ItemManager.h"
#include "WaveManager.h"
#include "game.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace EnemyUtil;

//==============================================================================
// 性能表
//==============================================================================
namespace
{
    struct Spec
    {
        const char*    body;
        int            hp;
        float          chaseSpeed;
        float          patrolSpeed;
        float          sight;
        int            score;
        float          hover;     // ボディ底面の地面からの高さ
        float          radius;    // ボディの球の半径（目の位置に使う）
        const wchar_t* name;
    };

    const Spec& SpecOf(EnemyBall::Kind kind)
    {
        //  body model                                      hp   chase patrol sight score hover radius name
        static const Spec specs[] =
        {
            { "resource/Models/enemy_wing_body.obj",     180, 5.0f, 1.8f, 18.0f, 1800, 0.45f, 0.32f, L"WING"    },
            { "resource/Models/enemy_gatling_body.obj",  520, 2.0f, 0.8f, 20.0f, 2600, 0.02f, 0.36f, L"GATLING" },
            { "resource/Models/enemy_orbiter_body.obj",  380, 3.0f, 1.2f, 18.0f, 2400, 0.22f, 0.34f, L"ORBITER" },
            { "resource/Models/enemy_walker_body.obj",   600, 3.2f, 1.2f, 16.0f, 2800, 0.32f, 0.34f, L"WALKER"  },
            { "resource/Models/enemy_halo_body.obj",     450, 2.4f, 1.0f, 20.0f, 2600, 0.25f, 0.30f, L"HALO"    },
        };
        return specs[static_cast<int>(kind)];
    }

    // パーツ
    const char* const WING_L   = "resource/Models/enemy_wing_l.obj";
    const char* const WING_R   = "resource/Models/enemy_wing_r.obj";
    const char* const GUN      = "resource/Models/enemy_gatling_gun.obj";
    const char* const BIT      = "resource/Models/enemy_orbiter_bit.obj";
    const char* const LEG      = "resource/Models/enemy_walker_leg.obj";
    const char* const RING     = "resource/Models/enemy_halo_ring.obj";

    constexpr int   BIT_COUNT  = 3;
    constexpr float BIT_RADIUS = 0.62f;

    // 脚の付け根と向き（右前・右後・左後・左前）。対角の脚は同じ位相で振る
    const XMFLOAT3 LEG_HIP[4]   = { { 0.2f, -0.2f, 0.2f }, { 0.2f, -0.2f, -0.2f }, { -0.2f, -0.2f, -0.2f }, { -0.2f, -0.2f, 0.2f } };
    const float    LEG_YAW[4]   = { 45.0f, 135.0f, -135.0f, -45.0f };
    const float    LEG_PHASE[4] = { 0.0f, XM_PI, 0.0f, XM_PI };

    void SetXZ(XMVECTOR& vel, float x, float z)
    {
        vel = XMVectorSetX(vel, x);
        vel = XMVectorSetZ(vel, z);
    }

    float Approach(float v, float target, float rate, float dt)
    {
        const float k = std::min(1.0f, rate * dt);
        return v + (target - v) * k;
    }

    void DrawPart(MODEL* model, const XMMATRIX& world, bool shadow)
    {
        if (!model) return;
        if (shadow) ShadowMap::DrawModel(model, world);
        else        ModelDraw(model, world);
    }
}

int   EnemyBall::GetKillScore() const               { return SpecOf(m_Kind).score; }
const wchar_t* EnemyBall::GetDisplayName() const     { return SpecOf(m_Kind).name; }
float EnemyBall::GetHoverHeight() const              { return SpecOf(m_Kind).hover; }
int   EnemyBall::SpecHP(Kind kind)                   { return SpecOf(kind).hp; }
float EnemyBall::SpecSpeed(Kind kind)                { return SpecOf(kind).chaseSpeed; }

//==============================================================================
// パーツの描画（インスタンス描画・影・図鑑プレビュー共通）
//==============================================================================
XMFLOAT3 EnemyBall::BitOffset(const Pose& pose, int index)
{
    const float a = pose.orbit + XM_2PI * index / BIT_COUNT;
    return { cosf(a) * BIT_RADIUS, 0.12f * sinf(a * 2.0f + index), sinf(a) * BIT_RADIUS };
}

void EnemyBall::DrawParts(Kind kind, const Pose& pose, const XMMATRIX& bodyWorld, bool shadow)
{
    const XMMATRIX body = XMMatrixTranslation(0.0f, pose.bob, 0.0f) * bodyWorld;

    // ボディ：目（マテリアル "eye"）だけ縦に縮めてまばたき・目を細める
    MODEL* bodyModel = EnemyParts_Get(SpecOf(kind).body);
    const int eyeMesh = shadow ? -1 : ModelFindMesh(bodyModel, "eye");
    if (eyeMesh < 0 || pose.eye >= 0.999f)
    {
        DrawPart(bodyModel, body, shadow);
    }
    else
    {
        const float eyeY = SpecOf(kind).radius * 0.185f;   // 目の中心の高さ（modelgen2 の目の形から）
        const XMMATRIX eyeWorld = XMMatrixTranslation(0.0f, -eyeY, 0.0f) * XMMatrixScaling(1.0f, pose.eye, 1.0f) *
                                  XMMatrixTranslation(0.0f, eyeY, 0.0f) * body;
        for (int i = 0; i < ModelGetMeshCount(bodyModel); ++i)
            ModelDrawMesh(bodyModel, i, (i == eyeMesh) ? eyeWorld : body);
    }

    switch (kind)
    {
    case Kind::Wing:
        DrawPart(EnemyParts_Get(WING_L), XMMatrixRotationZ(-pose.flap) * XMMatrixTranslation(-0.28f, 0.02f, 0.0f) * body, shadow);
        DrawPart(EnemyParts_Get(WING_R), XMMatrixRotationZ( pose.flap) * XMMatrixTranslation( 0.28f, 0.02f, 0.0f) * body, shadow);
        break;

    case Kind::Gatling:
        DrawPart(EnemyParts_Get(GUN), XMMatrixRotationZ(pose.gunSpin) * XMMatrixTranslation(0.42f, -0.06f, 0.06f) * body, shadow);
        break;

    case Kind::Orbiter:
        for (int i = 0; i < BIT_COUNT; ++i)
        {
            const XMFLOAT3 o = BitOffset(pose, i);
            // 子機は進行方向（周回の接線）を向く
            const float yaw = -(pose.orbit + XM_2PI * i / BIT_COUNT);
            DrawPart(EnemyParts_Get(BIT), XMMatrixRotationY(yaw) * XMMatrixTranslation(o.x, o.y, o.z) * body, shadow);
        }
        break;

    case Kind::Walker:
        for (int i = 0; i < 4; ++i)
        {
            const float swing = sinf(pose.walk + LEG_PHASE[i]) * 0.45f * pose.walkAmp;
            DrawPart(EnemyParts_Get(LEG),
                     XMMatrixRotationX(swing) * XMMatrixRotationY(XMConvertToRadians(LEG_YAW[i])) *
                     XMMatrixTranslation(LEG_HIP[i].x, LEG_HIP[i].y - pose.bob, LEG_HIP[i].z) * body, shadow);
        }
        break;

    case Kind::Halo:
        DrawPart(EnemyParts_Get(RING), XMMatrixRotationY(pose.ringSpin) * XMMatrixRotationX(pose.ringTilt) * body, shadow);
        break;

    default:
        break;
    }
}

void EnemyBall::DrawPreview(Kind kind, const XMMATRIX& world, float time)
{
    // アイドル中の動き（実機の Update と同じ式）
    Pose p;
    p.time     = time;
    p.flap     = 0.15f + sinf(time * 14.0f) * 0.45f;
    p.gunSpin  = time * 6.0f;
    p.orbit    = time * 2.0f;
    p.walk     = time * 6.0f;
    p.walkAmp  = 1.0f;
    p.ringSpin = time * 3.0f;
    p.ringTilt = 0.25f * sinf(time * 1.3f);
    p.bob      = (kind == Kind::Walker) ? fabsf(sinf(p.walk)) * 0.03f : sinf(time * 3.0f) * 0.04f;
    const float blink = fmodf(time, 3.2f);   // 3.2 秒ごとにまばたき
    p.eye      = (blink < 0.14f) ? 1.0f - 0.88f * sinf(blink / 0.14f * XM_PI) : 1.0f;
    DrawParts(kind, p, world, false);
}

//==============================================================================
// 初期化 / 終了
//==============================================================================
void EnemyBall::Initialize(const XMFLOAT3& position)
{
    const Spec& s = SpecOf(m_Kind);

    m_Position    = position;
    m_Velocity    = { 0.0f, 0.0f, 0.0f };
    m_Front       = { 0.0f, 0.0f, 1.0f };
    m_Destination = MapPatrolAI_GetReachableDestination(m_Position);
    m_IsAlive     = true;
    m_IsGround    = false;
    m_WasChasing  = false;

    SetHP(s.hp, s.hp);

    // ボディは共有モデル（Finalize で解放しない）。当たり判定・描画の高さはボディから決める
    m_pModel = EnemyParts_Get(s.body);
    ComputeLockOnOffsetFromModel();

    // 攻撃の開始と動きの位相をばらけさせる
    m_Cooldown    = 0.8f + static_cast<float>(rand() % 100) * 0.015f;
    m_StrafeSign  = (rand() & 1) ? 1.0f : -1.0f;
    m_StrafeTimer = 1.5f;
    m_Pose.time   = static_cast<float>(rand() % 1000) * 0.01f;
    m_Pose.orbit  = m_Pose.time;
    m_SpinSpeed   = 3.0f;

    Enemy_LoadSE();
}

void EnemyBall::Finalize()
{
    m_pModel = nullptr;   // EnemyParts が持っているので解放しない
}

//==============================================================================
// 共通の動き
//==============================================================================
void EnemyBall::Patrol(float dt, XMVECTOR& vel)
{
    const Spec& s = SpecOf(m_Kind);
    EnemyAI_Update(&m_Position, &m_Velocity, &m_Front, &m_Destination,
                   &m_WasChasing, &m_LastSeenPos, &m_InvestigateTimer,
                   dt, s.chaseSpeed, s.patrolSpeed, s.sight);
    vel = XMLoadFloat3(&m_Velocity);
}

void EnemyBall::FacePlayer(float dt, float turnSpeed)
{
    const XMFLOAT3 target = DirToPlayerXZ(m_Position);
    const float t = std::min(1.0f, turnSpeed * dt);
    XMVECTOR f = XMVectorLerp(XMLoadFloat3(&m_Front), XMLoadFloat3(&target), t);
    f = XMVectorSetY(f, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(f)) > 1e-6f)
        XMStoreFloat3(&m_Front, XMVector3Normalize(f));
}

XMMATRIX EnemyBall::BodyWorld() const
{
    return XMMatrixRotationY(atan2f(m_Front.x, m_Front.z)) *
           XMMatrixTranslation(m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z);
}

XMFLOAT3 EnemyBall::LocalToWorld(const XMFLOAT3& local) const
{
    XMFLOAT3 out;
    XMStoreFloat3(&out, XMVector3TransformCoord(XMLoadFloat3(&local),
                  XMMatrixTranslation(0.0f, m_Pose.bob, 0.0f) * BodyWorld()));
    return out;
}

//==============================================================================
// 行動（種類ごと）
//==============================================================================
void EnemyBall::Think(float dt, XMVECTOR& vel, float dist, bool seen)
{
    const XMFLOAT3 to = DirToPlayerXZ(m_Position);
    m_Cooldown -= dt;

    switch (m_Kind)
    {
    //--------------------------------------------------------------------------
    // Wing：7m 前後を旋回しながら単発射撃 → 翼をたたんで溜め → 急降下突撃
    //--------------------------------------------------------------------------
    case Kind::Wing:
    {
        constexpr float WINDUP = 0.35f, DASH_TIME = 0.45f, DASH_SPEED = 13.0f;
        if (m_State == 0)
        {
            if (!seen || dist > 16.0f) { Patrol(dt, vel); break; }
            FacePlayer(dt, 8.0f);
            m_SpeedCap = 5.5f;
            // 旋回：接線方向に回りつつ、7m に保つ
            const float radial = (dist - 7.0f) * 0.8f;
            SetXZ(vel, -to.z * 4.5f * m_StrafeSign + to.x * radial, to.x * 4.5f * m_StrafeSign + to.z * radial);

            m_SubTimer -= dt;
            if (m_SubTimer <= 0.0f)
            {
                const XMFLOAT3 from = LocalToWorld({ 0.0f, 0.0f, 0.35f });
                Fire(from, LeadDirection(from, 11.0f, 0.6f), 180, 11.0f);
                m_SubTimer = 1.4f;
            }
            if (m_Cooldown <= 0.0f && dist < 11.0f) { m_State = 1; m_StateTimer = 0.0f; }
        }
        else if (m_State == 1)   // 溜め
        {
            m_StateTimer += dt;
            m_Flash = m_StateTimer / WINDUP;
            FacePlayer(dt, 20.0f);
            SetXZ(vel, XMVectorGetX(vel) * 0.3f, XMVectorGetZ(vel) * 0.3f);
            if (m_StateTimer >= WINDUP)
            {
                m_DashDir = DirToPlayerXZ(m_Position);
                m_State = 2; m_StateTimer = 0.0f; m_HitDone = false;
            }
        }
        else                     // 急降下突撃
        {
            m_StateTimer += dt;
            m_Flash = 1.0f;
            m_SpeedCap = DASH_SPEED;
            SetXZ(vel, m_DashDir.x * DASH_SPEED, m_DashDir.z * DASH_SPEED);
            if (!m_HitDone && PlayerInReach(m_Position, 1.2f, 0.7f))
            {
                Player_TakeDamage(500);
                KnockbackPlayer(m_Position, 9.0f, 3.0f);
                m_HitDone = true;
            }
            if (m_StateTimer >= DASH_TIME)
            {
                m_State = 0; m_Flash = 0.0f;
                m_Cooldown = 3.0f;
                m_StrafeSign = -m_StrafeSign;
            }
        }
        break;
    }

    //--------------------------------------------------------------------------
    // Gatling：7〜15m を保つ → 砲身を回して加速（0.8 秒）→ 18 連射 → 冷却
    //--------------------------------------------------------------------------
    case Kind::Gatling:
    {
        constexpr float SPIN_UP = 0.8f;
        if (!seen || dist > 18.0f)
        {
            m_State = 0; m_Flash = 0.0f;
            Patrol(dt, vel);
            break;
        }
        FacePlayer(dt, m_State == 2 ? 2.5f : 6.0f);   // 撃っている間は旋回が鈍い（横へ逃げれば避けられる）
        if (dist < 7.0f)       SetXZ(vel, -to.x * 1.6f, -to.z * 1.6f);
        else if (dist > 15.0f) SetXZ(vel,  to.x * 2.0f,  to.z * 2.0f);
        else                   SetXZ(vel, 0.0f, 0.0f);

        if (m_State == 0 && m_Cooldown <= 0.0f) { m_State = 1; m_StateTimer = 0.0f; }
        if (m_State == 1)
        {
            m_StateTimer += dt;
            m_Flash = 0.4f * (m_StateTimer / SPIN_UP);
            if (m_StateTimer >= SPIN_UP) { m_State = 2; m_Shots = 18; m_SubTimer = 0.0f; m_Flash = 0.0f; }
        }
        else if (m_State == 2)
        {
            m_SubTimer -= dt;
            if (m_SubTimer <= 0.0f)
            {
                const XMFLOAT3 from = LocalToWorld({ 0.42f, -0.06f, 0.5f });
                const float jitter = XMConvertToRadians(static_cast<float>(rand() % 81 - 40) * 0.1f);
                // 砲口から正面へ撃つ（予測なし。向きの追従が遅いので横移動で外せる）
                Fire(from, RotateYaw(m_Front, jitter), 70, 13.0f);
                m_SubTimer = 0.1f;
                if (--m_Shots <= 0) { m_State = 0; m_Cooldown = 2.2f; }
            }
        }
        // 砲身の回転速度：待機 2 → 加速中・連射中 30
        m_SpinSpeed = Approach(m_SpinSpeed, (m_State == 0) ? 2.0f : 30.0f, 3.0f, dt);
        break;
    }

    //--------------------------------------------------------------------------
    // Orbiter：6〜12m で横移動。子機が順番に撃ち、6 秒ごとに3機で一斉射
    //--------------------------------------------------------------------------
    case Kind::Orbiter:
    {
        if (!seen || dist > 16.0f) { Patrol(dt, vel); break; }
        FacePlayer(dt, 6.0f);
        m_StrafeTimer -= dt;
        if (m_StrafeTimer <= 0.0f)
        {
            m_StrafeSign  = -m_StrafeSign;
            m_StrafeTimer = 2.0f + static_cast<float>(rand() % 100) * 0.01f;
        }
        if (dist < 6.0f)       SetXZ(vel, -to.x * 2.5f, -to.z * 2.5f);
        else if (dist > 12.0f) SetXZ(vel,  to.x * 2.8f,  to.z * 2.8f);
        else                   SetXZ(vel, -to.z * 2.0f * m_StrafeSign, to.x * 2.0f * m_StrafeSign);

        m_SubTimer -= dt;
        if (m_SubTimer <= 0.0f)
        {
            const XMFLOAT3 from = LocalToWorld(BitOffset(m_Pose, m_NextBit));
            Fire(from, LeadDirection(from, 10.0f, 0.5f), 160, 10.0f);
            SparkEffect_Create(from, 0.4f);
            m_NextBit  = (m_NextBit + 1) % BIT_COUNT;
            m_SubTimer = 1.0f;
        }
        if (m_Cooldown <= 0.0f)
        {
            for (int i = 0; i < BIT_COUNT; ++i)
            {
                const XMFLOAT3 from = LocalToWorld(BitOffset(m_Pose, i));
                const XMFLOAT3 dir  = LeadDirection(from, 9.0f, 0.7f);
                for (float deg : { -10.0f, 0.0f, 10.0f })
                    Fire(from, RotateYaw(dir, XMConvertToRadians(deg)), 140, 9.0f);
            }
            m_Cooldown = 6.0f;
        }
        break;
    }

    //--------------------------------------------------------------------------
    // Walker：歩いて接近 → 5m 以内で跳び上がる → 着地で踏みつけと衝撃波
    //--------------------------------------------------------------------------
    case Kind::Walker:
    {
        if (m_State == 0)
        {
            Patrol(dt, vel);
            if (seen && dist < 5.0f && m_Cooldown <= 0.0f && m_IsGround)
            {
                // 予兆：一瞬かがんで光る
                m_State = 1; m_StateTimer = 0.0f;
            }
        }
        else if (m_State == 1)
        {
            m_StateTimer += dt;
            m_Flash = m_StateTimer / 0.3f;
            FacePlayer(dt, 15.0f);
            SetXZ(vel, 0.0f, 0.0f);
            if (m_StateTimer >= 0.3f)
            {
                const float reach = std::min(dist, 4.5f) / 0.7f;   // 0.7 秒ほどで届く速さ
                vel = XMVectorSet(to.x * reach, 6.5f, to.z * reach, 0.0f);
                m_SpeedCap = 7.0f;
                m_State = 2; m_StateTimer = 0.0f;
                m_WasAirborne = false;
            }
        }
        else   // 跳躍中
        {
            m_StateTimer += dt;
            m_SpeedCap = 7.0f;
            if (!m_IsGround) m_WasAirborne = true;
            if (m_WasAirborne && m_IsGround && m_StateTimer > 0.15f)
            {
                const XMFLOAT3 c = { m_Position.x, m_Position.y + 0.15f, m_Position.z };
                SparkEffect_Create(c, 3.0f);
                for (int i = 0; i < 10; ++i)
                    Fire(c, DirFromYaw(XM_2PI * i / 10.0f), 180, 6.0f);
                if (PlayerInReach(m_Position, 2.4f, 0.5f))
                {
                    Player_TakeDamage(700);
                    KnockbackPlayer(m_Position, 12.0f, 4.0f);
                }
                m_State = 0; m_Flash = 0.0f; m_Cooldown = 3.0f;
                SetXZ(vel, 0.0f, 0.0f);
            }
            else if (m_StateTimer > 2.0f)   // 引っかかったときの保険
            {
                m_State = 0; m_Flash = 0.0f; m_Cooldown = 1.5f;
            }
        }
        break;
    }

    //--------------------------------------------------------------------------
    // Halo：8〜14m を保つ → リングを高速回転させて溜め → 螺旋弾幕（1.4 秒）
    //--------------------------------------------------------------------------
    case Kind::Halo:
    {
        constexpr float CHARGE = 0.8f, SPIRAL_TIME = 1.4f;
        if (!seen || dist > 18.0f)
        {
            m_State = 0; m_Flash = 0.0f;
            Patrol(dt, vel);
        }
        else
        {
            FacePlayer(dt, 4.0f);
            if (dist < 8.0f)       SetXZ(vel, -to.x * 2.0f, -to.z * 2.0f);
            else if (dist > 14.0f) SetXZ(vel,  to.x * 2.4f,  to.z * 2.4f);
            else                   SetXZ(vel, 0.0f, 0.0f);

            if (m_State == 0 && m_Cooldown <= 0.0f) { m_State = 1; m_StateTimer = 0.0f; }
            if (m_State == 1)
            {
                m_StateTimer += dt;
                m_Flash = m_StateTimer / CHARGE;
                if (m_StateTimer >= CHARGE)
                {
                    m_State = 2; m_StateTimer = 0.0f; m_SubTimer = 0.0f; m_Flash = 0.0f;
                    m_SpiralAngle = atan2f(to.x, to.z);
                }
            }
            else if (m_State == 2)
            {
                m_StateTimer += dt;
                m_SubTimer   -= dt;
                if (m_SubTimer <= 0.0f)
                {
                    const XMFLOAT3 from = { m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z };
                    Fire(from, DirFromYaw(m_SpiralAngle),         150, 6.0f);
                    Fire(from, DirFromYaw(m_SpiralAngle + XM_PI), 150, 6.0f);
                    m_SpiralAngle += XMConvertToRadians(17.0f);
                    m_SubTimer = 0.07f;
                }
                if (m_StateTimer >= SPIRAL_TIME) { m_State = 0; m_Cooldown = 4.5f; }
            }
        }
        // リングの回転速度：待機 3 → 溜め・弾幕中 25
        m_SpinSpeed = Approach(m_SpinSpeed, (m_State == 0) ? 3.0f : 25.0f, 4.0f, dt);
        break;
    }

    default:
        break;
    }
}

//==============================================================================
// 更新
//==============================================================================
void EnemyBall::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    const float dt = static_cast<float>(std::min(elapsed_time, 1.0 / 30.0));

    const Spec& s = SpecOf(m_Kind);
    const float dist = DistXZ(m_Position, Player_GetPosition());
    const bool  seen = (dist <= s.sight * EnemyAI_GetSightMultiplier())
                    && MapPatrolAI_HasLineOfSight(m_Position, Player_GetPosition());

    XMVECTOR vel = XMLoadFloat3(&m_Velocity);
    m_SpeedCap = MAX_SPEED;

    Think(dt, vel, dist, seen);

    XMVECTOR pos = XMLoadFloat3(&m_Position);
    const bool dashing  = (m_Kind == Kind::Wing && m_State == 2);
    const bool jumping  = (m_Kind == Kind::Walker && m_State == 2);

    vel += XMVectorSet(0.0f, -9.8f * GRAVITY_MUL * dt, 0.0f, 0.0f);
    vel  = ClampXZSpeed(vel, m_SpeedCap);
    if (!dashing && !jumping)
        vel += -vel * (FRICTION * dt);

    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);

    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    //--------------------------------------------------------------------------
    // アニメーション
    //--------------------------------------------------------------------------
    const float speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
    Pose& p = m_Pose;
    p.time += dt;
    switch (m_Kind)
    {
    case Kind::Wing:
        if (m_State == 1)      p.flap = Approach(p.flap, -0.6f, 12.0f, dt);                 // 溜め：翼をたたむ
        else if (m_State == 2) p.flap = -0.35f + sinf(p.time * 30.0f) * 0.1f;               // 突撃：後ろへ流す
        else                   p.flap = 0.15f + sinf(p.time * 14.0f) * 0.45f;              // 羽ばたき
        p.bob = sinf(p.time * 3.0f) * 0.05f;
        break;
    case Kind::Gatling:
        p.gunSpin += m_SpinSpeed * dt;
        p.bob = 0.0f;
        break;
    case Kind::Orbiter:
        p.orbit += (m_Cooldown < 1.0f ? 4.5f : 2.0f) * dt;   // 一斉射の前は速く回る
        p.bob = sinf(p.time * 2.5f) * 0.04f;
        break;
    case Kind::Walker:
        p.walkAmp = (m_State == 2) ? 0.0f : std::min(1.0f, speed / 2.5f);
        p.walk   += dt * (4.0f + speed * 3.0f);
        p.bob     = (m_State == 1) ? -0.08f : fabsf(sinf(p.walk)) * 0.03f * p.walkAmp;   // 溜めでかがむ
        break;
    case Kind::Halo:
        p.ringSpin += m_SpinSpeed * dt;
        p.ringTilt  = 0.25f * sinf(p.time * 1.3f);
        p.bob       = sinf(p.time * 2.0f) * 0.04f;
        break;
    default:
        break;
    }

    // まばたき（溜め中は発光の強さに合わせて目を細める）
    UpdateAnim(dt);
    p.eye = BlinkScale();
    if (m_Flash > 0.01f) p.eye = std::min(p.eye, 1.0f - 0.55f * std::min(m_Flash, 1.0f));

    if (m_ContactDamageCooldown > 0.0f) m_ContactDamageCooldown -= dt;
    ResolveBulletHits();

    // 死亡（Enemy::Update と同じ処理）
    if (IsDead() && IsAlive())
    {
        m_IsAlive = false;
        GiveReward();
        SparkEffect_Create({ m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z }, 1.5f);
    }
}

//==============================================================================
// 描画
//==============================================================================
void EnemyBall::Draw()
{
    if (!m_IsAlive || !m_pModel) return;

    Light_SetSpecularWorld(Player_Camera_GetPosition(), 8.0f, { 0.4f, 0.4f, 0.45f, 1.0f });

    // 溜め中は明るく光らせて予兆を見せる
    if (m_Flash > 0.01f)
    {
        const XMFLOAT3 prev = Light_GetAmbient();
        const float k = 1.0f + 2.0f * std::min(m_Flash, 1.0f);
        Light_SetAmbient({ prev.x * k, prev.y * k * 0.8f, prev.z * k * 0.6f });
        DrawParts(m_Kind, m_Pose, BodyWorld(), false);
        Light_SetAmbient(prev);
    }
    else
    {
        DrawParts(m_Kind, m_Pose, BodyWorld(), false);
    }
}

void EnemyBall::DrawShadow()
{
    if (!m_IsAlive || !m_pModel) return;
    DrawParts(m_Kind, m_Pose, BodyWorld(), true);
}
