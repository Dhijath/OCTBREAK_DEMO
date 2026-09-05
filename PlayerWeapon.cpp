/*==============================================================================

   プレイヤー武器システム [PlayerWeapon.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

==============================================================================*/
#include "PlayerWeapon.h"
#include "bullet.h"
#include "Audio.h"
#include "game.h"     // Game_GetLockOnWorldPos
#include <DirectXMath.h>
#include <cstdlib>   // rand()
#include <cmath>
#include <algorithm> // std::clamp

using namespace DirectX;

//==============================================================================
// WeaponNormal（通常弾）
//==============================================================================

void WeaponNormal::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/machine_gun.wav", 0.8f);
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
// WeaponTripleGun（トリプルマシンガン：横3列平行発射）
//==============================================================================

void WeaponTripleGun::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/machine_gun.wav", 0.8f);
}

void WeaponTripleGun::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponTripleGun::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
}

bool WeaponTripleGun::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    XMVECTOR aimV = XMVector3Normalize(XMLoadFloat3(&aimDir));

    // 横方向（右）ベクトル：aim が真上/真下のときは別軸を基準にする
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right;
    if (fabsf(XMVectorGetX(XMVector3Dot(aimV, up))) > 0.99f)
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    else
        right = XMVector3Normalize(XMVector3Cross(up, aimV));

    // 3列とも同じ方向（平行）。速度ベクトルは共通
    XMFLOAT3 vel;
    XMStoreFloat3(&vel, aimV * BULLET_SPEED);

    // マズルを横にずらして中央・右・左の3列を平行発射（拡散なし）
    const float offsets[BARREL_COUNT] = { 0.0f, BARREL_SPACING, -BARREL_SPACING };
    XMVECTOR muzzleV = XMLoadFloat3(&muzzlePos);
    for (int i = 0; i < BARREL_COUNT; ++i)
    {
        XMFLOAT3 mp;
        XMStoreFloat3(&mp, muzzleV + right * offsets[i]);
        Bullet_Create(mp, vel, finalDamage);
    }

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
    m_shootSE  = LoadAudioWithVolume("resource/sound/rocket_launcher.wav", 0.8f);
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
// WeaponMultiMissile（マルチミサイル）
//==============================================================================

void WeaponMultiMissile::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/rocket_launcher.wav", 0.8f); // ミサイルと同じ発射音
}

void WeaponMultiMissile::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;
}

void WeaponMultiMissile::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;
}

