/*==============================================================================

   ポーズメニュー [Pause.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/13
--------------------------------------------------------------------------------
   ■メインメニュー（PauseState::Main）
     0: RESUME   – ゲーム再開
     1: OPTION   – オプションサブメニューへ
     2: TITLE    – タイトルへ戻る

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
    static constexpr int OPTION_COUNT = 3;   // ボリューム / 感度（統一） / Y軸反転

    static constexpr float SENS_STEP = 0.0025f;
    static constexpr int   SENS_MIN = 1;
    static constexpr int   SENS_MAX = 20;

    // オプションパネル（仮想1600×900空間）
    static constexpr float OPT_PNL_W = 700.0f;
    static constexpr float OPT_PNL_H = 320.0f;  // 3行に縮小
    static constexpr float OPT_PNL_X = (1600.0f - OPT_PNL_W) * 0.5f;
    static constexpr float OPT_PNL_Y = 255.0f;
    static constexpr float OPT_ROW_Y0 = OPT_PNL_Y + 78.0f;    // ボリューム
    static constexpr float OPT_ROW_Y1 = OPT_PNL_Y + 143.0f;   // 感度（統一）
    static constexpr float OPT_ROW_Y2 = OPT_PNL_Y + 208.0f;   // Y軸反転
    static constexpr float OPT_BAR_X = OPT_PNL_X + 310.0f;
    static constexpr float OPT_BAR_W = 300.0f;
    static constexpr float OPT_BAR_H = 16.0f;
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
    const bool trigLeft = nowLeft && !g_PrevLeft;
    const bool trigRight = nowRight && !g_PrevRight;
    const bool trigEnter = (nowEnter && !g_PrevEnter) || UI_IsMouseLeftTrig();
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

        // 値変更
        if (g_Cursor == 0) // ボリューム
        {
            if (trigLeft) { g_Volume = std::max(0.0f, g_Volume - 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeChange, false); }
            if (trigRight) { g_Volume = std::min(1.0f, g_Volume + 0.1f); SetMasterVolume(g_Volume); PlayAudio(g_SeChange, false); }
        }
        else if (g_Cursor == 1) // 感度（横縦統一）
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
        else // Y軸反転（cursor == 2）
        {
            if (trigLeft || trigRight) { Player_Camera_SetMouseInvertY(!Player_Camera_GetMouseInvertY()); PlayAudio(g_SeChange, false); }
        }

        // 戻る（ESC / PAD_B）
        const bool trigBack = UI_IsCancel();
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

    // ESC → 即リジューム
    if (trigEsc) return PauseResult::Resume;

    // カーソル上下
    if (trigUp) { g_Cursor = (g_Cursor - 1 + MAIN_COUNT) % MAIN_COUNT; PlayAudio(g_SeCursorMove, false); }
    if (trigDown) { g_Cursor = (g_Cursor + 1) % MAIN_COUNT; PlayAudio(g_SeCursorMove, false); }

    // 決定
    if (trigEnter)
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
// 描画
//==============================================================================
void Pause_Draw()
{
    if (g_TexWhite < 0) return;

    const float W = static_cast<float>(SPRITE_SCREEN_W);
    const float H = static_cast<float>(SPRITE_SCREEN_H);

    Sprite_Begin();

    //------------------------------------------------------------------
    // 半透明ダークオーバーレイ
    //------------------------------------------------------------------
    Sprite_Draw(g_TexWhite, 0.0f, 0.0f, W, H, XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.55f });
    Sprite_Begin();

    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth()) / 1600.0f;
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

    //==================================================================
    // PauseState::Option – サブパネル
    //==================================================================
    if (g_State == PauseState::Option)
    {
        // パネル背景・枠
        const XMFLOAT4 BG = { 0.04f, 0.06f, 0.12f, 0.92f };
        const XMFLOAT4 BORDER = { 0.2f, 0.72f, 1.0f, 0.8f };
        const XMFLOAT4 BARFIL = { 0.2f, 0.72f, 1.0f, 1.0f };
        const XMFLOAT4 BAREMP = { 0.12f, 0.12f, 0.12f, 1.0f };

        Sprite_Draw(g_TexWhite, OPT_PNL_X - 2.0f, OPT_PNL_Y - 2.0f,
            OPT_PNL_W + 4.0f, OPT_PNL_H + 4.0f, BORDER);
        Sprite_Draw(g_TexWhite, OPT_PNL_X, OPT_PNL_Y, OPT_PNL_W, OPT_PNL_H, BG);

        // 感度ステップを計算（横を基準）
        auto toStep = [](float sens) -> int {
            return std::max(SENS_MIN, std::min(SENS_MAX,
                static_cast<int>(roundf(sens / SENS_STEP))));
            };
        const int   sensStep = toStep(Player_Camera_GetMouseSensitivity());
        const float sensRatio = static_cast<float>(sensStep - SENS_MIN) / (SENS_MAX - SENS_MIN);

        // ─ バー描画 ────────────────────────────────────────────────────
        auto drawBar = [&](float rowY, float ratio)
            {
                Sprite_Draw(g_TexWhite, OPT_BAR_X, rowY - OPT_BAR_H * 0.5f,
                    OPT_BAR_W, OPT_BAR_H, BAREMP);
                const float f = ratio * OPT_BAR_W;
                if (f >= 1.0f)
                    Sprite_Draw(g_TexWhite, OPT_BAR_X, rowY - OPT_BAR_H * 0.5f,
                        f, OPT_BAR_H, BARFIL);
            };
        drawBar(OPT_ROW_Y0, g_Volume);
        drawBar(OPT_ROW_Y1, sensRatio);
        // Y軸反転はバーなし（ON/OFFトグルのみ）

        // ─ 選択行ハイライト ───────────────────────────────────────────
        const XMFLOAT4 SEL_HL = { 0.2f, 0.72f, 1.0f, 0.15f };
        const float rowYs[OPTION_COUNT] = { OPT_ROW_Y0, OPT_ROW_Y1, OPT_ROW_Y2 };
        Sprite_Draw(g_TexWhite, OPT_PNL_X + 4.0f, rowYs[g_Cursor] - 22.0f,
            OPT_PNL_W - 8.0f, 44.0f, SEL_HL);

        // ─ テキスト ──────────────────────────────────────────────────
        if (g_pDW && g_pDW_Label)
        {

            const D2D1_COLOR_F dCYAN = D2D1::ColorF(0.2f, 0.72f, 1.0f, 1.0f);
            const D2D1_COLOR_F dWHITE = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            const D2D1_COLOR_F dGRAY = D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
            const D2D1_COLOR_F dAMBER = D2D1::ColorF(1.0f, 0.75f, 0.2f, 1.0f);

            const float LBL_HW = 120.0f;
            const float LBL_CX = OPT_BAR_X - 12.0f - LBL_HW;
            const float VAL_CX = OPT_BAR_X + OPT_BAR_W + 50.0f;
            const float VAL_HW = 55.0f;
            const float cx = OPT_PNL_X + OPT_PNL_W * 0.5f;

            char volPct[8];  snprintf(volPct, sizeof(volPct), "%d%%", static_cast<int>(roundf(g_Volume * 100.0f)));
            char sensStr[8]; snprintf(sensStr, sizeof(sensStr), "%d", sensStep);
            const bool invertY = Player_Camera_GetMouseInvertY();

            // ラベル（右揃え DW）
            g_pDW_Label->SetScale(scaleX, scaleY);
            g_pDW_Label->BeginBatch();
            g_pDW_Label->DrawAt(std::wstring(L"ボリューム"), LBL_CX, OPT_ROW_Y0, LBL_HW, (g_Cursor == 0) ? dCYAN : dWHITE, 1.5f);
            g_pDW_Label->DrawAt(std::wstring(L"感度"), LBL_CX, OPT_ROW_Y1, LBL_HW, (g_Cursor == 1) ? dCYAN : dWHITE, 1.5f);
            g_pDW_Label->DrawAt(std::wstring(L"Y軸反転"), LBL_CX, OPT_ROW_Y2, LBL_HW, (g_Cursor == 2) ? dCYAN : dWHITE, 1.5f);
            g_pDW_Label->EndBatch();
            g_pDW_Label->SetScale(1.0f, 1.0f);

            // ヘッダー・値・フッター（中央揃え DW）
            g_pDW->SetScale(scaleX, scaleY);
            g_pDW->BeginBatch();
            g_pDW->DrawAt(std::wstring(L"オプション"), cx, OPT_PNL_Y + 30.0f, 160.0f, dAMBER, 1.5f);
            g_pDW->DrawAt(volPct, VAL_CX, OPT_ROW_Y0, VAL_HW, (g_Cursor == 0) ? dCYAN : dGRAY, 1.5f);
            g_pDW->DrawAt(sensStr, VAL_CX, OPT_ROW_Y1, VAL_HW, (g_Cursor == 1) ? dCYAN : dGRAY, 1.5f);
            g_pDW->DrawAt(invertY ? "ON" : "OFF", VAL_CX, OPT_ROW_Y2, VAL_HW, (g_Cursor == 2) ? dCYAN : dGRAY, 1.5f);
            g_pDW->DrawAt(std::wstring(L"ESC / B : 戻る"), cx, OPT_PNL_Y + OPT_PNL_H - 25.0f, 180.0f, dGRAY, 1.2f);
            g_pDW->EndBatch();
            g_pDW->SetScale(1.0f, 1.0f);

        }
        return;
    }

    //==================================================================
    // PauseState::Main – メインメニュー
    //==================================================================
    static constexpr float MENU_BOX_W = 280.0f;
    static constexpr float MENU_BOX_H = 80.0f;
    static constexpr int   MAIN_COUNT_DRAW = 3;

    const float baseX = W * 0.5f;
    const float baseY = H * 0.35f;
    const float gapY = 100.0f;

    // グローボックス
    for (int i = 0; i < MAIN_COUNT_DRAW; ++i)
    {
        const bool  sel = (i == g_Cursor);
        const float scale = sel ? 1.1f : 1.0f;
        const float bob = sel ? std::sinf(g_Time * 6.0f) * 5.0f : 0.0f;
        const float bw = MENU_BOX_W * scale;
        const float bh = MENU_BOX_H * scale;
        const float bx = baseX - bw * 0.5f;
        const float by = baseY + i * gapY - bh * 0.5f + bob;

        Sprite_Draw(g_TexWhite, bx - 5.0f, by - 5.0f,
            bw + 10.0f, bh + 10.0f, XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f });
        if (sel)
        {
            const float pulse = std::sinf(g_Time * 8.0f) * 0.5f + 0.5f;
            const float a = 0.25f + 0.25f * pulse;
            Sprite_Draw(g_TexWhite, bx - 12.0f, by - 12.0f,
                bw + 24.0f, bh + 24.0f, XMFLOAT4{ 1.0f, 1.0f, 0.3f, a });
        }
    }

    // TextLogo ラベル
    {
        LogoStyle s;
        s.fontSize = 68.0f;
        s.fontName = L"Gill Sans Ultra Bold";
        s.colorTop = D2D1::ColorF(1.0f, 0.92f, 0.70f, 1.0f);
        s.colorBottom = D2D1::ColorF(0.85f, 0.55f, 0.10f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.05f, 0.02f, 0.00f, 1.0f);
        s.outlineWidth = 2.5f;

        static const wchar_t* labels[MAIN_COUNT_DRAW] = { L"RESUME", L"OPTION", L"TITLE" };
        for (int i = 0; i < MAIN_COUNT_DRAW; ++i)
        {
            const bool  sel = (i == g_Cursor);
            const float sc = sel ? 1.1f : 1.0f;
            const float bob = sel ? std::sinf(g_Time * 6.0f) * 5.0f : 0.0f;
            TextLogo_Draw(labels[i], baseX, baseY + i * gapY + bob, s, sc);
        }
    }
}