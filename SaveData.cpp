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
#include <windows.h>
#include <cstdio>
#include <cstring>

//------------------------------------------------------------------------------
// INI ファイルパス（実行ファイルからの相対パス）
//------------------------------------------------------------------------------
static const char* INI_PATH    = "resource/Savedata/config.ini";
static const char* SEC_AUDIO   = "Audio";
static const char* SEC_CAMERA  = "Camera";
static const char* SEC_DISPLAY = "Display";
static const char* SEC_RECORDS  = "Records";
static const char* SEC_ASSEMBLY = "Assembly";

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

    // ── Audio ──────────────────────────────────────────────
    float volume = ReadFloat(SEC_AUDIO, "Volume", 0.8f, path);
    SetMasterVolume(volume);

    // ── Camera ─────────────────────────────────────────────
    float sens   = ReadFloat(SEC_CAMERA, "Sensitivity", 0.005f, path);
    int   invertY = ReadInt(SEC_CAMERA, "InvertY", 0, path);
    Player_Camera_SetMouseSensitivity(sens);
    Player_Camera_SetMouseInvertY(invertY != 0);

    // ── Display ────────────────────────────────────────────
    int savedFS = ReadInt(SEC_DISPLAY, "Fullscreen", 0, path);
    bool currentFS = GameWindow_IsFullscreen();
    if ((savedFS != 0) != currentFS)
        GameWindow_RequestFullscreenToggle();

    // ── Assembly ───────────────────────────────────────────
    {
        int rw = ReadInt(SEC_ASSEMBLY, "LastRight", 0, path);
        int lw = ReadInt(SEC_ASSEMBLY, "LastLeft",  3, path);
        AssemblyScreen_SetDefaults(static_cast<WeaponID>(rw), static_cast<WeaponID>(lw));
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
    WriteFloat(SEC_CAMERA, "Sensitivity", Player_Camera_GetMouseSensitivity(), path);
    WriteInt  (SEC_CAMERA, "InvertY",     Player_Camera_GetMouseInvertY() ? 1 : 0, path);

    // ── Display ────────────────────────────────────────────
    WriteInt(SEC_DISPLAY, "Fullscreen", GameWindow_IsFullscreen() ? 1 : 0, path);

    // ── Assembly ───────────────────────────────────────────
    WriteInt(SEC_ASSEMBLY, "LastRight", (int)AssemblyScreen_GetRightWeapon(), path);
    WriteInt(SEC_ASSEMBLY, "LastLeft",  (int)AssemblyScreen_GetLeftWeapon(),  path);

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
