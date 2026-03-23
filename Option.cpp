/*==============================================================================

   オプション画面 [Option.cpp]
   Author : 51106
   Date   : 2026/03/23
--------------------------------------------------------------------------------
   DirectWrite（Arial）で描画するオプション画面。
   フルスクリーン切替はオプション画面からのみ行う（F11/Alt+Enter は削除済み）。

   ■項目
     0: VOLUME     – LEFT / RIGHT で 0.1 刻み増減
     1: FULLSCREEN – LEFT / RIGHT でウィンドウ ↔ ボーダーレス切替

   ■操作
     UP / DOWN (W/S / 十字↑↓)  : 項目選択
     LEFT / RIGHT (A/D / 十字←→): 値変更
     ENTER / PAD_B              : タイトルへ戻る

   ■描画座標（仮想 1600×900 空間 ─ DirectWrite::SetScale で実解像度にマップ）
     タイトル "OPTIONS" : cx=800, cy=78
     VOLUME 行          : ラベル cx=350 / 値 cx=1200 / y=237
     FULLSCREEN 行      : ラベル cx=380 / 値 cx=1200 / y=300
     フッター           : cx=800, cy=660
     パネル             : x=100, y=205, w=1400, h=215
     ボリュームバー     : x=650, y=237, w=400

==============================================================================*/

#include "Option.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "pad_logger.h"
#include "Audio.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "game_window.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cstdio>
#include <cmath>

using namespace DirectX;

namespace
{
    //==========================================================================
    // リソース
    //==========================================================================
    int g_BgTexID    = -1;
    int g_WhiteTexID = -1;

    DirectWrite* g_pDWTitle = nullptr;  // タイトル "OPTIONS"（Arial Bold 30pt）
    DirectWrite* g_pDWBody  = nullptr;  // 項目ラベル・値・フッター（Arial 22pt）

    int g_SeCursorMove = -1;
    int g_SeTabSwitch  = -1;
    int g_SeCancel     = -1;

    //==========================================================================
    // 状態
    //==========================================================================
    int    g_CursorItem        = 0;
    bool   g_End               = false;
    double g_Time              = 0.0;

    float  g_Volume            = 0.5f;
    bool   g_VolumeInitialized = false;
}

//------------------------------------------------------------------------------
// レイアウト定数（仮想 1600×900 空間）
//------------------------------------------------------------------------------
static constexpr float PNL_X     = 100.0f;
static constexpr float PNL_Y     = 205.0f;
static constexpr float PNL_W     = 1400.0f;
static constexpr float PNL_H     = 215.0f;

static constexpr float BAR_X     = 650.0f;
static constexpr float BAR_Y     = 237.0f;
static constexpr float BAR_W_MAX = 400.0f;
static constexpr float BAR_H     = 18.0f;

