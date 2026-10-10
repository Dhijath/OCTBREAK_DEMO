/*==============================================================================

   ポーズメニュー [Pause.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
   ■メインメニュー（PauseState::Main）
     0: RESUME   – ゲーム再開
     1: OPTION   – オプションサブメニューへ
     2: ABORT    – 作戦を中止してタイトルへ戻る

   ■オプションサブメニュー（PauseState::Option）
     0: VOLUME      – LEFT/RIGHT で 0.1 刻み
     1: SENS        – LEFT/RIGHT で 1〜20 段階（横縦統一）
     2: Y軸反転     – LEFT/RIGHT でトグル
     ESC / PAD_B   – メインメニューへ戻る

==============================================================================*/
#include "Pause.h"
#include "sprite.h"
#include "audio.h"
#include "texture.h"
#include "direct3d.h"
#include "UIInput.h"
#include "text_logo.h"
#include "DirectWrite.h"
#include "player_camera.h"
#include "SaveData.h"
#include "SciFiUI.h"
#include <DirectXMath.h>
#include <d2d1helper.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DirectX;

namespace
{
    //==========================================================================
    // ステート
    //==========================================================================
    enum class PauseState { Main, Option };

    static PauseState g_State = PauseState::Main;
    static int        g_Cursor = 0;   // Mainは0-2、Optionは0-2
    static float      g_Time = 0.0f;

    //==========================================================================
    // リソース
    //==========================================================================
    static int g_TexWhite = -1;

    static DirectWrite* g_pDW = nullptr;  // オプションパネル用テキスト（中央揃え）
    static DirectWrite* g_pDW_Label = nullptr;  // ラベル専用（右揃え）

    //==========================================================================
    // オプション値
    //==========================================================================
    static float g_Volume = 0.5f;

    //==========================================================================
    // 入力立ち上がり
    //==========================================================================
    static bool g_PrevUp = false;
    static bool g_PrevDown = false;
    static bool g_PrevLeft = false;
    static bool g_PrevRight = false;
    static bool g_PrevEnter = false;
    static bool g_PrevEsc = false;

    //==========================================================================
    // SE
    //==========================================================================
    static int g_SeCursorMove = -1;
    static int g_SeSelect = -1;
    static int g_SeChange = -1;
    static int g_SeCancel = -1;

    //==========================================================================
    // 定数
    //==========================================================================
    static constexpr int MAIN_COUNT = 3;   // RESUME / OPTION / TITLE
    static constexpr int OPTION_COUNT = 4;   // ボリューム / マウス感度 / パッド感度 / Y軸反転

    static constexpr float SENS_STEP = 0.0025f;   // マウス感度1段階
    static constexpr int   SENS_MIN = 1;
    static constexpr int   SENS_MAX = 20;

    // ゲームパッド感度（右スティック倍率。step*0.1 = 倍率 0.1〜2.0）
    static constexpr float PAD_SENS_STEP = 0.1f;
    static constexpr int   PAD_SENS_MIN  = 1;
    static constexpr int   PAD_SENS_MAX  = 20;

    // オプションパネル（仮想1600×900空間）
    static constexpr float OPT_PNL_W = 700.0f;
    static constexpr float OPT_PNL_H = 385.0f;  // 4行
    static constexpr float OPT_PNL_X = (1600.0f - OPT_PNL_W) * 0.5f;
    static constexpr float OPT_PNL_Y = 235.0f;
    static constexpr float OPT_ROW_Y0 = OPT_PNL_Y + 78.0f;    // ボリューム
    static constexpr float OPT_ROW_Y1 = OPT_PNL_Y + 143.0f;   // マウス感度
    static constexpr float OPT_ROW_Y2 = OPT_PNL_Y + 208.0f;   // パッド感度
    static constexpr float OPT_ROW_Y3 = OPT_PNL_Y + 273.0f;   // Y軸反転
    static constexpr float OPT_BAR_X = OPT_PNL_X + 310.0f;
    static constexpr float OPT_BAR_W = 300.0f;
    static constexpr float OPT_BAR_H = 16.0f;
    static constexpr float OPT_ROW_YS[OPTION_COUNT] = { OPT_ROW_Y0, OPT_ROW_Y1, OPT_ROW_Y2, OPT_ROW_Y3 };

