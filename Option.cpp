/*==============================================================================

   オプション画面 [Option.cpp]
   Author : 51106
   Date   : 2026/04/01（2026/10/03 SF調に一新）
--------------------------------------------------------------------------------
   SciFiUI で描くオプション画面（ポーズのオプションと同じ部品）。

   ■項目
     0: ボリューム      – LEFT / RIGHT で 0.1 刻み増減
     1: マウス感度      – LEFT / RIGHT で 1〜20 の 20段階（横縦統一）
     2: パッド感度      – LEFT / RIGHT で 1〜20 の 20段階
     3: Y軸反転         – LEFT / RIGHT でトグル
     4: フルスクリーン  – LEFT / RIGHT でウィンドウ ↔ ボーダーレス切替
     5: シャドウ        – LEFT / RIGHT で なし / 低 / 中 / 高

   ■操作
     UP / DOWN (W/S / 十字↑↓)  : 項目選択
     LEFT / RIGHT (A/D / 十字←→): 値変更
     ESC / PAD_B                : 保存してタイトルへ戻る

   ■レイアウト（1600×900）
     設定パネル : x=180〜1000, y=120〜800（AUDIO / CONTROL / DISPLAY の3グループ）
     詳細パネル : x=1030〜1420, y=120〜800（選択中の項目の説明と現在値）

==============================================================================*/

#include "Option.h"
#include "UIInput.h"
#include "Audio.h"
#include "SaveData.h"
#include "game_window.h"
#include "player_camera.h"
#include "SciFiUI.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <cmath>

using namespace DirectX;
using namespace SciFiUI;

namespace
{
    //==========================================================================
    // リソース
    //==========================================================================
    int g_SeCursorMove = -1;
    int g_SeTabSwitch  = -1;
    int g_SeCancel     = -1;

    //==========================================================================
    // 状態
    //==========================================================================
    static constexpr int ITEM_COUNT = 6;   // ボリューム / マウス感度 / パッド感度 / Y軸反転 / フルスクリーン / シャドウ

    int    g_CursorItem        = 0;
    bool   g_End               = false;
    double g_Time              = 0.0;
    double g_ChangeTime        = 10.0;     // 値を変えてからの時間（変更時の点滅）

    float  g_Volume            = 0.5f;
    int    g_ShadowMode        = 3;        // 0=なし / 1=低（マル影） / 2=中（ハード） / 3=高（PCF）

    //==========================================================================
    // 感度定数
    //==========================================================================
    static constexpr float SENS_STEP = 0.0025f;   // マウス感度1段階
    static constexpr int   SENS_MIN  = 1;
    static constexpr int   SENS_MAX  = 20;

    //==========================================================================
    // ゲームパッド感度定数（右スティック倍率。step*0.1 = 倍率 0.1〜2.0）
    //==========================================================================
    static constexpr float PAD_SENS_STEP = 0.1f;
    static constexpr int   PAD_SENS_MIN  = 1;
    static constexpr int   PAD_SENS_MAX  = 20;

    //==========================================================================
    // レイアウト
    //==========================================================================
    constexpr float PNL_X = 180.0f,  PNL_Y = 120.0f, PNL_W = 820.0f, PNL_H = 680.0f;
    constexpr float INF_X = 1030.0f, INF_Y = 120.0f, INF_W = 390.0f, INF_H = 680.0f;
    constexpr float ROW_H = 74.0f;
    constexpr float CTRL_X = PNL_X + 400.0f, CTRL_W = 300.0f;   // ゲージ・スイッチの位置

