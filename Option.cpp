/*==============================================================================

   オプション画面 [Option.cpp]
   Author : 51106
   Date   : 2026/03/23
--------------------------------------------------------------------------------
   WeaponSelect と同スタイル（DebugText + Sprite）のオプション画面。
   フルスクリーン切替はオプション画面からのみ行う（F11/Alt+Enter は削除済み）。

   ■項目
     0: VOLUME     – LEFT / RIGHT で 0.1 刻み増減
     1: FULLSCREEN – LEFT / RIGHT でウィンドウ ↔ ボーダーレス切替

   ■操作
     UP / DOWN (W/S / 十字↑↓)  : 項目選択
     LEFT / RIGHT (A/D / 十字←→): 値変更
     ENTER / PAD_B              : タイトルへ戻る

   ■行レイアウト（lineSpacing=21, offsetY=65 の場合）
     line  0  y=  65 : "OPTIONS"
     line  8  y= 233 : VOLUME 行
     line 11  y= 296 : FULLSCREEN 行
     line 27  y= 632 : フッター

   ■列レイアウト（charSpacing=21）
     col  7 (x=147) : 項目先頭（カーソル ">" 位置）
     col 53 (x=1113): 値表示（% / ON-OFF）
     BAR_X=650, BAR_W_MAX=400 → x=650〜1050 がバー領域

==============================================================================*/

#include "Option.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "pad_logger.h"
#include "Audio.h"
#include "direct3d.h"
#include "debug_text.h"
#include "game_window.h"
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
    hal::DebugText* g_pText = nullptr;

    int g_SeCursorMove = -1;
    int g_SeTabSwitch  = -1;
    int g_SeCancel     = -1;

    //==========================================================================
    // 状態
    //==========================================================================
    int    g_CursorItem       = 0;
    bool   g_End              = false;
    double g_Time             = 0.0;

    float  g_Volume           = 0.5f;
    bool   g_VolumeInitialized = false;
}

//------------------------------------------------------------------------------
// レイアウト定数
//------------------------------------------------------------------------------
// パネル（ラベルも内側に収まる幅）
static constexpr float PNL_X  = 100.0f;
static constexpr float PNL_Y  = 205.0f;
static constexpr float PNL_W  = 1400.0f;
static constexpr float PNL_H  = 215.0f;

// line 8  → y = 65 + 8×21 = 233
// line 11 → y = 65 + 11×21 = 296
static constexpr float ITEM_Y0 = 233.0f;   // VOLUME 行 Y

