/*==============================================================================

   追加ボス [EnemyBossEx.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   概要は EnemyBossEx.h を参照。

   ■行動の組み立て
     休み → パターン → 休み → 次のパターン … を3種類で循環する。
     パターンは m_Step（段階）と m_StepTimer（段階内の経過時間）で進める。
     激昂（体力50%以下）で休みが短くなり、各パターンの弾数・回数が増える。

   ■最適化
     ・視線判定はしない（ボス戦はアリーナで常にプレイヤーを把握している前提）
     ・弾の発射は1フレームあたり最大でも全方位弾1回分（24発）程度に抑える
==============================================================================*/
#include "EnemyBossEx.h"
#include "EnemyUtil.h"
#include "EnemyManager.h"
#include "MapPatrolAI.h"
#include "BossIntro.h"
#include "model.h"
#include "light.h"
#include "player_camera.h"
#include "Shadow_Map.h"
#include "particle_spark.h"
#include "score.h"
#include "WaveManager.h"
#include "Audio.h"
#include "game.h"
#include "EnemyParts.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;
using namespace EnemyUtil;

namespace
{
    constexpr int SUMMON_CAP = 12;   // 召喚はこの体数を超えてまでは行わない（ボス含む全エネミー数）

    void SetXZ(XMVECTOR& vel, float x, float z)
    {
        vel = XMVectorSetX(vel, x);
        vel = XMVectorSetZ(vel, z);
    }
}

//==============================================================================
// 性能表
//==============================================================================
const EnemyBossEx::Spec& EnemyBossEx::SpecOf(Kind kind)
{
    static const Spec specs[] =
    {
        { "resource/Models/boss_argus.obj",   60000, 15000, L"ARGUS"   },
        { "resource/Models/boss_goliath.obj", 90000, 18000, L"GOLIATH" },
        { "resource/Models/boss_omega.obj",   80000, 20000, L"OMEGA CORE" },
        { "resource/Models/boss_hydra.obj",   70000, 18000, L"HYDRA"   },
        { "resource/Models/boss_spectre.obj", 55000, 22000, L"SPECTRE" },
        { "resource/Models/boss_bastion.obj", 100000, 25000, L"BASTION" },
        { "resource/Models/boss_nest.obj",     85000, 24000, L"NEST"    },
        { "resource/Models/boss_eclipse.obj", 120000, 30000, L"ECLIPSE" },
    };
    return specs[static_cast<int>(kind)];
}

int EnemyBossEx::GetKillScore() const               { return SpecOf(m_Kind).score; }
const wchar_t* EnemyBossEx::GetDisplayName() const  { return SpecOf(m_Kind).name; }

float EnemyBossEx::GetHoverHeight() const
{
    switch (m_Kind)
    {
    case Kind::Argus:   return 0.9f;    // 浮遊
    case Kind::Spectre: return 0.25f;
    case Kind::Nest:    return 2.0f;    // 高く浮く母艦
    case Kind::Eclipse: return 0.8f;
    default:            return 0.0f;
    }
}

//==============================================================================
// 球体型ボスの動くパーツの位置（描画と、そこから撃つ処理で共用）
//==============================================================================
namespace
{
    // NEST の子機 i（0〜3）：機体のまわりを周回しながら上下に揺れる
    XMFLOAT3 NestPodLocal(int i, float t)
    {
        const float a = t * 0.9f + i * XM_PIDIV2;
        return { cosf(a) * 2.7f, 1.0f + sinf(t * 2.0f + i) * 0.18f, sinf(a) * 2.7f };
    }

    // ECLIPSE の衛星 i（0〜3）：リングの外側を傾いた軌道で周回する
    XMFLOAT3 EclipseBitLocal(int i, float t)
    {
        const float a = t * 1.3f + i * XM_PIDIV2;
        return { cosf(a) * 3.5f, 1.75f + sinf(a * 2.0f) * 0.5f, sinf(a) * 3.5f };
    }
}

XMMATRIX EnemyBossEx::RigBase() const
{
    return XMMatrixRotationY(atan2f(m_Front.x, m_Front.z)) *
           XMMatrixTranslation(m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z);
}

XMFLOAT3 EnemyBossEx::RigPoint(const XMFLOAT3& local) const
{
    XMFLOAT3 out;
    XMStoreFloat3(&out, XMVector3TransformCoord(XMLoadFloat3(&local), RigBase()));
    return out;
}

//==============================================================================
// 初期化 / 終了
//==============================================================================
void EnemyBossEx::Initialize(const XMFLOAT3& position)
{
    const Spec& s = SpecOf(m_Kind);

    m_Position    = position;
    m_Velocity    = { 0.0f, 0.0f, 0.0f };
    m_Front       = { 0.0f, 0.0f, -1.0f };
    m_Destination = position;
    m_IsAlive     = true;
    m_LastPos     = position;

    SetHP(s.hp, s.hp);

    m_pModel = ModelLoad(s.model, 1.0f);
    ComputeLockOnOffsetFromModel();

    // モデルは足元が y=0（脚・腕を別パーツにしたので本体のAABBの底は足元より上）。足元に合わせ直す
    {
        const float delta = GetHoverHeight() - m_DrawOffsetY;
        m_DrawOffsetY        += delta;
        m_lockOnCenterOffset += delta;
        m_obbBottomY         += delta;
    }

    m_ShootSE = LoadAudioWithVolume("resource/sound/shotgun.wav", 0.45f);
    SetAudioAttenuationEnabled(m_ShootSE, true);

    Enemy_LoadSE();
}

void EnemyBossEx::Finalize()
{
    UnloadAudio(m_ShootSE);
    m_ShootSE = -1;
    Enemy::Finalize();
}

XMFLOAT3 EnemyBossEx::Center(float height) const
{
    return { m_Position.x, m_Position.y + height, m_Position.z };
}

