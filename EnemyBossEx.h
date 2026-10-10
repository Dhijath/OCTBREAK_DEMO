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

   ■第二作戦区域のボス（ユーザー作のエネミーと同じ球体型。パーツが動く）
     BASTION  要塞機         灰色の球＋両腕のガトリング＋周回する盾。ゆっくり迫る。
                             交差する掃射 / 回転する十字砲火 / ガトリング型の投下と衝撃波
     NEST     母艦機         白い球＋翼＋周回する子機。高く浮いて距離を取る。
                             翼型の射出 / 予告付きの絨毯爆撃 / 子機からの斉射
     ECLIPSE  日蝕           黄色い球＋3本のジャイロリング＋衛星。最終作戦の大型兵器。
                             二重螺旋 / 引き寄せてからの全方位爆発 / 衛星からの連射（激昂で瞬間移動）

   既存のボス（EnemyBoss）と同じく、撃破後の演出（BossDefeat）が終わるまで
   死亡を遅らせ（IsDeferDeath）、登場演出（BossIntro）中は攻撃しない。
==============================================================================*/
#pragma once
#include "Enemy.h"

class EnemyBossEx : public Enemy
{
public:
    enum class Kind { Argus, Goliath, Omega, Hydra, Spectre, Bastion, Nest, Eclipse };

    explicit EnemyBossEx(Kind kind) : m_Kind(kind) {}

    // 球体型ボス（BASTION / NEST / ECLIPSE）の本体＋パーツを描く。エネミー図鑑のプレビューと共用。
    //   t       : アニメーションの時刻（秒）
    //   gunSpin : BASTION のガトリングの回転角
    //   recoil  : 射撃の反動（0〜1）
    static void DrawBallRig(Kind kind, MODEL* body, const DirectX::XMMATRIX& base,
                            float t, float gunSpin, float recoil, bool shadow);

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
    void FanFrom(const DirectX::XMFLOAT3& from, int count, float spreadDeg, float speed, int damage, float lead);
    void Summon(int type, int count);
    bool Blink(float minDist, float maxDist);

    DirectX::XMFLOAT3 Center(float height) const;
    void FireR(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& dir, int damage, float speed);   // 発射＋反動
    void DrawRig(bool shadow);   // 本体＋動くパーツ

    // 描画と同じ配置の基準行列（向き＋位置＋浮遊）と、その上の点のワールド座標（子機・衛星から撃つ用）
    DirectX::XMMATRIX RigBase() const;
    DirectX::XMFLOAT3 RigPoint(const DirectX::XMFLOAT3& local) const;
    float RigTime() const { return m_AnimTime * (m_Enraged ? 1.6f : 1.0f); }

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

    // 第二作戦区域のボス
    float m_PatternTime = 0.0f;   // パターン開始からの経過時間（掃射の進み具合など）
    int   m_Shots       = 0;      // パターン内の発射回数（発射音を間引く）
    float m_GunSpin     = 0.0f;   // BASTION：ガトリングの回転角
    float m_GunSpinRate = 4.0f;   //          回転速度（掃射中は上がる）
    static constexpr int MARK_MAX = 6;
    DirectX::XMFLOAT3 m_Marks[MARK_MAX] = {};   // NEST：爆撃の着弾予告地点
    int   m_MarkCount   = 0;
    int   m_Salvo       = 0;      // NEST：爆撃の回数
    float m_BlinkTimer  = 0.0f;   // 予告の点滅
};