bool WeaponMultiMissile::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    if (m_cooldown > 0.0) return false;

    const int finalDamage = static_cast<int>(BASE_DAMAGE * damageMult);

    const XMVECTOR vMuzzle = XMLoadFloat3(&muzzlePos);
    const XMVECTOR vAim    = XMVector3Normalize(XMLoadFloat3(&aimDir));

    // 標的位置：ロックオン優先、なければ照準方向の前方 TARGET_FORWARD_DIST
    XMVECTOR vTarget;
    XMFLOAT3 lockPos;
    if (Game_GetLockOnWorldPos(&lockPos))
        vTarget = XMLoadFloat3(&lockPos);
    else
        vTarget = vMuzzle + vAim * TARGET_FORWARD_DIST;

    // 標的への方向・距離
    XMVECTOR toTarget = vTarget - vMuzzle;
    const float dist  = sqrtf(XMVectorGetX(XMVector3LengthSq(toTarget)));
    XMVECTOR dir      = (dist > 0.0001f) ? XMVector3Normalize(toTarget) : vAim;

    // 直交基底（横 right / 上 up）
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightB  = XMVector3Cross(worldUp, dir);
    if (XMVectorGetX(XMVector3LengthSq(rightB)) < 0.0001f)
        rightB = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);   // 真上/真下を向いている場合の保険
    rightB = XMVector3Normalize(rightB);
    XMVECTOR upB = XMVector3Normalize(XMVector3Cross(dir, rightB));

    const XMVECTOR mid = (vMuzzle + vTarget) * 0.5f;

    // 曲線を飛び切る時間（距離ベースでクランプ）
    const float duration = std::clamp(dist / BULLET_SPEED, 0.35f, 1.2f);

    // MISSILE_COUNT 本を横に均等配置し、弧を描いて拡散 → 標的へ収束
    for (int i = 0; i < MISSILE_COUNT; ++i)
    {
        const float lat = (MISSILE_COUNT > 1)
            ? (static_cast<float>(i) / (MISSILE_COUNT - 1)) * 2.0f - 1.0f  // -1..+1
            : 0.0f;

        const float lateral = lat * SPREAD_WIDTH;                 // 横方向の膨らみ
        const float arc     = ARC_HEIGHT * (1.0f - lat * lat);    // 中央ほど高く弧を描く

        // 制御点：中点から横＋上へオフセット
        const XMVECTOR p1 = mid + rightB * lateral + upB * (arc + ARC_HEIGHT * 0.5f);

        XMFLOAT3 f0, f1, f2;
        XMStoreFloat3(&f0, vMuzzle);
        XMStoreFloat3(&f1, p1);
        XMStoreFloat3(&f2, vTarget);

        Bullet_CreateMissileBezier(f0, f1, f2, duration, BULLET_SPEED, finalDamage, EXPLOSION_RADIUS);
    }

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

    // 最後にエネルギーを消費してから REGEN_DELAY 秒経過するまでは自動回復しない。
    // 発射・飛行・ダッシュのたびに待ち時間がリセットされる。アイテム回復は常に有効。
    if (m_regenDelay > 0.0)
    {
        m_regenDelay -= dt;
    }
    else if (m_energy < ENERGY_MAX)
    {
        m_energy += ENERGY_REGEN * static_cast<float>(dt);
        if (m_energy > ENERGY_MAX) m_energy = ENERGY_MAX;
    }
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
    m_regenDelay = REGEN_DELAY;   // 発射で消費 → 回復開始を REGEN_DELAY 秒後まで遅らせる

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
    // マイナス＝消費（飛行・ダッシュ）→ 回復開始を REGEN_DELAY 秒後まで遅らせる。
    // プラス＝アイテム回復 → 遅延させない。
    if (amount < 0.0f) m_regenDelay = REGEN_DELAY;

    m_energy += amount;
    if (m_energy > ENERGY_MAX) m_energy = ENERGY_MAX;
}


//==============================================================================
// WeaponShotgun（ショットガン）
//==============================================================================