//==============================================================================
// 弾の撃ち方
//==============================================================================
void EnemyBossEx::Ring(int count, float speed, int damage, float height, float angleOffset)
{
    const XMFLOAT3 from = Center(height);
    for (int i = 0; i < count; ++i)
        FireR(from, DirFromYaw(angleOffset + XM_2PI * i / count), damage, speed);
    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
    PlayAudio(m_ShootSE, false);
}

void EnemyBossEx::AimedFan(int count, float spreadDeg, float speed, int damage, float lead, float height)
{
    const XMFLOAT3 from = { m_Position.x + m_Front.x * 1.0f, m_Position.y + height, m_Position.z + m_Front.z * 1.0f };
    FanFrom(from, count, spreadDeg, speed, damage, lead);
}

void EnemyBossEx::FanFrom(const XMFLOAT3& from, int count, float spreadDeg, float speed, int damage, float lead)
{
    const XMFLOAT3 dir  = LeadDirection(from, speed, lead);
    for (int i = 0; i < count; ++i)
    {
        const float t = (count == 1) ? 0.0f : (static_cast<float>(i) / (count - 1) - 0.5f);
        FireR(from, RotateYaw(dir, XMConvertToRadians(spreadDeg * t)), damage, speed);
    }
    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
    PlayAudio(m_ShootSE, false);
}

void EnemyBossEx::Summon(int type, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (Game_GetAliveEnemyCount() + i >= SUMMON_CAP) break;
        const XMFLOAT3 at = MapPatrolAI_GetNearbyDestination(m_Position, 7.0f, 8);
        Game_RequestEnemySpawn(at, type);
        SparkEffect_Create({ at.x, at.y + 0.5f, at.z }, 1.5f);
    }
}

// プレイヤーから minDist〜maxDist の見通せる地点へ瞬間移動する
bool EnemyBossEx::Blink(float minDist, float maxDist)
{
    const XMFLOAT3 p = Player_GetPosition();
    for (int tries = 0; tries < 4; ++tries)
    {
        const XMFLOAT3 to = MapPatrolAI_GetNearbyDestination(p, maxDist, 10);
        const float d = DistXZ(to, p);
        if (d < minDist || d > maxDist || std::fabs(to.y - m_Position.y) > 1.5f) continue;

        SparkEffect_Create(Center(1.2f), 3.0f);
        m_Position = to;
        m_LastPos  = to;
        SparkEffect_Create(Center(1.2f), 3.0f);
        return true;
    }
    return false;
}

//==============================================================================
// 移動（パターンとは独立。溜め中・突進中は行わない）
//==============================================================================
void EnemyBossEx::Move(float dt, XMVECTOR& vel, float dist)
{
    const XMFLOAT3 to = DirToPlayerXZ(m_Position);
    const float tx = -to.z, tz = to.x;   // 接線（周回方向）

    m_StrafeTimer -= dt;
    if (m_StrafeTimer <= 0.0f)
    {
        m_StrafeSign  = -m_StrafeSign;
        m_StrafeTimer = 2.0f + static_cast<float>(rand() % 100) * 0.015f;
    }

    const float rage = m_Enraged ? 1.3f : 1.0f;
    switch (m_Kind)
    {
    case Kind::Argus:     // 10〜14m を保って周回
        if (dist < 10.0f)      SetXZ(vel, -to.x * 4.0f, -to.z * 4.0f);
        else if (dist > 14.0f) SetXZ(vel,  to.x * 4.0f,  to.z * 4.0f);
        else                   SetXZ(vel, tx * 3.5f * rage * m_StrafeSign, tz * 3.5f * rage * m_StrafeSign);
        break;

    case Kind::Goliath:   // ゆっくり迫る
        if (dist > 4.5f) SetXZ(vel, to.x * 2.0f * rage, to.z * 2.0f * rage);
        else             SetXZ(vel, 0.0f, 0.0f);
        break;

    case Kind::Omega:     // 動かない
        SetXZ(vel, 0.0f, 0.0f);
        break;

    case Kind::Hydra:     // 9〜13m を保って横へ素早く動く
    {
        float ax = tx * 4.5f * m_StrafeSign, az = tz * 4.5f * m_StrafeSign;
        if (dist < 9.0f)  { ax -= to.x * 3.0f; az -= to.z * 3.0f; }
        if (dist > 13.0f) { ax += to.x * 3.0f; az += to.z * 3.0f; }
        SetXZ(vel, ax * rage, az * rage);
        break;
    }
    case Kind::Spectre:   // 6m 前後で周回
    {
        float ax = tx * 4.5f * m_StrafeSign, az = tz * 4.5f * m_StrafeSign;
        if (dist < 5.0f) { ax -= to.x * 3.0f; az -= to.z * 3.0f; }
        if (dist > 8.0f) { ax += to.x * 3.5f; az += to.z * 3.5f; }
        SetXZ(vel, ax * rage, az * rage);
        break;
    }
    case Kind::Bastion:   // 6〜9m まで迫り、そこからはじりじり横へ
        if (dist > 9.0f)      SetXZ(vel, to.x * 1.8f * rage, to.z * 1.8f * rage);
        else if (dist < 6.0f) SetXZ(vel, -to.x * 1.5f, -to.z * 1.5f);
        else                  SetXZ(vel, tx * 1.2f * m_StrafeSign, tz * 1.2f * m_StrafeSign);
        break;

    case Kind::Nest:      // 12〜17m を保って大きく周回
        if (dist < 12.0f)      SetXZ(vel, -to.x * 4.5f, -to.z * 4.5f);
        else if (dist > 17.0f) SetXZ(vel,  to.x * 4.5f,  to.z * 4.5f);
        else                   SetXZ(vel, tx * 4.0f * rage * m_StrafeSign, tz * 4.0f * rage * m_StrafeSign);
        break;

    case Kind::Eclipse:   // 9〜13m を保ってゆっくり周回
    {
        float ax = tx * 3.0f * m_StrafeSign, az = tz * 3.0f * m_StrafeSign;
        if (dist < 9.0f)  { ax -= to.x * 3.0f; az -= to.z * 3.0f; }
        if (dist > 13.0f) { ax += to.x * 3.0f; az += to.z * 3.0f; }
        SetXZ(vel, ax * rage, az * rage);
        break;
    }
    }
}

