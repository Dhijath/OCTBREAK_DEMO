/*==============================================================================
   設定永続化 [SaveData.cpp]
   保存先 : resource/safedate/config.ini
   Win32 GetPrivateProfileString / WritePrivateProfileString を使用。
==============================================================================*/
#include "SaveData.h"
#include "Audio.h"
#include "player_camera.h"
#include "game_window.h"
#include "Score.h"
#include "AssemblyScreen.h"
#include "MissionDef.h"
#include "Option.h"
#include "EnemyDex.h"
#include "MechParts.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

//------------------------------------------------------------------------------
// INI ファイルパス（実行ファイルからの相対パス）
//------------------------------------------------------------------------------
static const char* INI_PATH      = "resource/Savedata/config.ini";
static const char* SEC_AUDIO     = "Audio";
static const char* SEC_CAMERA    = "Camera";
static const char* SEC_DISPLAY   = "Display";
static const char* SEC_GRAPHICS  = "Graphics";
static const char* SEC_RECORDS   = "Records";
static const char* SEC_ASSEMBLY  = "Assembly";
static const char* SEC_MISSION   = "Mission";
static const char* SEC_DEX       = "EnemyDex";

//------------------------------------------------------------------------------
// 内部ヘルパー: float を文字列で書き込む
//------------------------------------------------------------------------------
static void WriteFloat(const char* sec, const char* key, float val, const char* path)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", val);
    WritePrivateProfileStringA(sec, key, buf, path);
}

static float ReadFloat(const char* sec, const char* key, float def, const char* path)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", def);
    char out[32];
    GetPrivateProfileStringA(sec, key, buf, out, sizeof(out), path);
    return static_cast<float>(atof(out));
}

static void WriteInt(const char* sec, const char* key, int val, const char* path)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    WritePrivateProfileStringA(sec, key, buf, path);
}

static int ReadInt(const char* sec, const char* key, int def, const char* path)
{
    return static_cast<int>(GetPrivateProfileIntA(sec, key, def, path));
}

//------------------------------------------------------------------------------
// フルパス取得（カレントディレクトリ相対 → 絶対パスへ）
//------------------------------------------------------------------------------
static void GetAbsPath(char* out, size_t outSize)
{
    char dir[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, dir);
    snprintf(out, outSize, "%s\\%s", dir, INI_PATH);
    // スラッシュをバックスラッシュに変換（Win32 APIはバックスラッシュ必須）
    for (char* p = out; *p; ++p)
        if (*p == '/') *p = '\\';
}

//==============================================================================
// 読み込み → 各モジュールに反映
//==============================================================================
// フォルダが無ければ作成する
static void EnsureDir()
{
    char dir[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, dir);
    char fullDir[MAX_PATH];
    snprintf(fullDir, MAX_PATH, "%s\\resource\\Savedata", dir);
    CreateDirectoryA(fullDir, nullptr); // 既存なら何もしない
}

