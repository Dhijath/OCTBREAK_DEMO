/*==============================================================================

   追加エネミー [EnemyEx.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   概要は EnemyEx.h を参照。

   ■ダメージの目安（プレイヤーHP 8000）
     近接型の接触 90 / 狙撃型 405 と比べて
       自爆型の爆発 1300（半径 3m） / 突撃型 150×3 / 砲撃型 480×3 /
       固定砲台 220（全方位）・260（狙い撃ち） / 幻影型の斬撃 650
==============================================================================*/
#include "EnemyEx.h"
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
#include "EnemyParts.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace EnemyUtil;

//==============================================================================
// 性能表
//==============================================================================
const EnemyEx::Spec& EnemyEx::SpecOf(Kind kind)
{
    // model                                 hp   chase patrol sight score name
    static const Spec specs[] =
    {
        { "resource/Models/enemy_bomber.obj",     90, 6.5f, 2.0f, 14.0f, 1200, L"BOMBER"    },
        { "resource/Models/enemy_gunner.obj",    260, 3.5f, 1.2f, 15.0f, 2000, L"GUNNER"    },
        { "resource/Models/enemy_artillery.obj", 420, 1.2f, 0.6f, 26.0f, 2600, L"ARTILLERY" },
        { "resource/Models/enemy_turret.obj",    700, 0.0f, 0.0f, 20.0f, 3000, L"TURRET"    },
        { "resource/Models/enemy_phantom.obj",   200, 4.5f, 1.5f, 18.0f, 2400, L"PHANTOM"   },
    };
    return specs[static_cast<int>(kind)];
}

int EnemyEx::GetKillScore() const            { return SpecOf(m_Kind).score; }
const wchar_t* EnemyEx::GetDisplayName() const { return SpecOf(m_Kind).name; }

float EnemyEx::GetHoverHeight() const
{
    switch (m_Kind)
    {
    case Kind::Bomber:  return 0.05f;
    case Kind::Phantom: return 0.25f;   // 浮遊している
    default:            return 0.0f;
    }
}

//==============================================================================
// 初期化
//==============================================================================
void EnemyEx::Initialize(const XMFLOAT3& position)
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

    m_pModel = ModelLoad(s.model, 1.0f);
    ComputeLockOnOffsetFromModel();

    // モデルは足元が y=0 になるように作ってある（脚を別パーツにしたので本体のAABBの底は足元より上）。
    // 描画の持ち上げ量を「浮遊の高さだけ」に合わせ、ロックオン位置・当たり判定も同じだけずらす
    {
        const float delta = GetHoverHeight() - m_DrawOffsetY;
        m_DrawOffsetY        += delta;
        m_lockOnCenterOffset += delta;
        m_obbBottomY         += delta;
    }
    m_BaseYaw = atan2f(m_Front.x, m_Front.z);

    // 攻撃の開始をばらけさせる（同時に湧いた敵が一斉に撃たないように）
    m_Cooldown    = 0.5f + static_cast<float>(rand() % 100) * 0.015f;
    m_StrafeSign  = (rand() & 1) ? 1.0f : -1.0f;
    m_StrafeTimer = 1.5f;

    Enemy_LoadSE();
}

//==============================================================================
// 共通：巡回・追跡（EnemyAI）
//==============================================================================
void EnemyEx::Patrol(float dt, XMVECTOR& vel)
{
    const Spec& s = SpecOf(m_Kind);
    EnemyAI_Update(&m_Position, &m_Velocity, &m_Front, &m_Destination,
                   &m_WasChasing, &m_LastSeenPos, &m_InvestigateTimer,
                   dt, s.chaseSpeed, s.patrolSpeed, s.sight);
    vel = XMLoadFloat3(&m_Velocity);
}

