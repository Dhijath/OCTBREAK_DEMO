/*==============================================================================

   ゲーム管理 [Game_Manager.cpp]
   Author : 51106
   Date   : 2026/02/08

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
#include "ScoreCheck.h"
#include "Enemy.h"
#include "map.h"
#include <cstdint>
#include "pad_logger.h"
#include "Pause.h"
#include "BossIntro.h"
#include "ItemManager.h"
#include "player_camera.h"
#include "HUD.h"
#include "input_hint.h"
#include "UIInput.h"
#include "SaveData.h"
#include "Score.h"
#include <DirectXMath.h>
#include <cstdlib>
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
// ゴール到達によるダンジョン再生成待ちフラグ
static bool g_PendingDungeonRegenerate = false;
// ボス部屋フェーズ中フラグ（2回ゴール到達後に true になる）
static bool g_InBossRoom = false;
// ポーズ中フラグ（フェードなし即時停止）
static bool g_IsPaused = false;


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
    if      (next == GameState::PreGame) PreGame_Initialize();
    else if (next == GameState::Title)   Title_Initialize();
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
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void GameManager_Finalize()
{
    Title_Finalize();
    PreGame_Finalize();
    AssemblyScreen_Finalize();
    ScoreCheck_Finalize();
    Option_Finalize();
    Game_Finalize();
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
                SwitchInstant(GameState::PreGame);  // 同一背景・BGMなのでフェードなし
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

    case GameState::PreGame:
    {
        if (!g_IsTransitioning)
        {
            PreGame_Update(elapsed_time);
            PreGameResult pr = PreGame_GetResult();
            if (pr == PreGameResult::QuickStart)
            {
                SaveData_Save();
                BeginTransition(GameState::Playing, BGM_GAME, BGM_VOL_GAME);
            }
            else if (pr == PreGameResult::Assembly)
                BeginTransition(GameState::WeaponSelect, BGM_ASSEMBLY);
            else if (pr == PreGameResult::ScoreCheck)
                BeginTransition(GameState::ScoreCheck, BGM_SCOREBOARD);
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
                    BeginTransition(GameState::PreGame, BGM_TITLE);
                else
                {
                    SaveData_Save();   // 確定したアセンブリを保存
                    BeginTransition(GameState::Playing, BGM_GAME, BGM_VOL_GAME);
                }
            }
        }
        break;
    }

    case GameState::ScoreCheck:
    {
        if (!g_IsTransitioning)
        {
            ScoreCheck_Update(elapsed_time);
            if (ScoreCheck_IsEnd())
                BeginTransition(GameState::PreGame, BGM_TITLE);
        }
        break;
    }

    case GameState::Playing:
    {
        if (g_IsTransitioning) break;

        //----------------------------------------------------------
        // ポーズトグル（ESC / PAD_START）
        // ボス演出中・遷移中はポーズ不可
        //----------------------------------------------------------
        if (!BossIntro_IsPlaying() && !g_IsPaused)
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

        if (g_GoalCooldown > 0.0)
        {
            g_GoalCooldown -= elapsed_time;
        }

        // ゲームオーバー：プレイヤー無効化かつ演出中でないとき
        // → 即 Result に飛ばさず、PlayerDeath 演出ステートへ移行
        if (!Player_IsEnable() && !BossIntro_IsPlaying())
        {
            g_GameState        = GameState::PlayerDeath;
            g_DeathTimer       = 0.0;
            g_DeathExplodeNext = 0.0;
            g_IsPaused = false;
            Player_OnPause();  // ループSE（ブースト等）を停止
            break;
        }

        // ゴール到達判定（ボス部屋フェーズ中・クールダウン中・演出中はスキップ）
        if (!g_InBossRoom && g_GoalCooldown <= 0.0
            && !BossIntro_IsPlaying()
            && Map_IsPlayerReachedGoal())
        {
            Map_AddGoalReachCount();

            if (Map_IsClearConditionMet())
            {
                // 2回到達：ボス部屋フェーズへ移行
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
        if (g_InBossRoom && !BossIntro_IsPlaying() && !Game_IsBossAlive())
        {
            PlayAudio(g_PlayerclearSE);
            Score_AddRecord(Score_GetScore(),
                AssemblyScreen_GetRightWeapon(),
                AssemblyScreen_GetLeftWeapon());
            SaveData_SaveScores();
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
    {
        if (!g_IsTransitioning && UI_IsConfirm())
            BeginTransition(GameState::Title, BGM_TITLE);
        break;
    }

    case GameState::Clear:
    {
        if (!g_IsTransitioning && UI_IsConfirm())
            BeginTransition(GameState::Title, BGM_TITLE);
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
        else if (g_GameState == GameState::WeaponSelect)
        {
            AssemblyScreen_Initialize();
        }
        else if (g_GameState == GameState::ScoreCheck)
        {
            ScoreCheck_Initialize();
        }
        else if (g_GameState == GameState::Playing)
        {
            Game_Initialize();
            Player_SetNormalWeaponIndex(
                static_cast<int>(AssemblyScreen_GetRightWeapon()));  // 右腕武器を反映
            Player_SetLeftWeaponIndex(
                static_cast<int>(AssemblyScreen_GetLeftWeapon()));   // 左腕武器を反映
        }
        else if (g_GameState == GameState::Title)
        {
            Title_Initialize();
            Map_ResetGoalReachCount();
            g_InBossRoom = false;
            Game_SetBossRoomMode(false);
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
    case GameState::PreGame:      PreGame_Draw();        break;
    case GameState::WeaponSelect: AssemblyScreen_Draw(); break;
    case GameState::ScoreCheck:   ScoreCheck_Draw();     break;
    case GameState::Playing:
        Game_Draw();
        // ポーズ中はゲーム画面の上にメニューを重ねる
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
            "{TAB} R/L ARM    {W}{S} Move    {ENTER} Quick Start    {ESC} Back",
            "{LB}{RB} R/L ARM    {DPAD_UP}{DPAD_DN} Move    {A} Quick Start    {B} Back");
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
                "{ENTER} Select",
                "{A} Select");
        else
            InputHint_Draw(
                "{W}{K_A}{S}{K_D} Move    {SPACE} Jump    {MOUSE_MOVE} Aim    {MOUSE_R} R-ARM    {MOUSE_L} L-ARM    {ESC} Pause",
                "{L_STICK} Move    {A} Jump    {R_STICK} Aim    {RB} R-ARM    {LB} L-ARM    {B} Lock-On    {START} Pause");
        break;
    case GameState::Option:
        InputHint_Draw(
            "{UP}{DOWN} Move    {LEFT}{RIGHT} Change    {ENTER} Back",
            "{DPAD_UP}{DPAD_DN} Move    {DPAD_LR} Change    {B} Back");
        break;
    case GameState::Result:
    case GameState::Clear:
        InputHint_Draw(
            "{ENTER} Back to Title",
            "{A} Back to Title");
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