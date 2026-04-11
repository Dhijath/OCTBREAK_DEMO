/*==============================================================================

   プレイヤー武器システム [PlayerWeapon.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/08
--------------------------------------------------------------------------------

==============================================================================*/
#include "PlayerWeapon.h"
#include "bullet.h"
#include "Audio.h"
#include <DirectXMath.h>
#include <cstdlib>   // rand()
#include <cmath>

using namespace DirectX;

//==============================================================================
// WeaponNormal（通常弾）
//==============================================================================

void WeaponNormal::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/machine_gun.wav", 0.5f);
}

void WeaponNormal::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponNormal::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
}

bool WeaponNormal::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    XMFLOAT3 vel;
    XMStoreFloat3(&vel,
        XMVector3Normalize(XMLoadFloat3(&aimDir)) * BULLET_SPEED);

    Bullet_Create(muzzlePos, vel, finalDamage);

    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);

    m_cooldown = FIRE_INTERVAL;
    return true;
}


//==============================================================================
// WeaponMissile（ミサイル）
//==============================================================================

void WeaponMissile::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/maou_se_battle_gun05.wav", 0.5f);
}

void WeaponMissile::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponMissile::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
}

bool WeaponMissile::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    XMFLOAT3 vel;
    XMStoreFloat3(&vel,
        XMVector3Normalize(XMLoadFloat3(&aimDir)) * BULLET_SPEED);

    Bullet_CreateMissile(muzzlePos, vel, finalDamage, EXPLOSION_RADIUS);

    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);

    m_cooldown = FIRE_INTERVAL;
    return true;
}


//==============================================================================
// WeaponBeam（ビーム）
//==============================================================================

void WeaponBeam::Initialize()
{
    m_cooldown   = 0.0;
    m_seCooldown = 0.0;
    m_energy     = ENERGY_MAX;
    m_shootSE    = LoadAudioWithVolume("resource/sound/beam_shoot.wav", 0.5f);
}

void WeaponBeam::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponBeam::Update(double dt)
{
    if (m_cooldown   > 0.0) m_cooldown   -= dt;
    if (m_seCooldown > 0.0) m_seCooldown -= dt;
}

bool WeaponBeam::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0 || m_energy < ENERGY_COST) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    // Bullet_CreateBeam は内部で速度を正規化するので方向だけ渡す
    XMFLOAT3 dir;
    XMStoreFloat3(&dir, XMVector3Normalize(XMLoadFloat3(&aimDir)));
    Bullet_CreateBeam(muzzlePos, dir, finalDamage);

    m_energy -= ENERGY_COST;
    if (m_energy < 0.0f) m_energy = 0.0f;

    // SE は連射でも一定間隔以上空けてから鳴らす
    if (m_seCooldown <= 0.0 && m_shootSE >= 0)
    {
        PlayAudio(m_shootSE, false);
        m_seCooldown = SE_INTERVAL;
    }

    m_cooldown = FIRE_INTERVAL;
    return true;
}

void WeaponBeam::AddEnergy(float amount)
{
    m_energy += amount;
    if (m_energy > ENERGY_MAX) m_energy = ENERGY_MAX;
}


//==============================================================================
// WeaponShotgun（ショットガン）
//==============================================================================

void WeaponShotgun::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/shotgun.wav", 0.5f);
}

void WeaponShotgun::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponShotgun::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
}

bool WeaponShotgun::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    XMVECTOR aimV = XMVector3Normalize(XMLoadFloat3(&aimDir));
    XMVECTOR up   = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // aimDir が真上／真下の場合は別軸を基準に
    XMVECTOR right;
    float dotUp = fabsf(XMVectorGetX(XMVector3Dot(aimV, up)));
    if (dotUp > 0.99f)
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    else
        right = XMVector3Normalize(XMVector3Cross(up, aimV));

    XMVECTOR pelletUp = XMVector3Normalize(XMVector3Cross(aimV, right));

    const float spreadRad = XMConvertToRadians(SPREAD_DEG);

    for (int p = 0; p < PELLET_COUNT; ++p)
    {
        // 各ペレットにランダムな上下左右のブレを付ける
        float yawOff   = ((rand() % 2001) - 1000) / 1000.0f * spreadRad;
        float pitchOff = ((rand() % 2001) - 1000) / 1000.0f * spreadRad;

        XMVECTOR spreadDir = XMVector3TransformNormal(
            aimV,
            XMMatrixRotationAxis(pelletUp, yawOff) *
            XMMatrixRotationAxis(right,    pitchOff));

        XMFLOAT3 pelletVel;
        XMStoreFloat3(&pelletVel,
            XMVector3Normalize(spreadDir) * BULLET_SPEED);

        Bullet_Create(muzzlePos, pelletVel, finalDamage);
    }

    if (m_shootSE >= 0) PlayAudio(m_shootSE, false);

    m_cooldown = FIRE_INTERVAL;
    return true;
}
