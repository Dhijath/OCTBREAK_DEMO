/*==============================================================================

   ミッション定義テーブル [MissionDef.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03

==============================================================================*/
#include "MissionDef.h"
#include "EnemyManager.h"

namespace
{
    constexpr int BOSS_ARGUS   = static_cast<int>(EnemyType::BossArgus);
    constexpr int BOSS_GOLIATH = static_cast<int>(EnemyType::BossGoliath);
    constexpr int BOSS_OMEGA   = static_cast<int>(EnemyType::BossOmega);
    constexpr int BOSS_HYDRA   = static_cast<int>(EnemyType::BossHydra);
    constexpr int BOSS_SPECTRE = static_cast<int>(EnemyType::BossSpectre);
}

//------------------------------------------------------------------------------
// ミッションパラメータ実体
//
// ■難易度の目安（プレイヤーHP 8000）
//   ・部隊はステージ側に 25〜35 体。ここで指定する哨戒兵・増援はそれに上乗せされる
//   ・増援は 30〜40 秒おき。全滅させると前倒しで来るので、手が止まる時間がない
//   ・制限時間は「寄り道せず、交戦も最小限で進んで 6〜7 割」を目安にしている
//------------------------------------------------------------------------------
const MissionDef k_MissionDefs[MISSION_COUNT] =
{
    // code  title  client  area  objective
    // briefing
    // reward  rank  enemyHpScale  enemySight  legacy
    // phaseCount
    //   { stage, objective, enemyCount, enemyMix, timeLimit,
    //     { { time, count, mix } x4 }, bossType, noSquads }
    // floorCount  enemySpawnRate  enemyMix  hasBoss   （デフォルト用。ブロックステージでは未使用）

    /* MISSION_RECON */
    {
        L"MISSION 01", L"市街強行偵察", L"管理局 第七調査班", L"NEON FRONTIER 旧軌道都市", L"制限時間内に北端のヘリパッドへ到達",
        L"放棄された軌道都市を、所属不明の部隊が要塞化している。\n"
        L"交差点ごとに防衛線。大通りの両脇には狙撃型が張り付いている。\n"
        L"\n"
        L"3分以内に市街を突破し、北端のヘリパッドへ到達せよ。\n"
        L"時間をかければ外周から増援が降りてくる。屋上を使って足を止めるな。",
        15000, 2, 1.0f, 1.8f, false,
        1,
        {
            { BlockStageID::City, MissionObjective::ReachGoal, 4, EnemyMix::Balanced, 180.0f,
              { { 40.0f, 6, EnemyMix::Swarm }, { 85.0f, 6, EnemyMix::Balanced }, { 130.0f, 8, EnemyMix::Sniper } } },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_INTERCEPT */
    {
        L"MISSION 02", L"高機動部隊迎撃", L"企業連合 保安部", L"ORBITAL DOCK 第五貨物区", L"敵部隊の全滅および大型兵器の撃破",
        L"貨物ドックに高機動型の無人機が多数侵入した。\n"
        L"通路の両端から挟み込んでくる。足を止めた瞬間に囲まれるぞ。\n"
        L"\n"
        L"敵は三波に分けて増援を送り込んでくる。4分以内にすべて叩け。\n"
        L"コンテナの上を取れ。高機動型は上がってこられない。\n"
        L"全滅を確認したら、指揮機『ARGUS』が降下してくる。浮遊する眼だ。",
        30000, 3, 1.2f, 1.8f, false,
        2,
        {
            { BlockStageID::Terminal, MissionObjective::Annihilate, 4, EnemyMix::Swarm, 240.0f,
              { { 35.0f, 8, EnemyMix::Swarm }, { 75.0f, 8, EnemyMix::Swarm }, { 115.0f, 10, EnemyMix::Balanced } } },
            { BlockStageID::Terminal, MissionObjective::DestroyBoss, 0, EnemyMix::Swarm, 240.0f,
              { { 40.0f, 4, EnemyMix::Swarm }, { 90.0f, 6, EnemyMix::Balanced } }, BOSS_ARGUS, true },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_HEAVY */
    {
        L"MISSION 03", L"重装部隊排除", L"独立傭兵仲介所", L"FORTRESS GATE 外郭要塞", L"敵部隊の全滅および大型兵器の撃破",
        L"外郭の要塞を重装型の部隊が占拠している。\n"
        L"南門は重装型が塞ぎ、門をくぐればバリケードの裏から狙撃が来る。\n"
        L"\n"
        L"装甲は通常の1.35倍。半端な火力では押し切られる。\n"
        L"外壁を越えて背後を取るのも手だ。制限時間は5分。\n"
        L"制圧後、重装歩行機『GOLIATH』が南門から突入してくる。",
        50000, 4, 1.35f, 1.8f, false,
        2,
        {
            { BlockStageID::Fortress, MissionObjective::Annihilate, 4, EnemyMix::Heavy, 300.0f,
              { { 45.0f, 6, EnemyMix::Heavy }, { 100.0f, 8, EnemyMix::Sniper }, { 150.0f, 8, EnemyMix::Heavy } } },
            { BlockStageID::Fortress, MissionObjective::DestroyBoss, 0, EnemyMix::Heavy, 300.0f,
              { { 45.0f, 4, EnemyMix::Heavy }, { 100.0f, 6, EnemyMix::Sniper } }, BOSS_GOLIATH, true },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_BOSS */
    {
        L"MISSION 04", L"大型兵器撃破", L"管理局 作戦司令部", L"OMEGA CORE 中央制御区画", L"制限時間内に大型兵器を撃破",
        L"中央制御区画で、三つ首の大型兵器『HYDRA』の起動を確認した。\n"
        L"護衛の狙撃型と重装型が両脇を固めている。\n"
        L"\n"
        L"戦闘が長引けば区画の四方から増援が降りてくる。\n"
        L"柱を盾にしろ。5分以内に撃破できなければ作戦は失敗だ。",
        60000, 4, 1.35f, 2.0f, false,
        1,
        {
            { BlockStageID::Arena, MissionObjective::DestroyBoss, 0, EnemyMix::Balanced, 300.0f,
              { { 30.0f, 4, EnemyMix::Swarm }, { 60.0f, 4, EnemyMix::Sniper },
                { 90.0f, 6, EnemyMix::Balanced }, { 125.0f, 6, EnemyMix::Heavy } }, BOSS_HYDRA },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_ASSAULT */
    {
        L"MISSION 05", L"最深部強襲", L"依頼主不明", L"ASTEROID BELT 未登録区画", L"渓谷の突破および大型兵器の撃破",
        L"座標のみが送られてきた。送り主は名乗っていない。\n"
        L"渓谷の隔壁ごとに重装と狙撃の関門。南の入口からは追撃が来る。\n"
        L"\n"
        L"3分30秒で搬入口へ抜け、最深部の動力炉『OMEGA CORE』を5分以内に撃破せよ。\n"
        L"敵の装甲は通常の1.5倍。生きて戻れる保証はない。",
        100000, 5, 1.5f, 2.0f, false,
        2,
        {
            { BlockStageID::Trench, MissionObjective::ReachGoal, 4, EnemyMix::Sniper, 210.0f,
              { { 30.0f, 6, EnemyMix::Swarm }, { 75.0f, 6, EnemyMix::Swarm }, { 120.0f, 8, EnemyMix::Balanced } } },
            { BlockStageID::Arena, MissionObjective::DestroyBoss, 4, EnemyMix::Sniper, 300.0f,
              { { 25.0f, 6, EnemyMix::Swarm }, { 50.0f, 6, EnemyMix::Sniper },
                { 80.0f, 8, EnemyMix::Heavy }, { 110.0f, 8, EnemyMix::Balanced } }, BOSS_OMEGA },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_PLANT */
    {
        L"MISSION 06", L"二層プラント制圧", L"企業連合 開発局", L"REACTOR PLANT 第三反応炉", L"上層制御室の確保および大型兵器の撃破",
        L"反応炉プラントが武装勢力に占拠された。施設は地上階と上層デッキの二層構造。\n"
        L"デッキの上から狙撃型と砲撃型が撃ち下ろし、地上は自爆型と幻影型が走り回る。\n"
        L"\n"
        L"階段を押さえて上層へ上がり、北デッキの制御室を4分以内に確保せよ。\n"
        L"確保後、刃の騎士『SPECTRE』が現れる。瞬間移動に注意しろ。",
        120000, 5, 1.5f, 2.0f, false,
        2,
        {
            { BlockStageID::Plant, MissionObjective::ReachGoal, 4, EnemyMix::Balanced, 240.0f,
              { { 35.0f, 6, EnemyMix::Swarm }, { 80.0f, 6, EnemyMix::Sniper }, { 125.0f, 8, EnemyMix::Heavy } } },
            { BlockStageID::Plant, MissionObjective::DestroyBoss, 0, EnemyMix::Balanced, 300.0f,
              { { 40.0f, 4, EnemyMix::Swarm }, { 90.0f, 6, EnemyMix::Balanced }, { 140.0f, 6, EnemyMix::Sniper } }, BOSS_SPECTRE, true },
        },
        0, 0, EnemyMix::Balanced, false
    },
    /* MISSION_DEFAULT */
    {
        L"DEFAULT", L"デフォルト", L"訓練プログラム", L"自動生成ダンジョン", L"3階層の突破および大型兵器の撃破",
        L"従来のアドベンチャーモード。\n"
        L"入るたびに構造の変わる地下ダンジョンを3階層突破し、\n"
        L"最深部のボス部屋で大型兵器を撃破する。\n"
        L"\n"
        L"階層を進むごとに敵の数が増える。敵の装甲は通常の1.25倍。",
        60000, 3, 1.25f, 1.3f, true,
        0, {},
        3, 7, EnemyMix::Balanced, true
    },
};

//------------------------------------------------------------------------------
// 選択中ミッション / クリア済みフラグ
//------------------------------------------------------------------------------
static int  g_CurrentMission = MISSION_RECON;
static bool g_Cleared[MISSION_COUNT] = {};

int Mission_GetCurrent()
{
    return g_CurrentMission;
}

void Mission_SetCurrent(int index)
{
    if (index < 0 || index >= MISSION_COUNT) return;   // 不正値（壊れたセーブ等）は無視
    g_CurrentMission = index;
}

const MissionDef& Mission_GetCurrentDef()
{
    return k_MissionDefs[g_CurrentMission];
}

bool Mission_IsCleared(int index)
{
    if (index < 0 || index >= MISSION_COUNT) return false;
    return g_Cleared[index];
}

void Mission_SetCleared(int index, bool cleared)
{
    if (index < 0 || index >= MISSION_COUNT) return;
    g_Cleared[index] = cleared;
}

//------------------------------------------------------------------------------
// 表示用の集計
//------------------------------------------------------------------------------
bool Mission_HasBoss(const MissionDef& def)
{
    if (def.legacy) return def.hasBoss;

    for (int i = 0; i < def.phaseCount; ++i)
        if (def.phases[i].objective == MissionObjective::DestroyBoss) return true;
    return false;
}

int Mission_GetStageCount(const MissionDef& def)
{
    if (def.legacy) return def.floorCount + (def.hasBoss ? 1 : 0);
    return def.phaseCount;
}

EnemyMix Mission_GetMainEnemyMix(const MissionDef& def)
{
    return def.legacy ? def.enemyMix : def.phases[0].enemyMix;
}

float Mission_GetTimeLimit(const MissionDef& def)
{
    return def.legacy ? 0.0f : def.phases[0].timeLimit;
}

int Mission_GetReinforceCount(const MissionDef& def)
{
    if (def.legacy) return 0;

    int total = 0;
    for (int p = 0; p < def.phaseCount; ++p)
        for (const ReinforceWave& w : def.phases[p].waves)
            if (w.time > 0.0f) total += w.count;
    return total;
}