    // 項目ごとの表示データ
    struct ItemInfo
    {
        const wchar_t* code;
        const wchar_t* label;
        const wchar_t* group;   // 先頭の項目だけグループ名を持つ
        const wchar_t* detail;
    };
    const ItemInfo k_Items[ITEM_COUNT] =
    {
        { L"VOLUME",     L"ボリューム",     L"AUDIO",
          L"ゲーム全体の音量。\nBGMと効果音の両方に掛かる。" },
        { L"MOUSE SENS", L"マウス感度",     L"CONTROL",
          L"マウスで視点を動かす速さ。\n横と縦は同じ感度になる。" },
        { L"PAD SENS",   L"パッド感度",     nullptr,
          L"右スティックで視点を\n動かす速さ。" },
        { L"INVERT Y",   L"Y軸反転",        nullptr,
          L"上下の視点操作を反転する。\n（マウス・パッド共通）" },
        { L"FULLSCREEN", L"フルスクリーン", L"DISPLAY",
          L"ウィンドウ表示と\nボーダーレスのフルスクリーンを\n切り替える。" },
        { L"SHADOW",     L"シャドウ",       nullptr,
          L"影の描画品質。\nなし / 低（丸影）/ 中（くっきり）/\n高（なめらか）\n動作が重いときは下げる。" },
    };

    // 項目の行の上端Y（グループ見出しの分だけずらす）
    float RowY(int i)
    {
        float y = PNL_Y + 46.0f;
        for (int k = 0; k <= i; ++k)
            if (k_Items[k].group) y += 34.0f;
        return y + i * ROW_H;
    }

    int SensStep()
    {
        return std::max(SENS_MIN, std::min(SENS_MAX, static_cast<int>(roundf(Player_Camera_GetMouseSensitivity() / SENS_STEP))));
    }
    int PadStep()
    {
        return std::max(PAD_SENS_MIN, std::min(PAD_SENS_MAX, static_cast<int>(roundf(Player_Camera_GetPadSensitivity() / PAD_SENS_STEP))));
    }

    // 項目の現在値（表示用）
    std::wstring ValueText(int i)
    {
        wchar_t buf[32];
        switch (i)
        {
        case 0: swprintf_s(buf, L"%d%%", static_cast<int>(roundf(g_Volume * 100.0f))); break;
        case 1: swprintf_s(buf, L"%d", SensStep()); break;
        case 2: swprintf_s(buf, L"%d", PadStep()); break;
        case 3: swprintf_s(buf, L"%s", Player_Camera_GetMouseInvertY() ? L"ON" : L"OFF"); break;
        case 4: swprintf_s(buf, L"%s", GameWindow_IsFullscreen() ? L"ON" : L"OFF"); break;
        default:
        {
            static const wchar_t* shadow[] = { L"なし", L"低", L"中", L"高" };
            swprintf_s(buf, L"%s", shadow[g_ShadowMode % 4]);
            break;
        }
        }
        return buf;
    }
}