//==============================================================================
// 攻撃パターン（戻り値 true でパターン終了）
//==============================================================================
bool EnemyBossEx::RunPattern(int pattern, float dt)
{
    m_StepTimer += dt;
    const bool rage = m_Enraged;

    // 段階 m_Step を、前の段階から wait 秒たってから実行するための補助
    auto stepReady = [&](float wait) -> bool
    {
        if (m_StepTimer < wait) return false;
        m_StepTimer = 0.0f;
        return true;
    };

    switch (m_Kind)
    {
    //--------------------------------------------------------------------------
    case Kind::Argus:
        if (pattern == 0)   // 全方位弾（2〜3波。波ごとに角度を半分ずらす）
        {
            const int waves = rage ? 3 : 2;
            if (stepReady(m_Step == 0 ? 0.0f : 0.45f))
            {
                const int n = rage ? 24 : 16;
                Ring(n, 6.0f, 300, 1.3f, m_Spin + (m_Step % 2) * XM_PI / n);
                if (++m_Step >= waves) { m_Spin += 0.3f; return true; }
            }
        }
        else if (pattern == 1)   // 予測扇状弾（3〜5連）
        {
            const int volleys = rage ? 5 : 3;
            if (stepReady(m_Step == 0 ? 0.0f : 0.4f))
            {
                AimedFan(5, 40.0f, 9.0f, 300, 0.8f, 1.3f);
                if (++m_Step >= volleys) return true;
            }
        }
        else   // 突撃型を召喚（激昂中は全方位弾も添える）
        {
            Summon(static_cast<int>(EnemyType::Gunner), 2);
            if (rage) Ring(12, 5.0f, 260, 1.3f, m_Spin);
            return true;
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Goliath:
        if (pattern == 0)   // 衝撃波：足元の低い全方位弾を3〜4波
        {
            const int waves = rage ? 4 : 3;
            if (stepReady(m_Step == 0 ? 0.0f : 0.35f))
            {
                Ring(26, 6.5f, 380, 0.25f, m_Spin + m_Step * 0.12f);
                SparkEffect_Create(Center(0.2f), 3.5f);
                if (++m_Step >= waves) { m_Spin += 0.2f; return true; }
            }
        }
        else if (pattern == 1)   // 重砲：光って溜めてから重い砲弾（3〜5発の扇）
        {
            m_Hold  = true;
            m_Flash = std::min(1.0f, m_StepTimer / 0.6f);
            if (m_StepTimer >= 0.6f)
            {
                AimedFan(rage ? 5 : 3, 16.0f, 8.0f, 700, 0.9f, 2.2f);
                m_Flash = 0.0f;
                return true;
            }
        }
        else   // 突進：溜め（照準）→ 突進。壁に当たるか時間切れで止まる
        {
            if (m_Step == 0)
            {
                m_Hold  = true;
                m_Flash = std::min(1.0f, m_StepTimer / 0.7f);
                if (m_StepTimer >= 0.7f)
                {
                    const XMFLOAT3 d = LeadDirection(Center(0.3f), 26.0f, 0.5f);
                    const float len = std::sqrt(d.x * d.x + d.z * d.z);
                    m_DashDir   = (len > 1e-4f) ? XMFLOAT3{ d.x / len, 0.0f, d.z / len } : DirToPlayerXZ(m_Position);
                    m_DashSpeed = rage ? 32.0f : 26.0f;
                    m_Dashing   = true;
                    m_DashHit   = false;
                    m_Flash     = 0.0f;
                    m_Step      = 1;
                    m_StepTimer = 0.0f;
                }
            }
            else
            {
                if (!m_DashHit && DistXZ(m_Position, Player_GetPosition()) < 2.2f)
                {
                    Player_TakeDamage(2000);
                    KnockbackPlayer(m_Position, 30.0f, 6.0f);
                    m_DashHit = true;
                }
                if (!m_Dashing || m_StepTimer >= 0.5f)
                {
                    m_Dashing = false;
                    return true;
                }
            }
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Omega:
        if (pattern == 0)   // 螺旋弾幕（2本 / 激昂で4本）を4秒間
        {
            const int arms = rage ? 4 : 2;
            m_Spin += 2.2f * dt;
            if (stepReady(0.08f))
            {
                const XMFLOAT3 from = Center(2.0f);
                for (int k = 0; k < arms; ++k)
                    FireR(from, DirFromYaw(m_Spin + XM_2PI * k / arms), 240, 5.5f);
                ++m_Step;
            }
            if (m_Step * 0.08f >= 4.0f) return true;
        }
        else if (pattern == 1)   // 高速連射（10〜14発、わずかにばらつく）
        {
            const int shots = rage ? 14 : 10;
            if (stepReady(0.1f))
            {
                const XMFLOAT3 from = Center(2.2f);
                const float jitter = XMConvertToRadians(static_cast<float>(rand() % 81 - 40) * 0.1f);
                FireR(from, RotateYaw(LeadDirection(from, 13.0f, 0.8f), jitter), 200, 13.0f);
                if (m_Step % 3 == 0) { UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f); PlayAudio(m_ShootSE, false); }
                if (++m_Step >= shots) return true;
            }
        }
        else   // 固定砲台を召喚＋全方位弾
        {
            Summon(static_cast<int>(EnemyType::Turret), 2);
            Ring(20, 5.0f, 260, 2.0f, m_Spin);
            return true;
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Hydra:
        if (pattern == 0)   // 扇状の一斉射（左右に振りながら3〜5回）
        {
            const int volleys = rage ? 5 : 3;
            if (stepReady(m_Step == 0 ? 0.0f : 0.5f))
            {
                const float swing = (m_Step % 2 == 0) ? -20.0f : 20.0f;
                const XMFLOAT3 from = Center(2.4f);
                const XMFLOAT3 dir  = RotateYaw(LeadDirection(from, 8.0f, 0.5f), XMConvertToRadians(swing));
                for (int i = 0; i < 9; ++i)
                    FireR(from, RotateYaw(dir, XMConvertToRadians(-40.0f + 10.0f * i)), 280, 8.0f);
                UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                PlayAudio(m_ShootSE, false);
                if (++m_Step >= volleys) return true;
            }
        }
        else if (pattern == 1)   // 機雷：ゆっくり漂う弾をまき散らす
        {
            const int mines = rage ? 16 : 10;
            const XMFLOAT3 from = Center(0.4f);
            for (int i = 0; i < mines; ++i)
            {
                const float yaw   = XM_2PI * static_cast<float>(rand() % 1000) / 1000.0f;
                const float speed = 1.2f + static_cast<float>(rand() % 100) * 0.008f;
                FireR(from, DirFromYaw(yaw), 450, speed);
            }
            return true;
        }
        else   // 自爆型を召喚
        {
            Summon(static_cast<int>(EnemyType::Bomber), 3);
            return true;
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Spectre:
        if (pattern == 0)   // 瞬間移動 → 扇状弾（激昂で2回）
        {
            const int reps = rage ? 2 : 1;
            if (m_Step % 2 == 0)
            {
                if (stepReady(m_Step == 0 ? 0.0f : 0.5f)) { Blink(5.0f, 8.0f); ++m_Step; }
            }
            else
            {
                m_Hold = true;
                if (stepReady(0.3f))
                {
                    AimedFan(10, 60.0f, 10.0f, 320, 0.6f, 1.4f);
                    if (++m_Step >= reps * 2) return true;
                }
            }
        }
        else if (pattern == 1)   // 瞬間移動 → 溜め → 斬撃（突進）
        {
            if (m_Step == 0)
            {
                Blink(3.0f, 5.0f);
                m_Step = 1;
                m_StepTimer = 0.0f;
            }
            else if (m_Step == 1)
            {
                m_Hold  = true;
                m_Flash = std::min(1.0f, m_StepTimer / 0.45f);
                if (m_StepTimer >= 0.45f)
                {
                    m_DashDir   = DirToPlayerXZ(m_Position);
                    m_DashSpeed = 20.0f;
                    m_Dashing   = true;
                    m_DashHit   = false;
                    m_Flash     = 0.0f;
                    m_Step      = 2;
                    m_StepTimer = 0.0f;
                }
            }
            else
            {
                if (!m_DashHit && DistXZ(m_Position, Player_GetPosition()) < 1.8f)
                {
                    Player_TakeDamage(1500);
                    KnockbackPlayer(m_Position, 18.0f, 4.0f);
                    m_DashHit = true;
                }
                if (!m_Dashing || m_StepTimer >= 0.35f)
                {
                    m_Dashing = false;
                    return true;
                }
            }
        }
        else   // 幻影型を召喚
        {
            Summon(static_cast<int>(EnemyType::Phantom), 2);
            return true;
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Bastion:
        if (pattern == 0)   // 双腕ガトリング掃射：回転を上げてから、左右の腕が交差するように薙ぎ払う
        {
            constexpr float SPIN_UP = 0.7f;
            const float duration = rage ? 3.2f : 2.4f;
            if (m_Step == 0)
            {
                const float k = std::min(1.0f, m_StepTimer / SPIN_UP);
                m_GunSpinRate = 4.0f + 26.0f * k;
                m_Flash       = 0.6f * k;
                if (m_StepTimer >= SPIN_UP)
                {
                    m_Step = 1;
                    m_StepTimer = 0.0f;
                    m_PatternTime = 0.0f;
                    m_Flash = 0.0f;
                }
            }
            else
            {
                m_PatternTime += dt;
                const float u = std::min(1.0f, m_PatternTime / duration);
                if (stepReady(0.06f))
                {
                    // 銃口：機体の左右（向きに合わせて回す）。狙いはプレイヤーの方向から ±55° を交差して薙ぐ
                    const float    yaw   = atan2f(m_Front.x, m_Front.z);
                    const XMFLOAT3 to    = DirToPlayerXZ(m_Position);
                    const float    aim   = atan2f(to.x, to.z);
                    const float    sweep = XMConvertToRadians(55.0f - 110.0f * u);
                    for (float s : { -1.0f, 1.0f })
                    {
                        const XMFLOAT3 from = {
                            m_Position.x + cosf(yaw) * s * 1.68f + sinf(yaw) * 1.45f,
                            m_Position.y + m_DrawOffsetY + 1.55f,
                            m_Position.z - sinf(yaw) * s * 1.68f + cosf(yaw) * 1.45f };
                        FireR(from, DirFromYaw(aim + s * sweep), 150, rage ? 13.0f : 12.0f);
                    }
                    if (m_Shots++ % 3 == 0)
                    {
                        UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                        PlayAudio(m_ShootSE, false);
                    }
                }
                m_GunSpinRate = 30.0f;
                if (u >= 1.0f) { m_GunSpinRate = 4.0f; return true; }
            }
        }
        else if (pattern == 1)   // 十字砲火：回転する4本（激昂で6本）の弾の列を3秒間
        {
            const int arms = rage ? 6 : 4;
            m_Spin += (rage ? 1.0f : 0.8f) * dt;
            if (stepReady(0.08f))
            {
                const XMFLOAT3 from = Center(1.2f);
                for (int k = 0; k < arms; ++k)
                    FireR(from, DirFromYaw(m_Spin + XM_2PI * k / arms), 200, 6.5f);
                if (++m_Step % 4 == 0)
                {
                    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                    PlayAudio(m_ShootSE, false);
                }
            }
            if (m_Step * 0.08f >= 3.0f) return true;
        }
        else   // ガトリング型を投下して、足元から衝撃波（激昂で2波）
        {
            if (m_Step == 0)
            {
                Summon(static_cast<int>(EnemyType::Gatling), 2);
                Ring(24, 6.5f, 380, 0.25f, m_Spin);
                SparkEffect_Create(Center(0.2f), 3.5f);
                if (!rage) return true;
                m_Step = 1;
                m_StepTimer = 0.0f;
            }
            else if (stepReady(0.35f))
            {
                Ring(24, 6.5f, 380, 0.25f, m_Spin + XM_PI / 24.0f);
                SparkEffect_Create(Center(0.2f), 3.5f);
                return true;
            }
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Nest:
        if (pattern == 0)   // 翼型の射出：子機が光って溜めてから、翼型を3体（激昂で4体）放つ
        {
            m_Flash = std::min(1.0f, m_StepTimer / 0.6f);
            if (m_StepTimer >= 0.6f)
            {
                Summon(static_cast<int>(EnemyType::Wing), rage ? 4 : 3);
                for (int i = 0; i < 4; ++i) SparkEffect_Create(RigPoint(NestPodLocal(i, RigTime())), 1.5f);
                m_Flash = 0.0f;
                return true;
            }
        }
        else if (pattern == 1)   // 絨毯爆撃：着弾点を予告（火花が点滅）してから、各地点で全方位に炸裂
        {
            const int salvos = rage ? 2 : 1;
            if (m_Step == 0)
            {
                // 1つ目はプレイヤーの移動先、残りはそのまわりにばらまく
                const XMFLOAT3  p   = Player_GetPosition();
                const XMFLOAT3* v   = Player_GetVelocityPtr();
                const float     vx  = v ? v->x : 0.0f, vz = v ? v->z : 0.0f;
                m_MarkCount = rage ? 5 : 3;
                m_Marks[0]  = { p.x + vx * 0.8f, p.y, p.z + vz * 0.8f };
                for (int i = 1; i < m_MarkCount; ++i)
                {
                    const float a = XM_2PI * static_cast<float>(rand() % 1000) / 1000.0f;
                    const float r = 3.0f + static_cast<float>(rand() % 100) * 0.03f;
                    m_Marks[i] = { p.x + cosf(a) * r, p.y, p.z + sinf(a) * r };
                }
                m_Step       = 1;
                m_StepTimer  = 0.0f;
                m_BlinkTimer = 0.0f;
            }
            else
            {
                m_BlinkTimer -= dt;
                if (m_BlinkTimer <= 0.0f)
                {
                    m_BlinkTimer = 0.2f;
                    for (int i = 0; i < m_MarkCount; ++i)
                        SparkEffect_Create({ m_Marks[i].x, m_Marks[i].y + 0.3f, m_Marks[i].z }, 1.0f);
                }
                if (m_StepTimer >= 1.0f)
                {
                    for (int i = 0; i < m_MarkCount; ++i)
                    {
                        const XMFLOAT3 at = { m_Marks[i].x, m_Marks[i].y + 0.4f, m_Marks[i].z };
                        for (int k = 0; k < 10; ++k)
                            FireR(at, DirFromYaw(m_Spin + XM_2PI * k / 10.0f), 350, 6.0f);
                        SparkEffect_Create(at, 2.5f);
                    }
                    m_Spin += 0.31f;
                    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                    PlayAudio(m_ShootSE, false);
                    if (++m_Salvo >= salvos) return true;
                    m_Step = 0;
                }
            }
        }
        else   // 子機からの斉射：4つの子機が順番に3方向の弾を撃つ（2周 / 激昂で3周）
        {
            const int volleys = (rage ? 3 : 2) * 4;
            if (stepReady(m_Step == 0 ? 0.0f : 0.3f))
            {
                FanFrom(RigPoint(NestPodLocal(m_Step % 4, RigTime())), 3, 14.0f, 10.0f, 260, 0.7f);
                if (++m_Step >= volleys) return true;
            }
        }
        break;

    //--------------------------------------------------------------------------
    case Kind::Eclipse:
        if (pattern == 0)   // 二重螺旋：逆向きに回る2組の螺旋（3本ずつ / 激昂で4本ずつ）を3.5秒間
        {
            const int arms = rage ? 4 : 3;
            m_Spin += 1.6f * dt;
            if (stepReady(0.1f))
            {
                const XMFLOAT3 from = Center(1.4f);
                for (int k = 0; k < arms; ++k)
                {
                    const float a = XM_2PI * k / arms;
                    FireR(from, DirFromYaw(m_Spin + a), 220, 5.0f);
                    FireR(from, DirFromYaw(-m_Spin * 1.3f + a + XM_PI / arms), 220, 4.2f);
                }
                if (++m_Step % 4 == 0)
                {
                    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                    PlayAudio(m_ShootSE, false);
                }
            }
            if (m_Step * 0.1f >= 3.5f) return true;
        }
        else if (pattern == 1)   // 蝕の引力：光りながらプレイヤーを引き寄せ、全方位に炸裂（2波 / 激昂で3波）
        {
            if (m_Step == 0)
            {
                constexpr float PULL_TIME = 1.6f;
                m_Hold  = true;
                m_Flash = std::min(1.0f, m_StepTimer / PULL_TIME);

                // 引き寄せ（移動や回避で振り切れる強さ）。近すぎるときは引かない
                XMFLOAT3* v = Player_GetVelocityPtr();
                if (v && DistXZ(m_Position, Player_GetPosition()) > 2.5f)
                {
                    const XMFLOAT3 d = DirToPlayerXZ(m_Position);
                    v->x -= d.x * 16.0f * dt;
                    v->z -= d.z * 16.0f * dt;
                }
                m_BlinkTimer -= dt;
                if (m_BlinkTimer <= 0.0f)
                {
                    m_BlinkTimer = 0.3f;
                    SparkEffect_Create(Center(m_DrawOffsetY + 1.75f), 2.0f + 2.0f * m_Flash);
                }

                if (m_StepTimer >= PULL_TIME)
                {
                    Ring(32, 7.0f, 420, 1.0f, m_Spin);
                    SparkEffect_Create(Center(m_DrawOffsetY + 1.75f), 4.5f);
                    m_Flash     = 0.0f;
                    m_Step      = 1;
                    m_StepTimer = 0.0f;
                }
            }
            else if (stepReady(0.25f))
            {
                Ring(32, 6.0f, 420, 1.0f, m_Spin + m_Step * XM_PI / 32.0f);
                ++m_Step;
                if (!rage || m_Step >= 3) return true;
            }
        }
        else   // 衛星からの連射：4つの衛星が順番に狙い撃つ。激昂中は撃ち終わりに瞬間移動して光輪型を呼ぶ
        {
            const int shots = rage ? 24 : 16;
            if (stepReady(m_Step == 0 ? 0.0f : 0.11f))
            {
                const XMFLOAT3 from = RigPoint(EclipseBitLocal(m_Step % 4, RigTime()));
                FireR(from, LeadDirection(from, 12.0f, 0.7f), 200, 12.0f);
                if (m_Step % 4 == 0)
                {
                    UpdateAudioAttenuation(m_ShootSE, DistXZ(m_Position, Player_GetPosition()), 45.0f);
                    PlayAudio(m_ShootSE, false);
                }
                if (++m_Step >= shots)
                {
                    if (rage)
                    {
                        Blink(9.0f, 13.0f);
                        Summon(static_cast<int>(EnemyType::Halo), 1);
                    }
                    return true;
                }
            }
        }
        break;
    }
    return false;
}

//==============================================================================
// 更新
//==============================================================================
void EnemyBossEx::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    const float dt = static_cast<float>(std::min(elapsed_time, 1.0 / 30.0));

    XMVECTOR vel = XMLoadFloat3(&m_Velocity);
    m_Hold = false;

    const bool intro = BossIntro_IsPlaying();
    if (!intro && !IsDead())
    {
        m_Enraged = (GetHP() * 2 <= GetMaxHP());

        // 攻撃パターンの進行
        if (!m_InPattern)
        {
            m_RestTimer -= dt;
            if (m_RestTimer <= 0.0f)
            {
                m_InPattern   = true;
                m_Step        = 0;
                m_StepTimer   = 0.0f;
                m_PatternTime = 0.0f;
                m_Shots       = 0;
                m_Salvo       = 0;
                m_BlinkTimer  = 0.0f;
            }
        }
        else if (RunPattern(m_Pattern, dt))
        {
            m_InPattern = false;
            m_Pattern   = (m_Pattern + 1) % 3;
            m_RestTimer = m_Enraged ? 0.6f : 1.1f;
            m_Flash     = 0.0f;
        }

        // 移動（瞬間移動で位置が変わっている場合があるので距離はここで測る）
        const float dist = DistXZ(m_Position, Player_GetPosition());
        if (m_Dashing)       SetXZ(vel, m_DashDir.x * m_DashSpeed, m_DashDir.z * m_DashSpeed);
        else if (m_Hold)     SetXZ(vel, 0.0f, 0.0f);
        else                 Move(dt, vel, dist);
    }
    else
    {
        SetXZ(vel, 0.0f, 0.0f);   // 登場演出中・撃破後は動かない
    }

    // 向き：突進中は突進方向、それ以外はプレイヤーへ
    {
        const XMFLOAT3 target = m_Dashing ? m_DashDir : DirToPlayerXZ(m_Position);
        XMVECTOR f = XMVectorLerp(XMLoadFloat3(&m_Front), XMLoadFloat3(&target), std::min(1.0f, 6.0f * dt));
        f = XMVectorSetY(f, 0.0f);
        if (XMVectorGetX(XMVector3LengthSq(f)) > 1e-6f) XMStoreFloat3(&m_Front, XMVector3Normalize(f));
    }

    // 物理（重力・壁・床）
    XMVECTOR pos = XMLoadFloat3(&m_Position);
    vel += XMVectorSet(0.0f, -9.8f * GRAVITY_MUL * dt, 0.0f, 0.0f);
    vel  = ClampXZSpeed(vel, m_Dashing ? m_DashSpeed : MAX_SPEED * 1.2f);
    if (!m_Dashing) vel += -vel * (FRICTION * dt);

    const XMFLOAT3 before = m_Position;
    MoveWithSubSteps(&pos, &vel, dt);
    ResolveFloorCollision(&pos, &vel);
    ResolvePlayerCollision(&pos, &vel);
    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    // 突進が壁に止められたら（予定の 40% も進めなかったら）突進をやめる
    if (m_Dashing)
    {
        const float moved = DistXZ(before, m_Position);
        if (moved < m_DashSpeed * dt * 0.4f)
        {
            m_Dashing = false;
            SparkEffect_Create(Center(0.8f), 3.0f);
        }
    }

    // アニメーション（反動の戻り・歩行の位相）
    UpdateAnim(dt);
    {
        const float speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
        m_WalkPhase += dt * (2.5f + speed * 1.2f);
    }
    m_GunSpin += m_GunSpinRate * dt;   // BASTION のガトリング（掃射中は高速で回る）

    if (m_ContactDamageCooldown > 0.0f) m_ContactDamageCooldown -= dt;
    if (!intro) ResolveBulletHits();

    // 死亡：通常は撃破演出（BossDefeat）の終了まで遅らせる。サバイバルは即時
    if (IsDead() && IsAlive() && Game_IsSurvivalMode())
    {
        m_IsAlive = false;
        Score_Addscore(GetKillScore());
        WaveManager_AddCredits(GetKillScore() * 3);
    }
}

//==============================================================================
// 発射（反動のアニメーションを付ける）
//==============================================================================
void EnemyBossEx::FireR(const XMFLOAT3& from, const XMFLOAT3& dir, int damage, float speed)
{
    Fire(from, dir, damage, speed);
    m_Recoil = 1.0f;
}

//==============================================================================
// 本体＋動くパーツ
//   ARGUS   : 2本のリングが別々の軸で回る（激昂で速くなる）
//   GOLIATH : 4本の脚が対角ずつ交互に踏み出し、肩の重砲が撃つたびに後退する
//   OMEGA   : コアが回りながら浮き沈みし、2本のリングが直交して回る
//   HYDRA   : 3本の首がそれぞれゆらゆら揺れ、撃つと頭がのけぞる
//   SPECTRE : 腕（刃）がゆっくり揺れ、溜めで振りかぶり、斬撃（突進）で前へ振り抜く
//==============================================================================
void EnemyBossEx::DrawRig(bool shadow)
{
    auto draw = [shadow](MODEL* m, const XMMATRIX& w)
    {
        if (!m) return;
        if (shadow) ShadowMap::DrawModel(m, w);
        else        ModelDraw(m, w);
    };

    const XMMATRIX base = XMMatrixRotationY(atan2f(m_Front.x, m_Front.z)) *
                          XMMatrixTranslation(m_Position.x, m_Position.y + m_DrawOffsetY, m_Position.z);
    const float t     = m_AnimTime * (m_Enraged ? 1.6f : 1.0f);
    const float k     = m_Recoil * m_Recoil;
    const float speed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);

    switch (m_Kind)
    {
    case Kind::Argus:
    {
        draw(m_pModel, base);
        const XMMATRIX c = XMMatrixTranslation(0.0f, 1.2f, 0.0f) * base;
        draw(EnemyParts_Get("resource/Models/boss_argus_ring1.obj"), XMMatrixRotationY(t * 0.8f) * XMMatrixRotationX(-0.35f) * c);
        draw(EnemyParts_Get("resource/Models/boss_argus_ring2.obj"),
             XMMatrixRotationY(-t * 1.1f) * XMMatrixRotationZ(0.45f) * XMMatrixRotationY(1.0f) * c);
        break;
    }

    case Kind::Goliath:
    {
        const float amp = std::min(1.0f, speed / 3.0f + (m_Dashing ? 1.0f : 0.0f));
        const float bob = fabsf(sinf(m_WalkPhase)) * 0.06f * amp;   // 持ち上げる向きだけ
        const XMMATRIX body = XMMatrixTranslation(0.0f, bob, 0.0f) * base;
        draw(m_pModel, body);
        MODEL* leg = EnemyParts_Get("resource/Models/boss_goliath_leg.obj");
        const float lx[4] = { 0.95f, 0.95f, -0.95f, -0.95f }, lz[4] = { 0.8f, -0.8f, -0.8f, 0.8f };
        const float ph[4] = { 0.0f, XM_PI, 0.0f, XM_PI };   // 対角の脚が同じ位相
        for (int i = 0; i < 4; ++i)
            draw(leg, XMMatrixRotationX(sinf(m_WalkPhase + ph[i]) * 0.35f * amp) * XMMatrixTranslation(lx[i], 1.1f, lz[i]) * base);
        MODEL* cannon = EnemyParts_Get("resource/Models/boss_goliath_cannon.obj");
        for (float s : { -1.0f, 1.0f })
            draw(cannon, XMMatrixTranslation(0.0f, 0.0f, -0.3f * k) * XMMatrixRotationX(-0.12f * k) *
                         XMMatrixTranslation(s * 1.15f, 2.2f, 0.0f) * body);
        break;
    }

    case Kind::Omega:
    {
        draw(m_pModel, base);
        draw(EnemyParts_Get("resource/Models/boss_omega_core.obj"),
             XMMatrixRotationY(t * 1.5f) * XMMatrixTranslation(0.0f, 2.2f + 0.1f + sinf(t * 2.0f) * 0.1f, 0.0f) * base);
        const XMMATRIX c = XMMatrixTranslation(0.0f, 2.2f, 0.0f) * base;
        draw(EnemyParts_Get("resource/Models/boss_omega_ring1.obj"), XMMatrixRotationY(t * 0.7f) * c);
        draw(EnemyParts_Get("resource/Models/boss_omega_ring2.obj"), XMMatrixRotationY(t * 1.3f) * XMMatrixRotationX(1.22f) * XMMatrixRotationY(-t * 0.4f) * c);
        break;
    }

    case Kind::Hydra:
    {
        draw(m_pModel, base);
        MODEL* head = EnemyParts_Get("resource/Models/boss_hydra_head.obj");
        const float yaw[3] = { 0.0f, XMConvertToRadians(-40.0f), XMConvertToRadians(40.0f) };
        for (int i = 0; i < 3; ++i)
        {
            const XMMATRIX sway = XMMatrixRotationY(sinf(t * 1.3f + i * 2.1f) * 0.16f) *
                                  XMMatrixRotationX(sinf(t * 1.7f + i * 1.3f) * 0.08f - 0.3f * k);
            draw(head, sway * XMMatrixRotationY(yaw[i]) *
                       XMMatrixTranslation(0.5f * sinf(yaw[i]), 1.4f, 0.5f * cosf(yaw[i])) * base);
        }
        break;
    }

    case Kind::Spectre:
    {
        draw(m_pModel, base);
        // 振り：普段は小さく揺れる → 溜めで後ろへ振りかぶる → 斬撃で前へ振り抜く
        float swing = sinf(t * 1.5f) * 0.12f;
        if (m_Flash > 0.01f) swing = 0.7f * std::min(m_Flash, 1.0f);
        if (m_Dashing)       swing = -1.35f;
        for (int s = 0; s < 2; ++s)
        {
            const float side = s ? 1.0f : -1.0f;
            const float my   = swing + (s ? 0.0f : 0.15f) * sinf(t * 2.0f);   // 左右で少しずらす
            draw(EnemyParts_Get(s ? "resource/Models/boss_spectre_arm_r.obj" : "resource/Models/boss_spectre_arm_l.obj"),
                 XMMatrixRotationX(my) * XMMatrixTranslation(side * 0.78f, 1.45f, 0.0f) * base);
        }
        break;
    }

    case Kind::Bastion:
    case Kind::Nest:
    case Kind::Eclipse:
        DrawBallRig(m_Kind, m_pModel, base, t, m_GunSpin, k, shadow);
        break;
    }
}

//==============================================================================
// 球体型ボスの本体＋動くパーツ（エネミー図鑑のプレビューと共用）
//   BASTION : 両腕のガトリングが回り（掃射中は高速）、撃つと後退する。3枚の盾が周回する
//   NEST    : 底の格納リングが回り、翼がはばたき、4つの子機が周回する
//   ECLIPSE : 3本のリングがジャイロのように別々の軸で回り、4つの衛星が傾いた軌道を回る
//==============================================================================
void EnemyBossEx::DrawBallRig(Kind kind, MODEL* body, const XMMATRIX& base,
                              float t, float gunSpin, float recoil, bool shadow)
{
    auto draw = [shadow](MODEL* m, const XMMATRIX& w)
    {
        if (!m) return;
        if (shadow) ShadowMap::DrawModel(m, w);
        else        ModelDraw(m, w);
    };

    draw(body, base);
    switch (kind)
    {
    case Kind::Bastion:
    {
        MODEL* gun = EnemyParts_Get("resource/Models/boss_bastion_gun.obj");
        for (float s : { -1.0f, 1.0f })
            draw(gun, XMMatrixRotationZ(gunSpin * s) * XMMatrixTranslation(0.0f, 0.0f, -0.25f * recoil) *
                      XMMatrixTranslation(s * 1.68f, 1.55f, 0.0f) * base);
        MODEL* plate = EnemyParts_Get("resource/Models/boss_bastion_plate.obj");
        for (int i = 0; i < 3; ++i)
            draw(plate, XMMatrixRotationY(t * 0.5f + i * XM_2PI / 3.0f) * XMMatrixTranslation(0.0f, 1.45f, 0.0f) * base);
        break;
    }

    case Kind::Nest:
    {
        draw(EnemyParts_Get("resource/Models/boss_nest_ring.obj"), XMMatrixRotationY(t * 1.2f) * XMMatrixTranslation(0.0f, 0.42f, 0.0f) * base);
        const float flap = sinf(t * 2.6f) * 0.14f;
        draw(EnemyParts_Get("resource/Models/boss_nest_wing_l.obj"), XMMatrixRotationZ(-flap) * XMMatrixTranslation(-1.35f, 1.95f, 0.0f) * base);
        draw(EnemyParts_Get("resource/Models/boss_nest_wing_r.obj"), XMMatrixRotationZ( flap) * XMMatrixTranslation( 1.35f, 1.95f, 0.0f) * base);
        MODEL* pod = EnemyParts_Get("resource/Models/boss_nest_pod.obj");
        for (int i = 0; i < 4; ++i)
        {
            const XMFLOAT3 p = NestPodLocal(i, t);
            draw(pod, XMMatrixTranslation(p.x, p.y, p.z) * base);
        }
        break;
    }

    case Kind::Eclipse:
    {
        const XMMATRIX c = XMMatrixTranslation(0.0f, 1.75f, 0.0f) * base;
        draw(EnemyParts_Get("resource/Models/boss_eclipse_ring1.obj"),
             XMMatrixRotationY(t * 1.0f) * XMMatrixRotationX(0.55f) * XMMatrixRotationY(t * 0.4f) * c);
        draw(EnemyParts_Get("resource/Models/boss_eclipse_ring2.obj"),
             XMMatrixRotationY(-t * 0.8f) * XMMatrixRotationZ(1.05f) * XMMatrixRotationY(t * 0.3f) * c);
        draw(EnemyParts_Get("resource/Models/boss_eclipse_ring3.obj"),
             XMMatrixRotationY(t * 0.5f) * XMMatrixRotationX(-0.22f) * c);
        MODEL* bit = EnemyParts_Get("resource/Models/boss_eclipse_bit.obj");
        for (int i = 0; i < 4; ++i)
        {
            const XMFLOAT3 p = EclipseBitLocal(i, t);
            draw(bit, XMMatrixTranslation(p.x, p.y, p.z) * base);
        }
        break;
    }

    default:
        break;
    }
}

//==============================================================================
// 描画
//==============================================================================
void EnemyBossEx::Draw()
{
    if (!m_IsAlive || !m_pModel) return;

    Light_SetSpecularWorld(Player_Camera_GetPosition(), 6.0f, { 0.8f, 0.3f, 0.25f, 1.0f });

    // 激昂中はうっすら赤く、溜め中は明るく光らせる
    const XMFLOAT3 prev = Light_GetAmbient();
    const float k = 1.0f + 1.8f * std::min(m_Flash, 1.0f);
    const float red = m_Enraged ? 1.25f : 1.0f;
    Light_SetAmbient({ prev.x * k * red, prev.y * k, prev.z * k });
    DrawRig(false);
    Light_SetAmbient(prev);
}

void EnemyBossEx::DrawShadow()
{
    if (!m_IsAlive || !m_pModel) return;
    DrawRig(true);
}