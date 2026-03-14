/*==============================================================================

   プレイヤー武器システム [PlayerWeapon.h]
                                                         Author : 51106
                                                         Date   : 2026/03/08
--------------------------------------------------------------------------------

   ■設計方針
   ・PlayerWeapon  : 全武器の基底クラス（純粋仮想インターフェース）
   ・WeaponNormal  : 通常弾（単発・連射制限あり）
   ・WeaponBeam    : ビーム（高連射・エネルギー制）
   ・WeaponShotgun : ショットガン（複数ペレット散弾）

   ■使い方（player.cpp 側）
     // ビーム（固定）
     WeaponBeam* g_pBeamWeapon = new WeaponBeam();
     g_pBeamWeapon->Initialize();
     g_pBeamWeapon->Update(dt);
     if (rightClick) g_pBeamWeapon->TryFire(muzzlePos, aimDir, damageMult);
     g_pBeamWeapon->Finalize(); delete g_pBeamWeapon;

     // 通常スロット（Normal/Shotgun/Missile を E キーで切り替え）
     PlayerWeapon* g_NormalWeapons[3] = { new WeaponNormal, new WeaponShotgun, new WeaponMissile };
     g_NormalWeapons[idx]->Initialize();
     g_NormalWeapons[idx]->Update(dt);
     if (normalFire) g_NormalWeapons[idx]->TryFire(muzzlePos, aimDir, damageMult);
     g_NormalWeapons[idx]->Finalize(); delete g_NormalWeapons[idx];

==============================================================================*/
#pragma once
#include <DirectXMath.h>

//==============================================================================
// 武器基底クラス（純粋仮想）
//
// ■役割
// ・全武器に共通するインターフェースを定義する
// ・player.cpp はこのポインタで全武器を一元管理する
//==============================================================================
class PlayerWeapon
{
public:
    virtual ~PlayerWeapon() = default;

    //==========================================================================
    // リソースロード
    // ・SE の読み込みなど、初期化時の処理を行う
    //==========================================================================
    virtual void Initialize() = 0;

    //==========================================================================
    // リソース解放
    // ・Initialize で確保したリソースを解放する
    //==========================================================================
    virtual void Finalize() = 0;

    //==========================================================================
    // 毎フレーム更新
    // ・クールダウンタイマーなどを進める
    // ・dt : 経過時間（秒）
    //==========================================================================
    virtual void Update(double dt) = 0;

    //==========================================================================
    // 発射試行
    //
    // ■引数
    // ・muzzlePos  : バレル先端のワールド座標
    // ・aimDir     : 正規化済み照準方向ベクトル
    // ・damageMult : プレイヤーの攻撃力倍率
    //
    // ■戻り値
    // ・true  : 実際に発射した
    // ・false : クールダウン中 or エネルギー不足で発射しなかった
    //==========================================================================
    virtual bool TryFire(
        const DirectX::XMFLOAT3& muzzlePos,
        const DirectX::XMFLOAT3& aimDir,
        float                    damageMult) = 0;

    //==========================================================================
    // HUD 表示用の武器名
    //==========================================================================
    virtual const char* GetName() const = 0;

    //==========================================================================
    // エネルギー残量（0.0〜1.0）
    // ・エネルギー概念がない武器は 1.0 を返す
    //==========================================================================
    virtual float GetEnergyRatio() const { return 1.0f; }

    //==========================================================================
    // エネルギーが残っているか
    // ・false になったとき player.cpp 側で自動切り替えをトリガーする
    //==========================================================================
    virtual bool HasEnergy() const { return true; }
};


//==============================================================================
// 通常弾（WeaponNormal）
//
// ■特性
// ・1発ずつ発射・連射レート制限あり
// ・エネルギーなし（いつでも撃てる）
//==============================================================================
class WeaponNormal : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "ノーマル"; }

private:
    static constexpr double FIRE_INTERVAL = 0.09;   // 連射間隔（秒）
    static constexpr float  BULLET_SPEED  = 46.0f;  // 弾速（単位/秒）
    static constexpr int    BASE_DAMAGE   = 45;      // 基礎ダメージ

    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};


//==============================================================================
// ミサイル（WeaponMissile）
//
// ■特性
// ・1発ずつ発射・発射レート遅め（1秒）
// ・命中 or 壁衝突時に周囲エリアダメージ（爆発半径 2.5f）
//==============================================================================
class WeaponMissile : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "ミサイル"; }

private:
    static constexpr double FIRE_INTERVAL    = 1.0;    // 発射間隔（秒）
    static constexpr float  BULLET_SPEED     = 18.0f;  // 弾速（単位/秒）
    static constexpr int    BASE_DAMAGE      = 100;     // 爆発ダメージ
    static constexpr float  EXPLOSION_RADIUS = 7.5f;   // 爆発半径

    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};


//==============================================================================
// ビーム（WeaponBeam）
//
// ■特性
// ・超高連射・エネルギー消費制
// ・エネルギーがゼロになると player.cpp が自動で武器切り替えを行う
//==============================================================================
class WeaponBeam : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName()        const override { return "ビーム"; }
    float       GetEnergyRatio() const override { return m_energy / ENERGY_MAX; }
    bool        HasEnergy()      const override { return m_energy > 0.0f; }

    // player.cpp の API（Player_GetBeamEnergy など）から委譲用
    float GetEnergy()    const { return m_energy; }
    float GetEnergyMax() const { return ENERGY_MAX; }
    void  AddEnergy(float amount);

private:
    static constexpr double FIRE_INTERVAL = 0.001;   // 連射間隔（秒）
    static constexpr int    BASE_DAMAGE   = 4;        // 基礎ダメージ
    static constexpr float  ENERGY_MAX    = 300.0f;  // エネルギー最大値
    static constexpr float  ENERGY_COST   = 1.0f;    // 1発のエネルギーコスト
    static constexpr double SE_INTERVAL   = 0.1;     // SE重複再生防止間隔（秒）

    double m_cooldown   = 0.0;
    double m_seCooldown = 0.0;
    float  m_energy     = ENERGY_MAX;
    int    m_shootSE    = -1;
};


//==============================================================================
// ショットガン（WeaponShotgun）
//
// ■特性
// ・1射でペレット数発を扇状に発射
// ・発射レートは遅め・近距離高火力
//==============================================================================
class WeaponShotgun : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "ショットガン"; }

private:
    static constexpr double FIRE_INTERVAL = 0.7;   // 連射間隔（秒）
    static constexpr float  BULLET_SPEED  = 46.0f; // ペレット弾速（単位/秒）
    static constexpr int    BASE_DAMAGE   = 45;      // 1ペレットの基礎ダメージ
    static constexpr int    PELLET_COUNT  = 11;      // 1射のペレット数
    static constexpr float  SPREAD_DEG   = 9.0f;  // 最大拡散角（度）

    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};
