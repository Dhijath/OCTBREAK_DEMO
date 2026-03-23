/*==============================================================================

   入力デバイス検出 & チュートリアルバー [input_hint.cpp]
   Author : 51106
   Date   : 2026/03/25

==============================================================================*/
#include "input_hint.h"
#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include "DirectWrite.h"
#include "pad_logger.h"
#include "mouse.h"
#include <windows.h>
#include <DirectXMath.h>
#include <d2d1helper.h>

using namespace DirectX;

//==============================================================================
// 定数
//==============================================================================
static constexpr float SW       = 1600.0f;
static constexpr float SH       = 900.0f;
static constexpr float BAR_H    = 38.0f;   // ヒントバーの高さ
static constexpr float BAR_Y    = SH - BAR_H;
static constexpr float ICON_W   = 110.0f;  // 左側デバイスアイコン幅

//==============================================================================
// 内部状態
//==============================================================================
static InputDevice    s_Device      = InputDevice::KBMouse;
static int            s_WhiteTexID  = -1;
static DirectWrite*   s_pDW         = nullptr;
static DirectWrite*   s_pDWIcon     = nullptr;   // デバイス種別ラベル用

// 前フレームのキーボード状態（GetKeyboardState 用）
static BYTE s_PrevKeys[256] = {};

//==============================================================================
// 初期化
//==============================================================================
void InputHint_Initialize()
{
    s_WhiteTexID = Texture_Load(L"resource/texture/white.png");

    // ヒント本文フォント（Arial 中サイズ）
    static FontData fdBody;
    fdBody.font          = Font::Arial;
    fdBody.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
    fdBody.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
    fdBody.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
    fdBody.fontSize      = 17.0f;
    fdBody.localeName    = L"en-us";
    fdBody.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
    fdBody.Color         = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
    s_pDW = new DirectWrite(&fdBody);
    s_pDW->Init();

    // デバイスアイコン（"KB/Mouse" or "Gamepad"）小さいラベル
    static FontData fdIcon;
    fdIcon.font          = Font::Arial;
    fdIcon.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
    fdIcon.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
    fdIcon.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
    fdIcon.fontSize      = 13.0f;
    fdIcon.localeName    = L"en-us";
    fdIcon.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
    fdIcon.Color         = D2D1::ColorF(0.5f, 0.8f, 1.0f, 1.0f);   // 水色
    s_pDWIcon = new DirectWrite(&fdIcon);
    s_pDWIcon->Init();

    // 初期キー状態を取得しておく
    GetKeyboardState(s_PrevKeys);
}

//==============================================================================
// 終了
//==============================================================================
void InputHint_Finalize()
{
    if (s_pDW)     { s_pDW->Release();     delete s_pDW;     s_pDW     = nullptr; }
    if (s_pDWIcon) { s_pDWIcon->Release(); delete s_pDWIcon; s_pDWIcon = nullptr; }
    s_WhiteTexID = -1;
}