    // オプションの「戻る」ボタン（パネル下端の右側。マウスで押せる）
    static constexpr float OPT_BACK_W = 150.0f, OPT_BACK_H = 28.0f;
    static constexpr float OPT_BACK_X = OPT_PNL_X + OPT_PNL_W - OPT_BACK_W - 24.0f;
    static constexpr float OPT_BACK_Y = OPT_PNL_Y + OPT_PNL_H - 40.0f;

    // メインメニューの項目（仮想1600×900空間）
    static constexpr float MAIN_ITEM_W = 520.0f, MAIN_ITEM_H = 70.0f, MAIN_ITEM_GAP = 18.0f;
    static constexpr float MAIN_ITEM_X = (1600.0f - MAIN_ITEM_W) * 0.5f;
    static constexpr float MAIN_ITEM_Y0 = 300.0f;

    static int g_DragItem = -1;   // マウスでゲージをドラッグ中のオプション項目（-1 = なし）

    bool MouseOnMainItem(int i)
    {
        return UI_IsMouseIn(MAIN_ITEM_X, MAIN_ITEM_Y0 + i * (MAIN_ITEM_H + MAIN_ITEM_GAP), MAIN_ITEM_W, MAIN_ITEM_H);
    }

    bool MouseOnOptionRow(int i)
    {
        return UI_IsMouseIn(OPT_PNL_X + 12.0f, OPT_ROW_YS[i] - 24.0f, OPT_PNL_W - 24.0f, 48.0f);
    }

    // ゲージ上のカーソル位置 → 段階（左端より左は minStep）
    int BarStepAtMouse(int maxStep, int minStep)
    {
        float mx, my;
        if (!UI_GetMousePos(&mx, &my)) return minStep;
        const int s = static_cast<int>(floorf((mx - OPT_BAR_X) / OPT_BAR_W * maxStep)) + 1;
        return std::max(minStep, std::min(maxStep, s));
    }
}

//==============================================================================
// 初期化
//==============================================================================
void Pause_Initialize()
{
    g_State = PauseState::Main;
    g_Cursor = 0;
    g_Time = 0.0f;

    g_PrevUp = false;
    g_PrevDown = false;
    g_PrevLeft = false;
    g_PrevRight = false;
    g_PrevEnter = false;
    g_PrevEsc = false;

    // リソース読み込みのみここで行う（入力状態は Pause_Open でリセット）

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect < 0) g_SeSelect = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeChange < 0) g_SeChange = LoadAudio("resource/Sound/ui_tab_switch.wav");
    if (g_SeCancel < 0) g_SeCancel = LoadAudio("resource/Sound/ui_cancel.wav");

    if (g_TexWhite < 0)
        g_TexWhite = Texture_Load(L"resource/texture/white.png");

    if (!g_pDW)
    {
        static FontData fd;
        fd.font = Font::Arial;
        fd.fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        fd.fontSize = 22.0f;
        fd.localeName = L"ja-jp";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDW = new DirectWrite(&fd);
        g_pDW->Init();
    }
    if (!g_pDW_Label)
    {
        static FontData fdL;
        fdL.font = Font::Arial;
        fdL.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
        fdL.fontSize = 22.0f;
        fdL.localeName = L"ja-jp";
        fdL.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;  // 右揃え
        fdL.Color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDW_Label = new DirectWrite(&fdL);
        g_pDW_Label->Init();
    }

    // SaveData_Load() が先に SetMasterVolume() を設定済みなので、それを読む
    g_Volume = GetMasterVolume();

    // 横縦感度を統一
    Player_Camera_SetMouseSensitivityPitch(Player_Camera_GetMouseSensitivity());
}