void EnemyEx::FacePlayer(float dt, float turnSpeed)
{
    const XMFLOAT3 target = DirToPlayerXZ(m_Position);
    const float t = std::min(1.0f, turnSpeed * dt);
    XMVECTOR f = XMVectorLerp(XMLoadFloat3(&m_Front), XMLoadFloat3(&target), t);
    f = XMVectorSetY(f, 0.0f);
    if (XMVectorGetX(XMVector3LengthSq(f)) > 1e-6f)
        XMStoreFloat3(&m_Front, XMVector3Normalize(f));
}

XMFLOAT3 EnemyEx::Muzzle(float forward, float height) const
{
    return { m_Position.x + m_Front.x * forward, m_Position.y + height, m_Position.z + m_Front.z * forward };
}

// 水平速度を設定する（Y はそのまま）
static void SetXZ(XMVECTOR& vel, float x, float z)
{
    vel = XMVectorSetX(vel, x);
    vel = XMVectorSetZ(vel, z);
}

//==============================================================================
// 自爆型：接近 → 溜め（点滅・減速）→ 爆発
//==============================================================================
void EnemyEx::ThinkBomber(float dt, XMVECTOR& vel, float dist, bool seen)
{
    constexpr float ARM_RANGE  = 1.8f;
    constexpr float FUSE_TIME  = 0.55f;
    constexpr float BLAST_R    = 3.0f;
    constexpr int   BLAST_DMG  = 1300;

    m_SpeedCap = 7.0f;

    if (m_State == 0)
    {
        if (seen && dist < ARM_RANGE)
        {
            m_State = 1;
            m_StateTimer = 0.0f;
        }
        else
        {
            Patrol(dt, vel);
        }
        return;
    }

    // 溜め：急減速して点滅
    m_StateTimer += dt;
    m_Flash = 0.5f + 0.5f * sinf(m_StateTimer * 40.0f);
    SetXZ(vel, XMVectorGetX(vel) * 0.2f, XMVectorGetZ(vel) * 0.2f);

    if (m_StateTimer >= FUSE_TIME)
    {
        SparkEffect_Create({ m_Position.x, m_Position.y + 0.4f, m_Position.z }, 4.5f);
        if (DistXZ(m_Position, Player_GetPosition()) < BLAST_R)
        {
            Player_TakeDamage(BLAST_DMG);
            KnockbackPlayer(m_Position, 14.0f, 5.0f);
        }
        Enemy_PlayDeathSE();
        m_IsAlive = false;   // 自爆はスコア・ドロップなし
    }
}

//==============================================================================
// 突撃型：中距離を保って横移動しながら3連射
//==============================================================================
void EnemyEx::ThinkGunner(float dt, XMVECTOR& vel, float dist, bool seen)
{
    constexpr float NEAR_DIST = 5.0f, FAR_DIST = 11.0f, FIRE_RANGE = 14.0f;
    constexpr float BURST_GAP = 0.12f, BURST_INTERVAL = 1.6f;
    constexpr int   DAMAGE = 150;
    constexpr float BULLET_SPEED = 12.0f;

    if (!seen || dist > FIRE_RANGE)
    {
        m_Burst = 0;
        Patrol(dt, vel);
        return;
    }

    FacePlayer(dt, 10.0f);
    const XMFLOAT3 to = DirToPlayerXZ(m_Position);

    // 間合い：近すぎれば下がる、遠すぎれば寄る、ちょうどよければ横へ回り込む
    m_StrafeTimer -= dt;
    if (m_StrafeTimer <= 0.0f)
    {
        m_StrafeSign  = -m_StrafeSign;
        m_StrafeTimer = 1.8f + static_cast<float>(rand() % 100) * 0.012f;
    }
    if (dist < NEAR_DIST)      SetXZ(vel, -to.x * 2.6f, -to.z * 2.6f);
    else if (dist > FAR_DIST)  SetXZ(vel,  to.x * 3.0f,  to.z * 3.0f);
    else                       SetXZ(vel, -to.z * 2.2f * m_StrafeSign, to.x * 2.2f * m_StrafeSign);

    // 3連射（1発ごとに ±3° ばらつく予測射撃）
    m_Cooldown -= dt;
    if (m_Burst == 0 && m_Cooldown <= 0.0f)
    {
        m_Burst      = 3;
        m_BurstTimer = 0.0f;
        m_Cooldown   = BURST_INTERVAL;
    }
    if (m_Burst > 0)
    {
        m_BurstTimer -= dt;
        if (m_BurstTimer <= 0.0f)
        {
            const XMFLOAT3 from = Muzzle(0.35f, 0.42f);
            const float jitter = XMConvertToRadians(static_cast<float>(rand() % 61 - 30) * 0.1f);
            FireR(from, RotateYaw(LeadDirection(from, BULLET_SPEED, 0.7f), jitter), DAMAGE, BULLET_SPEED);
            --m_Burst;
            m_BurstTimer = BURST_GAP;
        }
    }
}