// VOLUME バー（Sprite）
// col 7+4+6 = 17 → x=357 がラベル末尾
// BAR_X=650 (col~31) → バーはラベルと値の間に来る
static constexpr float BAR_X     = 650.0f;
static constexpr float BAR_W_MAX = 400.0f;
static constexpr float BAR_H     = 18.0f;
static constexpr float BAR_Y_OFF = 2.0f;

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
    delete g_pText; g_pText = nullptr;

    g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");
    g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");

    g_pText = new hal::DebugText(
        Direct3D_GetDevice(), Direct3D_GetContext(),
        L"Resource/Texture/consolab_ascii_512.png",
        SPRITE_SCREEN_W, SPRITE_SCREEN_H,
        0.0f, 65.0f,
        0, 0,
        21.0f, 21.0f
    );

    if (!g_VolumeInitialized)
    {
        g_Volume           = 0.5f;
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

    delete g_pText; g_pText = nullptr;
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
    static const XMFLOAT4 GRAY     = { 0.55f, 0.55f, 0.55f, 1.0f };
    static const XMFLOAT4 PANEL_BG = { 0.0f,  0.0f,  0.0f,  0.70f };
    static const XMFLOAT4 BORDER   = { 1.0f,  1.0f,  1.0f,  0.40f };
    static const XMFLOAT4 BAR_FILL = { 0.2f,  0.72f, 1.0f,  1.0f  };
    static const XMFLOAT4 BAR_MPTY = { 0.12f, 0.12f, 0.12f, 1.0f  };
    static const XMFLOAT4 SEL_CLR  = { 0.2f,  0.72f, 1.0f,  1.0f  };

    //--------------------------------------------------------------------------
    // 背景
    //--------------------------------------------------------------------------
    if (g_BgTexID >= 0)
        Sprite_Draw(g_BgTexID, 0.0f, 0.0f, 1600.0f, 900.0f, WHITE);

    if (g_WhiteTexID < 0 || !g_pText) return;

    //--------------------------------------------------------------------------
    // パネル（枠 + 背景）
    // x=100 〜 1500 でラベル（col7, x=147）から値（col53, x=1113）まで内包
    //--------------------------------------------------------------------------
    Sprite_Draw(g_WhiteTexID,
        PNL_X - 2.0f, PNL_Y - 2.0f, PNL_W + 4.0f, PNL_H + 4.0f, BORDER);
    Sprite_Draw(g_WhiteTexID,
        PNL_X, PNL_Y, PNL_W, PNL_H, PANEL_BG);

    //--------------------------------------------------------------------------
    // VOLUME バー（Sprite）
    // ラベル末尾 x=357（col17）とバー x=650 の間に視覚的スペースを取る
    //--------------------------------------------------------------------------
    const float barY = ITEM_Y0 + BAR_Y_OFF;
    Sprite_Draw(g_WhiteTexID, BAR_X, barY, BAR_W_MAX, BAR_H, BAR_MPTY);
    const float filled = g_Volume * BAR_W_MAX;
    if (filled >= 1.0f)
        Sprite_Draw(g_WhiteTexID, BAR_X, barY, filled, BAR_H, BAR_FILL);

    //--------------------------------------------------------------------------
    // テキスト（DebugText）
    //
    // 列レイアウト（charSpacing=21）:
    //   col  0 (x=  0) : 画面左端
    //   col  7 (x=147) : 項目開始（パネル内: PNL_X=100）
    //   col 11 (x=231) : ラベル開始（cursor=2chars 込み）
    //   col 53 (x=1113): 値列（%, ON/OFF）
    //   col 76 (x=1596): 画面右端
    //
    // VOLUME行:   [7sp][" > " or "   "][VOLUME][36sp → col53][  50%\n]
    // FULLSCREEN: [7sp]["   " or " > "][FULLSCREEN][32sp → col53][< OFF >\n]
    //--------------------------------------------------------------------------
    g_pText->Clear();

    // ── タイトル（line 0, y=65）
    // "OPTIONS" 7chars → 中央揃え: (76-7)/2 ≈ 34 スペース
    g_pText->SetText("                                  OPTIONS\n", WHITE);

    // ── 空行 ×7 → line 8 (y=233)
    g_pText->SetText("\n\n\n\n\n\n\n", WHITE);

    // ── VOLUME（line 8, y=233）
    // col 7+2=9 始まり: 7sp + cursor(2) = 9, + " " + "VOLUME" = col 16
    // → col53 まで: 53-16=37sp, その後 "%3d%%\n"
    {
        const bool sel = (g_CursorItem == 0);
        char line[128];
        int  pct = static_cast<int>(roundf(g_Volume * 100.0f));

        g_pText->SetText("       ", WHITE);                              // 7sp → col7
        g_pText->SetText(sel ? "> " : "  ", sel ? SEL_CLR : GRAY);      // cursor 2chars
        g_pText->SetText(" VOLUME", WHITE);                              // 7chars → col16

        // col16 → col53: 37sp + "100%\n" (4chars) = 41chars
        snprintf(line, sizeof(line),
            "                                     %3d%%\n", pct);       // 37sp + val
        g_pText->SetText(line, sel ? WHITE : GRAY);
    }

    // ── 空行 ×2 → line 11 (y=296)
    g_pText->SetText("\n\n", WHITE);

    // ── FULLSCREEN（line 11, y=296）
    // 7sp + cursor(2) + " FULLSCREEN" (11) = col20
    // col20 → col53: 33sp + "< ON  >" or "< OFF >"
    {
        const bool sel = (g_CursorItem == 1);
        const bool fs  = GameWindow_IsFullscreen();

        g_pText->SetText("       ", WHITE);                              // 7sp
        g_pText->SetText(sel ? "> " : "  ", sel ? SEL_CLR : GRAY);      // cursor
        g_pText->SetText(" FULLSCREEN", WHITE);                          // 11chars → col20

        // col20 → col53: 33sp
        g_pText->SetText("                                 ", WHITE);    // 33sp
        g_pText->SetText(fs ? "< ON  >" : "< OFF >", sel ? SEL_CLR : GRAY);
    }

    // ── 空行 ×16 → line 27 (y=632)
    g_pText->SetText("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", WHITE);

    // ── フッター（line 27, y=632）
    g_pText->SetText(
        "     UP/DOWN : ITEM    LEFT/RIGHT : CHANGE    ENTER / B : BACK", GRAY);

    g_pText->Draw();
    Sprite_Begin();   // DebugText 後のパイプライン再設定
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