//==============================================================================
// 更新 ─ 入力デバイス検出
//   優先度：パッド入力 > キーボード入力 > マウス入力
//   （最後に何かが押された方に切り替わる）
//==============================================================================
void InputHint_Update()
{
    // ── ゲームパッド検出 ────────────────────────────────────────────────
    static const WORD PAD_BUTTONS[] = {
        PAD_A, PAD_B, PAD_X, PAD_Y,
        PAD_DPAD_UP, PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_DPAD_RIGHT,
        PAD_START, PAD_BACK,
        PAD_LEFT_SHOULDER, PAD_RIGHT_SHOULDER,
        PAD_LEFT_THUMB, PAD_RIGHT_THUMB,
    };
    bool padTrig = false;
    for (WORD btn : PAD_BUTTONS)
    {
        if (PadLogger_IsTrigger(btn)) { padTrig = true; break; }
    }
    // スティック・トリガーも検出
    if (!padTrig)
    {
        float lx, ly, rx, ry;
        PadLogger_GetLeftStick(&lx, &ly);
        PadLogger_GetRightStick(&rx, &ry);
        const float lt = PadLogger_GetLeftTrigger();
        const float rt = PadLogger_GetRightTrigger();
        constexpr float DEAD = 0.25f;
        if (lx*lx + ly*ly > DEAD*DEAD || rx*rx + ry*ry > DEAD*DEAD ||
            lt > DEAD || rt > DEAD)
            padTrig = true;
    }
    if (padTrig) { s_Device = InputDevice::Gamepad; return; }

    // ── キーボード検出（全256キーをスキャン）───────────────────────────
    BYTE curKeys[256] = {};
    GetKeyboardState(curKeys);
    for (int i = 1; i < 256; ++i)
    {
        const bool cur  = (curKeys[i]      & 0x80) != 0;
        const bool prev = (s_PrevKeys[i]   & 0x80) != 0;
        if (cur && !prev)   // 押された瞬間
        {
            s_Device = InputDevice::KBMouse;
            break;
        }
    }
    memcpy(s_PrevKeys, curKeys, sizeof(s_PrevKeys));

    // ── マウスボタン検出 ────────────────────────────────────────────────
    Mouse_State ms{};
    Mouse_GetState(&ms);
    static bool s_PrevL = false, s_PrevR = false;
    if ((ms.leftButton && !s_PrevL) || (ms.rightButton && !s_PrevR))
        s_Device = InputDevice::KBMouse;
    s_PrevL = ms.leftButton;
    s_PrevR = ms.rightButton;
}

//==============================================================================
// アクティブデバイス取得
//==============================================================================
InputDevice InputHint_GetActiveDevice()
{
    return s_Device;
}

//==============================================================================
// 描画 ─ 画面下部ヒントバー
//   ┌──────────────────────────────────────────────────────────────────────┐
//   │ [KB/Mouse]  │        <ヒントテキスト>                               │
//   └──────────────────────────────────────────────────────────────────────┘
//==============================================================================
void InputHint_Draw(const char* kbmText, const char* padText)
{
    if (s_WhiteTexID < 0) return;

    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();

    // ── バー背景 ─────────────────────────────────────────────────────────
    const XMFLOAT4 colBg     = { 0.02f, 0.04f, 0.10f, 0.88f };  // 濃いネイビー
    const XMFLOAT4 colTop    = { 0.30f, 0.55f, 1.00f, 0.70f };  // 上辺アクセントライン
    const XMFLOAT4 colIcon   = { 0.05f, 0.10f, 0.25f, 0.92f };  // アイコン背景
    Sprite_Draw(s_WhiteTexID, 0.0f,  BAR_Y,        SW,   BAR_H,    colBg);
    Sprite_Draw(s_WhiteTexID, 0.0f,  BAR_Y,        SW,   1.5f,     colTop);  // 上辺ライン
    Sprite_Draw(s_WhiteTexID, 0.0f,  BAR_Y,        ICON_W, BAR_H,  colIcon); // アイコン部分

    // ── DirectWrite テキスト ─────────────────────────────────────────────
    const float CY = BAR_Y + BAR_H * 0.5f + 2.0f;   // バー中心Y

    Direct3D_BindMainRenderTarget();

    // デバイスラベル（左側）
    if (s_pDWIcon)
    {
        s_pDWIcon->BeginBatch();
        const char* label = (s_Device == InputDevice::KBMouse) ? "KB / Mouse" : "Gamepad";
        s_pDWIcon->DrawAt(label,
            ICON_W * 0.5f, CY,
            ICON_W * 0.5f - 4.0f,
            D2D1::ColorF(0.5f, 0.8f, 1.0f, 1.0f));
        s_pDWIcon->EndBatch();
    }

    // ヒント本文（残り幅中央）
    if (s_pDW)
    {
        const char* hint = (s_Device == InputDevice::KBMouse) ? kbmText : padText;
        if (hint && hint[0] != '\0')
        {
            const float textCX = ICON_W + (SW - ICON_W) * 0.5f;
            const float halfW  = (SW - ICON_W) * 0.5f - 12.0f;
            s_pDW->BeginBatch();
            s_pDW->DrawAt(hint, textCX, CY, halfW,
                D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f));
            s_pDW->EndBatch();
        }
    }

    // D2D 後に RTV を再バインドしてスプライト系に戻す
    Direct3D_BindMainRenderTarget();
    Sprite_Begin();
}