//==============================================================================
// 砲撃型：遠距離で停止 → 発光して溜め → 重い砲弾を3発（扇状・予測射撃）
//==============================================================================
void EnemyEx::ThinkArtillery(float dt, XMVECTOR& vel, float dist, bool seen)
{
    constexpr float FIRE_RANGE = 26.0f, TOO_CLOSE = 6.0f;
    constexpr float CHARGE_TIME = 0.8f, INTERVAL = 3.2f;
    constexpr int   DAMAGE = 480;
    constexpr float SHELL_SPEED = 7.5f;

    if (!seen || dist > FIRE_RANGE)
    {
        m_State = 0;
        m_Flash = 0.0f;
        Patrol(dt, vel);
        return;
    }

    FacePlayer(dt, 3.0f);
    const XMFLOAT3 to = DirToPlayerXZ(m_Position);
    if (dist < TOO_CLOSE) SetXZ(vel, -to.x * 1.5f, -to.z * 1.5f);   // 近づかれたら下がる
    else                  SetXZ(vel, 0.0f, 0.0f);

    if (m_State == 0)
    {
        m_Cooldown -= dt;
        if (m_Cooldown <= 0.0f) { m_State = 1; m_StateTimer = 0.0f; }
        return;
    }

    // 溜め（発光が強まる）→ 発射
    m_StateTimer += dt;
    m_Flash = m_StateTimer / CHARGE_TIME;
    if (m_StateTimer >= CHARGE_TIME)
    {
        const XMFLOAT3 from = Muzzle(0.9f, 0.48f);
        const XMFLOAT3 dir  = LeadDirection(from, SHELL_SPEED, 0.85f);
        for (float deg : { -6.0f, 0.0f, 6.0f })
            FireR(from, RotateYaw(dir, XMConvertToRadians(deg)), DAMAGE, SHELL_SPEED);
        SparkEffect_Create(from, 1.2f);
        m_State    = 0;
        m_Flash    = 0.0f;
        m_Cooldown = INTERVAL;
    }
}

//==============================================================================
// 固定砲台：動かない。一定間隔で全方位弾、その合間に2連の狙い撃ち
//==============================================================================
void EnemyEx::ThinkTurret(float dt, XMVECTOR& vel, float dist, bool seen)
{
    constexpr float RING_INTERVAL = 2.6f, AIM_INTERVAL = 1.3f;
    constexpr int   RING_COUNT = 12, RING_DAMAGE = 220, AIM_DAMAGE = 260;
    constexpr float RING_SPEED = 5.5f, AIM_SPEED = 10.0f;

    SetXZ(vel, 0.0f, 0.0f);
    if (!seen) return;

    FacePlayer(dt, 4.0f);

    m_StateTimer += dt;   // 全方位弾用
    m_Cooldown   -= dt;   // 狙い撃ち用

    if (m_StateTimer >= RING_INTERVAL)
    {
        m_StateTimer = 0.0f;
        const XMFLOAT3 from = { m_Position.x, m_Position.y + 0.55f, m_Position.z };
        for (int i = 0; i < RING_COUNT; ++i)
            FireR(from, DirFromYaw(m_Spin + XM_2PI * i / RING_COUNT), RING_DAMAGE, RING_SPEED);
        m_Spin += XMConvertToRadians(15.0f);   // 毎回少しずらして安全地帯を変える
    }

    if (m_Cooldown <= 0.0f && m_Burst == 0 && dist < 18.0f)
    {
        m_Burst = 2;
        m_BurstTimer = 0.0f;
        m_Cooldown = AIM_INTERVAL;
    }
    if (m_Burst > 0)
    {
        m_BurstTimer -= dt;
        if (m_BurstTimer <= 0.0f)
        {
            const XMFLOAT3 from = Muzzle(0.5f, 0.6f);
            FireR(from, LeadDirection(from, AIM_SPEED, 0.6f), AIM_DAMAGE, AIM_SPEED);
            --m_Burst;
            m_BurstTimer = 0.18f;
        }
    }
}