//==============================================================================
// 初期化
//==============================================================================
void Option_Initialize()
{
    g_CursorItem = 0;
    g_End        = false;
    g_Time       = 0.0;
    g_ChangeTime = 10.0;

    // 現在のマスター音量を表示値に反映（SaveData_Load 後の値が正）
    g_Volume = GetMasterVolume();

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudio("resource/Sound/ui_tab_switch.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

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
}

//==============================================================================
// 更新処理
//==============================================================================
void Option_Update(double elapsed_time)
{
    g_Time       += elapsed_time;
    g_ChangeTime += elapsed_time;

    // ── カーソル上下 ──────────────────────────────────────────────────────
    if (UI_IsMoveUp())   { g_CursorItem = (g_CursorItem - 1 + ITEM_COUNT) % ITEM_COUNT; PlayAudio(g_SeCursorMove, false); }
    if (UI_IsMoveDown()) { g_CursorItem = (g_CursorItem + 1) % ITEM_COUNT;              PlayAudio(g_SeCursorMove, false); }

    // ── 値変更 ────────────────────────────────────────────────────────────
    const bool goLeft  = UI_IsMoveLeft();
    const bool goRight = UI_IsMoveRight();
    if (goLeft || goRight) g_ChangeTime = 0.0;

    if (g_CursorItem == 0) // ボリューム
    {
        if (goLeft)  { g_Volume = std::max(0.0f, g_Volume - 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeTabSwitch, false); }
        if (goRight) { g_Volume = std::min(1.0f, g_Volume + 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 1) // マウス感度（横縦統一）
    {
        int step = SensStep();
        if (goLeft  && step > SENS_MIN) { --step; Player_Camera_SetMouseSensitivity(step * SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
        if (goRight && step < SENS_MAX) { ++step; Player_Camera_SetMouseSensitivity(step * SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 2) // ゲームパッド感度
    {
        int step = PadStep();
        if (goLeft  && step > PAD_SENS_MIN) { --step; Player_Camera_SetPadSensitivity(step * PAD_SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
        if (goRight && step < PAD_SENS_MAX) { ++step; Player_Camera_SetPadSensitivity(step * PAD_SENS_STEP); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 3) // Y軸反転
    {
        if (goLeft || goRight) { Player_Camera_SetMouseInvertY(!Player_Camera_GetMouseInvertY()); PlayAudio(g_SeTabSwitch, false); }
    }
    else if (g_CursorItem == 4) // フルスクリーン
    {
        if (goLeft || goRight) { GameWindow_RequestFullscreenToggle(); PlayAudio(g_SeTabSwitch, false); }
    }
    else // シャドウ（cursor == 5）：0→1→2→3→0 サイクル
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
    const float t = static_cast<float>(g_Time);
    const D2D1_COLOR_F white = D2D1::ColorF(0.93f, 0.97f, 1.0f, 1.0f);
    const D2D1_COLOR_F dim   = D2D1::ColorF(0.70f, 0.78f, 0.86f, 0.75f);

    BeginSprites();

    //--------------------------------------------------------------------------
    // 背景
    //--------------------------------------------------------------------------
    Fill(0.0f, 0.0f, 1600.0f, 900.0f, { 0.010f, 0.018f, 0.032f, 1.0f });
    Grid(0.0f, 0.0f, 1600.0f, 900.0f, 40.0f, WithAlpha(kCyan, 0.045f));
    Scanlines(0.0f, 0.0f, 1600.0f, 900.0f, 4.0f, 0.03f);
    const float sweepY = fmodf(t * 120.0f, 1000.0f) - 60.0f;
    Fill(0.0f, sweepY, 1600.0f, 50.0f, WithAlpha(kCyan, 0.03f));
    Brackets(20.0f, 20.0f, 1560.0f, 860.0f, 40.0f, WithAlpha(kCyan, 0.5f), 2.0f);
    Fill(40.0f, 96.0f, 1520.0f, 1.0f, WithAlpha(kCyan, 0.35f));
    Ticks(40.0f, 98.0f, 1520.0f, 76, 4, WithAlpha(kCyan, 0.3f));

    //--------------------------------------------------------------------------
    // 設定パネル
    //--------------------------------------------------------------------------
    Panel(PNL_X, PNL_Y, PNL_W, PNL_H, kPanel, WithAlpha(kCyan, 0.55f), 18.0f);
    Brackets(PNL_X - 5.0f, PNL_Y - 5.0f, PNL_W + 10.0f, PNL_H + 10.0f, 14.0f, WithAlpha(kCyan, 0.85f));
    Fill(PNL_X + 1.0f, PNL_Y + 34.0f, PNL_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.3f));

    const bool flashOn = g_ChangeTime < 0.25;
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        const float y   = RowY(i);
        const bool  sel = (i == g_CursorItem);
        const float cy  = y + ROW_H * 0.5f;

        // グループ見出しの線
        if (k_Items[i].group)
            Fill(PNL_X + 24.0f, y - 12.0f, PNL_W - 48.0f, 1.0f, WithAlpha(kCyan, 0.25f));

        if (sel)
        {
            Fill(PNL_X + 14.0f, y + 4.0f, PNL_W - 28.0f, ROW_H - 8.0f, WithAlpha(kCyan, flashOn ? 0.22f : 0.12f));
            Fill(PNL_X + 14.0f, y + 4.0f, 3.0f, ROW_H - 8.0f, kCyan);
            Diamond(PNL_X + 4.0f, cy, 5.0f, kCyan);
        }

        const XMFLOAT4 on  = WithAlpha(sel ? kCyan : kCyanDim, sel ? 0.95f : 0.8f);
        const XMFLOAT4 off = WithAlpha(kCyan, 0.10f);
        switch (i)
        {
        case 0: SegmentBar(CTRL_X, cy - 8.0f, CTRL_W, 16.0f, 10, g_Volume, on, off); break;
        case 1: SegmentBar(CTRL_X, cy - 8.0f, CTRL_W, 16.0f, SENS_MAX, static_cast<float>(SensStep()) / SENS_MAX, on, off); break;
        case 2: SegmentBar(CTRL_X, cy - 8.0f, CTRL_W, 16.0f, PAD_SENS_MAX, static_cast<float>(PadStep()) / PAD_SENS_MAX, on, off); break;
        case 3:
        case 4:
        {
            // OFF | ON のスイッチ
            const bool value = (i == 3) ? Player_Camera_GetMouseInvertY() : GameWindow_IsFullscreen();
            Frame(CTRL_X, cy - 14.0f, 160.0f, 28.0f, WithAlpha(kCyan, sel ? 0.9f : 0.5f), 1.0f);
            Fill(value ? CTRL_X + 82.0f : CTRL_X + 2.0f, cy - 12.0f, 76.0f, 24.0f,
                 WithAlpha(value ? kGreen : kCyanDim, sel ? 0.8f : 0.5f));
            break;
        }
        default:
        {
            // なし / 低 / 中 / 高 の4段
            for (int k = 0; k < 4; ++k)
            {
                const float bx = CTRL_X + k * 76.0f;
                const bool cur = (k == g_ShadowMode);
                Fill(bx, cy - 14.0f, 70.0f, 28.0f, WithAlpha(cur ? kCyan : kCyanDim, cur ? (sel ? 0.5f : 0.3f) : 0.08f));
                Frame(bx, cy - 14.0f, 70.0f, 28.0f, WithAlpha(kCyan, cur ? 0.9f : 0.3f), 1.0f);
            }
            break;
        }
        }
    }

    //--------------------------------------------------------------------------
    // 詳細パネル
    //--------------------------------------------------------------------------
    Panel(INF_X, INF_Y, INF_W, INF_H, kPanel, WithAlpha(kCyan, 0.45f), 16.0f);
    Fill(INF_X + 1.0f, INF_Y + 34.0f, INF_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.3f));
    Fill(INF_X + 24.0f, INF_Y + 60.0f, 3.0f, 52.0f, kCyan);
    Fill(INF_X + 24.0f, INF_Y + 330.0f, INF_W - 48.0f, 1.0f, WithAlpha(kCyan, 0.25f));
    // 飾り：回る照準
    for (int k = 0; k < 4; ++k)
    {
        const float a = t * 0.7f + k * XM_PIDIV2;
        LineAngle(INF_X + INF_W * 0.5f + cosf(a) * 70.0f, INF_Y + 500.0f + sinf(a) * 70.0f, 18.0f, a, WithAlpha(kCyan, 0.4f), 2.0f);
    }
    Diamond(INF_X + INF_W * 0.5f, INF_Y + 500.0f, 40.0f, WithAlpha(kCyan, 0.08f));

    //--------------------------------------------------------------------------
    // 文字
    //--------------------------------------------------------------------------
    Text(L"SYSTEM SETTINGS", 44.0f, 38.0f, 38.0f, ToD2D(kCyan), UIFont::Display, UIAlign::Left, true, 1.0f);
    Text(L"オプション", 330.0f, 52.0f, 18.0f, ToD2D(kCyan, 0.75f), UIFont::Body);
    Text(L"SYS://CONFIG", 1556.0f, 36.0f, 13.0f, ToD2D(kCyan, 0.55f), UIFont::Mono, UIAlign::Right);
    Text(L"CHANGES ARE SAVED ON EXIT", 1556.0f, 58.0f, 14.0f, ToD2D(kAmber, 0.6f + 0.4f * sinf(t * 2.5f)),
         UIFont::Mono, UIAlign::Right, true);

    Text(L"CONFIGURATION", PNL_X + 20.0f, PNL_Y + 9.0f, 16.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        const float y   = RowY(i);
        const bool  sel = (i == g_CursorItem);
        const float cy  = y + ROW_H * 0.5f;

        if (k_Items[i].group)
            Text(k_Items[i].group, PNL_X + 24.0f, y - 34.0f, 13.0f, ToD2D(kAmber, 0.85f), UIFont::Mono, UIAlign::Left, true);

        Text(k_Items[i].code, PNL_X + 36.0f, cy - 24.0f, 12.0f, ToD2D(kCyan, sel ? 0.9f : 0.5f), UIFont::Mono, UIAlign::Left, true);
        Text(k_Items[i].label, PNL_X + 36.0f, cy - 8.0f, 21.0f, sel ? white : dim, UIFont::Body, UIAlign::Left, sel);
        Text(ValueText(i), PNL_X + PNL_W - 30.0f, cy - 17.0f, 28.0f, ToD2D(sel ? kCyan : kCyanDim), UIFont::Display, UIAlign::Right, true);

        if (i == 3 || i == 4)
        {
            Text(L"OFF", CTRL_X + 40.0f, cy - 9.0f, 13.0f, ToD2D(kCyan, 0.8f), UIFont::Mono, UIAlign::Center, true);
            Text(L"ON",  CTRL_X + 120.0f, cy - 9.0f, 13.0f, ToD2D(kCyan, 0.8f), UIFont::Mono, UIAlign::Center, true);
        }
        else if (i == 5)
        {
            static const wchar_t* names[4] = { L"なし", L"低", L"中", L"高" };
            for (int k = 0; k < 4; ++k)
                Text(names[k], CTRL_X + k * 76.0f + 35.0f, cy - 10.0f, 15.0f,
                     k == g_ShadowMode ? white : dim, UIFont::Body, UIAlign::Center, k == g_ShadowMode);
        }
    }

    const ItemInfo& info = k_Items[g_CursorItem];
    Text(L"DETAIL", INF_X + 20.0f, INF_Y + 9.0f, 16.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
    Text(info.code, INF_X + 38.0f, INF_Y + 58.0f, 13.0f, ToD2D(kCyan, 0.8f), UIFont::Mono, UIAlign::Left, true);
    Text(info.label, INF_X + 38.0f, INF_Y + 76.0f, 26.0f, white, UIFont::Body, UIAlign::Left, true);
    Text(info.detail, INF_X + 26.0f, INF_Y + 140.0f, 17.0f, dim, UIFont::Body, UIAlign::Left);
    Text(L"CURRENT", INF_X + 26.0f, INF_Y + 346.0f, 13.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    Text(ValueText(g_CursorItem), INF_X + 26.0f, INF_Y + 364.0f, 56.0f,
         ToD2D(kCyan, flashOn ? 1.0f : 0.9f), UIFont::Display, UIAlign::Left, true, 1.0f);
    Text(L"LEFT / RIGHT : CHANGE", INF_X + INF_W * 0.5f, INF_Y + INF_H - 60.0f, 13.0f, ToD2D(kCyan, 0.55f),
         UIFont::Mono, UIAlign::Center, true);
    Text(L"ESC / B : SAVE & BACK", INF_X + INF_W * 0.5f, INF_Y + INF_H - 38.0f, 13.0f, ToD2D(kCyan, 0.55f),
         UIFont::Mono, UIAlign::Center, true);

    FlushText();
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
