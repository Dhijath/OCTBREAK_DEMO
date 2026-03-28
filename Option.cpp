/*==============================================================================

   オプション画面 [Option.cpp]
   Author : 51106
   Date   : 2026/03/23
--------------------------------------------------------------------------------
   DirectWrite（Arial）で描画するオプション画面。
   Pause のオプションサブパネルと同じスタイル（濃紺パネル＋シアン枠）。

   ■項目
     0: ボリューム      – LEFT / RIGHT で 0.1 刻み増減
     1: 感度            – LEFT / RIGHT で 1〜20 の 20段階（横縦統一）
     2: Y軸反転         – LEFT / RIGHT でトグル
     3: フルスクリーン  – LEFT / RIGHT でウィンドウ ↔ ボーダーレス切替

   ■操作
     UP / DOWN (W/S / 十字↑↓)  : 項目選択
     LEFT / RIGHT (A/D / 十字←→): 値変更
     ENTER / PAD_B              : タイトルへ戻る

==============================================================================*/

#include "Option.h"
#include "texture.h"
#include "sprite.h"
#include "UIInput.h"
#include "Audio.h"
#include "SaveData.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "game_window.h"
#include "player_camera.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <cmath>

using namespace DirectX;

namespace
{
    //==========================================================================
    // リソース
    //==========================================================================
    int g_BgTexID    = -1;
    int g_WhiteTexID = -1;

    DirectWrite* g_pDWTitle = nullptr;   // "OPTIONS" ヘッダー（Bold 30pt・中央揃え）
    DirectWrite* g_pDWBody  = nullptr;   // 値・フッター（22pt・中央揃え）
    DirectWrite* g_pDW_Label = nullptr;  // 項目ラベル（22pt Bold・右揃え）

    int g_SeCursorMove = -1;
    int g_SeTabSwitch  = -1;
    int g_SeCancel     = -1;

    //==========================================================================
    // 状態
    //==========================================================================
    static constexpr int ITEM_COUNT = 5;   // ボリューム / 感度 / Y軸反転 / フルスクリーン / シャドウ

    int    g_CursorItem        = 0;
    bool   g_End               = false;
    double g_Time              = 0.0;

    float  g_Volume            = 0.5f;
    int    g_ShadowMode        = 3;        // 0=なし / 1=低（マル影） / 2=中（ハード） / 3=高（PCF）

    //==========================================================================
    // 感度定数
    //==========================================================================
    static constexpr float SENS_STEP = 0.0025f;
    static constexpr int   SENS_MIN  = 1;
    static constexpr int   SENS_MAX  = 20;
}

//------------------------------------------------------------------------------
// レイアウト定数（仮想 1600×900 空間）
//------------------------------------------------------------------------------
static constexpr float PNL_W   = 700.0f;
static constexpr float PNL_H   = 450.0f;
static constexpr float PNL_X   = (1600.0f - PNL_W) * 0.5f;   // 450
static constexpr float PNL_Y   = 200.0f;
static constexpr float ROW_Y0  = PNL_Y + 78.0f;    // ボリューム
static constexpr float ROW_Y1  = PNL_Y + 143.0f;   // 感度
static constexpr float ROW_Y2  = PNL_Y + 208.0f;   // Y軸反転
static constexpr float ROW_Y3  = PNL_Y + 273.0f;   // フルスクリーン
static constexpr float ROW_Y4  = PNL_Y + 338.0f;   // シャドウ
static constexpr float BAR_X   = PNL_X + 310.0f;   // 760
static constexpr float BAR_W   = 300.0f;
static constexpr float BAR_H   = 16.0f;
static constexpr float LBL_HW  = 120.0f;
static constexpr float LBL_CX  = BAR_X - 12.0f - LBL_HW;   // 628
static constexpr float VAL_CX  = BAR_X + BAR_W + 55.0f;     // 1115
static constexpr float VAL_HW  = 85.0f;

