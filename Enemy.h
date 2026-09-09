/*==============================================================================

   エネミー制御 [Enemy.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
==============================================================================*/

#ifndef ENEMY_H
#define ENEMY_H

#include <DirectXMath.h>
#include <cmath>
#include "collision.h"
#include "collision_obb.h"

struct MODEL;

//==============================================================================
// エネミー基底クラス
//==============================================================================
class Enemy
{
public:
    static constexpr float ENEMY_SIZE = 0.25;
    static constexpr float ENEMY_HEIGHT = 0.3f;  // 描画オフセット用（OBBはモデルAABBから自動計算）
    static constexpr float ENEMY_HALF_WIDTH_X = 0.25f;
    static constexpr float ENEMY_HALF_WIDTH_Z = 0.25f;

    static constexpr float SIGHT_DIST   = 10.0f;   // 視野距離（5m→10m に拡大）
    static constexpr float CHASE_SPD    = 3.2f;    // 追跡速度（2→3.2 m/s に強化）
    static constexpr float PATROL_SPD   = 1.0f;
    static constexpr float ATTACK_RANGE    = 1.5f;  // 攻撃を開始する距離
    static constexpr float ATTACK_WINDUP  = 0.4f;  // 溜め時間（秒）
    static constexpr float ATTACK_DASH_SPD = 8.0f; // 攻撃ダッシュ速度
    static constexpr float ATTACK_COOLDOWN = 2.0f; // 攻撃後の再発動待機時間（秒）
    static constexpr float MAX_SPEED   = 5.0f;
    static constexpr float GRAVITY_MUL = 3.0f;
    static constexpr float FRICTION = 4.0f;

    static constexpr float SUBSTEP_MAX_STEP = 0.05f;
    static constexpr int   SUBSTEP_MAX_COUNT = 128;

    static constexpr int BULLET_DAMAGE = 1;

    virtual ~Enemy() = default;

    //==========================================================================
    // 初期化
    //
    // ■引数
    // ・position : 初期位置
    //==========================================================================
    virtual void Initialize(const DirectX::XMFLOAT3& position);

    //==========================================================================
    // 終了処理
    //==========================================================================
    virtual void Finalize();

    //==========================================================================
    // 更新処理
    //
    // ■引数
    // ・elapsed_time : 前フレームからの経過時間（秒）
    //==========================================================================
    virtual void Update(double elapsed_time);

    //==========================================================================
    // 描画処理
    //==========================================================================
    virtual void Draw();

    //==========================================================================
    // OBB取得
    //
    // ■戻り値
    // ・エネミーのOBB
    //==========================================================================
    OBB GetOBB() const;

    //==========================================================================
    // AABB取得（デバッグ・旧コード互換用）
    //
    // ■戻り値
    // ・軸平行なAABB
    //==========================================================================
    AABB GetAABB() const;

    //==========================================================================
    // 位置取得
    //
    // ■戻り値
    // ・座標（XMFLOAT3の参照）
    //==========================================================================
    const DirectX::XMFLOAT3& GetPosition() const;

    //==========================================================================
    // ロックオン照準のYオフセット（モデルの視覚的中心までの高さ）
    //
    // ■戻り値
    // ・ロックオン位置のY軸オフセット（m_Position.y からの高さ）
    //==========================================================================
    virtual float GetLockOnCenterOffset() const { return m_lockOnCenterOffset; }

    //==========================================================================
    // 速度ポインタ取得（ノックバック用）
    //
    // ■戻り値
    // ・速度ベクトルのポインタ
    //==========================================================================
    DirectX::XMFLOAT3* GetVelocityPtr();

    //==========================================================================
    // HP設定
    //
    // ■引数
    // ・hp    : 現在のHP
    // ・maxHp : 最大HP
    //==========================================================================
    void SetHP(int hp, int maxHp);

    //==========================================================================
    // ダメージ処理
    //
    // ■引数
    // ・value : ダメージ量
    //==========================================================================
    void Damage(int value);

    //==========================================================================
    // ノックバック
    // ・center から自分へ向かう水平方向へ dist だけ押し出す（近接ヒット用）
    //==========================================================================
    void ApplyKnockback(const DirectX::XMFLOAT3& center, float dist);

    //==========================================================================
    // HP取得
    //
    // ■戻り値
    // ・現在のHP
    //==========================================================================
    int GetHP() const;

    //==========================================================================
    // 最大HP取得
    //
    // ■戻り値
    // ・最大HP
    //==========================================================================
    int GetMaxHP() const;

    //==========================================================================
    // 死亡判定
    //
    // ■戻り値
    // ・true : 死亡 / false : 生存
    //==========================================================================
    bool IsDead() const;

    //==========================================================================
    // 生存判定
    //
    // ■戻り値
    // ・true : 生存 / false : 死亡
    //==========================================================================
    bool IsAlive() const { return m_IsAlive; }