//==============================================================================
// ポーズを開く時に呼ぶ（入力状態をリセットして誤検知を防ぐ）
//==============================================================================
void Pause_Open()
{
    g_State  = PauseState::Main;
    g_Cursor = 0;
    g_Time   = 0.0f;

    // 現在の入力状態を「押されている」として記録
    // → 次フレームで離すまでトリガーが発生しない
    g_PrevUp    = UI_IsMoveUpHeld();
    g_PrevDown  = UI_IsMoveDownHeld();
    g_PrevLeft  = UI_IsMoveLeftHeld();
    g_PrevRight = UI_IsMoveRightHeld();
    g_PrevEnter = UI_IsConfirmHeld();
    g_PrevEsc   = UI_IsCancelHeld();   // ESC が押されたまま → 即リジューム防止
    g_DragItem  = -1;
}

//==============================================================================
// 更新
//==============================================================================
PauseResult Pause_Update()
{
    g_Time += 1.0f / 60.0f;

    //------------------------------------------------------------------
    // 入力取得（立ち上がり）
    //------------------------------------------------------------------
    const bool nowUp    = UI_IsMoveUpHeld();
    const bool nowDown  = UI_IsMoveDownHeld();
    const bool nowLeft  = UI_IsMoveLeftHeld();
    const bool nowRight = UI_IsMoveRightHeld();
    const bool nowEnter = UI_IsConfirmHeld();
    const bool nowEsc   = UI_IsCancelHeld();

    const bool trigUp = nowUp && !g_PrevUp;
    const bool trigDown = nowDown && !g_PrevDown;
    bool trigLeft = nowLeft && !g_PrevLeft;
    bool trigRight = nowRight && !g_PrevRight;
    const bool trigEnter = nowEnter && !g_PrevEnter;   // マウスは項目上のクリックだけ（下で判定）
    // ※ 右クリックではリジュームしない（右ボタンは R-ARM。押したまま戦闘に戻ると即発射するため）
    const bool trigEsc = nowEsc && !g_PrevEsc;

    g_PrevUp = nowUp;
    g_PrevDown = nowDown;
    g_PrevLeft = nowLeft;
    g_PrevRight = nowRight;
    g_PrevEnter = nowEnter;
    g_PrevEsc = nowEsc;

    //==================================================================
    // PauseState::Option  – サブメニュー
    //==================================================================
    if (g_State == PauseState::Option)
    {
        // カーソル上下
        if (trigUp) { g_Cursor = (g_Cursor - 1 + OPTION_COUNT) % OPTION_COUNT; PlayAudio(g_SeCursorMove, false); }
        if (trigDown) { g_Cursor = (g_Cursor + 1) % OPTION_COUNT; PlayAudio(g_SeCursorMove, false); }

        // マウス：ホバーで項目選択
        int hover = -1;
        for (int i = 0; i < OPTION_COUNT; ++i)
            if (MouseOnOptionRow(i)) { hover = i; break; }
        if (hover >= 0 && hover != g_Cursor && g_DragItem < 0
            && (UI_IsMouseMoved() || UI_IsMouseLeftTrig()))
        {
            g_Cursor = hover;
            PlayAudio(g_SeCursorMove, false);
        }

        // マウス：ゲージはクリック・ドラッグで値を合わせる、スイッチは行のクリックで切り替え
        if (UI_IsMouseLeftTrig() && hover >= 0)
        {
            const float y = OPT_ROW_YS[hover];
            if (hover <= 2)
            {
                if (UI_IsMouseIn(OPT_BAR_X - 24.0f, y - 18.0f, OPT_BAR_W + 48.0f, 36.0f)) g_DragItem = hover;
            }
            else
            {
                Player_Camera_SetMouseInvertY(!Player_Camera_GetMouseInvertY());
                PlayAudio(g_SeChange, false);
            }
        }
        if (g_DragItem >= 0)
        {
            bool changed = false;
            if (!UI_IsMouseLeftHeld())
            {
                g_DragItem = -1;
            }
            else if (g_DragItem == 0)
            {
                const float v = BarStepAtMouse(10, 0) * 0.1f;
                if (fabsf(v - g_Volume) > 0.001f) { g_Volume = v; SetMasterVolume(g_Volume); changed = true; }
            }
            else if (g_DragItem == 1)
            {
                const int s = BarStepAtMouse(SENS_MAX, SENS_MIN);
                if (s != static_cast<int>(roundf(Player_Camera_GetMouseSensitivity() / SENS_STEP)))
                {
                    Player_Camera_SetMouseSensitivity(s * SENS_STEP);
                    Player_Camera_SetMouseSensitivityPitch(s * SENS_STEP);
                    changed = true;
                }
            }
            else
            {
                const int s = BarStepAtMouse(PAD_SENS_MAX, PAD_SENS_MIN);
                if (s != static_cast<int>(roundf(Player_Camera_GetPadSensitivity() / PAD_SENS_STEP)))
                {
                    Player_Camera_SetPadSensitivity(s * PAD_SENS_STEP);
                    changed = true;
                }
            }
            if (changed) PlayAudio(g_SeChange, false);
        }

        // カーソル下の項目はホイールでも増減できる
        const int wheel = (hover == g_Cursor) ? UI_GetMouseWheel() : 0;
        if (wheel < 0) trigLeft  = true;
        if (wheel > 0) trigRight = true;

        // 値変更
        if (g_Cursor == 0) // ボリューム
        {
            if (trigLeft) { g_Volume = std::max(0.0f, g_Volume - 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeChange, false); }
            if (trigRight) { g_Volume = std::min(1.0f, g_Volume + 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeChange, false); }
        }
        else if (g_Cursor == 1) // マウス感度（横縦統一）
        {
            int step = static_cast<int>(roundf(Player_Camera_GetMouseSensitivity() / SENS_STEP));
            step = std::max(SENS_MIN, std::min(SENS_MAX, step));
            if (trigLeft && step > SENS_MIN)
            {
                --step;
                Player_Camera_SetMouseSensitivity(step * SENS_STEP);
                Player_Camera_SetMouseSensitivityPitch(step * SENS_STEP);
                PlayAudio(g_SeChange, false);
            }
            if (trigRight && step < SENS_MAX)
            {
                ++step;
                Player_Camera_SetMouseSensitivity(step * SENS_STEP);
                Player_Camera_SetMouseSensitivityPitch(step * SENS_STEP);
                PlayAudio(g_SeChange, false);
            }
        }
        else if (g_Cursor == 2) // ゲームパッド感度
        {
            int step = static_cast<int>(roundf(Player_Camera_GetPadSensitivity() / PAD_SENS_STEP));
            step = std::max(PAD_SENS_MIN, std::min(PAD_SENS_MAX, step));
            if (trigLeft && step > PAD_SENS_MIN)
            {
                --step;
                Player_Camera_SetPadSensitivity(step * PAD_SENS_STEP);
                PlayAudio(g_SeChange, false);
            }
            if (trigRight && step < PAD_SENS_MAX)
            {
                ++step;
                Player_Camera_SetPadSensitivity(step * PAD_SENS_STEP);
                PlayAudio(g_SeChange, false);
            }
        }
        else // Y軸反転（cursor == 3）
        {
            if (trigLeft || trigRight) { Player_Camera_SetMouseInvertY(!Player_Camera_GetMouseInvertY()); PlayAudio(g_SeChange, false); }
        }

        // 戻る（ESC / PAD_B / 右クリック / 戻るボタン）
        const bool trigBack = UI_IsCancel() || UI_IsClickIn(OPT_BACK_X, OPT_BACK_Y, OPT_BACK_W, OPT_BACK_H);
        if (trigBack)
        {
            SaveData_Save();            // 設定を config.ini に書き込む
            g_State = PauseState::Main;
            g_Cursor = 1; // OPTION に戻る
            PlayAudio(g_SeCancel, false);
        }

        return PauseResult::None;
    }

    //==================================================================
    // PauseState::Main  – メインメニュー
    //==================================================================

    // ESC / B → 即リジューム（閉じる効果音を鳴らす）
    if (trigEsc) { PlayAudio(g_SeCancel, false); return PauseResult::Resume; }

    // カーソル上下
    if (trigUp) { g_Cursor = (g_Cursor - 1 + MAIN_COUNT) % MAIN_COUNT; PlayAudio(g_SeCursorMove, false); }
    if (trigDown) { g_Cursor = (g_Cursor + 1) % MAIN_COUNT; PlayAudio(g_SeCursorMove, false); }

    // マウス：ホバーで選択、項目クリックで決定
    bool clicked = false;
    for (int i = 0; i < MAIN_COUNT; ++i)
    {
        if (!MouseOnMainItem(i)) continue;
        if (i != g_Cursor && UI_IsMouseMoved()) { g_Cursor = i; PlayAudio(g_SeCursorMove, false); }
        if (UI_IsMouseLeftTrig()) { g_Cursor = i; clicked = true; }
        break;
    }

    // 決定
    if (trigEnter || clicked)
    {
        PlayAudio(g_SeSelect, false);
        switch (g_Cursor)
        {
        case 0: return PauseResult::Resume;
        case 1:
            g_State = PauseState::Option;
            g_Cursor = 0;
            return PauseResult::None;
        case 2: return PauseResult::GoTitle;
        }
    }

    return PauseResult::None;
}