//==============================================================================
// 初期化
//==============================================================================
void Option_Initialize()
{
    g_CursorItem = 0;
    g_End        = false;
    g_Time       = 0.0;

    // 現在のマスター音量を表示値に反映（SaveData_Load 後の値が正）
    g_Volume = GetMasterVolume();

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudio("resource/Sound/ui_tab_switch.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }

    g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");
    g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");

    // "OPTIONS" タイトル（Arial Bold 30pt・中央揃え）
    if (!g_pDWTitle)
    {
        static FontData fdTitle;
        fdTitle.font          = Font::Arial;
        fdTitle.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdTitle.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdTitle.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdTitle.fontSize      = 30.0f;
        fdTitle.localeName    = L"en-us";
        fdTitle.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdTitle.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWTitle = new DirectWrite(&fdTitle);
        g_pDWTitle->Init();
    }

    // 値・フッター（Arial 22pt・中央揃え）
    if (!g_pDWBody)
    {
        static FontData fdBody;
        fdBody.font          = Font::Arial;
        fdBody.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        fdBody.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdBody.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdBody.fontSize      = 22.0f;
        fdBody.localeName    = L"ja-jp";
        fdBody.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdBody.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWBody = new DirectWrite(&fdBody);
        g_pDWBody->Init();
    }

    // 項目ラベル（Arial Bold 22pt・右揃え）
    if (!g_pDW_Label)
    {
        static FontData fdLabel;
        fdLabel.font          = Font::Arial;
        fdLabel.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdLabel.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdLabel.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdLabel.fontSize      = 22.0f;
        fdLabel.localeName    = L"ja-jp";
        fdLabel.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;   // 右揃え
        fdLabel.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDW_Label = new DirectWrite(&fdLabel);
        g_pDW_Label->Init();
    }

    // g_Volume は GetMasterVolume() で既に反映済み（SaveData_Load が先に実行）
    SetMasterVolume(g_Volume);
}

//==============================================================================
// 終了処理
//==============================================================================
void Option_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeTabSwitch);  g_SeTabSwitch  = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;

    if (g_pDWTitle)   { g_pDWTitle->Release();   delete g_pDWTitle;   g_pDWTitle   = nullptr; }
    if (g_pDWBody)    { g_pDWBody->Release();     delete g_pDWBody;    g_pDWBody    = nullptr; }
    if (g_pDW_Label)  { g_pDW_Label->Release();   delete g_pDW_Label;  g_pDW_Label  = nullptr; }

    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }
}

