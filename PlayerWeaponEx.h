/*==============================================================================

   追加武装 [PlayerWeaponEx.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   アセンブリで選べる追加の5武装（性能値は WeaponDef.cpp の表と同じ）。

     WeaponRailgun     RAIL CANNON   貫通ビーム 520 / 1.40s
     WeaponGatling     GATLING       24 × 毎秒20発・±2.5°の拡散
     WeaponGrenade     GRENADE       放物線を描く炸裂弾 260（半径4.5m）/ 1.00s
     WeaponBurstRifle  BURST RIFLE   70 × 3点バースト / 0.60s
     WeaponSpreadLaser SPREAD LASER  60 × 5本の扇状ビーム / 0.50s

==============================================================================*/
#pragma once
#include "PlayerWeapon.h"

class WeaponRailgun : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos, const DirectX::XMFLOAT3& aimDir, float damageMult) override;
    const char* GetName() const override { return "レールキャノン"; }

    static constexpr double FIRE_INTERVAL = 1.40;
    static constexpr int    BASE_DAMAGE   = 520;
private:
    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};

class WeaponGatling : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos, const DirectX::XMFLOAT3& aimDir, float damageMult) override;
    const char* GetName() const override { return "ガトリング"; }

    static constexpr double FIRE_INTERVAL = 0.05;
    static constexpr float  BULLET_SPEED  = 48.0f;
    static constexpr int    BASE_DAMAGE   = 24;
    static constexpr float  SPREAD_DEG    = 2.5f;
private:
    double m_cooldown   = 0.0;
    double m_seCooldown = 0.0;   // 連射が速いので発射音は間引く
    int    m_shootSE    = -1;
};

class WeaponGrenade : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos, const DirectX::XMFLOAT3& aimDir, float damageMult) override;
    const char* GetName() const override { return "グレネード"; }

    static constexpr double FIRE_INTERVAL    = 1.00;
    static constexpr int    BASE_DAMAGE      = 260;
    static constexpr float  EXPLOSION_RADIUS = 4.5f;
    static constexpr float  RANGE            = 18.0f;   // 非ロックオン時の着弾距離
    static constexpr float  ARC_HEIGHT       = 4.0f;    // 放物線の高さ
private:
    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};

class WeaponBurstRifle : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos, const DirectX::XMFLOAT3& aimDir, float damageMult) override;
    const char* GetName() const override { return "バーストライフル"; }

    static constexpr double FIRE_INTERVAL = 0.60;
    static constexpr double BURST_GAP     = 0.07;
    static constexpr float  BULLET_SPEED  = 60.0f;
    static constexpr int    BASE_DAMAGE   = 70;
    static constexpr int    BURST_COUNT   = 3;
private:
    void Shot();
    double m_cooldown  = 0.0;
    double m_burstTimer = 0.0;
    int    m_burstLeft = 0;          // 残りの弾数（1発目は TryFire で撃つ）
    int    m_damage    = 0;
    DirectX::XMFLOAT3 m_muzzle = {}; // 2発目以降は1発目の位置・向きで撃つ
    DirectX::XMFLOAT3 m_aim    = { 0, 0, 1 };
    int    m_shootSE   = -1;
};

class WeaponSpreadLaser : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos, const DirectX::XMFLOAT3& aimDir, float damageMult) override;
    const char* GetName() const override { return "スプレッドレーザー"; }

    static constexpr double FIRE_INTERVAL = 0.50;
    static constexpr int    BASE_DAMAGE   = 60;
    static constexpr int    BEAM_COUNT    = 5;
    static constexpr float  SPREAD_STEP   = 7.0f;   // 隣のビームとの角度（度）
private:
    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};
