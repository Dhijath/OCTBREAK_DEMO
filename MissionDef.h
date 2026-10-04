/*==============================================================================

   ミッション定義テーブル [MissionDef.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   アドベンチャーモードの全ミッションの静的パラメータを一元管理する。
   MissionDef.cpp に実体 k_MissionDefs[] を定義。

   ■ミッションの2つの形式
     ・ブロックステージ（legacy = false）
         専用に設計したステージ（BlockStage）を 1〜2 フェーズ続けて攻略する。
         フェーズごとにステージ・達成条件・制限時間・増援を指定する。
         敵の主力はステージ側が要所に配置する部隊（BlockStageLayouts.cpp）。
     ・デフォルト（legacy = true）
         従来の自動生成ダンジョン。floorCount 回ゴールに到達すると
         ボス部屋へ進み、ボス撃破でクリア。

   ■難易度を決める値
     ・enemyHpScale : 通常エネミーの耐久倍率（ボスは対象外）
     ・enemySight   : 敵の視野距離の倍率（広いステージでも敵が反応するように）
     ・timeLimit    : フェーズの制限時間。超えると作戦失敗
     ・waves        : 増援。フェーズ開始からの時刻に、降下地点から出現する。
                      敵を全滅させた場合は次の増援が前倒しで来る

==============================================================================*/
#pragma once
#include "BlockStage.h"

enum MissionID
{
    MISSION_RECON = 0,     // 廃棄市街の強行偵察（目標地点へ到達）
    MISSION_INTERCEPT,     // 輸送ターミナルで高機動部隊を全滅
    MISSION_HEAVY,         // 要塞の重装部隊を全滅
    MISSION_BOSS,          // 制御区画で大型兵器を撃破
    MISSION_ASSAULT,       // 渓谷を突破 → 大型兵器を撃破（2フェーズ）
    MISSION_PLANT,         // 二層プラントの制御室を確保 → 大型兵器を撃破（2フェーズ）
    MISSION_DEFAULT,       // デフォルト（従来の自動生成ダンジョン）
    MISSION_COUNT
};

// 敵編成（スポーン種別の割り振り方）
enum class EnemyMix
{
    Balanced,  // 混成（従来の割り振り）
    Swarm,     // 高機動型主体
    Heavy,     // 重装型主体
    Sniper,    // 狙撃型主体
};

// フェーズの達成条件
enum class MissionObjective
{
    ReachGoal,    // 目標地点（ゴール）へ到達
    Annihilate,   // 敵を全滅（増援もすべて含む）
    DestroyBoss,  // 大型兵器（ボス）を撃破。最終フェーズにのみ指定できる
};

// 増援1回ぶん
struct ReinforceWave
{
    float    time;    // フェーズ開始からの秒数（0 = この枠は使わない）
    int      count;   // 出現数
    EnemyMix mix;     // 編成
};

constexpr int MISSION_WAVE_MAX  = 4;
constexpr int MISSION_PHASE_MAX = 2;

struct MissionPhase
{
    BlockStageID     stage;
    MissionObjective objective;
    int              enemyCount;   // 部隊とは別に散らばる哨戒兵の数
    EnemyMix         enemyMix;     // 哨戒兵の編成
    float            timeLimit;    // 制限時間（秒。0 = なし）
    ReinforceWave    waves[MISSION_WAVE_MAX];
    int              bossType = 0;     // DestroyBoss で出す大型兵器（EnemyType の値。0 = 従来のボス）
    bool             noSquads = false; // true でステージ設計の部隊を置かない（同じステージで続けるボス戦用）
};

struct MissionDef
{
    const wchar_t* code;        // 識別コード（"MISSION 01"）
    const wchar_t* title;       // 作戦名
    const wchar_t* client;      // 依頼主
    const wchar_t* area;        // 作戦領域
    const wchar_t* objective;   // 作戦目標
    const wchar_t* briefing;    // ブリーフィング本文（\n で改行）

    int   reward;               // 成功報酬（スコアとアセンブリの予算に加算）
    int   rank;                 // 難度（1〜5）
    float enemyHpScale;         // 通常エネミーの耐久倍率
    float enemySight;           // 敵の視野距離の倍率

    bool legacy;                // true = 従来の自動生成ダンジョン

    // ── ブロックステージ用（legacy = false）──
    int          phaseCount;
    MissionPhase phases[MISSION_PHASE_MAX];

    // ── デフォルト用（legacy = true）──
    int      floorCount;        // 突破する階層数（ゴール到達回数）
    int      enemySpawnRate;    // 敵の密度（小さいほど多い。階層ごとに 2 ずつ減る）
    EnemyMix enemyMix;          // 敵編成
    bool     hasBoss;           // 階層突破後にボス部屋へ進むか
};

extern const MissionDef k_MissionDefs[MISSION_COUNT];

// 出撃するミッション（ミッション選択画面で確定・SaveData で保存）
int               Mission_GetCurrent();
void              Mission_SetCurrent(int index);
const MissionDef& Mission_GetCurrentDef();

// クリア済みフラグ（ミッション選択画面の「CLEARED」表示用・SaveData で保存）
bool Mission_IsCleared(int index);
void Mission_SetCleared(int index, bool cleared);

// 表示用の集計（形式の違いを吸収する）
bool     Mission_HasBoss(const MissionDef& def);       // 大型兵器が出るか
int      Mission_GetStageCount(const MissionDef& def); // 作戦段階の数
EnemyMix Mission_GetMainEnemyMix(const MissionDef& def);
float    Mission_GetTimeLimit(const MissionDef& def);  // 最初のフェーズの制限時間（0 = なし）
int      Mission_GetReinforceCount(const MissionDef& def); // 増援の総数
