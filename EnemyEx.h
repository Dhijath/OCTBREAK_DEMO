/*==============================================================================

   追加エネミー [EnemyEx.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   5種類の追加エネミーを1つのクラスで扱う（種類ごとに性能表と行動を切り替える）。

     Bomber    自爆型   高速で接近し、近くで溜めてから爆発（範囲ダメージ）
     Gunner    突撃型   6〜11m の距離を保ち、横移動しながら3連射（予測射撃）
     Artillery 砲撃型   遠距離で停止し、光って溜めてから重い砲弾を3発
     Turret    固定砲台 動かない。全方位弾と2連の狙い撃ち。耐久が高い
     Phantom   幻影型   プレイヤーの近くへ瞬間移動し、溜めてから斬りかかる

   移動・衝突・被弾は Enemy 基底の処理をそのまま使う。
   モデルは resource/Models/enemy_*.obj（modelgen3 で生成。テクスチャなし）。
   本体と動くパーツ（_spikes / _leg_l,_r / _barrel / _dome / _blades,_ring）に分かれている。
==============================================================================*/
#pragma once
#include "Enemy.h"

class EnemyEx : public Enemy
{
public:
    enum class Kind { Bomber, Gunner, Artillery, Turret, Phantom };

    explicit EnemyEx(Kind kind) : m_Kind(kind) {}

    void Initialize(const DirectX::XMFLOAT3& position) override;
    void Update(double elapsed_time) override;
    void Draw() override;
    void DrawShadow() override;

    int  GetKillScore() const override;
    const wchar_t* GetDisplayName() const override;

protected:
    float GetHoverHeight() const override;

private:
    struct Spec
    {
        const char*    model;
        int            hp;
        float          chaseSpeed;
        float          patrolSpeed;
        float          sight;
        int            score;
        const wchar_t* name;
    };
    static const Spec& SpecOf(Kind kind);

    // 種類ごとの行動（速度 vel を決める。攻撃もここで行う）
    void ThinkBomber   (float dt, DirectX::XMVECTOR& vel, float dist, bool seen);
    void ThinkGunner   (float dt, DirectX::XMVECTOR& vel, float dist, bool seen);
    void ThinkArtillery(float dt, DirectX::XMVECTOR& vel, float dist, bool seen);
    void ThinkTurret   (float dt, DirectX::XMVECTOR& vel, float dist, bool seen);
    void ThinkPhantom  (float dt, DirectX::XMVECTOR& vel, float dist, bool seen);

    void Patrol(float dt, DirectX::XMVECTOR& vel);   // 共通の巡回・追跡（EnemyAI）
    void FacePlayer(float dt, float turnSpeed);       // プレイヤーの方へ向き直る
    DirectX::XMFLOAT3 Muzzle(float forward, float height) const;
    void FireR(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& dir, int damage, float speed);   // 発射＋反動
    void DrawRig(bool shadow);   // 本体＋動くパーツ

    Kind  m_Kind;
    int   m_State      = 0;      // 種類ごとの状態番号
    float m_StateTimer = 0.0f;   // 状態に入ってからの時間
    float m_Cooldown   = 0.0f;   // 次の攻撃までの時間
    int   m_Burst      = 0;      // 連射の残り
    float m_BurstTimer = 0.0f;
    float m_StrafeSign = 1.0f;   // 横移動の向き
    float m_StrafeTimer = 0.0f;
    float m_Spin       = 0.0f;   // 全方位弾の回転角
    float m_Flash      = 0.0f;   // 溜め中の発光（0〜1）
    bool  m_HitDone    = false;  // 斬撃・爆発のダメージを与えたか
    DirectX::XMFLOAT3 m_DashDir = { 0.0f, 0.0f, 1.0f };

    // パーツのアニメーション
    float m_PartSpin  = 0.0f;   // トゲ・刃の回転角
    float m_WalkPhase = 0.0f;   // 歩行の位相（突撃型の脚）
    float m_BaseYaw   = 0.0f;   // 固定砲台の台座の向き（台座は回らず、ドームだけ向きを変える）
};