//==============================================================================
// 幻影型：接近 → プレイヤーの近くへ瞬間移動 → 溜め → 斬りかかる
//==============================================================================
void EnemyEx::ThinkPhantom(float dt, XMVECTOR& vel, float dist, bool seen)
{
    constexpr float BLINK_RANGE = 16.0f, BLINK_INTERVAL = 3.5f;
    constexpr float WINDUP = 0.4f, DASH_TIME = 0.25f, DASH_SPEED = 14.0f;
    constexpr float HIT_RANGE = 1.3f;
    constexpr int   DAMAGE = 650;

    switch (m_State)
    {
    case 0:   // 接近（通常AI）。条件がそろえば瞬間移動
    {
        m_SpeedCap = MAX_SPEED;
        m_Cooldown -= dt;
        Patrol(dt, vel);
        if (seen && dist < BLINK_RANGE && m_Cooldown <= 0.0f)
        {
            // プレイヤーから 1.5〜4.5m の見通せる地点へ
            const XMFLOAT3 p = Player_GetPosition();
            const XMFLOAT3 to = MapPatrolAI_GetNearbyDestination(p, 3.5f, 8);
            const float d = DistXZ(to, p);
            if (d > 1.5f && d < 4.5f && std::fabs(to.y - m_Position.y) < 1.5f)
            {
                SparkEffect_Create({ m_Position.x, m_Position.y + 0.5f, m_Position.z }, 1.0f);
                m_Position = { to.x, to.y, to.z };
                SparkEffect_Create({ m_Position.x, m_Position.y + 0.5f, m_Position.z }, 1.0f);
                SetXZ(vel, 0.0f, 0.0f);
                m_State = 1;
                m_StateTimer = 0.0f;
            }
            m_Cooldown = BLINK_INTERVAL;
        }
        break;
    }
    case 1:   // 溜め：プレイヤーを向いて止まる
        m_StateTimer += dt;
        m_Flash = m_StateTimer / WINDUP;
        FacePlayer(dt, 20.0f);
        SetXZ(vel, 0.0f, 0.0f);
        if (m_StateTimer >= WINDUP)
        {
            m_DashDir = DirToPlayerXZ(m_Position);
            m_State = 2;
            m_StateTimer = 0.0f;
            m_HitDone = false;
        }
        break;

    case 2:   // 斬りかかり：短い突進。触れたら1回だけダメージ
        m_StateTimer += dt;
        m_Flash = 1.0f;
        m_SpeedCap = DASH_SPEED;
        SetXZ(vel, m_DashDir.x * DASH_SPEED, m_DashDir.z * DASH_SPEED);
        if (!m_HitDone && DistXZ(m_Position, Player_GetPosition()) < HIT_RANGE)
        {
            Player_TakeDamage(DAMAGE);
            KnockbackPlayer(m_Position, 10.0f, 3.0f);
            m_HitDone = true;
        }
        if (m_StateTimer >= DASH_TIME)
        {
            m_State = 0;
            m_Flash = 0.0f;
        }
        break;
    }
}