void SaveData_Load()
{
    EnsureDir();
    char path[MAX_PATH];
    GetAbsPath(path, MAX_PATH);

    // ── Enemy Dex（種別ごとの撃破数。キーは EnemyType の値）──
    for (int i = 0; i < EnemyDex_TypeCount(); ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "Kill%02d", i);
        EnemyDex_SetKills(i, ReadInt(SEC_DEX, key, 0, path));
    }

    // ── Audio ──────────────────────────────────────────────
    float volume = ReadFloat(SEC_AUDIO, "Volume", 0.8f, path);
    SetMasterVolume(volume);

    // ── Camera ─────────────────────────────────────────────
    float sens   = ReadFloat(SEC_CAMERA, "Sensitivity", 0.005f, path);
    int   invertY = ReadInt(SEC_CAMERA, "InvertY", 0, path);
    float padSens = ReadFloat(SEC_CAMERA, "PadSensitivity", 1.0f, path);
    Player_Camera_SetMouseSensitivity(sens);
    Player_Camera_SetMouseInvertY(invertY != 0);
    Player_Camera_SetPadSensitivity(padSens);

    // ── Display ────────────────────────────────────────────
    int savedFS = ReadInt(SEC_DISPLAY, "Fullscreen", 0, path);
    bool currentFS = GameWindow_IsFullscreen();
    if ((savedFS != 0) != currentFS)
        GameWindow_RequestFullscreenToggle();

    // ── Assembly ───────────────────────────────────────────
    {
        int rw = ReadInt(SEC_ASSEMBLY, "LastRight", 0, path);
        int lw = ReadInt(SEC_ASSEMBLY, "LastLeft",  3, path);
    MechParts_SetFrameSel(FRAME_HEAD, ReadInt(SEC_ASSEMBLY, "Head", 0, path));
    MechParts_SetFrameSel(FRAME_BODY, ReadInt(SEC_ASSEMBLY, "Body", 0, path));
    MechParts_SetFrameSel(FRAME_LEGS, ReadInt(SEC_ASSEMBLY, "Legs", 0, path));
    MechParts_SetInternalSel(0, ReadInt(SEC_ASSEMBLY, "Internal1", 0, path));
    MechParts_SetInternalSel(1, ReadInt(SEC_ASSEMBLY, "Internal2", 0, path));
        AssemblyScreen_SetDefaults(static_cast<WeaponID>(rw), static_cast<WeaponID>(lw));
    }

    // ── Mission ────────────────────────────────────────────
    Mission_SetCurrent(ReadInt(SEC_MISSION, "Last", 0, path));
    for (int i = 0; i < MISSION_COUNT; ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "Cleared_%d", i);
        Mission_SetCleared(i, ReadInt(SEC_MISSION, key, 0, path) != 0);
    }

    // ── Graphics ───────────────────────────────────────────
    {
        int sm = ReadInt(SEC_GRAPHICS, "ShadowMode", 3, path); // デフォルト=3（高・PCF）
        Option_SetShadowMode(sm);
    }

    // ── Records ────────────────────────────────────────────
    Score_ClearRecords();
    int count = ReadInt(SEC_RECORDS, "Count", 0, path);
    if (count > SCORE_RECORD_MAX) count = SCORE_RECORD_MAX;
    for (int i = 0; i < count; ++i)
    {
        char keyScore[16], keyRight[16], keyLeft[16];
        snprintf(keyScore, sizeof(keyScore), "Score_%d", i);
        snprintf(keyRight, sizeof(keyRight), "Right_%d", i);
        snprintf(keyLeft,  sizeof(keyLeft),  "Left_%d",  i);

        unsigned int sc = (unsigned int)ReadInt(SEC_RECORDS, keyScore, 0, path);
        int          rw = ReadInt(SEC_RECORDS, keyRight, 0, path);
        int          lw = ReadInt(SEC_RECORDS, keyLeft,  0, path);

        if (sc > 0)
            Score_AddRecord(sc, static_cast<WeaponID>(rw), static_cast<WeaponID>(lw));
    }
}

