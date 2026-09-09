/*==============================================================================

   追加武装 [PlayerWeaponEx.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   概要は PlayerWeaponEx.h を参照。発射の仕組みは既存の武器と同じ
   （Bullet_Create / Bullet_CreateBeam / Bullet_CreateMissileBezier）。

==============================================================================*/
#include "PlayerWeaponEx.h"
#include "bullet.h"
#include "Audio.h"
#include "game.h"     // Game_GetLockOnWorldPos
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace DirectX;

namespace
{
    // aim を基準にした右・上の軸（真上／真下を向いているときの保険つき）
    void AimBasis(const XMVECTOR& aim, XMVECTOR* right, XMVECTOR* up)
    {
        const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR r = XMVector3Cross(worldUp, aim);
        if (XMVectorGetX(XMVector3LengthSq(r)) < 1e-4f) r = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        r = XMVector3Normalize(r);
        *right = r;
        *up    = XMVector3Normalize(XMVector3Cross(aim, r));
    }

    float RandSigned() { return static_cast<float>(rand() % 2001 - 1000) / 1000.0f; }
}

//==============================================================================
// RAIL CANNON：貫通ビームを1発
//==============================================================================
void WeaponRailgun::Initialize() { m_cooldown = 0.0; m_shootSE = LoadAudioWithVolume("resource/sound/beam_shoot.wav", 0.9f); }
void WeaponRailgun::Finalize()   { UnloadAudio(m_shootSE); m_shootSE = -1; }
void WeaponRailgun::Update(double dt) { if (m_cooldown > 0.0) m_cooldown -= dt; }

bool WeaponRailgun::TryFire(const XMFLOAT3& muzzlePos, const XMFLOAT3& aimDir, float damageMult)
{
    if (m_cooldown > 0.0) return false;
    XMFLOAT3 dir;
    XMStoreFloat3(&dir, XMVector3Normalize(XMLoadFloat3(&aimDir)));
    Bullet_CreateBeam(muzzlePos, dir, static_cast<int>(BASE_DAMAGE * damageMult));
    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);
    m_cooldown = FIRE_INTERVAL;
    return true;
}

//==============================================================================
// GATLING：わずかに拡散する高速連射
//==============================================================================
void WeaponGatling::Initialize() { m_cooldown = 0.0; m_seCooldown = 0.0; m_shootSE = LoadAudioWithVolume("resource/sound/machine_gun.wav", 0.7f); }
void WeaponGatling::Finalize()   { UnloadAudio(m_shootSE); m_shootSE = -1; }
void WeaponGatling::Update(double dt)
{
    if (m_cooldown   > 0.0) m_cooldown   -= dt;
    if (m_seCooldown > 0.0) m_seCooldown -= dt;
}

bool WeaponGatling::TryFire(const XMFLOAT3& muzzlePos, const XMFLOAT3& aimDir, float damageMult)
{
    if (m_cooldown > 0.0) return false;

    const XMVECTOR aim = XMVector3Normalize(XMLoadFloat3(&aimDir));
    XMVECTOR right, up;
    AimBasis(aim, &right, &up);
    const float spread = XMConvertToRadians(SPREAD_DEG);
    const XMVECTOR dir = XMVector3TransformNormal(aim,
        XMMatrixRotationAxis(up, RandSigned() * spread) * XMMatrixRotationAxis(right, RandSigned() * spread));

    XMFLOAT3 vel;
    XMStoreFloat3(&vel, XMVector3Normalize(dir) * BULLET_SPEED);
    Bullet_Create(muzzlePos, vel, static_cast<int>(BASE_DAMAGE * damageMult));

    if (m_seCooldown <= 0.0 && m_shootSE >= 0)
    {
        PlayAudio(m_shootSE, false);
        m_seCooldown = 0.1;
    }
    m_cooldown = FIRE_INTERVAL;
    return true;
}

//==============================================================================
// GRENADE：放物線（ベジェ曲線）で標的へ落ちる炸裂弾
//==============================================================================
void WeaponGrenade::Initialize() { m_cooldown = 0.0; m_shootSE = LoadAudioWithVolume("resource/sound/rocket_launcher.wav", 0.7f); }
void WeaponGrenade::Finalize()   { UnloadAudio(m_shootSE); m_shootSE = -1; }
void WeaponGrenade::Update(double dt) { if (m_cooldown > 0.0) m_cooldown -= dt; }

