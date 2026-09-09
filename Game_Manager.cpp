/*==============================================================================

   ゲーム管理 [Game_Manager.cpp]
   Author : 51106
   Date   : 2026/04/01

--------------------------------------------------------------------------------
   状態: タイトル / プレイ / オプション / リザルト / クリア / 終了
   目的:
     - フェードと状態遷移を一元管理
     - BGMは遷移時に差し替え（旧をUnload→新をLoad&Loop）
     - 　ゴールを2回踏む→ボス部屋へ→ボス撃破でクリア
==============================================================================*/

#include "Game_Manager.h"
#include "Title.h"
#include "Option.h"
#include "game.h"
#include "player.h"
#include "Score.h"
#include "fade.h"
#include "Audio.h"
#include "key_logger.h"
#include "bullet.h"
#include "EnemyBullet.h"
#include "bullet_hit_effect.h"
#include "particle_spark.h"
#include "effect.h"
#include "Clear.h"
#include "Result.h"
#include "AssemblyScreen.h"
#include "PreGame.h"
#include "EnemyDex.h"
#include "EnemyParts.h"
#include "Enemy.h"
#include "map.h"
#include <cstdint>
#include <algorithm>
#include "pad_logger.h"
#include "Pause.h"
#include "BossIntro.h"
#include "StageSelect.h"
#include "MissionSelect.h"
#include "MissionDef.h"
#include "MissionHud.h"
#include "MissionReport.h"
#include "BlockStage.h"
#include "SciFiUI.h"
#include "EnemyAI.h"
#include "Tutorial.h"
#include "WaveManager.h"
#include "WeaponDef.h"
#include "Shop.h"
#include "BossDefeat.h"
#include "ItemManager.h"
#include "player_camera.h"
#include "HUD.h"
#include "input_hint.h"
#include "UIInput.h"
#include "SaveData.h"
#include "Score.h"
#include <DirectXMath.h>
#include <cstdlib>
#include <cmath>
#include <cwchar>
using namespace DirectX;

// 現在/次の状態
static GameState g_GameState = GameState::Title;
static GameState g_NextState = GameState::Title;

// フェード中に多重遷移を防ぐフラグ（フェードアウト開始〜完了まで true）
static bool g_IsTransitioning = false;

// 現在鳴っているBGMのハンドル（Unload 用）
static int g_CurrentBgmId = -1;

// ダンジョン生成用シード
static std::uint32_t g_DungeonSeed = 12345u;

// ゴール判定のクールダウン（秒）
static double g_GoalCooldown = 0.0;
// 現在ルームの経過時間（タイムボーナス計算用）
static double g_RoomTimer = 0.0;
// ゴール到達によるダンジョン再生成待ちフラグ
static bool g_PendingDungeonRegenerate = false;
// ボス部屋フェーズ中フラグ（2回ゴール到達後に true になる）
static bool g_InBossRoom = false;
// ブロックステージのミッションで進行中のフェーズ（MissionDef::phases の添字）
static int g_PhaseIndex = 0;
// フェーズ開始からの経過時間（制限時間・増援の時刻に使う。ボス演出中は止める）
static double g_PhaseTimer = 0.0;
// 次に出す増援（MissionPhase::waves の添字）
static int g_NextWave = 0;
// 増援警告の残り表示時間と、その増援の数
static double g_WarningTimer = 0.0;
static int    g_WarningCount = 0;
// 増援の通し番号（出現位置・種別のばらつきに使う）
static int g_ReinforceSerial = 0;
// 時間切れで作戦失敗した（Result へのフェード中に「MISSION FAILED」を出す）
static bool g_MissionFailed = false;
// ポーズ中フラグ（フェードなし即時停止）
static bool g_IsPaused = false;
// 作戦開始からの経過時間（ポーズ中は止める。結果画面の集計用）
static double g_MissionTime = 0.0;
// 直近の作戦結果（クリア / リザルト画面が表示する）
static MissionReport g_Report;
// ゲーム（Playing/Survival）を初期化済みか。
// これが true の状態で再度ゲームを開始する前に Game_Finalize() を呼び、
// 前回ゲームの SE などのリソースを解放する（SEスロット枯渇で音が消えるバグ対策）。
static bool g_GameInitialized = false;

// アセンブリ（WeaponSelect）をキャンセルした時の戻り先。
// PreGame の「ASSEMBLY」から入った時は PreGame、アドベンチャー選択から入った時は StageSelect。
static GameState g_WeaponSelectBack = GameState::PreGame;


// PlayerDeath 演出
static double g_DeathTimer       = 0.0;  // 死亡後の経過時間（秒）
static double g_DeathExplodeNext = 0.0;  // 次の爆発スポーンまでの残時間

// BGMパス（必要に応じて差し替え）
static const char* BGM_TITLE      = "resource/sound/newspaper.wav";
static const char* BGM_GAME       = "resource/sound/loop_wav/Loop_08_RageoftheMachines.wav";           // ステージ（音量2倍）
static const char* BGM_BOSS       = "resource/sound/Experimenta_Model_short.wav";                // ボス
static const char* BGM_ASSEMBLY   = "resource/sound/loop_wav/Loop_07_DynamicVoltage.wav";
static const char* BGM_SCOREBOARD = "resource/sound/loop_wav/Loop_06_StealthStalker.wav";
static const char* BGM_OPTION     = "resource/sound/maou_bgm_cyber40.wav";
static const char* BGM_RESULT     = "resource/sound/maou_bgm_cyber45_1.wav"; // リザルト/クリア兼用
static constexpr float BGM_VOL         = 0.35f;
static constexpr float BGM_VOL_GAME    = 0.70f; // 音小さめなので2倍に

int g_PlayerWarpSE = -1;
int g_PlayerclearSE = -1;


//------------------------------------------------------------------------------
// 内部: BGMを差し替えてループ再生（前のBGMはUnloadして重なり回避）
//------------------------------------------------------------------------------
static void StartBgmLoop(const char* path, float volume = 0.35f)
{
    if (g_CurrentBgmId >= 0)
    {
        UnloadAudio(g_CurrentBgmId);
        g_CurrentBgmId = -1;
    }

    if (path && path[0] != '\0')
    {
        g_CurrentBgmId = LoadAudioWithVolume(path, volume);
        if (g_CurrentBgmId >= 0)
        {
            PlayAudio(g_CurrentBgmId, /*Loop=*/true);
        }
    }
}