//==============================================================================
// 現在値を書き込み（オプション画面を閉じた時に呼ぶ）
//==============================================================================
void SaveData_Save()
{
    EnsureDir();
    char path[MAX_PATH];
    GetAbsPath(path, MAX_PATH);

    // ── Audio ──────────────────────────────────────────────
    WriteFloat(SEC_AUDIO, "Volume", GetMasterVolume(), path);

    // ── Camera ─────────────────────────────────────────────
    WriteFloat(SEC_CAMERA, "Sensitivity",    Player_Camera_GetMouseSensitivity(), path);
    WriteInt  (SEC_CAMERA, "InvertY",        Player_Camera_GetMouseInvertY() ? 1 : 0, path);
    WriteFloat(SEC_CAMERA, "PadSensitivity", Player_Camera_GetPadSensitivity(), path);

    // ── Display ────────────────────────────────────────────
    WriteInt(SEC_DISPLAY, "Fullscreen", GameWindow_IsFullscreen() ? 1 : 0, path);

    // ── Graphics ───────────────────────────────────────────
    WriteInt(SEC_GRAPHICS, "ShadowMode", Option_GetShadowMode(), path);

    // ── Assembly ───────────────────────────────────────────
    WriteInt(SEC_ASSEMBLY, "LastRight", (int)AssemblyScreen_GetRightWeapon(), path);
    WriteInt(SEC_ASSEMBLY, "LastLeft",  (int)AssemblyScreen_GetLeftWeapon(),  path);
    // 機体パーツ（頭・胴体・脚部）と内部パーツ（2スロット）
    WriteInt(SEC_ASSEMBLY, "Head",      MechParts_GetFrameSel(FRAME_HEAD), path);
    WriteInt(SEC_ASSEMBLY, "Body",      MechParts_GetFrameSel(FRAME_BODY), path);
    WriteInt(SEC_ASSEMBLY, "Legs",      MechParts_GetFrameSel(FRAME_LEGS), path);
    WriteInt(SEC_ASSEMBLY, "Internal1", MechParts_GetInternalSel(0),       path);
    WriteInt(SEC_ASSEMBLY, "Internal2", MechParts_GetInternalSel(1),       path);

    // ── Records ────────────────────────────────────────────
    int count = Score_GetRecordCount();
    WriteInt(SEC_RECORDS, "Count", count, path);
    const ScoreRecord* recs = Score_GetRecords();
    for (int i = 0; i < count; ++i)
    {
        char keyScore[16], keyRight[16], keyLeft[16];
        snprintf(keyScore, sizeof(keyScore), "Score_%d", i);
        snprintf(keyRight, sizeof(keyRight), "Right_%d", i);
        snprintf(keyLeft,  sizeof(keyLeft),  "Left_%d",  i);
        WriteInt(SEC_RECORDS, keyScore, (int)recs[i].score,       path);
        WriteInt(SEC_RECORDS, keyRight, (int)recs[i].rightWeapon, path);
        WriteInt(SEC_RECORDS, keyLeft,  (int)recs[i].leftWeapon,  path);
    }
}

void SaveData_SaveMissions()
{
    EnsureDir();
    char path[MAX_PATH];
    GetAbsPath(path, MAX_PATH);

    WriteInt(SEC_MISSION, "Last", Mission_GetCurrent(), path);
    for (int i = 0; i < MISSION_COUNT; ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "Cleared_%d", i);
        WriteInt(SEC_MISSION, key, Mission_IsCleared(i) ? 1 : 0, path);
    }
}

void SaveData_SaveScores()
{
    EnsureDir();
    char path[MAX_PATH];
    GetAbsPath(path, MAX_PATH);

    int count = Score_GetRecordCount();
    WriteInt(SEC_RECORDS, "Count", count, path);
    const ScoreRecord* recs = Score_GetRecords();
    for (int i = 0; i < count; ++i)
    {
        char keyScore[16], keyRight[16], keyLeft[16];
        snprintf(keyScore, sizeof(keyScore), "Score_%d", i);
        snprintf(keyRight, sizeof(keyRight), "Right_%d", i);
        snprintf(keyLeft,  sizeof(keyLeft),  "Left_%d",  i);
        WriteInt(SEC_RECORDS, keyScore, (int)recs[i].score,       path);
        WriteInt(SEC_RECORDS, keyRight, (int)recs[i].rightWeapon, path);
        WriteInt(SEC_RECORDS, keyLeft,  (int)recs[i].leftWeapon,  path);
    }
}

//==============================================================================
// エネミー図鑑の撃破数のみ書き込み
//==============================================================================
void SaveData_SaveDex()
{
    EnsureDir();
    char path[MAX_PATH];
    GetAbsPath(path, MAX_PATH);
    for (int i = 0; i < EnemyDex_TypeCount(); ++i)
    {
        char key[16];
        snprintf(key, sizeof(key), "Kill%02d", i);
        WriteInt(SEC_DEX, key, EnemyDex_GetKills(i), path);
    }
}