//==============================================================================
// 初期化
//==============================================================================
void Option_Initialize()
{
    g_CursorItem = 0;
    g_End        = false;
    g_Time       = 0.0;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudio("resource/Sound/ui_tab_switch.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }

    g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");
    g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");

    // タイトルフォント（Arial Bold 30pt・中央揃え）
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

    // 本文フォント（Arial 22pt・中央揃え）
    if (!g_pDWBody)
    {
        static FontData fdBody;
        fdBody.font          = Font::Arial;
        fdBody.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        fdBody.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdBody.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdBody.fontSize      = 22.0f;
        fdBody.localeName    = L"en-us";
        fdBody.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdBody.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWBody = new DirectWrite(&fdBody);
        g_pDWBody->Init();
    }

    if (!g_VolumeInitialized)
    {
        g_Volume            = 0.5f;
        g_VolumeInitialized = true;
    }
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

    if (g_pDWTitle) { g_pDWTitle->Release(); delete g_pDWTitle; g_pDWTitle = nullptr; }
    if (g_pDWBody)  { g_pDWBody->Release();  delete g_pDWBody;  g_pDWBody  = nullptr; }

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
    const bool moveUp   = KeyLogger_IsTrigger(KK_UP)   || KeyLogger_IsTrigger(KK_W)
                       || PadLogger_IsTrigger(PAD_DPAD_UP);
    const bool moveDown = KeyLogger_IsTrigger(KK_DOWN)  || KeyLogger_IsTrigger(KK_S)
                       || PadLogger_IsTrigger(PAD_DPAD_DOWN);

    if (moveUp || moveDown)
    {
        g_CursorItem = (g_CursorItem + 1) % 2;
        PlayAudio(g_SeCursorMove, false);
    }

    // ── 値変更 ────────────────────────────────────────────────────────────
    const bool goLeft  = KeyLogger_IsTrigger(KK_LEFT)  || KeyLogger_IsTrigger(KK_A)
                      || PadLogger_IsTrigger(PAD_DPAD_LEFT);
    const bool goRight = KeyLogger_IsTrigger(KK_RIGHT) || KeyLogger_IsTrigger(KK_D)
                      || PadLogger_IsTrigger(PAD_DPAD_RIGHT);

    if (g_CursorItem == 0) // VOLUME
    {
        if (goLeft)
        {
            g_Volume = std::max(0.0f, g_Volume - 0.1f);
            SetMasterVolume(g_Volume);
            PlayAudio(g_SeTabSwitch, false);
        }
        if (goRight)
        {
            g_Volume = std::min(1.0f, g_Volume + 0.1f);
            SetMasterVolume(g_Volume);
            PlayAudio(g_SeTabSwitch, false);
        }
    }
    else // FULLSCREEN
    {
        if (goLeft || goRight)
        {
            GameWindow_RequestFullscreenToggle();   // 次フレーム先頭で安全に実行
            PlayAudio(g_SeTabSwitch, false);
        }
    }

    // ── 戻る ─────────────────────────────────────────────────────────────
    if (KeyLogger_IsTrigger(KK_ENTER) || PadLogger_IsTrigger(PAD_B))
    {
        PlayAudio(g_SeCancel, false);
        g_End = true;
    }
}

//==============================================================================
// 描画処理
//==============================================================================
void Option_Draw()
{
    static const XMFLOAT4 WHITE    = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const XMFLOAT4 PANEL_BG = { 0.0f,  0.0f,  0.0f,  0.70f };
    static const XMFLOAT4 BORDER   = { 1.0f,  1.0f,  1.0f,  0.40f };
    static const XMFLOAT4 BAR_FILL = { 0.2f,  0.72f, 1.0f,  1.0f  };
    static const XMFLOAT4 BAR_MPTY = { 0.12f, 0.12f, 0.12f, 1.0f  };

    //--------------------------------------------------------------------------
    // 背景
    //--------------------------------------------------------------------------
    if (g_BgTexID >= 0)
        Sprite_Draw(g_BgTexID, 0.0f, 0.0f, 1600.0f, 900.0f, WHITE);

    if (g_WhiteTexID < 0 || !g_pDWTitle || !g_pDWBody) return;

    //--------------------------------------------------------------------------
    // パネル（枠 + 背景）
    //--------------------------------------------------------------------------
    Sprite_Draw(g_WhiteTexID,
        PNL_X - 2.0f, PNL_Y - 2.0f, PNL_W + 4.0f, PNL_H + 4.0f, BORDER);
    Sprite_Draw(g_WhiteTexID,
        PNL_X, PNL_Y, PNL_W, PNL_H, PANEL_BG);

    //--------------------------------------------------------------------------
    // VOLUME バー（Sprite）
    //--------------------------------------------------------------------------
    Sprite_Draw(g_WhiteTexID, BAR_X, BAR_Y, BAR_W_MAX, BAR_H, BAR_MPTY);
    const float filled = g_Volume * BAR_W_MAX;
    if (filled >= 1.0f)
        Sprite_Draw(g_WhiteTexID, BAR_X, BAR_Y, filled, BAR_H, BAR_FILL);

    //--------------------------------------------------------------------------
    // DirectWrite テキスト描画
    //
    // SetScale で仮想 1600×900 座標 → 実ピクセル座標に変換する。
    // BeginBatch が D3D11 RTV をアンバインドし、EndBatch が再バインドする。
    //--------------------------------------------------------------------------
    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

    const D2D1_COLOR_F dCYAN  = D2D1::ColorF(0.2f,  0.72f, 1.0f,  1.0f);
    const D2D1_COLOR_F dWHITE = D2D1::ColorF(1.0f,  1.0f,  1.0f,  1.0f);
    const D2D1_COLOR_F dGRAY  = D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);

    // ── タイトル ─────────────────────────────────────────────────────────
    g_pDWTitle->SetScale(scaleX, scaleY);
    g_pDWTitle->BeginBatch();
    g_pDWTitle->DrawAt("OPTIONS", 800.0f, 78.0f, 400.0f, dWHITE, 2.0f);
    g_pDWTitle->EndBatch();
    g_pDWTitle->SetScale(1.0f, 1.0f);

    // ── 項目ラベル・値・フッター ──────────────────────────────────────────
    const bool selVol = (g_CursorItem == 0);
    const bool selFs  = (g_CursorItem == 1);
    const bool fs     = GameWindow_IsFullscreen();

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", static_cast<int>(roundf(g_Volume * 100.0f)));

    g_pDWBody->SetScale(scaleX, scaleY);
    g_pDWBody->BeginBatch();

    // VOLUME 行（y=237）
    g_pDWBody->DrawAt(selVol ? "> VOLUME" : "  VOLUME",
                      350.0f, 237.0f, 200.0f, selVol ? dCYAN : dWHITE, 1.5f);
    g_pDWBody->DrawAt(pctStr,
                      1200.0f, 237.0f, 100.0f, selVol ? dCYAN : dGRAY, 1.5f);

    // FULLSCREEN 行（y=300）
    g_pDWBody->DrawAt(selFs ? "> FULLSCREEN" : "  FULLSCREEN",
                      380.0f, 300.0f, 230.0f, selFs ? dCYAN : dWHITE, 1.5f);
    g_pDWBody->DrawAt(fs ? "< ON  >" : "< OFF >",
                      1200.0f, 300.0f, 130.0f, selFs ? dCYAN : dGRAY, 1.5f);

    // フッター（y=660）
    g_pDWBody->DrawAt("UP/DOWN : ITEM   LEFT/RIGHT : CHANGE   ENTER / B : BACK",
                      800.0f, 660.0f, 750.0f, dGRAY, 1.5f);

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