//------------------------------------------------------------------------------
// 内部: 遷移開始（フェードアウト + 次状態予約 + 以降の判定を停止）
//------------------------------------------------------------------------------
// フェードなし即切り替え（同一背景・BGM の状態間に使用）
static void SwitchInstant(GameState next)
{
    g_GameState = next;
    if      (next == GameState::PreGame)      PreGame_Initialize();
    else if (next == GameState::Title)        Title_Initialize();
    else if (next == GameState::StageSelect)  StageSelect_Initialize();
    else if (next == GameState::MissionSelect) MissionSelect_Initialize();
}

static void BeginTransition(GameState next, const char* nextBgmPath, float bgmVolume = BGM_VOL)
{
    if (g_IsTransitioning) return;                 // 多重開始を防止
    g_IsTransitioning = true;
    g_NextState = next;
    Fade_Start(1.0, /*out=*/true, { 1,1,1 });     // 白フェードアウト開始
    StartBgmLoop(nextBgmPath, bgmVolume);          // 次シーンBGMへ差し替え
    Player_OnPause();                              // ループSE（ブースト等）を停止
}

//------------------------------------------------------------------------------
// 内部: 評価ランク（S〜D。失敗は E）
//   成功で 40 点、残り時間で最大 30 点、被ダメージの少なさで最大 30 点
//------------------------------------------------------------------------------
static wchar_t CalcRank(bool success, bool survival, float time, float timeLimit, int damageTaken)
{
    if (!success) return L'E';

    const float damageScore = std::clamp(1.0f - damageTaken / 12000.0f, 0.0f, 1.0f);
    float pts = 40.0f;
    if (survival)
    {
        pts += 60.0f * damageScore;
    }
    else
    {
        // 制限時間の 1/3 で抜ければ満点。制限のない作戦は 15 分を基準にする
        const float timeScore = (timeLimit > 0.0f)
            ? std::clamp(1.5f * (1.0f - time / timeLimit), 0.0f, 1.0f)
            : std::clamp(1.0f - time / 900.0f, 0.0f, 1.0f);
        pts += 30.0f * timeScore + 30.0f * damageScore;
    }

    if (pts >= 85.0f) return L'S';
    if (pts >= 72.0f) return L'A';
    if (pts >= 58.0f) return L'B';
    if (pts >= 45.0f) return L'C';
    return L'D';
}

//------------------------------------------------------------------------------
// 内部: 作戦結果をまとめる（クリア / リザルト画面へ移る直前に呼ぶ）
//------------------------------------------------------------------------------
static void BuildReport(bool success, bool survival, const wchar_t* failReason)
{
    MissionReport r;
    r.survival    = survival;
    r.success     = success;
    r.failReason  = success ? L"" : failReason;
    r.time        = static_cast<float>(g_MissionTime);
    r.kills       = Game_GetKillCount();
    r.damageDealt = Score_GetDamageDealt();
    r.damageTaken = Score_GetDamageTaken();
    r.score       = static_cast<int>(Score_GetScore());

    float timeLimit = 0.0f;
    if (survival)
    {
        r.code  = L"SURVIVAL";
        r.title = L"殲滅戦";
        r.area  = L"OUTDOOR ARENA";
    }
    else
    {
        const MissionDef& def = Mission_GetCurrentDef();
        r.code   = def.code;
        r.title  = def.title;
        r.area   = def.area;
        r.reward = success ? def.reward : 0;
        r.phaseCount = Mission_GetStageCount(def);
        if (def.legacy)
            r.phaseReached = success ? r.phaseCount
                                     : std::min(r.phaseCount, Map_GetGoalReachCount() + 1 + (g_InBossRoom ? 1 : 0));
        else
            r.phaseReached = g_PhaseIndex + 1;

        if (!def.legacy)
            for (int i = 0; i < def.phaseCount; ++i) timeLimit += def.phases[i].timeLimit;
    }

    r.rank   = CalcRank(success, survival, r.time, timeLimit, r.damageTaken);
    g_Report = r;

    SaveData_SaveDex();   // この作戦の撃破をエネミー図鑑に残す
}

const MissionReport& MissionReport_Get()
{
    return g_Report;
}

//------------------------------------------------------------------------------
// 内部: プレイ中の進行状態を初期状態へ戻す（新しいゲームの開始時・選択画面へ戻るとき）
//------------------------------------------------------------------------------
static void ResetPlayState()
{
    Map_ResetGoalReachCount();
    g_InBossRoom = false;
    g_PendingDungeonRegenerate = false;
    Game_SetBossRoomMode(false);
    Game_SetSurvivalMode(false);
    Game_SetBossType(0);
}

//------------------------------------------------------------------------------
// 内部: ミッション成功（報酬加算 → クリア済み保存 → スコア記録 → クリア画面へ）
//------------------------------------------------------------------------------
static void CompleteMission()
{
    PlayAudio(g_PlayerclearSE);
    Score_Addscore(Mission_GetCurrentDef().reward);   // 成功報酬
    Mission_SetCleared(Mission_GetCurrent(), true);
    SaveData_SaveMissions();
    Score_AddRecord(Score_GetScore(),
        AssemblyScreen_GetRightWeapon(),
        AssemblyScreen_GetLeftWeapon());
    SaveData_SaveScores();
    BuildReport(true, false, L"");
    BeginTransition(GameState::Clear, BGM_RESULT);
}

//------------------------------------------------------------------------------
// 内部: マップを差し替えた後の共通処理
//   床の再登録 → プレイヤーをスポーン位置へ → 敵の再配置 → 残弾・エフェクトの掃除
//------------------------------------------------------------------------------
static void PlacePlayerAndResetField()
{
    Map_RegisterFloors();
    Player_SetPosition(Map_GetSpawnPosition(), true);
    Player_SetFront({ 0.0f, 0.0f, 1.0f });
    Game_RespawnEnemies();
    Bullet_ClearAll();             // ルーム遷移時に残弾・エフェクト・パーティクルをクリア
    EnemyBullet_ClearAll();
    BulletHitEffect_ClearAll();
    SparkEffect_ClearAll();
    Effect_ClearAll();
    Player_ClearParticles();
    ItemManager_ClearAll();        // ドロップアイテムをクリア
    Player_Camera_Update(0.0);     // 新スポーン位置にカメラを即更新（BossIntro の g_PreIntroEye を正しく取るため）
}