bool WeaponGrenade::TryFire(const XMFLOAT3& muzzlePos, const XMFLOAT3& aimDir, float damageMult)
{
    if (m_cooldown > 0.0) return false;

    const XMVECTOR muzzle = XMLoadFloat3(&muzzlePos);
    const XMVECTOR aim    = XMVector3Normalize(XMLoadFloat3(&aimDir));

    // 着弾点：ロックオン中はその敵、なければ照準方向 RANGE m 先
    XMVECTOR target;
    XMFLOAT3 lock;
    if (Game_GetLockOnWorldPos(&lock)) target = XMLoadFloat3(&lock);
    else                               target = muzzle + aim * RANGE;

    const float dist = XMVectorGetX(XMVector3Length(target - muzzle));
    const XMVECTOR control = (muzzle + target) * 0.5f + XMVectorSet(0.0f, ARC_HEIGHT + dist * 0.08f, 0.0f, 0.0f);

    XMFLOAT3 p0, p1, p2;
    XMStoreFloat3(&p0, muzzle);
    XMStoreFloat3(&p1, control);
    XMStoreFloat3(&p2, target);
    const float duration = std::clamp(dist / 16.0f, 0.4f, 1.1f);
    Bullet_CreateMissileBezier(p0, p1, p2, duration, 16.0f, static_cast<int>(BASE_DAMAGE * damageMult), EXPLOSION_RADIUS);

    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);
    m_cooldown = FIRE_INTERVAL;
    return true;
}

//==============================================================================
// BURST RIFLE：1回の発射で3発（2発目以降は Update で一定間隔ごとに）
//==============================================================================
void WeaponBurstRifle::Initialize() { m_cooldown = 0.0; m_burstLeft = 0; m_shootSE = LoadAudioWithVolume("resource/sound/machine_gun.wav", 0.9f); }
void WeaponBurstRifle::Finalize()   { UnloadAudio(m_shootSE); m_shootSE = -1; }

void WeaponBurstRifle::Shot()
{
    XMFLOAT3 vel;
    XMStoreFloat3(&vel, XMVector3Normalize(XMLoadFloat3(&m_aim)) * BULLET_SPEED);
    Bullet_Create(m_muzzle, vel, m_damage);
    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);
}

void WeaponBurstRifle::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
    if (m_burstLeft > 0)
    {
        m_burstTimer -= dt;
        if (m_burstTimer <= 0.0)
        {
            Shot();
            --m_burstLeft;
            m_burstTimer = BURST_GAP;
        }
    }
}

bool WeaponBurstRifle::TryFire(const XMFLOAT3& muzzlePos, const XMFLOAT3& aimDir, float damageMult)
{
    if (m_cooldown > 0.0 || m_burstLeft > 0) return false;
    m_muzzle = muzzlePos;
    m_aim    = aimDir;
    m_damage = static_cast<int>(BASE_DAMAGE * damageMult);
    Shot();
    m_burstLeft  = BURST_COUNT - 1;
    m_burstTimer = BURST_GAP;
    m_cooldown   = FIRE_INTERVAL;
    return true;
}

//==============================================================================
// SPREAD LASER：水平に扇状に並んだ5本のビーム
//==============================================================================
void WeaponSpreadLaser::Initialize() { m_cooldown = 0.0; m_shootSE = LoadAudioWithVolume("resource/sound/beam_shoot.wav", 0.6f); }
void WeaponSpreadLaser::Finalize()   { UnloadAudio(m_shootSE); m_shootSE = -1; }
void WeaponSpreadLaser::Update(double dt) { if (m_cooldown > 0.0) m_cooldown -= dt; }

bool WeaponSpreadLaser::TryFire(const XMFLOAT3& muzzlePos, const XMFLOAT3& aimDir, float damageMult)
{
    if (m_cooldown > 0.0) return false;

    const XMVECTOR aim = XMVector3Normalize(XMLoadFloat3(&aimDir));
    XMVECTOR right, up;
    AimBasis(aim, &right, &up);
    const int damage = static_cast<int>(BASE_DAMAGE * damageMult);
    for (int i = 0; i < BEAM_COUNT; ++i)
    {
        const float yaw = XMConvertToRadians(SPREAD_STEP * (i - (BEAM_COUNT - 1) * 0.5f));
        XMFLOAT3 dir;
        XMStoreFloat3(&dir, XMVector3Normalize(XMVector3TransformNormal(aim, XMMatrixRotationAxis(up, yaw))));
        Bullet_CreateBeam(muzzlePos, dir, damage);
    }
    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);
    m_cooldown = FIRE_INTERVAL;
    return true;
}