//==============================================================================
// 描画（SF調。SciFiUI で描く）
//   メイン     : 画面中央の縦メニュー（RESUME / OPTION / ABORT）
//   オプション : 4行の設定パネル（区切りゲージ＋値）
//==============================================================================
void Pause_Draw()
{
    using namespace SciFiUI;

    const float W = static_cast<float>(SPRITE_SCREEN_W);
    const float H = static_cast<float>(SPRITE_SCREEN_H);

    BeginSprites();

    // 暗幕＋走査線＋画面四隅の枠
    // 暗幕＋走査線。見出し類は中央の列にまとめる（背後のHUDパネルと重ねない）
    Fill(0.0f, 0.0f, W, H, { 0.0f, 0.01f, 0.03f, 0.74f });
    Scanlines(0.0f, 0.0f, W, H, 4.0f, 0.04f);
    const float headY = (g_State == PauseState::Option) ? OPT_PNL_Y - 40.0f : 216.0f;
    if (g_State == PauseState::Main)
        HazardTape(W * 0.5f - 300.0f, 240.0f, 600.0f, 4.0f, g_Time * 60.0f, WithAlpha(kAmber, 0.5f));

    Text(L"SYS://PAUSE", W * 0.5f - 300.0f, headY, 13.0f, ToD2D(kCyan, 0.85f), UIFont::Mono, UIAlign::Left, true);
    Text(L"OPERATION SUSPENDED", W * 0.5f + 300.0f, headY, 13.0f, ToD2D(kAmber, 0.6f + 0.4f * sinf(g_Time * 3.0f)),
         UIFont::Mono, UIAlign::Right, true);

    //==================================================================
    // PauseState::Option – 設定パネル
    //==================================================================
    if (g_State == PauseState::Option)
    {
        auto toStep = [](float sens) -> int {
            return std::max(SENS_MIN, std::min(SENS_MAX, static_cast<int>(roundf(sens / SENS_STEP))));
        };
        auto toPadStep = [](float sens) -> int {
            return std::max(PAD_SENS_MIN, std::min(PAD_SENS_MAX, static_cast<int>(roundf(sens / PAD_SENS_STEP))));
        };
        const int   sensStep  = toStep(Player_Camera_GetMouseSensitivity());
        const float sensRatio = static_cast<float>(sensStep) / SENS_MAX;
        const int   padStep   = toPadStep(Player_Camera_GetPadSensitivity());
        const float padRatio  = static_cast<float>(padStep) / PAD_SENS_MAX;
        const bool  invertY   = Player_Camera_GetMouseInvertY();

        Panel(OPT_PNL_X, OPT_PNL_Y, OPT_PNL_W, OPT_PNL_H, kPanel, WithAlpha(kCyan, 0.55f), 18.0f);
        Brackets(OPT_PNL_X - 5.0f, OPT_PNL_Y - 5.0f, OPT_PNL_W + 10.0f, OPT_PNL_H + 10.0f, 14.0f, WithAlpha(kCyan, 0.85f));
        Fill(OPT_PNL_X + 1.0f, OPT_PNL_Y + 44.0f, OPT_PNL_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.3f));

        const float* rowYs = OPT_ROW_YS;
        const float ratios[3] = { g_Volume, sensRatio, padRatio };

        // 戻るボタン（マウスが乗ったら枠を出す）
        const bool backHover = UI_IsMouseIn(OPT_BACK_X, OPT_BACK_Y, OPT_BACK_W, OPT_BACK_H);
        if (backHover)
        {
            Fill(OPT_BACK_X, OPT_BACK_Y, OPT_BACK_W, OPT_BACK_H, WithAlpha(kCyan, 0.16f));
            Brackets(OPT_BACK_X - 3.0f, OPT_BACK_Y - 3.0f, OPT_BACK_W + 6.0f, OPT_BACK_H + 6.0f, 8.0f, kCyan, 1.0f);
        }
        for (int i = 0; i < OPTION_COUNT; ++i)
        {
            const bool sel = (i == g_Cursor);
            const float y = rowYs[i];
            if (sel)
            {
                Fill(OPT_PNL_X + 12.0f, y - 24.0f, OPT_PNL_W - 24.0f, 48.0f, WithAlpha(kCyan, 0.12f));
                Fill(OPT_PNL_X + 12.0f, y - 24.0f, 3.0f, 48.0f, kCyan);
            }
            if (i < 3)
            {
                SegmentBar(OPT_BAR_X, y - OPT_BAR_H * 0.5f, OPT_BAR_W, OPT_BAR_H, 20, ratios[i],
                           WithAlpha(sel ? kCyan : kCyanDim, sel ? 0.95f : 0.8f), WithAlpha(kCyan, 0.10f));
            }
            else
            {
                // ON / OFF の切り替えスイッチ
                Frame(OPT_BAR_X, y - 13.0f, 120.0f, 26.0f, WithAlpha(kCyan, sel ? 0.9f : 0.5f), 1.0f);
                Fill(invertY ? OPT_BAR_X + 62.0f : OPT_BAR_X + 2.0f, y - 11.0f, 56.0f, 22.0f,
                     WithAlpha(invertY ? kGreen : kCyanDim, sel ? 0.8f : 0.5f));
            }
        }

        static const wchar_t* LABELS[OPTION_COUNT] = { L"ボリューム", L"マウス感度", L"パッド感度", L"Y軸反転" };
        static const wchar_t* CODES[OPTION_COUNT]  = { L"VOLUME", L"MOUSE SENS", L"PAD SENS", L"INVERT Y" };
        wchar_t vals[OPTION_COUNT][16];
        swprintf_s(vals[0], L"%d%%", static_cast<int>(roundf(g_Volume * 100.0f)));
        swprintf_s(vals[1], L"%d", sensStep);
        swprintf_s(vals[2], L"%d", padStep);
        swprintf_s(vals[3], L"%s", invertY ? L"ON" : L"OFF");

        Text(L"SETTINGS", OPT_PNL_X + 22.0f, OPT_PNL_Y + 12.0f, 20.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
        Text(L"オプション", OPT_PNL_X + OPT_PNL_W - 26.0f, OPT_PNL_Y + 12.0f, 18.0f, ToD2D(kCyan, 0.7f),
             UIFont::Body, UIAlign::Right);
        for (int i = 0; i < OPTION_COUNT; ++i)
        {
            const bool sel = (i == g_Cursor);
            const float y = rowYs[i];
            Text(CODES[i], OPT_PNL_X + 30.0f, y - 21.0f, 12.0f, ToD2D(kCyan, sel ? 0.9f : 0.5f), UIFont::Mono, UIAlign::Left, true);
            Text(LABELS[i], OPT_PNL_X + 30.0f, y - 5.0f, 19.0f,
                 sel ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f) : D2D1::ColorF(0.75f, 0.82f, 0.9f, 1.0f),
                 UIFont::Body, UIAlign::Left, sel);
            Text(vals[i], OPT_PNL_X + OPT_PNL_W - 30.0f, y - 16.0f, 26.0f, ToD2D(sel ? kCyan : kCyanDim, 1.0f),
                 UIFont::Display, UIAlign::Right, true);
        }
        Text(L"LEFT / RIGHT : CHANGE", OPT_PNL_X + 30.0f, OPT_PNL_Y + OPT_PNL_H - 34.0f, 13.0f,
             ToD2D(kCyan, 0.6f), UIFont::Mono, UIAlign::Left, true);
        Text(L"ESC / B : BACK", OPT_BACK_X + OPT_BACK_W * 0.5f, OPT_PNL_Y + OPT_PNL_H - 34.0f, 13.0f,
             ToD2D(kCyan, backHover ? 1.0f : 0.6f), UIFont::Mono, UIAlign::Center, true);

        FlushText();
        return;
    }

    //==================================================================
    // PauseState::Main – メインメニュー
    //==================================================================
    constexpr float ITEM_W = MAIN_ITEM_W, ITEM_H = MAIN_ITEM_H, ITEM_GAP = MAIN_ITEM_GAP;
    const float itemX  = MAIN_ITEM_X;
    const float itemY0 = MAIN_ITEM_Y0;

    Fill(itemX - 40.0f, 196.0f, ITEM_W + 80.0f, 1.0f, WithAlpha(kCyan, 0.4f));
    Ticks(itemX - 40.0f, 198.0f, ITEM_W + 80.0f, 40, 5, WithAlpha(kCyan, 0.35f));

    struct Item { const wchar_t* label; const wchar_t* sub; XMFLOAT4 color; };
    static const Item items[MAIN_COUNT] =
    {
        { L"RESUME", L"作戦を再開する",               kCyan },
        { L"OPTION", L"音量・感度の設定",             kCyan },
        { L"ABORT",  L"作戦を中止してタイトルへ",     kRed  },
    };

    for (int i = 0; i < MAIN_COUNT; ++i)
    {
        const bool sel = (i == g_Cursor);
        const float y = itemY0 + i * (ITEM_H + ITEM_GAP);
        const XMFLOAT4& c = items[i].color;
        if (sel)
        {
            const float e = 4.0f + 2.0f * sinf(g_Time * 6.0f);
            Panel(itemX, y, ITEM_W, ITEM_H, kPanelHi, c, 14.0f);
            Brackets(itemX - e, y - e, ITEM_W + e * 2.0f, ITEM_H + e * 2.0f, 12.0f, c, 2.0f);
            Fill(itemX + 1.0f, y + 1.0f, 4.0f, ITEM_H - 2.0f, c);
            Diamond(itemX - 26.0f, y + ITEM_H * 0.5f, 6.0f, c);
        }
        else
        {
            Panel(itemX, y, ITEM_W, ITEM_H, kPanel, WithAlpha(c, 0.35f), 14.0f);
        }
    }

    Text(L"PAUSE", W * 0.5f, 112.0f, 64.0f, ToD2D(kCyan), UIFont::Display, UIAlign::Center, true, 1.5f);
    for (int i = 0; i < MAIN_COUNT; ++i)
    {
        const bool sel = (i == g_Cursor);
        const float y = itemY0 + i * (ITEM_H + ITEM_GAP);
        wchar_t num[8];
        swprintf_s(num, L"%02d", i + 1);
        Text(num, itemX + 22.0f, y + 24.0f, 16.0f, ToD2D(items[i].color, sel ? 0.9f : 0.4f), UIFont::Mono, UIAlign::Left, true);
        Text(items[i].label, itemX + 64.0f, y + 12.0f, 36.0f, ToD2D(items[i].color, sel ? 1.0f : 0.6f),
             UIFont::Display, UIAlign::Left, true);
        Text(items[i].sub, itemX + ITEM_W - 20.0f, y + 26.0f, 16.0f,
             D2D1::ColorF(0.85f, 0.9f, 0.95f, sel ? 1.0f : 0.5f), UIFont::Body, UIAlign::Right);
    }

    FlushText();
}