//==============================================================================
// 更新
//==============================================================================
void EnemyEx::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    const float dt = static_cast<float>(std::min(elapsed_time, 1.0 / 30.0));

    // 視認：視野距離（難易度の倍率込み）以内で、視線が通っている
    const Spec& s = SpecOf(m_Kind);
    const float dist = DistXZ(m_Position, Player_GetPosition());
    const bool  seen = (dist <= s.sight * EnemyAI_GetSightMultiplier())
                    && MapPatrolAI_HasLineOfSight(m_Position, Player_GetPosition());

    XMVECTOR vel = XMLoadFloat3(&m_Velocity);
    m_SpeedCap = MAX_SPEED;

    switch (m_Kind)
    {
    case Kind::Bomber:    ThinkBomber   (dt, vel, dist, seen); break;
    case Kind::Gunner:    ThinkGunner   (dt, vel, dist, seen); break;
    case Kind::Artillery: ThinkArtillery(dt, vel, dist, seen); break;
    case Kind::Turret:    ThinkTurret   (dt, vel, dist, seen); break;
    case Kind::Phantom:   ThinkPhantom  (dt, vel, dist, seen); break;
    }
    if (!m_IsAlive) return;   // 自爆した

    // パーツのアニメーション（回転・歩行）
    {
        float spin = 0.0f;
        if (m_Kind == Kind::Bomber)  spin = (m_State == 1) ? 22.0f : 2.0f;                        // 点滅中は高速回転
        if (m_Kind == Kind::Phantom) spin = (m_State == 1) ? 10.0f : (m_State == 2) ? 18.0f : 1.4f;
        m_PartSpin += spin * dt;
        const float speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
        m_WalkPhase += dt * (4.0f + speed * 3.0f);
    }
    // 位置は行動の後に読む（幻影型の瞬間移動を反映するため）
    XMVECTOR pos = XMLoadFloat3(&m_Position);

    vel += XMVectorSet(0.0f, -9.8f * GRAVITY_MUL * dt, 0.0f, 0.0f);
    vel  = ClampXZSpeed(vel, m_SpeedCap);
    if (m_Kind != Kind::Phantom || m_State != 2)
        vel += -vel * (FRICTION * dt);

    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);

    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    UpdateAnim(dt);

    if (m_ContactDamageCooldown > 0.0f) m_ContactDamageCooldown -= dt;
    ResolveBulletHits();

    // 死亡（Enemy::Update と同じ処理）
    if (IsDead() && IsAlive())
    {
        m_IsAlive = false;
        Score_Addscore(GetKillScore());
        if (Game_IsSurvivalMode())
            WaveManager_AddCredits(GetKillScore() * 3);
        if (IsDropItem())
            ItemManager_SpawnRandom(m_Position);
        if (m_Kind == Kind::Bomber)   // 撃ち落とした自爆型は小さく誘爆する（ダメージなし）
            SparkEffect_Create({ m_Position.x, m_Position.y + 0.4f, m_Position.z }, 2.0f);
    }
}

//==============================================================================
// 発射（反動のアニメーションを付ける）
//==============================================================================
void EnemyEx::FireR(const XMFLOAT3& from, const XMFLOAT3& dir, int damage, float speed)
{
    Fire(from, dir, damage, speed);
    m_Recoil = 1.0f;
}