//==============================================================================
// 更新処理
//==============================================================================
void Option_Update(double elapsed_time)
{
    g_Time += elapsed_time;

    // ── カーソル上下 ──────────────────────────────────────────────────────
    if (UI_IsMoveUp())   { g_CursorItem = (g_CursorItem - 1 + ITEM_COUNT) % ITEM_COUNT; PlayAudio(g_SeCursorMove, false); }
    if (UI_IsMoveDown()) { g_CursorItem = (g_CursorItem + 1) % ITEM_COUNT;              PlayAudio(g_SeCursorMove, false); }

    // ── 値変更 ────────────────────────────────────────────────────────────
    const bool goLeft  = UI_IsMoveLeft();
    const bool goRight = UI_IsMoveRight();

    if (g_CursorItem == 0) // ボリューム
    {
        if (goLeft)  { g_Volume = std::max(0.0f, g_Volume - 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeTabSwitch, false); }
        if (goRight) { g_Volume = std::min(1.0f, g_Volume + 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 1) // 感度（横縦統一）
    {
        int step = std::max(SENS_MIN, std::min(SENS_MAX, static_cast<int>(roundf(Player_Camera_GetMouseSensitivity() / SENS_STEP))));
        if (goLeft  && step > SENS_MIN) { --step; Player_Camera_SetMouseSensitivity(step * SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
        if (goRight && step < SENS_MAX) { ++step; Player_Camera_SetMouseSensitivity(step * SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 2) // Y軸反転
    {
        if (goLeft || goRight) { Player_Camera_SetMouseInvertY(!Player_Camera_GetMouseInvertY()); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 3) // フルスクリーン
    {
        if (goLeft || goRight) { GameWindow_RequestFullscreenToggle(); PlayAudio(g_SeTabSwitch, false); }
    }
    else // シャドウ（cursor == 4）：0→1→2→3→0 サイクル
    {
        if (goRight) { g_ShadowMode = (g_ShadowMode + 1) % 4; PlayAudio(g_SeTabSwitch, false); }
        if (goLeft)  { g_ShadowMode = (g_ShadowMode + 3) % 4; PlayAudio(g_SeTabSwitch, false); }
    }

    // ── 戻る（ESC / PAD_B）──────────────────────────────────────────────
    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        SaveData_Save();    // 設定を config.ini に書き込む
        g_End = true;
    }
}

//==============================================================================
// 描画処理
//==============================================================================
void Option_Draw()
{
    static const XMFLOAT4 WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };

    //--------------------------------------------------------------------------
    // 背景
    //--------------------------------------------------------------------------
    if (g_BgTexID >= 0)
        Sprite_Draw(g_BgTexID, 0.0f, 0.0f, 1600.0f, 900.0f, WHITE);

    if (g_WhiteTexID < 0 || !g_pDWTitle || !g_pDWBody || !g_pDW_Label) return;

    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

    //--------------------------------------------------------------------------
    // "OPTIONS" タイトル（パネル外・上部）
    //--------------------------------------------------------------------------
    {
        const D2D1_COLOR_F dWHITE = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWTitle->SetScale(scaleX, scaleY);
        g_pDWTitle->BeginBatch();
        g_pDWTitle->DrawAt("OPTIONS", 800.0f, 130.0f, 400.0f, dWHITE, 2.0f);
        g_pDWTitle->EndBatch();
        g_pDWTitle->SetScale(1.0f, 1.0f);
    }

    //--------------------------------------------------------------------------
    // パネル背景・枠（Pause オプションパネルと同スタイル）
    //--------------------------------------------------------------------------
    const XMFLOAT4 BG     = { 0.04f, 0.06f, 0.12f, 0.92f };
    const XMFLOAT4 BORDER = { 0.2f,  0.72f, 1.0f,  0.8f  };
    const XMFLOAT4 BARFIL = { 0.2f,  0.72f, 1.0f,  1.0f  };
    const XMFLOAT4 BAREMP = { 0.12f, 0.12f, 0.12f, 1.0f  };

    Sprite_Draw(g_WhiteTexID, PNL_X - 2.0f, PNL_Y - 2.0f, PNL_W + 4.0f, PNL_H + 4.0f, BORDER);
    Sprite_Draw(g_WhiteTexID, PNL_X, PNL_Y, PNL_W, PNL_H, BG);

    //--------------------------------------------------------------------------
    // バー（ボリューム・感度）
    //--------------------------------------------------------------------------
    auto drawBar = [&](float rowY, float ratio)
    {
        Sprite_Draw(g_WhiteTexID, BAR_X, rowY - BAR_H * 0.5f, BAR_W, BAR_H, BAREMP);
        const float f = ratio * BAR_W;
        if (f >= 1.0f)
            Sprite_Draw(g_WhiteTexID, BAR_X, rowY - BAR_H * 0.5f, f, BAR_H, BARFIL);
    };

    drawBar(ROW_Y0, g_Volume);

    auto toStep = [](float s) -> int { return std::max(SENS_MIN, std::min(SENS_MAX, static_cast<int>(roundf(s / SENS_STEP)))); };
    const int   sensStep  = toStep(Player_Camera_GetMouseSensitivity());
    const float sensRatio = static_cast<float>(sensStep - SENS_MIN) / (SENS_MAX - SENS_MIN);
    drawBar(ROW_Y1, sensRatio);
    // Y軸反転・フルスクリーンはバーなし

    //--------------------------------------------------------------------------
    // 選択行ハイライト
    //--------------------------------------------------------------------------
    const XMFLOAT4 SEL_HL = { 0.2f, 0.72f, 1.0f, 0.15f };
    const float rowYs[ITEM_COUNT] = { ROW_Y0, ROW_Y1, ROW_Y2, ROW_Y3, ROW_Y4 };
    Sprite_Draw(g_WhiteTexID, PNL_X + 4.0f, rowYs[g_CursorItem] - 22.0f,
        PNL_W - 8.0f, 44.0f, SEL_HL);

    //--------------------------------------------------------------------------
    // DirectWrite テキスト
    //--------------------------------------------------------------------------
    const D2D1_COLOR_F dCYAN  = D2D1::ColorF(0.2f,  0.72f, 1.0f,  1.0f);
    const D2D1_COLOR_F dWHITE = D2D1::ColorF(1.0f,  1.0f,  1.0f,  1.0f);
    const D2D1_COLOR_F dGRAY  = D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
    const D2D1_COLOR_F dAMBER = D2D1::ColorF(1.0f,  0.75f, 0.2f,  1.0f);

    char volPct[8];   snprintf(volPct,  sizeof(volPct),  "%d%%", static_cast<int>(roundf(g_Volume * 100.0f)));
    char sensStr[8];  snprintf(sensStr, sizeof(sensStr), "%d",   sensStep);
    const bool invertY = Player_Camera_GetMouseInvertY();
    const bool fs      = GameWindow_IsFullscreen();

    const float cx = PNL_X + PNL_W * 0.5f;

    // ラベル（右揃え）
    g_pDW_Label->SetScale(scaleX, scaleY);
    g_pDW_Label->BeginBatch();
    g_pDW_Label->DrawAt(std::wstring(L"ボリューム"),    LBL_CX, ROW_Y0, LBL_HW, (g_CursorItem == 0) ? dCYAN : dWHITE, 1.5f);
    g_pDW_Label->DrawAt(std::wstring(L"感度"),          LBL_CX, ROW_Y1, LBL_HW, (g_CursorItem == 1) ? dCYAN : dWHITE, 1.5f);
    g_pDW_Label->DrawAt(std::wstring(L"Y軸反転"),       LBL_CX, ROW_Y2, LBL_HW, (g_CursorItem == 2) ? dCYAN : dWHITE, 1.5f);
    g_pDW_Label->DrawAt(std::wstring(L"フルスクリーン"), LBL_CX, ROW_Y3, LBL_HW, (g_CursorItem == 3) ? dCYAN : dWHITE, 1.5f);
    g_pDW_Label->DrawAt(std::wstring(L"シャドウ"),       LBL_CX, ROW_Y4, LBL_HW, (g_CursorItem == 4) ? dCYAN : dWHITE, 1.5f);
    g_pDW_Label->EndBatch();
    g_pDW_Label->SetScale(1.0f, 1.0f);

    // パネルヘッダー・値・フッター（中央揃え）
    g_pDWBody->SetScale(scaleX, scaleY);
    g_pDWBody->BeginBatch();
    g_pDWBody->DrawAt(std::wstring(L"オプション"), cx, PNL_Y + 30.0f, 160.0f, dAMBER, 1.5f);
    g_pDWBody->DrawAt(volPct,                VAL_CX, ROW_Y0, VAL_HW, (g_CursorItem == 0) ? dCYAN : dGRAY, 1.5f);
    g_pDWBody->DrawAt(sensStr,               VAL_CX, ROW_Y1, VAL_HW, (g_CursorItem == 1) ? dCYAN : dGRAY, 1.5f);
    g_pDWBody->DrawAt(invertY ? "ON" : "OFF",  VAL_CX, ROW_Y2, VAL_HW, (g_CursorItem == 2) ? dCYAN : dGRAY, 1.5f);
    g_pDWBody->DrawAt(fs ? "ON" : "OFF",       VAL_CX, ROW_Y3, VAL_HW, (g_CursorItem == 3) ? dCYAN : dGRAY, 1.5f);
    {
        static const wchar_t* shadowLabels[] = { L"なし", L"低", L"中", L"高" };
        g_pDWBody->DrawAt(std::wstring(shadowLabels[g_ShadowMode % 4]), VAL_CX, ROW_Y4, VAL_HW + 60.0f, (g_CursorItem == 4) ? dCYAN : dGRAY, 1.5f);
    }
    g_pDWBody->DrawAt(std::wstring(L"ENTER / B : 戻る"), cx, PNL_Y + PNL_H - 25.0f, 200.0f, dGRAY, 1.2f);
    g_pDWBody->EndBatch();
    g_pDWBody->SetScale(1.0f, 1.0f);
}

//==============================================================================
// 終了判定
//==============================================================================
bool Option_IsEnd()
{
    if (g_End)
    {
        g_End = false;
        return true;
    }
    return false;
}

//==============================================================================
// シャドウモード
//==============================================================================
int Option_GetShadowMode()
{
    return g_ShadowMode;
}

void Option_SetShadowMode(int mode)
{
    if (mode < 0) mode = 0;
    if (mode > 3) mode = 3;
    g_ShadowMode = mode;
}