    //==========================================================================
    // 外部から強制的に死亡扱いにする（爆発エリアダメージによる二重スコア防止）
    //==========================================================================
    void Kill() { m_IsAlive = false; }
    void ConfirmDeath();  // IsDeferDeath() エネミーの死亡を外部から確定させる

    //==========================================================================
    // 討伐スコアを返す（サブクラスでオーバーライド）
    //==========================================================================
    virtual int  GetKillScore()  const { return 2000; }
    virtual const wchar_t* GetDisplayName() const { return L"HOSTILE"; }   // HUD 表示名（ボスの体力表示など）
    virtual bool IsDropItem()    const { return true; }  // アイテムをドロップするか
    virtual bool IsDeferDeath()  const { return false; } // 死亡フラグを遅延させるか
    virtual bool IsArmored()     const { return false; } // 装甲持ち：弾直撃で10%カット＋ヒット音は従来のまま（ボス用）

    //==========================================================================
    // 向き（正面ベクトル）を直接セット
    // BossIntro など外部から初期方向を指定する場合に使用
    //==========================================================================
    void SetFront(const DirectX::XMFLOAT3& front) { m_Front = front; }

    //==========================================================================
    // シャドウパス用深度描画
    // ShadowMap::BeginPass() ～ EndPass() の間で呼ぶ
    //==========================================================================
    virtual void DrawShadow();

    //==========================================================================
    // 壁・他のエネミーとの衝突に使う半径（モデルの横幅から算出。許容範囲つき）
    //==========================================================================
    float GetCollisionRadius() const;

    // ボス（他のエネミーに押されない・大きく押し返す）か
    virtual bool IsBoss() const { return IsDeferDeath(); }

    // 生成時の種別（EnemyType の値。EnemyManager::Spawn が設定する。-1 = 不明）
    void SetTypeId(int id) { m_TypeId = id; }
    int  GetTypeId() const { return m_TypeId; }

    // 体の上端（m_Position.y からの高さ）。高さを考慮した壁判定に使う
    float GetBodyTop() const { return m_obbBottomY + m_obbHalfHeight; }

    // 水平方向に押し出す（エネミー同士の押し合い用。壁は越えない）
    void Nudge(float dx, float dz)
    {
        const float len = sqrtf(dx * dx + dz * dz);
        if (len < 1e-5f) return;
        MoveHorizWithWallClamp(m_Position, dx / len, dz / len, len);
    }

protected:
    // 地面から浮かせて描く量（浮遊する機種がオーバーライドする）
    virtual float GetHoverHeight() const { return 0.0f; }
    //==========================================================================
    // 見た目のアニメーション（球体型エネミー共通）
    //   Update の最後で UpdateAnim(dt) を呼び、Draw で下の関数を使う
    //==========================================================================
    void  UpdateAnim(float dt);
    // まばたき中の目の縦の倍率（1 = 開いている）。squint を掛けて「目を細める」表現にも使う
    float BlinkScale() const;
    // 本体中心まわりの傾き（進む方向へ前傾・旋回で横に傾く）と弾み。
    // 向きの回転の後、位置の移動の前に掛ける。leanMax / bankMax はラジアン、hop は弾む高さ（m）
    DirectX::XMMATRIX BallMotion(float leanMax, float bankMax, float hop) const;
    // 目のメッシュだけ縦に縮めて描く（eyeY = モデル空間での目の中心の高さ）
    void  DrawEyeModel(MODEL* model, int eyeMesh, float eyeY, const DirectX::XMMATRIX& world, float eyeScaleY) const;

    float m_AnimTime   = 0.0f;   // アニメーションの経過時間
    float m_BlinkTimer = 0.0f;   // まばたきのタイマー
    float m_BlinkNext  = 2.5f;   // 次のまばたきまで（秒）
    float m_Lean01     = 0.0f;   // 前傾の度合い（0〜1。速さから）
    float m_Bank01     = 0.0f;   // 横の傾き（-1〜1。旋回の速さから）
    float m_PrevYaw    = 0.0f;
    float m_Recoil     = 0.0f;   // 射撃の反動（1 → 0 へ戻る）

    // 高さを考慮した壁判定：壁 AABB がこのエネミーの体の高さと重なるか。
    // 足元より下の壁（下の階の手すり等）や頭上の壁（上の階の床板）は無視する
    bool WallOverlapsBody(const AABB& wall, float posY) const
    {
        return wall.max.y >= posY + 0.1f && wall.min.y <= posY + GetBodyTop();
    }

    //==========================================================================
    // 任意の位置からエネミー用OBBを作成
    //
    // ■引数
    // ・pos : 中心座標（XMVECTORで渡す）
    //
    // ■戻り値
    // ・指定位置でのエネミーOBB
    //==========================================================================
    OBB ConvertPositionToOBB(const DirectX::XMVECTOR& pos) const;

