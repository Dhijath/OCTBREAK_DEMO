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
    constexpr int BOSS_BASTION = static_cast<int>(EnemyType::BossBastion);
    constexpr int BOSS_NEST    = static_cast<int>(EnemyType::BossNest);
    constexpr int BOSS_ECLIPSE = static_cast<int>(EnemyType::BossEclipse);
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

    //==========================================================================
    // 第二作戦区域（2ページ目）
    //   敵の装甲は 1.4〜2.0 倍、増援は4波まで。1ページ目を終えたプレイヤー向けの続編
    //==========================================================================

    /* MISSION_SPACEPORT */
    {
        L"MISSION 07", L"宇宙港夜間降下", L"管理局 第七調査班", L"ORBITAL SPACEPORT 第二発着場", L"制限時間内に北端の打ち上げ台へ到達",
        L"夜間の宇宙港に降下する。武装勢力が発着場ごと占拠した。\n"
        L"北端の打ち上げ台に、脱出用のシャトルを待機させてある。\n"
        L"\n"
        L"誘導路の両脇はパッドと格納庫。陰から挟み込まれるぞ。\n"
        L"3分30秒以内に打ち上げ台へ上がれ。増援は四波来る。",
        70000, 3, 1.4f, 1.9f, false,
        1,
        {
            { BlockStageID::Spaceport, MissionObjective::ReachGoal, 6, EnemyMix::Balanced, 210.0f,
              { { 35.0f, 6, EnemyMix::Swarm }, { 80.0f, 8, EnemyMix::Balanced },
                { 125.0f, 8, EnemyMix::Sniper }, { 165.0f, 8, EnemyMix::Heavy } } },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_VAULT */
    {
        L"MISSION 08", L"電子金庫侵入", L"企業連合 情報部", L"DATA VAULT 第零記憶区画", L"記憶区画の敵部隊を全滅",
        L"企業連合の電子金庫が乗っ取られた。区画は天井のある屋内だ。\n"
        L"サーバーラックの列が迷路になっている。角を曲がれば至近距離だ。\n"
        L"\n"
        L"交差点は固定砲台が押さえ、通路を自爆型と幻影型が走る。\n"
        L"4分30秒以内に区画内の敵をすべて排除せよ。",
        85000, 4, 1.5f, 1.6f, false,
        1,
        {
            { BlockStageID::DataVault, MissionObjective::Annihilate, 6, EnemyMix::Swarm, 270.0f,
              { { 30.0f, 6, EnemyMix::Swarm }, { 70.0f, 8, EnemyMix::Balanced },
                { 115.0f, 8, EnemyMix::Swarm }, { 160.0f, 8, EnemyMix::Heavy } } },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_CRYO */
    {
        L"MISSION 09", L"氷原採掘場奪還", L"独立傭兵仲介所", L"CRYO MINE 第四採掘場", L"敵部隊の全滅および大型兵器の撃破",
        L"氷原の採掘場を奪還する。中央の掘削リグに狙撃型が陣取っている。\n"
        L"地上は脚付きと重装型。岩陰を伝ってリグの階段へ取り付け。\n"
        L"\n"
        L"制圧すると、要塞機『BASTION』が北の広場に降りてくる。\n"
        L"両腕のガトリングが回り始めたら掃射の合図だ。横へ抜けろ。",
        130000, 4, 1.6f, 1.9f, false,
        2,
        {
            { BlockStageID::CryoMine, MissionObjective::Annihilate, 6, EnemyMix::Heavy, 300.0f,
              { { 40.0f, 6, EnemyMix::Heavy }, { 90.0f, 8, EnemyMix::Sniper }, { 140.0f, 8, EnemyMix::Balanced } } },
            { BlockStageID::CryoMine, MissionObjective::DestroyBoss, 0, EnemyMix::Heavy, 300.0f,
              { { 35.0f, 4, EnemyMix::Heavy }, { 80.0f, 6, EnemyMix::Swarm }, { 130.0f, 6, EnemyMix::Sniper } }, BOSS_BASTION, true },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_TWOFRONT */
    {
        L"MISSION 10", L"二正面作戦", L"管理局 作戦司令部", L"ORBITAL DOCK ／ NEON FRONTIER", L"貨物区の制圧および市街の突破",
        L"貨物区と市街の二か所で同時に敵が動いた。片付けてから向かう余裕はない。\n"
        L"まず貨物区の高機動部隊を4分以内に全滅させ、\n"
        L"その足で市街へ降下し、3分以内に北端のヘリパッドへ抜けろ。\n"
        L"\n"
        L"どちらも一度見た戦場だが、装甲は1.7倍。増援の数も前回の比ではない。",
        140000, 5, 1.7f, 2.0f, false,
        2,
        {
            { BlockStageID::Terminal, MissionObjective::Annihilate, 6, EnemyMix::Swarm, 240.0f,
              { { 30.0f, 8, EnemyMix::Swarm }, { 70.0f, 8, EnemyMix::Balanced }, { 110.0f, 10, EnemyMix::Swarm } } },
            { BlockStageID::City, MissionObjective::ReachGoal, 6, EnemyMix::Sniper, 180.0f,
              { { 30.0f, 8, EnemyMix::Balanced }, { 70.0f, 8, EnemyMix::Sniper },
                { 110.0f, 10, EnemyMix::Swarm }, { 150.0f, 10, EnemyMix::Heavy } } },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_CARRIER */
    {
        L"MISSION 11", L"艦上決戦", L"企業連合 保安部", L"CARRIER DECK 強襲空母の飛行甲板", L"艦首への到達および大型兵器の撃破",
        L"敵の強襲空母に取り付いた。飛行甲板を南から北の艦首まで突っ切れ。\n"
        L"甲板の上は翼型が飛び交い、艦橋の上からは狙撃が来る。\n"
        L"\n"
        L"艦首に着いたら、母艦機『NEST』が甲板の上に現れる。\n"
        L"火花が散った地点は爆撃の着弾点だ。見えたらすぐに離れろ。",
        160000, 5, 1.8f, 2.0f, false,
        2,
        {
            { BlockStageID::CarrierDeck, MissionObjective::ReachGoal, 6, EnemyMix::Swarm, 210.0f,
              { { 30.0f, 6, EnemyMix::Swarm }, { 75.0f, 8, EnemyMix::Swarm }, { 120.0f, 8, EnemyMix::Balanced } } },
            { BlockStageID::CarrierDeck, MissionObjective::DestroyBoss, 0, EnemyMix::Swarm, 300.0f,
              { { 40.0f, 4, EnemyMix::Swarm }, { 90.0f, 6, EnemyMix::Balanced }, { 140.0f, 6, EnemyMix::Swarm } }, BOSS_NEST, true },
        },
        0, 0, EnemyMix::Balanced, false
    },

    /* MISSION_HAUNT */
    {
        L"MISSION 12", L"亡霊の巣", L"依頼主不明", L"REACTOR PLANT 第三反応炉（再汚染区画）", L"敵部隊の全滅および強化型大型兵器の撃破",
        L"一度制圧した反応炉プラントで、刃の騎士の反応が再び確認された。\n"
        L"今度の個体は装甲が厚い。前回の1.6倍の耐久を見込め。\n"
        L"\n"
        L"まず地上とデッキの部隊を5分以内に全滅させろ。\n"
        L"騎士は瞬間移動で背後に回る。足を止めずに撃ち続けろ。",
        180000, 5, 1.9f, 2.0f, false,
        2,
        {
            { BlockStageID::Plant, MissionObjective::Annihilate, 6, EnemyMix::Balanced, 300.0f,
              { { 35.0f, 8, EnemyMix::Swarm }, { 80.0f, 8, EnemyMix::Sniper },
                { 125.0f, 10, EnemyMix::Heavy }, { 170.0f, 8, EnemyMix::Swarm } } },
            { BlockStageID::Plant, MissionObjective::DestroyBoss, 0, EnemyMix::Balanced, 300.0f,
              { { 30.0f, 6, EnemyMix::Swarm }, { 70.0f, 6, EnemyMix::Balanced }, { 120.0f, 8, EnemyMix::Sniper } }, BOSS_SPECTRE, true },
        },
        0, 0, EnemyMix::Balanced, false,
        1.6f
    },

    /* MISSION_ECLIPSE */
    {
        L"MISSION 13", L"最終作戦《日蝕》", L"管理局 最高評議会", L"DATA VAULT ／ OMEGA CORE 最深部", L"金庫最深部の突破および最終兵器の撃破",
        L"すべての襲撃は、電子金庫の最深部に眠る最終兵器を起こすためだった。\n"
        L"4分以内に金庫を突破し、その奥の制御区画へ降りろ。\n"
        L"\n"
        L"最終兵器『ECLIPSE』は、光りながら機体を引き寄せて炸裂する。\n"
        L"引力は走れば振り切れる。これが最後の作戦だ。必ず戻ってこい。",
        250000, 5, 2.0f, 2.2f, false,
        2,
        {
            { BlockStageID::DataVault, MissionObjective::ReachGoal, 6, EnemyMix::Balanced, 240.0f,
              { { 30.0f, 8, EnemyMix::Swarm }, { 70.0f, 8, EnemyMix::Balanced },
                { 110.0f, 10, EnemyMix::Heavy }, { 150.0f, 10, EnemyMix::Swarm } } },
            { BlockStageID::Arena, MissionObjective::DestroyBoss, 4, EnemyMix::Balanced, 360.0f,
              { { 30.0f, 6, EnemyMix::Swarm }, { 60.0f, 6, EnemyMix::Sniper },
                { 95.0f, 8, EnemyMix::Heavy }, { 135.0f, 8, EnemyMix::Balanced } }, BOSS_ECLIPSE },
        },
        0, 0, EnemyMix::Balanced, false
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
