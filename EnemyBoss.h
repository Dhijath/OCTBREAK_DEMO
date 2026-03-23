/*==============================================================================
   ボス型エネミー [EnemyBoss.h]
                                                         Author : 51106
                                                         Date   : 2026/03/09
--------------------------------------------------------------------------------
   ・HP極大・高視野のボス敵
   ・ダンジョン末端のボスルーム（13×13タイル）中央にスポーンする
   ・左右2門のバレルからプレイヤーへ弾を発射する（間隔ランダム、弾速遅め、ダメ24）
   ・3発に1回は扇状5発の散弾を発射する
   ・プレイヤーへ突進攻撃（0.5s溜め→1.5s突進→2.0sクールダウン）
   ・近距離では距離を保ち、遠距離では追跡する
   ・射撃音はショットガンSEを使用
==============================================================================*/
#ifndef ENEMY_BOSS_H
#define ENEMY_BOSS_H
#include "Enemy.h"

//==============================================================================
// ボスのフェーズ
//==============================================================================
enum class BossPhase
{
    INTRO,          // 登場イントロ（下バレルを拳のように合わせて上げる）
    NORMAL,         // 通常（AI移動・射撃・距離保持）
    CHARGE_WINDUP,  // 突進溜め（0.5秒・停止してプレイヤーを狙う）
    CHARGING,       // 突進中（1.5秒・高速直線移動）
    COOLDOWN        // クールダウン（2.0秒・減速して通常へ戻る）
};

//==============================================================================
// ボス型エネミークラス
//
// 特徴
// ・HP 8000（通常の約27倍）
// ・視野距離 20m（全エネミー中最大）
// ・モデルスケール 0.9f
// ・左右バレル2門からプレイヤーへ射撃（間隔ランダム0.7〜1.6秒、弾速4.0f、ダメージ24）
// ・3発に1回は扇状散弾（±30°で5発）
// ・バレルがプレイヤー方向を追従する
// ・プレイヤーが5m以内に近づくと距離を保つため後退する
// ・7秒に1回、突進攻撃を行う（接触で40ダメージ＋強ノックバック）
//==============================================================================
class EnemyBoss : public Enemy
{
public:
    static constexpr int   HP = 8000;  // ボスHP
    static constexpr float BOSS_SIZE = 0.9f;  // モデルスケール
    static constexpr float SIGHT_DIST = 200.0f; // 視野距離
    static constexpr float CHASE_SPD = 4.5f;  // 追跡速度（m/s）
    static constexpr float PATROL_SPD = 2.0f;  // 巡回速度（m/s）
    // 射撃パラメータ
    static constexpr int   SHOOT_DAMAGE       = 216;   // 1発あたりのダメージ
    static constexpr float BOSS_BULLET_SPEED  = 4.0f;  // 弾速（遅め）
    // バレル配置（前面2×2）— 前面OBBサーフェスに上右・上左・下右・下左
    static constexpr int   BARREL_COUNT    = 4;      // バレル門数（前面2×2）
    static constexpr float BARREL_FLIP_DEG = 180.0f; // モデル反転補正（Player.cpp と同値）
    // イントロパラメータ
    static constexpr float INTRO_DURATION  = 2.5f;   // イントロ全体の長さ（秒）
    static constexpr float INTRO_RAISE_END = 2.0f;   // バレルを上げ終わるまでの時間（秒）
    // 突進パラメータ
    static constexpr float CHARGE_WINDUP_TIME = 0.5f;  // 溜め時間（秒）
    static constexpr float CHARGE_MOVE_TIME   = 1.5f;  // 突進持続時間（秒）
    static constexpr float CHARGE_COOLDOWN_TIME = 2.0f; // クールダウン時間（秒）ｄ
    static constexpr float CHARGE_SPEED       = 34.0f; // 突進速度（m/s）
    static constexpr float CHARGE_INTERVAL    = 7.0f;  // 突進インターバル（秒）
    static constexpr float CHARGE_DAMAGE_DIST = 2.0f;  // 接触判定距離
    static constexpr int   CHARGE_DAMAGE      = 360;   // 突進ダメージ
    static constexpr float CHARGE_KNOCKBACK   = 25.0f; // ノックバック強さ
    static constexpr float KEEP_DISTANCE      = 5.0f;   // 距離を保つ閾値
    static constexpr float RETREAT_SPEED      = 13.0f;   // 後退速度（m/s）
    static constexpr float STRAFE_SPEED       = 3.0f;   // ストレイフ速度（m/s）
    static constexpr float STRAFE_CHANGE_MIN  = 2.0f;   // 方向転換の最短間隔（秒）
    static constexpr float STRAFE_CHANGE_MAX  = 5.0f;   // 方向転換の最長間隔（秒）
    static constexpr float LOCK_ROTATION_SPD  = 250.0f; // ロックオン回転速度（度/秒）
    static constexpr float SHOOT_CHARGE_GAP   = 0.6f;   // 射撃直後に突進を抑制する時間（秒）

    //==========================================================================
    // 初期化処理
    //==========================================================================
    void Initialize(const DirectX::XMFLOAT3& position) override;
    //==========================================================================
    // 終了処理（SEのアンロード・モデル解放）
    //==========================================================================
    void Finalize() override;
    //==========================================================================
    // 更新処理（基底AI＋バレル追従＋射撃タイマー＋突進ステートマシン）
    //==========================================================================
    void Update(double elapsed_time) override;
    //==========================================================================
    // 描画処理（本体＋左右バレル）
    //==========================================================================
    void Draw() override;

private:
    // 前面2×2バレル: [0]=上右 [1]=上左 [2]=下右 [3]=下左
    MODEL* m_pBarrel[BARREL_COUNT] = {};
    float  m_BarrelRotX = 0.0f;  // バレル仰角（プレイヤー追従）
    float  m_shootTimer = 0.0f;  // 射撃インターバルタイマー
    float  m_nextShootInterval = 1.0f; // 次の射撃間隔（ランダム化）
    int    m_shotCount  = 0;     // 発射回数カウンタ（散弾切替用）
    int    m_shootSE = -1;       // ショットガンSEハンドル

    // 突進ステート
    BossPhase         m_bossPhase         = BossPhase::NORMAL;
    float             m_chargeTimer       = 0.0f;  // フェーズ内経過時間
    float             m_chargeCooldown    = 3.0f;  // 次の突進まで（秒）
    DirectX::XMFLOAT3 m_chargeDir         = {};    // 突進方向（正規化済み）
    bool              m_chargeDamageDealt = false; // 突進中ダメージ済みフラグ

    // ストレイフ（横移動）
    float m_strafeTimer = 2.0f;  // 次に方向転換するまでの時間
    int   m_strafeSign  = 1;     // +1=右回り / -1=左回り

    // アクション調停
    float m_lastShotTimer = 0.0f; // 最後に射撃してから経過した時間（突進抑制用）

    //==========================================================================
    // バレルのワールド行列取得
    // faceIndex : 0=前面 / 1=右面 / 2=後面 / 3=左面
    // OBBの各面サーフェスに底面を合わせ、マズルがプレイヤーを向く
    //==========================================================================
    DirectX::XMMATRIX GetBarrelWorldMatrix(int faceIndex);


    //==========================================================================
    // 射撃処理（左右バレルから各1発）
    //==========================================================================
    void Shoot();
    //==========================================================================
    // 散弾処理（±30°の扇状5発）
    //==========================================================================
    void SpreadShoot();
};
#endif // ENEMY_BOSS_H