    //==========================================================================
    // XZ速度制限
    //
    // ■引数
    // ・v        : 速度ベクトル
    // ・maxSpeed : 最大速度
    //
    // ■戻り値
    // ・制限後の速度ベクトル
    //==========================================================================
    DirectX::XMVECTOR ClampXZSpeed(DirectX::XMVECTOR v, float maxSpeed) const;

    //==========================================================================
    // 壁衝突解決
    //
    // ■引数
    // ・ioPos  : 位置（入出力）
    // ・ioVel  : 速度（入出力）
    // ・ioDest : 巡回目的地（入出力）
    //==========================================================================
    void ResolveWallCollisionAtPosition(DirectX::XMVECTOR* ioPos, DirectX::XMVECTOR* ioVel, DirectX::XMFLOAT3* ioDest);

    // 水平方向へ dist だけ移動。壁（円 r=ENEMY_HALF_WIDTH_X）に当たったら押し出して止める。
    void MoveHorizWithWallClamp(DirectX::XMFLOAT3& p, float nx, float nz, float dist);

    //==========================================================================
    // サブステップ移動（トンネリング対策）
    //
    // ■引数
    // ・ioPos : 位置（入出力）
    // ・ioVel : 速度（入出力）
    // ・dt    : 経過時間
    //==========================================================================
    void MoveWithSubSteps(DirectX::XMVECTOR* ioPos, DirectX::XMVECTOR* ioVel, float dt);

    //==========================================================================
    // 床衝突解決
    //
    // ■引数
    // ・ioPos : 位置（入出力）
    // ・ioVel : 速度（入出力）
    //==========================================================================
    void ResolveFloorCollision(DirectX::XMVECTOR* ioPos, DirectX::XMVECTOR* ioVel);

    //==========================================================================
    // プレイヤー衝突処理（ダメージ＋ノックバック）
    //
    // ■引数
    // ・ioPos : エネミーの位置（入出力）
    // ・ioVel : エネミーの速度（入出力）
    //==========================================================================
    void ResolvePlayerCollision(DirectX::XMVECTOR* ioPos, DirectX::XMVECTOR* ioVel);

    //==========================================================================
    // 弾ヒット処理
    //==========================================================================
    void ResolveBulletHits();

    //==========================================================================
    // モデルAABBからロックオンYオフセットを自動計算してセット
    //==========================================================================
    void ComputeLockOnOffsetFromModel();

    DirectX::XMFLOAT3 m_Position    {};  // 現在位置
    DirectX::XMFLOAT3 m_Velocity    {};  // 速度
    DirectX::XMFLOAT3 m_KnockbackVel{};  // ノックバック速度（AI/クランプと独立に減衰しながら位置へ反映）
    DirectX::XMFLOAT3 m_Front       {};  // 向き（正面ベクトル）
    DirectX::XMFLOAT3 m_Destination {};  // 巡回目的地

    MODEL* m_pModel = nullptr;         // モデル

    int  m_Hp    = 0;                  // 現在のHP
    int  m_MaxHp = 0;                  // 最大HP

    bool m_IsAlive = false;            // 生存フラグ
    int  m_TypeId = -1;                // 生成時の種別（EnemyType の値）
    mutable bool m_IsGround = false;   // 接地フラグ
    bool m_WasChasing = false;         // 前フレームの追跡フラグ

    // 記憶・索敵
    DirectX::XMFLOAT3 m_LastSeenPos   {};   // プレイヤーを最後に視認した位置
    float             m_InvestigateTimer = 0.0f; // 索敵残り時間（>0 で Investigate 状態）

    // 攻撃モーション
    float m_AttackTimer    = 0.0f;  // 溜め経過時間
    float m_AttackCooldown = 0.0f;  // 攻撃後の再発動ウェイト
    bool  m_IsAttacking    = false; // 攻撃溜め中フラグ

    // 接触ダメージクールダウン
    float m_ContactDamageCooldown = 0.0f;

    // 速度上限オーバーライド（サブクラスが突進などで一時的に引き上げる用）
    // デフォルトは MAX_SPEED。Enemy::Update の ClampXZSpeed はこの値を参照する
    float m_SpeedCap = MAX_SPEED;

    float m_lockOnCenterOffset = 0.4f; // ロックオンYオフセット（モデル読込後に自動設定）
    float m_DrawOffsetY = ENEMY_HEIGHT; // 描画の持ち上げ量（モデル底面を足元に合わせる。モデル読込後に自動設定）

    // モデルAABBから算出した衝突OBB半径（ComputeLockOnOffsetFromModel で設定）
    float m_obbHalfWidth  = ENEMY_HALF_WIDTH_X; // X/Z 半径
    float m_obbHalfHeight = ENEMY_HEIGHT * 0.5f; // Y 半径
    float m_obbBottomY    = ENEMY_HEIGHT * 0.5f; // m_Position.y からOBB中心までのY距離
};

void Enemy_LoadSE();
void Enemy_UnloadSE();
void Enemy_PlayDeathSE();

#endif // ENEMY_H