//==============================================================================
// 本体＋動くパーツ
//   Bomber    : トゲの輪が回る（点滅中は高速）
//   Gunner    : 脚が交互に振れて歩き、撃つと胴が後ろへ跳ねる
//   Artillery : 砲身が撃つと後退し、跳ね上がる
//   Turret    : 台座は動かず、ドーム（砲身）だけプレイヤーへ向く。撃つと砲身が引っ込む
//   Phantom   : 刃がゆっくり周回し、輪が逆向きに回る。溜め・斬撃で速くなる。本体は浮き沈みする
//==============================================================================
void EnemyEx::DrawRig(bool shadow)
{
    auto draw = [shadow](MODEL* m, const XMMATRIX& w)
    {
        if (!m) return;
        if (shadow) ShadowMap::DrawModel(m, w);
        else        ModelDraw(m, w);
    };

    const XMMATRIX at   = XMMatrixTranslation(m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z);
    const XMMATRIX base = XMMatrixRotationY(atan2f(m_Front.x, m_Front.z)) * at;
    const float    k    = m_Recoil * m_Recoil;
    const float    speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);

    switch (m_Kind)
    {
    case Kind::Bomber:
        draw(m_pModel, base);
        draw(EnemyParts_Get("resource/Models/enemy_bomber_spikes.obj"),
             XMMatrixRotationY(m_PartSpin) * XMMatrixTranslation(0.0f, 0.32f, 0.0f) * base);
        break;

    case Kind::Gunner:
    {
        const float amp = std::min(1.0f, speed / 2.5f);
        const float bob = fabsf(sinf(m_WalkPhase)) * 0.02f * amp;
        draw(m_pModel, XMMatrixTranslation(0.0f, bob, -0.05f * k) * base);
        for (int s = 0; s < 2; ++s)
        {
            const float side  = s ? 1.0f : -1.0f;
            const float swing = sinf(m_WalkPhase + (s ? XM_PI : 0.0f)) * 0.55f * amp;
            draw(EnemyParts_Get(s ? "resource/Models/enemy_gunner_leg_r.obj" : "resource/Models/enemy_gunner_leg_l.obj"),
                 XMMatrixRotationX(swing) * XMMatrixTranslation(side * 0.12f, 0.32f, 0.0f) * base);
        }
        break;
    }

    case Kind::Artillery:
        draw(m_pModel, base);
        draw(EnemyParts_Get("resource/Models/enemy_artillery_barrel.obj"),
             XMMatrixTranslation(0.0f, 0.0f, -0.16f * k) * XMMatrixRotationX(-(0.04f + 0.22f * k + 0.08f * m_Flash)) *
             XMMatrixTranslation(0.0f, 0.48f, 0.1f) * base);
        break;

    case Kind::Turret:
        draw(m_pModel, XMMatrixRotationY(m_BaseYaw) * at);
        draw(EnemyParts_Get("resource/Models/enemy_turret_dome.obj"),
             XMMatrixTranslation(0.0f, 0.0f, -0.07f * k) * XMMatrixRotationY(atan2f(m_Front.x, m_Front.z)) *
             XMMatrixTranslation(0.0f, 0.52f, 0.0f) * at);
        break;

    case Kind::Phantom:
    {
        const float bob = 0.04f + sinf(m_AnimTime * 2.4f) * 0.04f;   // 0〜0.08（浮いている）
        const XMMATRIX b = XMMatrixTranslation(0.0f, bob, 0.0f) * base;
        draw(m_pModel, b);
        draw(EnemyParts_Get("resource/Models/enemy_phantom_blades.obj"),
             XMMatrixRotationY(m_PartSpin) * XMMatrixTranslation(0.0f, 0.44f, 0.0f) * b);
        draw(EnemyParts_Get("resource/Models/enemy_phantom_ring.obj"),
             XMMatrixRotationY(-m_PartSpin * 1.6f) * XMMatrixTranslation(0.0f, 0.2f, 0.0f) * b);
        break;
    }
    }
}

//==============================================================================
// 描画
//==============================================================================
void EnemyEx::Draw()
{
    if (!m_IsAlive || !m_pModel) return;

    Light_SetSpecularWorld(Player_Camera_GetPosition(), 8.0f, { 0.4f, 0.4f, 0.45f, 1.0f });

    // 溜め中は明るく光らせて予兆を見せる
    if (m_Flash > 0.01f)
    {
        const XMFLOAT3 prev = Light_GetAmbient();
        const float k = 1.0f + 2.0f * std::min(m_Flash, 1.0f);
        Light_SetAmbient({ prev.x * k, prev.y * k * 0.8f, prev.z * k * 0.6f });
        DrawRig(false);
        Light_SetAmbient(prev);
    }
    else
    {
        DrawRig(false);
    }
}

void EnemyEx::DrawShadow()
{
    if (!m_IsAlive || !m_pModel) return;
    DrawRig(true);
}