void WeaponShotgun::Initialize()
{
    m_cooldown = 0.0;
    m_shootSE  = LoadAudioWithVolume("resource/sound/shotgun.wav", 0.2f);
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


//==============================================================================
// WeaponMelee（近接：前方薙ぎ払い）
//==============================================================================

//------------------------------------------------------------------------------
// 薙ぎ払いの3フェーズ境界（正規化 0..1）と補間ヘルパー。Update より前に置く。
//------------------------------------------------------------------------------
namespace
{
    constexpr float MELEE_WINDUP_END = 0.25f;  // タメ終わり
    constexpr float MELEE_SWEEP_END  = 0.60f;  // 薙ぎ切り（ここで姿勢を止める）

    // 0..1 を滑らかに（smootherstep：両端の加減速がより滑らかなイージング）
    float SmoothStep01(float t)
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
}

void WeaponMelee::Initialize()
{
    m_cooldown   = 0.0;
    m_swingTimer = 0.0;
    m_swinging   = false;
    m_hasHit     = false;
    m_hitDamage  = 0;
    // 振りの風切り音（ダッシュと同じ素材を流用）＋ヒットの打撃音
    m_swingSE = LoadAudioWithVolume("resource/sound/dash_whoosh.wav", 0.7f);
    m_hitSE   = LoadAudioWithVolume("resource/sound/dageki5.wav", 0.6f);
}

void WeaponMelee::Finalize()
{
    UnloadAudio(m_swingSE);
    UnloadAudio(m_hitSE);
    m_swingSE = -1;
    m_hitSE   = -1;
}

void WeaponMelee::Update(double dt)
{
    if (m_cooldown > 0.0) m_cooldown -= dt;

    if (!m_swinging) return;

    m_swingTimer += dt;

    // 振りの当たるフレームで一度だけヒット判定を出す
    if (!m_hasHit && m_swingTimer >= CONTACT_TIME)
    {
        // 当たり判定は武器の刃先位置で出す（描画側が毎フレーム更新。未更新なら前方フォールバック）
        // knockback を渡すと game.cpp 側で敵の押し出し＋ヒットストップが発生する
        Bullet_AddExplosion(m_bladeWorldPos, HIT_RADIUS, m_hitDamage, KNOCKBACK_STRENGTH, HIT_VSCALE);
        if (m_hitSE >= 0) PlayAudio(m_hitSE, false);
        m_hasHit = true;
    }

    // 振り終了（タメ→薙ぎ→戻りを SWING_DURATION で通しで再生）
    if (m_swingTimer >= SWING_DURATION)
    {
        m_swinging   = false;
        m_swingTimer = 0.0;
    }
}

bool WeaponMelee::TryFire(
    const XMFLOAT3& muzzlePos,
    const XMFLOAT3& aimDir,
    float           damageMult)
{
    // クールダウン中・振り動作中は新しい振りを受け付けない
    if (m_cooldown > 0.0 || m_swinging) return false;

    // 前方 REACH の位置を当たり球の中心に確定（振り開始時に固定）
    XMVECTOR vAim = XMVector3Normalize(XMLoadFloat3(&aimDir));
    XMVECTOR vCenter = XMLoadFloat3(&muzzlePos) + vAim * REACH;
    XMStoreFloat3(&m_hitCenter, vCenter);
    m_bladeWorldPos = m_hitCenter;   // 刃先が未更新でも当たるようフォールバック初期化

    m_hitDamage  = static_cast<int>(BASE_DAMAGE * damageMult);
    m_swinging   = true;
    m_swingTimer = 0.0;
    m_hasHit     = false;
    m_cooldown   = FIRE_INTERVAL;

    if (m_swingSE >= 0) PlayAudio(m_swingSE, false);
    return true;
}

//------------------------------------------------------------------------------
// 描画側が参照する振り角（度）。振り切り(MELEE_SWEEP_END)でクランプし、
// ホールド中はその姿勢のまま。非振り時は 0。
//------------------------------------------------------------------------------
float WeaponMelee::GetSwingAngleDeg() const
{
    if (!m_swinging) return 0.0f;

    float p = static_cast<float>(m_swingTimer / SWING_DURATION);
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    // 右手基準：+ が右（外側）、- が左（内側）、0 が正面。
    // 左前へ構える（START=-40°）→ 外側（右 END=+90°）へ振り抜く → 戻る。
    if (p < MELEE_WINDUP_END)          // タメ：0 → START（左前へ構える）
    {
        float t = SmoothStep01(p / MELEE_WINDUP_END);
        return SWING_START_DEG * t;
    }
    if (p < MELEE_SWEEP_END)           // 薙ぎ：START → END（左前→右へ外向きに）
    {
        float t = SmoothStep01((p - MELEE_WINDUP_END) / (MELEE_SWEEP_END - MELEE_WINDUP_END));
        return SWING_START_DEG + (SWING_END_DEG - SWING_START_DEG) * t;
    }
    float t = SmoothStep01((p - MELEE_SWEEP_END) / (1.0f - MELEE_SWEEP_END)); // 戻り：END → 0
    return SWING_END_DEG * (1.0f - t);
}

//------------------------------------------------------------------------------
// 突き出し量（0..1）。タメでは短く、薙ぎで前方へ伸ばし（先端を突き出す）、
// 戻りで縮める。描画側が腕の前方リーチに掛けて使う。
//------------------------------------------------------------------------------
float WeaponMelee::GetSwingThrust01() const
{
    if (!m_swinging) return 0.0f;

    float p = static_cast<float>(m_swingTimer / SWING_DURATION);
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    if (p < MELEE_WINDUP_END)          // タメ：0 → 1.0（先端を左前へ突き出す）
    {
        float t = SmoothStep01(p / MELEE_WINDUP_END);
        return t;
    }
    if (p < MELEE_SWEEP_END)           // 薙ぎ：伸ばしたまま外側へ薙ぐ
    {
        return 1.0f;
    }
    float t = SmoothStep01((p - MELEE_SWEEP_END) / (1.0f - MELEE_SWEEP_END)); // 戻り：1.0 → 0
    return 1.0f - t;
}