//------------------------------------------------------------------------------
// 内部: ブロックステージのミッションで、指定フェーズのステージを読み込む
//   ボス戦フェーズならボスを出現させ、登場演出を開始する
//------------------------------------------------------------------------------
static void LoadMissionPhase(int phase)
{
    const MissionPhase& ph = Mission_GetCurrentDef().phases[phase];
    const bool isBoss = (ph.objective == MissionObjective::DestroyBoss);

    g_PhaseIndex = phase;
    g_InBossRoom = isBoss;
    Game_SetBossRoomMode(isBoss);       // Game_RespawnEnemies がボスを出すかどうか
    Game_SetEnemyMix(ph.enemyMix);
    Game_SetBossType(ph.bossType);      // 0 = 従来のボス

    BlockStage_Build(ph.stage, Map_GenerateRandomSeed(), ph.enemyCount, !ph.noSquads);
    PlacePlayerAndResetField();

    g_GoalCooldown = 1.0;               // 読み込み直後の誤判定を防ぐ
    g_RoomTimer    = 0.0;

    g_PhaseTimer   = 0.0;
    g_NextWave     = 0;
    g_WarningTimer = 0.0;

    if (isBoss)
        BossIntro_Start(Map_GetBossSpawnPosition());
}

//------------------------------------------------------------------------------
// 内部: このフェーズにまだ出していない増援が残っているか
//------------------------------------------------------------------------------
static bool HasPendingWave(const MissionPhase& ph)
{
    return g_NextWave < MISSION_WAVE_MAX && ph.waves[g_NextWave].time > 0.0f;
}

//------------------------------------------------------------------------------
// 内部: 増援を降下させる（プレイヤーから離れた降下地点のまわりに出現）
//------------------------------------------------------------------------------
static void SpawnReinforcements(const ReinforceWave& wave)
{
    const XMFLOAT3 playerPos = Player_GetPosition();
    const std::uint32_t random = static_cast<std::uint32_t>(rand()) * 2654435761u
                               + static_cast<std::uint32_t>(g_ReinforceSerial) * 977u;

    int spawned = 0;
    for (int i = 0; i < wave.count; ++i)
    {
        XMFLOAT3 pos;
        if (!BlockStage_PickReinforcePoint(playerPos, random, i, &pos)) break;

        Game_SpawnEnemy(pos, Game_GetEnemyTypeForMix(wave.mix, g_ReinforceSerial + i));
        SparkEffect_Create({ pos.x, pos.y + 0.8f, pos.z }, 1.5f);   // 降下の閃光
        ++spawned;
    }

    g_ReinforceSerial += wave.count;
    g_WarningTimer = 3.5;
    g_WarningCount = spawned;
}

//------------------------------------------------------------------------------
// 内部: 作戦失敗（時間切れ）。スコアを記録して Result へ
//------------------------------------------------------------------------------
static void FailMission()
{
    g_MissionFailed = true;
    Player_OnPause();
    Score_AddRecord(Score_GetScore(),
        AssemblyScreen_GetRightWeapon(),
        AssemblyScreen_GetLeftWeapon());
    SaveData_SaveScores();
    BuildReport(false, false, L"TIME OVER ― 作戦時間を超過");
    BeginTransition(GameState::Result, BGM_RESULT);
}

//------------------------------------------------------------------------------
// 内部: 難易度の倍率を既定値に戻す（デフォルト以外のモードへ入るとき）
//------------------------------------------------------------------------------
static void ResetDifficulty()
{
    Game_SetEnemyHpScale(1.0f);
    EnemyAI_SetSightMultiplier(1.0f);
}

//------------------------------------------------------------------------------
// 内部: プレイ中に表示する作戦情報（ブロックステージのミッションのみ）
//------------------------------------------------------------------------------
static void DrawMissionObjective()
{
    const MissionDef& mission = Mission_GetCurrentDef();
    if (mission.legacy) return;
    if (!g_MissionFailed && (BossIntro_IsPlaying() || BossDefeat_IsPlaying())) return;

    const MissionPhase& ph = mission.phases[g_PhaseIndex];

    MissionHudState s;
    s.phase          = g_PhaseIndex + 1;
    s.phaseCount     = mission.phaseCount;
    s.timeLeft       = (ph.timeLimit > 0.0f) ? static_cast<float>(ph.timeLimit - g_PhaseTimer) : -1.0f;
    s.warning        = static_cast<float>(g_WarningTimer);
    s.reinforceCount = g_WarningCount;
    s.bossPhase      = (ph.objective == MissionObjective::DestroyBoss);
    s.failed         = g_MissionFailed;

    wchar_t detail[64] = L"";
    switch (ph.objective)
    {
    case MissionObjective::ReachGoal:
    {
        const XMFLOAT3 p = Player_GetPosition();
        const XMFLOAT3 g = Map_GetGoalPosition();
        const float dist = sqrtf((g.x - p.x) * (g.x - p.x) + (g.z - p.z) * (g.z - p.z));
        s.objective = L"目標地点へ到達せよ";
        swprintf_s(detail, L"DIST %4dm", static_cast<int>(dist));
        break;
    }
    case MissionObjective::Annihilate:
        s.objective = L"敵部隊を全滅させろ";
        swprintf_s(detail, HasPendingWave(ph) ? L"HOSTILE %2d +REINF" : L"HOSTILE %2d",
                   Game_GetAliveEnemyCount());
        break;

    case MissionObjective::DestroyBoss:
        s.objective = L"大型兵器を撃破せよ";
        swprintf_s(detail, L"HOSTILE %2d", Game_GetAliveEnemyCount());
        break;
    }
    s.detail = detail;

    MissionHud_Draw(s);
}

