/*==============================================================================

   追加ボス [EnemyBossEx.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   5体の追加ボスを1つのクラスで扱う。どのボスも「攻撃パターン3種を順番に繰り返し、
   体力が半分を切ると激昂して弾数・速度・頻度が上がる」構成。

     ARGUS    浮遊する眼     距離を取って周回。全方位弾 / 予測扇状弾 / 突撃型を召喚
     GOLIATH  重装歩行機     ゆっくり迫る。衝撃波 / 重砲 / 突進（壁に当たると止まる）
     OMEGA    動力炉         ほぼ動かない。螺旋弾幕 / 高速連射 / 固定砲台を召喚
     HYDRA    三つ首         横へ素早く移動。扇状の一斉射 / 機雷散布 / 自爆型を召喚
     SPECTRE  刃の騎士       瞬間移動を多用。移動後の扇状弾 / 斬撃 / 幻影型を召喚

   既存のボス（EnemyBoss）と同じく、撃破後の演出（BossDefeat）が終わるまで
   死亡を遅らせ（IsDeferDeath）、登場演出（BossIntro）中は攻撃しない。
==============================================================================*/
#pragma once
#include "Enemy.h"

class EnemyBossEx : public Enemy
{
public:
    enum class Kind { Argus, Goliath, Omega, Hydra, Spectre };

    explicit EnemyBossEx(Kind kind) : m_Kind(kind) {}

    void Initialize(const DirectX::XMFLOAT3& position) override;
    void Finalize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void DrawShadow() override;

    int  GetKillScore() const override;
    bool IsDropItem()   const override { return false; }
    bool IsDeferDeath() const override { return true; }
    bool IsArmored()    const override { return true; }
    const wchar_t* GetDisplayName() const override;

protected:
    float GetHoverHeight() const override;

private:
    struct Spec
    {
        const char*    model;
        int            hp;
        int            score;
        const wchar_t* name;
    };
    static const Spec& SpecOf(Kind kind);

    // 移動（種類ごと）
    void Move(float dt, DirectX::XMVECTOR& vel, float dist);

    // 攻撃パターン：step はパターン内の段階、戻り値 true でパターン終了
    bool RunPattern(int pattern, float dt);

    // 弾の撃ち方
    void Ring(int count, float speed, int damage, float height, float angleOffset);
    void AimedFan(int count, float spreadDeg, float speed, int damage, float lead, float height);
    void Summon(int type, int count);
    bool Blink(float minDist, float maxDist);

    DirectX::XMFLOAT3 Center(float height) const;
    void FireR(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& dir, int damage, float speed);   // 発射＋反動
    void DrawRig(bool shadow);   // 本体＋動くパーツ

    Kind  m_Kind;
    bool  m_Enraged = false;

    int   m_Pattern      = 0;     // 実行中のパターン番号（0〜2）
    int   m_Step         = 0;     // パターン内の段階
    float m_StepTimer    = 0.0f;
    float m_RestTimer    = 1.5f;  // パターン間の休み（登場直後は長め）
    bool  m_InPattern    = false;

    float m_Spin         = 0.0f;  // 螺旋・全方位弾の回転角
    float m_StrafeSign   = 1.0f;
    float m_StrafeTimer  = 2.0f;
    float m_Flash        = 0.0f;  // 溜め中の発光
    bool  m_Hold         = false; // このフレームは移動しない（溜め中など）
    int   m_ShootSE      = -1;    // 発射音

    // 突進（GOLIATH）・斬撃（SPECTRE）
    bool              m_Dashing   = false;
    float             m_DashSpeed = 0.0f;
    DirectX::XMFLOAT3 m_DashDir   = { 0.0f, 0.0f, 1.0f };
    bool              m_DashHit   = false;
    DirectX::XMFLOAT3 m_LastPos   = {};

    float m_WalkPhase = 0.0f;   // 歩行の位相（GOLIATH の脚）
};
