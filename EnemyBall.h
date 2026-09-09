/*==============================================================================

   球体型エネミー（複数パーツ・アニメーション付き） [EnemyBall.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   既存の enemy / enemy_speed / enemy_sniper と同じ作り（Maya の球体ボディ＋
   つり目のスリット＋単色ランバート）の追加エネミー5種。
   パーツを別モデルにして、動きに合わせてアニメーションさせる。

     Wing     白い球＋赤い翼     翼を羽ばたかせて旋回し、溜めてから急降下突撃
     Gatling  灰色の球＋回転砲   砲身を回して加速 → 長い連射
     Orbiter  青い球＋子機3つ    子機が周回し、順番に子機の位置から撃つ
     Walker   赤い球＋4本脚      脚で歩く。近づくと跳び上がって着地の衝撃波
     Halo     黄色い球＋リング   リングを回す。高速回転させてから螺旋弾幕

   モデル : resource/Models/enemy_<種類>_<パーツ>.obj（modelgen2 で生成）
   モデルは EnemyParts で共有する（インスタンスごとに読み込まない）。
==============================================================================*/
#pragma once
#include "Enemy.h"

class EnemyBall : public Enemy
{
public:
    enum class Kind { Wing, Gatling, Orbiter, Walker, Halo, Count };

    explicit EnemyBall(Kind kind) : m_Kind(kind) {}

    void Initialize(const DirectX::XMFLOAT3& position) override;
    void Finalize() override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void DrawShadow() override;

    int  GetKillScore() const override;
    const wchar_t* GetDisplayName() const override;

    // エネミー図鑑のプレビュー用：world（ボディ中心の行列）にアイドル中の姿を描く
    static void DrawPreview(Kind kind, const DirectX::XMMATRIX& world, float time);
    // 図鑑用の性能値
    static int   SpecHP(Kind kind);
    static float SpecSpeed(Kind kind);

protected:
    float GetHoverHeight() const override;

private:
    // アニメーションの状態（描画に使う値だけ）
    struct Pose
    {
        float time      = 0.0f;   // 経過時間
        float flap      = 0.0f;   // 翼の角度（ラジアン）
        float gunSpin   = 0.0f;   // 回転砲の角度
        float orbit     = 0.0f;   // 子機の周回角
        float walk      = 0.0f;   // 歩行の位相
        float walkAmp   = 0.0f;   // 脚の振り幅（0〜1。止まっているときは 0）
        float ringSpin  = 0.0f;   // リングの回転角
        float ringTilt  = 0.0f;   // リングの傾き
        float bob       = 0.0f;   // 上下の揺れ（m）
        float eye       = 1.0f;   // 目の縦の倍率（まばたき・溜めで細める）
    };
    static void DrawParts(Kind kind, const Pose& pose, const DirectX::XMMATRIX& bodyWorld, bool shadow);
    static DirectX::XMFLOAT3 BitOffset(const Pose& pose, int index);   // 子機のボディ中心からの位置（向き回転前）

    void Think(float dt, DirectX::XMVECTOR& vel, float dist, bool seen);
    void Patrol(float dt, DirectX::XMVECTOR& vel);
    void FacePlayer(float dt, float turnSpeed);
    DirectX::XMMATRIX BodyWorld() const;
    DirectX::XMFLOAT3 LocalToWorld(const DirectX::XMFLOAT3& local) const;   // ボディ中心基準 → ワールド

    Kind  m_Kind;
    Pose  m_Pose;
    int   m_State       = 0;
    float m_StateTimer  = 0.0f;
    float m_Cooldown    = 0.0f;
    float m_SubTimer    = 0.0f;   // 連射・子機射撃の間隔
    int   m_Shots       = 0;      // 連射の残り
    int   m_NextBit     = 0;      // 次に撃つ子機
    float m_StrafeSign  = 1.0f;
    float m_StrafeTimer = 0.0f;
    float m_SpinSpeed   = 0.0f;   // 回転砲・リングの回転速度（rad/s）
    float m_Flash       = 0.0f;   // 溜め中の発光（0〜1）
    bool  m_HitDone     = false;
    bool  m_WasAirborne = false;  // 跳躍中（着地の瞬間を検出する）
    float m_SpiralAngle = 0.0f;
    DirectX::XMFLOAT3 m_DashDir = { 0.0f, 0.0f, 1.0f };
};