//------------------------------------------------------------------------------
// 初期化
//------------------------------------------------------------------------------
void GameManager_Initialize()
{
    Game_InitializeD3D();             // D3Dリソースをアプリ起動時に1回だけ初期化
    SaveData_Load();                  // 設定ファイルを読み込み各モジュールに反映
    Title_Initialize();               // まずはタイトル
    StartBgmLoop(BGM_TITLE);          // タイトルBGM開始
    g_IsTransitioning = false;        // 遷移中フラグOFF
    g_GoalCooldown = 0.0;             // クールダウンリセット
    Fade_Start(1.0, /*isFadeOut=*/false, { 1,1,1 }); // 起動フェードイン
    g_PlayerWarpSE = LoadAudioWithVolume("resource/sound/warp.wav", 0.5f);
    g_PlayerclearSE = LoadAudioWithVolume("resource/sound/clear.wav", 0.5f);
    g_IsPaused = false;
    Pause_Initialize();
    MissionHud_Initialize();
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void GameManager_Finalize()
{
    Title_Finalize();
    PreGame_Finalize();
    AssemblyScreen_Finalize();
    MissionSelect_Finalize();
    MissionHud_Finalize();
    BlockStage_Finalize();
    SciFiUI::Finalize();
    EnemyDex_Finalize();
    EnemyParts_Release();   // 図鑑・エネミーが共有するパーツモデル
    Tutorial_Finalize();
    Option_Finalize();
    if (g_GameInitialized) { Game_Finalize(); g_GameInitialized = false; }
    Game_FinalizeD3D();             // D3Dリソースをアプリ終了時に解放
    Clear_Finalize();
    Result_Finalize();
    UnloadAudio(g_PlayerWarpSE);

    if (g_CurrentBgmId >= 0)
    {
        UnloadAudio(g_CurrentBgmId);
        g_CurrentBgmId = -1;
    }

    Enemy_UnloadSE();
}

//------------------------------------------------------------------------------
// 更新
//------------------------------------------------------------------------------
//==============================================================================
// 更新
//
// ■役割
// ・現在の状態に応じた更新処理を呼び出す
// ・フェードアウト完了後に状態遷移・初期化を実行する
// ・ゴール到達によるダンジョン再生成もフェード完了後に処理する
//
// ■引数
// ・elapsed_time : 経過時間（秒）
//==============================================================================
void GameManager_Update(double elapsed_time)
{
    UIInput_Update();   // マウストリガー等を毎フレーム先頭で更新

    switch (g_GameState)
    {
    case GameState::Title:
    {
        Title_Update(elapsed_time);

        if (!g_IsTransitioning)
        {
            TitleResult tr = Title_GetResult();
            if (tr == TitleResult::Start)
            {
                SwitchInstant(GameState::PreGame);
            }
            else if (tr == TitleResult::Option)
            {
                BeginTransition(GameState::Option, BGM_OPTION);
            }
            else if (tr == TitleResult::Exit)
            {
                g_GameState = GameState::Exit;
            }

        }
        break;
    }

    case GameState::StageSelect:
    {
        if (!g_IsTransitioning)
        {
            StageSelect_Update(elapsed_time);
            const StageSelectResult r = StageSelect_GetResult();
            if (r == StageSelectResult::Adventure)
            {
                // アドベンチャーはアセンブリ（武器選択）を経由してから開始する
                StageSelect_Finalize();
                g_WeaponSelectBack = GameState::StageSelect;  // キャンセルでステージ選択へ戻る
                BeginTransition(GameState::WeaponSelect, BGM_ASSEMBLY);
            }
            else if (r == StageSelectResult::Survival)
            {
                StageSelect_Finalize();
                BeginTransition(GameState::Survival, BGM_BOSS, BGM_VOL);
            }
            else if (r == StageSelectResult::Back)
            {
                StageSelect_Finalize();
                SwitchInstant(GameState::PreGame);
            }
        }
        break;
    }

    case GameState::PreGame:
    {
        if (!g_IsTransitioning)
        {
            PreGame_Update(elapsed_time);
            PreGameResult pr = PreGame_GetResult();
            if (pr == PreGameResult::QuickStart)
            {
                SwitchInstant(GameState::StageSelect);
            }
            else if (pr == PreGameResult::Tutorial)
                BeginTransition(GameState::Tutorial, BGM_TITLE);
            else if (pr == PreGameResult::EnemyDex)
                BeginTransition(GameState::EnemyDex, BGM_SCOREBOARD);
            else if (pr == PreGameResult::Back)
                SwitchInstant(GameState::Title);  // 同一背景・BGMなのでフェードなし
        }
        break;
    }

    case GameState::WeaponSelect:
    {
        if (!g_IsTransitioning)
        {
            if (AssemblyScreen_Update(elapsed_time))
            {
                if (AssemblyScreen_WasCancelled())
                    BeginTransition(g_WeaponSelectBack, BGM_TITLE);  // 入ってきた画面へ戻る
                else
                {
                    SaveData_Save();   // 確定したアセンブリを保存
                    // 続けて出撃ミッションを選ぶ（BGMはアセンブリのまま継続するのでフェードなし）
                    SwitchInstant(GameState::MissionSelect);
                }
            }
        }
        break;
    }

    case GameState::MissionSelect:
    {
        if (!g_IsTransitioning)
        {
            MissionSelect_Update(elapsed_time);
            const MissionSelectResult mr = MissionSelect_GetResult();
            if (mr == MissionSelectResult::Sortie)
            {
                SaveData_SaveMissions();   // 選んだミッションを次回の初期カーソルとして保存

                // 最初のフェーズがボス戦のミッションは、最初からボス戦BGMで始める
                const MissionDef& mission = Mission_GetCurrentDef();
                const bool bossFirst = !mission.legacy
                    && mission.phases[0].objective == MissionObjective::DestroyBoss;
                if (bossFirst) BeginTransition(GameState::Playing, BGM_BOSS, BGM_VOL);
                else           BeginTransition(GameState::Playing, BGM_GAME, BGM_VOL_GAME);
            }
            else if (mr == MissionSelectResult::Back)
            {
                // アセンブリへ戻る。AssemblyScreen は確定時の状態（READY にフォーカス）を
                // 保持しているので、Initialize し直さずそのまま再開する。
                g_GameState = GameState::WeaponSelect;
            }
        }
        break;
    }

    case GameState::EnemyDex:
    {
        if (!g_IsTransitioning)
        {
            EnemyDex_Update(elapsed_time);
            if (EnemyDex_IsEnd())
                BeginTransition(GameState::PreGame, BGM_TITLE);
        }
        break;
    }

    case GameState::Tutorial:
    {
        if (!g_IsTransitioning)
        {
            Tutorial_Update(elapsed_time);
            if (Tutorial_IsEnd())
            {
                Tutorial_Finalize();
                SwitchInstant(GameState::PreGame);  // 同一背景・BGMなのでフェードなし
            }
        }
        break;
    }

    case GameState::Playing:
    {
        MissionHud_Update(elapsed_time);   // 作戦表示の点滅など（暗転中も動かす）
        if (g_IsTransitioning) break;

        //----------------------------------------------------------
        // ポーズトグル（ESC / PAD_START）
        // ボス演出中・遷移中はポーズ不可
        //----------------------------------------------------------
        if (!BossIntro_IsPlaying() && !BossDefeat_IsPlaying() && !g_IsPaused)
        {
            const bool pauseKey = KeyLogger_IsTrigger(KK_ESCAPE)
                                || PadLogger_IsTrigger(PAD_START);
            if (pauseKey)
            {
                g_IsPaused = true;
                Player_OnPause();  // ループSE（ブースト等）を停止
                Pause_Open();      // 入力状態をリセット（同フレームの誤検知防止）
            }
        }

        //----------------------------------------------------------
        // ポーズ中
        //----------------------------------------------------------
        if (g_IsPaused)
        {
            PauseResult pr = Pause_Update();
            if (pr == PauseResult::Resume)
            {
                g_IsPaused = false;
            }
            else if (pr == PauseResult::GoTitle)
            {
                g_IsPaused = false;
                BeginTransition(GameState::Title, BGM_TITLE);
            }
            break; // ポーズ中はゲーム更新をスキップ
        }

        //----------------------------------------------------------
        // 通常更新
        //----------------------------------------------------------
        Game_Update(elapsed_time);
        g_MissionTime += elapsed_time;

        if (g_GoalCooldown > 0.0)
        {
            g_GoalCooldown -= elapsed_time;
        }

        // ルーム内経過時間を加算（タイムボーナス計算用）
        g_RoomTimer += elapsed_time;

        // ゲームオーバー：プレイヤー無効化かつ演出中でないとき
        // → 即 Result に飛ばさず、PlayerDeath 演出ステートへ移行
        if (!Player_IsEnable() && !BossIntro_IsPlaying() && !BossDefeat_IsPlaying())
        {
            g_Report.survival  = false;
            g_GameState        = GameState::PlayerDeath;
            g_DeathTimer       = 0.0;
            g_DeathExplodeNext = 0.0;
            g_IsPaused = false;
            Player_OnPause();  // ループSE（ブースト等）を停止
            break;
        }

        const MissionDef& mission = Mission_GetCurrentDef();

        // ブロックステージのミッション：増援と制限時間（ボスの演出中は時間を止める）
        if (!mission.legacy && !BossIntro_IsPlaying() && !BossDefeat_IsPlaying())
        {
            const MissionPhase& ph = mission.phases[g_PhaseIndex];
            g_PhaseTimer += elapsed_time;
            if (g_WarningTimer > 0.0) g_WarningTimer -= elapsed_time;

            // 増援：予定時刻になったら降下。敵を全滅させた場合は前倒しで降下する
            if (HasPendingWave(ph))
            {
                const ReinforceWave& wave = ph.waves[g_NextWave];
                if (g_PhaseTimer >= wave.time || Game_GetAliveEnemyCount() == 0)
                {
                    SpawnReinforcements(wave);
                    ++g_NextWave;
                }
            }

            // 時間切れ：作戦失敗
            if (ph.timeLimit > 0.0f && g_PhaseTimer >= ph.timeLimit)
            {
                FailMission();
                break;
            }
        }

        // ブロックステージのミッション：フェーズの達成条件を判定する
        //（ボス撃破フェーズは、下の「ボスを倒したらクリア」の判定に任せる）
        if (!mission.legacy && !g_InBossRoom && g_GoalCooldown <= 0.0)
        {
            const MissionPhase&    ph        = mission.phases[g_PhaseIndex];
            const MissionObjective objective = ph.objective;
            const bool achieved =
                (objective == MissionObjective::ReachGoal  && Map_IsPlayerReachedGoal()) ||
                (objective == MissionObjective::Annihilate && Game_GetAliveEnemyCount() == 0
                                                           && !HasPendingWave(ph));

            if (achieved)
            {
                // タイムボーナス（デフォルトのゴール到達時と同じ計算）
                const int timeBonus = std::max(0, 20000 - static_cast<int>(g_RoomTimer * 100.0));
                Score_Addscore(timeBonus);
                g_RoomTimer = 0.0;

                if (g_PhaseIndex + 1 >= mission.phaseCount)
                {
                    CompleteMission();   // 最終フェーズ達成：作戦成功
                    break;
                }

                // 次のフェーズへ（暗転中にステージを読み込む）
                ++g_PhaseIndex;
                if (mission.phases[g_PhaseIndex].objective == MissionObjective::DestroyBoss)
                    StartBgmLoop(BGM_BOSS);   // ボス戦BGMへ切り替え
                g_PendingDungeonRegenerate = true;
                g_IsTransitioning = true;
                Fade_Start(0.5, true, { 0.0f, 0.0f, 0.0f });
                PlayAudio(g_PlayerWarpSE);
                break;
            }
        }

        // ゴール到達判定（ボス部屋フェーズ中・クールダウン中・演出中はスキップ）
        // ※デフォルト（自動生成ダンジョン）のミッションのみ
        if (mission.legacy && !g_InBossRoom && g_GoalCooldown <= 0.0
            && !BossIntro_IsPlaying()
            && Map_IsPlayerReachedGoal())
        {
            // タイムボーナス：最大20000pt、1秒ごとに100pt減少（最小0）
            // 例: 10秒クリア→19000pt、60秒クリア→14000pt、200秒以上→0pt
            const int timeBonus = std::max(0, 20000 - static_cast<int>(g_RoomTimer * 100.0));
            Score_Addscore(timeBonus);
            g_RoomTimer = 0.0;  // 次のルーム用にリセット

            Map_AddGoalReachCount();

            // ボスのいないミッションは、規定階層を突破した時点で作戦成功
            if (Map_IsClearConditionMet() && !mission.hasBoss)
            {
                CompleteMission();
                break;
            }

            if (Map_IsClearConditionMet())
            {
                // 規定階層を突破：ボス部屋フェーズへ移行
                g_InBossRoom = true;
                StartBgmLoop(BGM_BOSS);   // ボス戦BGMへ切り替え
            }
            // どちらの場合もダンジョン再生成
            g_PendingDungeonRegenerate = true;
            g_IsTransitioning = true;
            Fade_Start(0.5, true, { 0.0f, 0.0f, 0.0f });
            PlayAudio(g_PlayerWarpSE);
        }

        // ボス部屋フェーズ中にボスを倒したらクリア（演出終了後のみチェック）
        if (g_InBossRoom && !BossIntro_IsPlaying() && !BossDefeat_IsPlaying() && !Game_IsBossAlive())
        {
            CompleteMission();
        }

        break;
    }

    case GameState::Survival:
    {
        if (g_IsTransitioning) break;

        //----------------------------------------------------------
        // ポーズトグル（ESC / PAD_START）
        // ショップ表示中は ESC がショップを閉じる操作と被るため抑止
        //----------------------------------------------------------
        if (!g_IsPaused && !Shop_IsOpen())
        {
            const bool pauseKey = KeyLogger_IsTrigger(KK_ESCAPE)
                                || PadLogger_IsTrigger(PAD_START);
            if (pauseKey)
            {
                g_IsPaused = true;
                Player_OnPause();  // ループSE（ブースト等）を停止
                Pause_Open();      // 入力状態をリセット（同フレームの誤検知防止）
            }
        }

        //----------------------------------------------------------
        // ポーズ中
        //----------------------------------------------------------
        if (g_IsPaused)
        {
            PauseResult pr = Pause_Update();
            if (pr == PauseResult::Resume)
            {
                g_IsPaused = false;
            }
            else if (pr == PauseResult::GoTitle)
            {
                g_IsPaused = false;
                Shop_Finalize();
                WaveManager_Finalize();
                BeginTransition(GameState::Title, BGM_TITLE);
            }
            break; // ポーズ中はゲーム更新をスキップ
        }

        // ショップUIを開いている間はゲーム進行を凍結（ポーズ）する。
        // Shop_Update だけは常に回し、カーソル移動・購入・閉じる操作を受け付ける。
        Shop_Update(elapsed_time);

        if (!Shop_IsOpen())
        {
            Game_Update(elapsed_time);
            WaveManager_Update(elapsed_time);
            g_MissionTime += elapsed_time;
        }

        // プレイヤー死亡
        if (!Player_IsEnable())
        {
            Shop_Finalize();
            WaveManager_Finalize();
            g_Report.survival  = true;   // 死亡演出のあとの集計でサバイバルとして扱う
            g_GameState        = GameState::PlayerDeath;
            g_DeathTimer       = 0.0;
            g_DeathExplodeNext = 0.0;
            g_IsPaused         = false;
            Player_OnPause();
            break;
        }

        // 全ウェーブクリア
        if (WaveManager_IsVictory())
        {
            Shop_Finalize();
            WaveManager_Finalize();
            Score_AddRecord(Score_GetScore(),
                WeaponID::WEAPON_MACHINEGUN,
                WeaponID::WEAPON_SHIELD);
            SaveData_SaveScores();
            BuildReport(true, true, L"");
            BeginTransition(GameState::Clear, BGM_RESULT);
        }

        break;
    }

    case GameState::PlayerDeath:
    {
        if (g_IsTransitioning) break;

        g_DeathTimer       += elapsed_time;
        g_DeathExplodeNext -= elapsed_time;

        // パーティクル・カメラだけ更新（ゲームロジックは止める）
        SparkEffect_Update(elapsed_time);
        HUD_Update(elapsed_time);

        // 爆発を連続スポーン（死亡後 2.5 秒間、だんだん密に）
        if (g_DeathExplodeNext <= 0.0 && g_DeathTimer < 2.5)
        {
            XMFLOAT3 base = Player_GetPosition();
            // プレイヤー中心付近にランダムオフセット
            const float ox = ((rand() % 200) - 100) * 0.018f;
            const float oz = ((rand() % 200) - 100) * 0.018f;
            const XMFLOAT3 pos = { base.x + ox, base.y + 0.9f, base.z + oz };

            // スケール 5〜8 のでかい爆発
            const float sc = 5.0f + (rand() % 31) * 0.1f;
            SparkEffect_Create(pos, sc);

            // 序盤はゆっくり（0.35秒）→ 後半は密集（0.15秒）
            g_DeathExplodeNext = (g_DeathTimer < 1.0) ? 0.35 : 0.15;
        }

        // 3.8 秒後にリザルト遷移
        if (g_DeathTimer >= 3.8)
        {
            Score_AddRecord(Score_GetScore(),
                AssemblyScreen_GetRightWeapon(),
                AssemblyScreen_GetLeftWeapon());
            SaveData_SaveScores();
            BuildReport(false, g_Report.survival, L"SIGNAL LOST ― 機体大破");
            BeginTransition(GameState::Result, BGM_RESULT);
        }

        break;
    }

    case GameState::Option:
    {
        if (!g_IsTransitioning)
        {
            Option_Update(elapsed_time);

            if (Option_IsEnd())
            {
                BeginTransition(GameState::Title, BGM_TITLE);
            }
        }
        break;
    }

    case GameState::Result:
    case GameState::Clear:
    {
        if (g_GameState == GameState::Result) Result_Update(elapsed_time);
        else                                  Clear_Update(elapsed_time);

        // 作戦の選択画面へ戻る（アドベンチャー → ミッション選択、サバイバル → モード選択）
        if (!g_IsTransitioning && UI_IsConfirm())
        {
            if (g_Report.survival) BeginTransition(GameState::StageSelect,   BGM_TITLE);
            else                   BeginTransition(GameState::MissionSelect, BGM_ASSEMBLY);
        }
        break;
    }

    case GameState::Exit:
        PostQuitMessage(0);
        break;
    }

    //--------------------------------------------------------------------------
    // フェードアウト完了で遷移を確定
    //--------------------------------------------------------------------------
    if (g_IsTransitioning && Fade_IsOutEnd())
    {
        // ゴール到達によるダンジョン再生成（状態は Playing のまま）
        if (g_PendingDungeonRegenerate)
        {
            g_PendingDungeonRegenerate = false;

            // ブロックステージのミッション：次のフェーズのステージを読み込む
            if (!Mission_GetCurrentDef().legacy)
            {
                LoadMissionPhase(g_PhaseIndex);
                Fade_StartIn(0.5, { 0.0f, 0.0f, 0.0f });
                g_IsTransitioning = false;
                return;
            }

            if (g_InBossRoom)
            {
                // ボス部屋フェーズ：単一アリーナを生成（ゴールなし・雑魚なし）
                Game_SetBossRoomMode(true);
                Map_GenerateBossRoom(++g_DungeonSeed);
            }
            else
            {
                // 通常フェーズ：ランダムダンジョン再生成（ボスなし）
                Game_SetBossRoomMode(false);
                Map_GenerateDungeon(++g_DungeonSeed);
                Score_Addscore(5000);
            }

            PlacePlayerAndResetField();

            g_GoalCooldown = 1.0;

            // ボス部屋フェーズ：フェードイン後に登場演出を開始
            if (g_InBossRoom)
            {
                BossIntro_Start(Map_GetBossSpawnPosition());
            }

            // 再生成完了後にフェードイン
            Fade_StartIn(0.5, { 0.0f, 0.0f, 0.0f });
            g_IsTransitioning = false;
            return;
        }

        // 通常の状態遷移
        g_GameState = g_NextState;

        if (g_GameState == GameState::PreGame)
        {
            PreGame_Initialize();
        }
        else if (g_GameState == GameState::StageSelect)
        {
            StageSelect_Initialize();   // アセンブリのキャンセル・サバイバル終了でフェード遷移してくる
            ResetPlayState();
        }
        else if (g_GameState == GameState::MissionSelect)
        {
            MissionSelect_Initialize(); // 作戦終了後にフェード遷移してくる
            ResetPlayState();
        }
        else if (g_GameState == GameState::WeaponSelect)
        {
            AssemblyScreen_Initialize();
        }
        else if (g_GameState == GameState::EnemyDex)
        {
            EnemyDex_Initialize();
        }
        else if (g_GameState == GameState::Tutorial)
        {
            Tutorial_Initialize();
        }
        else if (g_GameState == GameState::Playing)
        {
            if (g_GameInitialized) Game_Finalize();   // 前回ゲームのリソースを解放（SEリーク防止）

            // 選択したミッションの内容を反映する。
            // Game_Initialize 内の初期ダンジョン生成・敵スポーンに使われるため、必ず先に設定する。
            const MissionDef& mission = Mission_GetCurrentDef();
            ResetPlayState();           // 前回のゲーム（サバイバル含む）のフラグを持ち込まない
            g_MissionTime     = 0.0;
            g_Report          = MissionReport{};
            g_PhaseIndex      = 0;
            g_MissionFailed   = false;
            g_ReinforceSerial = 0;
            Game_SetEnemyHpScale(mission.enemyHpScale);
            EnemyAI_SetSightMultiplier(mission.enemySight);
            if (mission.legacy)
            {
                Map_SetStageConfig(mission.floorCount, mission.enemySpawnRate);
                Game_SetEnemyMix(mission.enemyMix);
            }
            else
            {
                Map_SetStageConfig(2, 10);   // 下の Game_Initialize が作る仮のダンジョン用（既定値）
            }

            Game_Initialize();
            g_GameInitialized = true;
            Score_Reset();
            g_RoomTimer = 0.0;
            Player_SetNormalWeaponIndex(
                static_cast<int>(AssemblyScreen_GetRightWeapon()));
            Player_SetLeftWeaponIndex(
                static_cast<int>(AssemblyScreen_GetLeftWeapon()));

            // ブロックステージのミッションは、初期化後にステージを差し替える
            //（Game_Initialize が内部でダンジョンを生成するため。屋外アリーナと同じ手順）
            if (!mission.legacy)
                LoadMissionPhase(0);
        }
        else if (g_GameState == GameState::Survival)
        {
            // Game_Initialize は内部の Map_Initialize でダンジョンを生成するため、
            // 屋外アリーナは「初期化後」に生成して上書きする（逆順だと消される）
            if (g_GameInitialized) Game_Finalize();   // 前回ゲームのリソースを解放（SEリーク防止）
            ResetDifficulty();                        // ミッションの難易度倍率を持ち込まない
            ResetPlayState();
            g_MissionTime = 0.0;
            g_Report      = MissionReport{};
            g_Report.survival = true;
            Game_Initialize();
            g_GameInitialized = true;
            Game_SetSurvivalMode(true);
            Game_SetBossRoomMode(false);
            Map_GenerateOutdoor(Map_GenerateRandomSeed());
            Map_RegisterFloors();
            Game_ClearEnemies();   // ダンジョン用に湧いた初期敵を消す（敵はウェーブで湧かせる）
            Player_SetPosition(Map_GetSpawnPosition(), true);
            Player_SetFront({ 0.0f, 0.0f, 1.0f });
            Player_Camera_Update(0.0);   // 新スポーン位置にカメラを即追従
            Score_Reset();
            Player_SetNormalWeaponIndex(static_cast<int>(WeaponID::WEAPON_MACHINEGUN));
            Player_SetLeftWeaponIndex  (static_cast<int>(WeaponID::WEAPON_SHIELD));
            WaveManager_Initialize();
            WaveManager_StartSurvival();
            Shop_Initialize(Map_GetSpawnPosition());
        }
        else if (g_GameState == GameState::Title)
        {
            Title_Initialize();
            Map_ResetGoalReachCount();
            g_InBossRoom = false;
            Game_SetBossRoomMode(false);
            Game_SetSurvivalMode(false);
        }
        else if (g_GameState == GameState::Option)
        {
            Option_Initialize();
        }
        else if (g_GameState == GameState::Result)
        {
            Result_Initialize();
        }
        else if (g_GameState == GameState::Clear)
        {
            Clear_Initialize();
        }

        Fade_StartIn(1.0, { 1.0f, 1.0f, 1.0f });
        g_IsTransitioning = false;
    }
}
//------------------------------------------------------------------------------
// 描画
//------------------------------------------------------------------------------
void GameManager_Draw()
{
    switch (g_GameState)
    {
    case GameState::Title:        Title_Draw();          break;
    case GameState::StageSelect:  StageSelect_Draw();    break;
    case GameState::PreGame:      PreGame_Draw();        break;
    case GameState::WeaponSelect: AssemblyScreen_Draw(); break;
    case GameState::MissionSelect: MissionSelect_Draw(); break;
    case GameState::EnemyDex:     EnemyDex_Draw();       break;
    case GameState::Tutorial:     Tutorial_Draw();       break;
    case GameState::Playing:
        Game_Draw();
        if (g_IsPaused)
            Pause_Draw();
        else
            DrawMissionObjective();   // ブロックステージの作戦目標（上部中央）
        break;
    case GameState::Survival:
        Game_Draw();   // ショップの目印（Shop_DrawWorld）は Game_Draw 内の3Dパスで描画される
        WaveManager_Draw();
        Shop_DrawUI();
        if (!g_IsPaused)
            Shop_DrawPrompt();   // 近接時「E/Aで開く」案内（「SHOP」目印は Shop_DrawWorld 内で3D描画）
        if (g_IsPaused)
            Pause_Draw();
        break;
    case GameState::PlayerDeath:
        Game_Draw();
        {
            // 0.8秒後からフェードイン（0.7秒かけて完全表示）
            float alpha = (float)((g_DeathTimer - 0.8) / 0.7);
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
            HUD_DrawGameOver(alpha);
        }
        break;
    case GameState::Option:  Option_Draw();  break;
    case GameState::Result:  Result_Draw();  break;
    case GameState::Clear:   Clear_Draw();   break;
    case GameState::Exit:    /* 何も描かない */ break;
    }

    // ── チュートリアルヒントバー（フェードの直前）──────────────────────
    switch (g_GameState)
    {
    case GameState::WeaponSelect:
        InputHint_Draw(
            "{W}{S} Move    {K_A}{K_D} Tab    {ENTER} Set / Ready    {TAB} Section    {ESC} Back",
            "{DPAD_UP}{DPAD_DN} Move    {DPAD_LR} Tab    {A} Set / Ready    {LB}{RB} Section    {B} Back");
        break;
    case GameState::Title:
    {
        static const wchar_t* titleDesc[] = {
            L"ゲームを開始します",
            L"音量・感度などの設定を変更します",
            L"ゲームを終了します",
        };
        const wchar_t* desc = titleDesc[Title_GetSelected()];
        InputHint_Draw(
            "{UP}{DOWN} Move    {ENTER} Select",
            "{DPAD_UP}{DPAD_DN} Move    {A} Select",
            desc);
        break;
    }
    case GameState::Playing:
        if (g_IsPaused)
            InputHint_Draw(
                "{UP}{DOWN} Move    {ENTER} Select    {ESC} Back",
                "{DPAD_UP}{DPAD_DN} Move    {A} Select    {B} Back");
        else
            InputHint_Draw(
                "{W}{K_A}{S}{K_D} Move    {SPACE} Jump    {SHIFT} Dash    {MOUSE_MOVE} Aim    {MOUSE_R} R-ARM    {MOUSE_L} L-ARM    {ESC} Pause",
                "{L_STICK} Move    {A} Jump    {B} Dash    {R_STICK} Aim    {RB} R-ARM    {LB} L-ARM    {X} Lock-On    {START} Pause");
        break;
    case GameState::Survival:
        if (g_IsPaused)
            InputHint_Draw(
                "{UP}{DOWN} Move    {ENTER} Select    {ESC} Back",
                "{DPAD_UP}{DPAD_DN} Move    {A} Select    {B} Back");
        else if (!Shop_IsOpen())   // ショップ表示中は Shop_DrawUI が自前のヒントを描く
            InputHint_Draw(
                "{W}{K_A}{S}{K_D} Move    {SPACE} Jump    {SHIFT} Dash    {MOUSE_MOVE} Aim    {MOUSE_R} R-ARM    {MOUSE_L} L-ARM    {ESC} Pause",
                "{L_STICK} Move    {A} Jump    {B} Dash    {R_STICK} Aim    {RB} R-ARM    {LB} L-ARM    {X} Lock-On    {START} Pause");
        break;
    case GameState::EnemyDex:
        InputHint_Draw(
            "{UP}{DOWN} Select    {LEFT}{RIGHT} Rotate    {ESC} Back",
            "{DPAD_UP}{DPAD_DN} Select    {DPAD_LR} Rotate    {B} Back");
        break;
    case GameState::Option:
        InputHint_Draw(
            "{UP}{DOWN} Move    {LEFT}{RIGHT} Change    {ESC} Back",
            "{DPAD_UP}{DPAD_DN} Move    {DPAD_LR} Change    {B} Back");
        break;
    case GameState::Result:
    case GameState::Clear:
        if (g_Report.survival)
            InputHint_Draw("{ENTER} Mode Select", "{A} Mode Select");
        else
            InputHint_Draw("{ENTER} Mission Select", "{A} Mission Select");
        break;
    default:
        break;
    }

    // フェードは最後に重ねる
    Fade_Draw();
}

//------------------------------------------------------------------------------
// ポーズ中フラグ取得
//------------------------------------------------------------------------------
bool GameManager_IsPaused()
{
    return g_IsPaused;
}

GameState GameManager_GetState()
{
    return g_GameState;
}