/*==============================================================================

   プレイヤー武器システム [PlayerWeapon.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
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

    //==========================================================================
    // スイング角（度）
    // ・近接武器が振り動作中に返す。描画側が武器モデルの向きに加算して
    //   「薙ぎ払い」を手続きアニメで表現する
    // ・射撃武器は 0（振らない）を返す
    //==========================================================================
    virtual float GetSwingAngleDeg() const { return 0.0f; }

    //==========================================================================
    // 振り動作中か
    // ・true の間、描画側は肩を軸にした薙ぎ払いの姿勢で武器を描く
    //==========================================================================
    virtual bool IsSwinging() const { return false; }

    //==========================================================================
    // 突き出し量（0.0〜1.0）
    // ・薙ぎのピーク付近で 1.0 に近づく。描画側が腕の長さ（前方リーチ）を
    //   伸ばして「先端を前へ突き出す」表現に使う
    //==========================================================================
    virtual float GetSwingThrust01() const { return 0.0f; }

    //==========================================================================
    // 刃先のワールド座標を設定
    // ・描画側が毎フレーム武器モデルの先端位置を渡す。近接武器はこの位置で
    //   当たり判定を出す（＝武器自体に当たり判定を載せる）
    //==========================================================================
    virtual void SetBladeWorldPos(const DirectX::XMFLOAT3&) {}

    // 【診断用】振り開始からの経過秒（非振り時は 0）。スピン表示に使う。
    virtual double GetSwingElapsed() const { return 0.0; }
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
// トリプルマシンガン（WeaponTripleGun）
//
// ■特性
// ・マシンガンと同じ連射レート・弾速
// ・横に3列（中央・右・左）平行に同時発射する
// ・1発の威力はマシンガンの60%（40%減）
//==============================================================================
class WeaponTripleGun : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "トリプルマシンガン"; }

private:
    static constexpr double FIRE_INTERVAL = 0.09;   // 連射間隔（秒・MG同等）
    static constexpr float  BULLET_SPEED  = 46.0f;  // 弾速（単位/秒・MG同等）
    static constexpr int    BASE_DAMAGE   = 27;     // 基礎ダメージ（45×0.6＝40%減）
    static constexpr int    BARREL_COUNT  = 3;      // 横バレル本数
    static constexpr float  BARREL_SPACING = 0.18f; // 隣り合うバレルの横間隔（ワールド単位）

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
    static constexpr double FIRE_INTERVAL    = 0.85;    // 発射間隔（秒）
    static constexpr float  BULLET_SPEED     = 28.0f;  // 弾速（単位/秒）
    static constexpr int    BASE_DAMAGE      = 140;     // 爆発ダメージ
    static constexpr float  EXPLOSION_RADIUS = 7.5f;   // 爆発半径

    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};


//==============================================================================
// マルチミサイル（WeaponMultiMissile）
//
// ■特性
// ・1射で MISSILE_COUNT 発を一斉発射
// ・各ミサイルが二次ベジェ曲線で横に弧を描いて拡散 → 標的へ収束
// ・標的はロックオン優先、無ければ照準方向の前方
//==============================================================================
class WeaponMultiMissile : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "マルチミサイル"; }

private:
    static constexpr double FIRE_INTERVAL    = 1.20;    // 発射間隔（秒）
    static constexpr float  BULLET_SPEED     = 28.0f;   // 曲線到達後の弾速（単位/秒）
    static constexpr int    BASE_DAMAGE      = 90;      // 1発あたりの爆発ダメージ
    static constexpr float  EXPLOSION_RADIUS = 3.0f;    // 爆発半径（5発同時のため小さめ）

    static constexpr int    MISSILE_COUNT       = 5;      // 1射あたりの本数
    static constexpr float  SPREAD_WIDTH        = 5.0f;   // 横方向の膨らみ幅
    static constexpr float  ARC_HEIGHT          = 3.0f;   // 上方向の弧の高さ
    static constexpr float  TARGET_FORWARD_DIST = 30.0f;  // 非ロックオン時の標的までの前方距離

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
    static constexpr float  ENERGY_MAX    = 3000.0f;  // エネルギー最大値
    static constexpr float  ENERGY_COST   = 1.0f;    // 1発のエネルギーコスト
    static constexpr float  ENERGY_REGEN  = 50.0f;   // 自動回復量/秒（3000 を約60秒で全回復）
    static constexpr double SE_INTERVAL   = 0.1;     // SE重複再生防止間隔（秒）
    static constexpr double REGEN_DELAY   = 0.7;     // 最後に消費してから自動回復が始まるまでの待ち時間（秒）

    double m_cooldown   = 0.0;
    double m_seCooldown = 0.0;
    float  m_energy     = ENERGY_MAX;
    int    m_shootSE    = -1;
    double m_regenDelay = 0.0;      // 自動回復再開までの残り待ち時間（秒）。消費のたび REGEN_DELAY にリセット
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
    static constexpr int    BASE_DAMAGE   = 35;      // 1ペレットの基礎ダメージ
    static constexpr int    PELLET_COUNT  = 11;      // 1射のペレット数
    static constexpr float  SPREAD_DEG   = 9.0f;  // 最大拡散角（度）

    double m_cooldown = 0.0;
    int    m_shootSE  = -1;
};


//==============================================================================
// 近接武器（WeaponMelee）
//
// ■特性
// ・弾を撃たず、前方を薙ぎ払って範囲ダメージを与える白兵武器
// ・TryFire で振り動作を開始し、振りの当たるフレームで前方の敵にヒット
// ・ヒットは Bullet_AddExplosion（球状の範囲ダメージ）で既存の爆発経路に載せる
// ・見た目はマシンガンモデルを流用し、振りは手続きアニメ（GetSwingAngleDeg）
//==============================================================================
class WeaponMelee : public PlayerWeapon
{
public:
    void        Initialize() override;
    void        Finalize()   override;
    void        Update(double dt) override;
    bool        TryFire(const DirectX::XMFLOAT3& muzzlePos,
                        const DirectX::XMFLOAT3& aimDir,
                        float damageMult) override;
    const char* GetName() const override { return "近接ブレード"; }
    float GetSwingAngleDeg() const override;
    bool  IsSwinging()       const override { return m_swinging; }
    float GetSwingThrust01() const override;
    void  SetBladeWorldPos(const DirectX::XMFLOAT3& p) override { m_bladeWorldPos = p; }
    double GetSwingElapsed() const override { return m_swinging ? m_swingTimer : 0.0; }

private:
    static constexpr int    BASE_DAMAGE     = 920;    // 1振りのダメージ（DPSでトリプルMGの約1.2倍：920/0.85≈1082 vs 900）
    static constexpr float  REACH           = 3.0f;   // 前方の当たり中心までの距離
    static constexpr float  HIT_RADIUS      = 2.5f;   // 当たり半径（水平＝前方リーチ）
    static constexpr float  HIT_VSCALE      = 0.5f;   // 縦判定の倍率（<1で上下に広い。縦半径≒HIT_RADIUS/HIT_VSCALE）
    static constexpr float  KNOCKBACK_STRENGTH = 15.0f;  // ヒット時に敵へ与えるノックバック速度（減衰して止まる）
    static constexpr double SWING_DURATION  = 0.60;   // 振り動作の長さ（秒）※ゆっくり
    static constexpr double CONTACT_TIME    = 0.26;   // 振り開始から何秒でヒット判定するか（薙ぎが正面を通る頃）
    static constexpr double FIRE_INTERVAL   = 0.85;   // 次の振りまでのクールダウン（秒）
    // 肩を軸にした薙ぎの角度（右手基準：+が右=外側, -が左=内側, 0が正面）
    static constexpr float  SWING_START_DEG = -50.0f; // タメ位置＝左前（斜め前。真左にはしない）
    static constexpr float  SWING_END_DEG   = 120.0f; // 振り抜き＝右（外側）

    double            m_cooldown   = 0.0;
    double            m_swingTimer = 0.0;             // 振り開始からの経過（秒）
    bool              m_swinging   = false;           // 振り動作中か
    bool              m_hasHit     = false;           // この振りで既にヒット判定を出したか
    DirectX::XMFLOAT3 m_hitCenter  = { 0.0f, 0.0f, 0.0f }; // ヒット球の中心（フォールバック）
    DirectX::XMFLOAT3 m_bladeWorldPos = { 0.0f, 0.0f, 0.0f }; // 刃先ワールド座標（描画側が毎フレーム更新）
    int               m_hitDamage  = 0;               // この振りのダメージ量（倍率適用済み）
    int               m_swingSE    = -1;              // 振りSE（風切り音）
    int               m_hitSE      = -1;              // ヒットSE（打撃音）
};
