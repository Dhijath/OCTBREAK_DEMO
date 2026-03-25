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
#include <vector>
#include <string>
#include <cstring>

using namespace DirectX;

//==============================================================================
// 定数
//==============================================================================
static constexpr float SW       = 1600.0f;
static constexpr float SH       = 900.0f;
static constexpr float BAR_H    = 38.0f;
static constexpr float BAR_Y    = SH - BAR_H;
static constexpr float ICON_W   = 110.0f;   // 左側デバイスラベル幅

static constexpr float BTN_SZ   = BAR_H - 8.0f;  // ボタンアイコン一辺 (30 px)
static constexpr float BTN_GAP  = 2.0f;           // アイコン直後の余白
static constexpr float CHAR_W   = 8.4f;           // Arial 17pt 平均文字幅（推定）

//==============================================================================
// アイコンエントリー
//   tag  : "{XXX}" 形式のプレースホルダー文字列
//   path : テクスチャファイルパス
//   texID: Texture_Load() で取得した ID（-1 = 未ロード）
//==============================================================================
struct IconEntry
{
    const char*    tag;
    const wchar_t* path;
    int            texID;
};

static IconEntry s_Icons[] =
{
    // ── キーボード ───────────────────────────────────────────────────────
    { "{TAB}",     L"resource/texture/Keyboard & Mouse/Default/keyboard_tab.png",          -1 },
    { "{ENTER}",   L"resource/texture/Keyboard & Mouse/Default/keyboard_enter.png",        -1 },
    { "{ESC}",     L"resource/texture/Keyboard & Mouse/Default/keyboard_escape.png",       -1 },
    { "{W}",       L"resource/texture/Keyboard & Mouse/Default/keyboard_w.png",            -1 },
    { "{S}",       L"resource/texture/Keyboard & Mouse/Default/keyboard_s.png",            -1 },
    { "{K_A}",     L"resource/texture/Keyboard & Mouse/Default/keyboard_a.png",            -1 },
    { "{K_D}",     L"resource/texture/Keyboard & Mouse/Default/keyboard_d.png",            -1 },
    { "{UP}",      L"resource/texture/Keyboard & Mouse/Default/keyboard_arrow_up.png",     -1 },
    { "{DOWN}",    L"resource/texture/Keyboard & Mouse/Default/keyboard_arrow_down.png",   -1 },
    { "{LEFT}",    L"resource/texture/Keyboard & Mouse/Default/keyboard_arrow_left.png",   -1 },
    { "{RIGHT}",   L"resource/texture/Keyboard & Mouse/Default/keyboard_arrow_right.png",  -1 },
    // ── マウス ──────────────────────────────────────────────────────────
    { "{MOUSE_MOVE}", L"resource/texture/Keyboard & Mouse/Default/mouse_move.png",         -1 },
    { "{MOUSE_L}",    L"resource/texture/ic_input_mouse-left_01_512.png",                  -1 },
    { "{MOUSE_R}",    L"resource/texture/ic_input_mouse-right_01_512.png",                 -1 },
    // ── Xbox ゲームパッド ────────────────────────────────────────────────
    { "{LB}",      L"resource/texture/Xbox Series/Default/xbox_lb.png",                   -1 },
    { "{RB}",      L"resource/texture/Xbox Series/Default/xbox_rb.png",                   -1 },
    { "{LT}",      L"resource/texture/Xbox Series/Default/xbox_lt.png",                   -1 },
    { "{RT}",      L"resource/texture/Xbox Series/Default/xbox_rt.png",                   -1 },
    { "{A}",       L"resource/texture/Xbox Series/Default/xbox_button_color_a.png",       -1 },
    { "{B}",       L"resource/texture/Xbox Series/Default/xbox_button_color_b.png",       -1 },
    { "{START}",   L"resource/texture/Xbox Series/Default/xbox_button_start_icon.png",    -1 },
    { "{L_STICK}", L"resource/texture/Xbox Series/Default/xbox_stick_l.png",              -1 },
    { "{R_STICK}", L"resource/texture/Xbox Series/Default/xbox_stick_r.png",              -1 },
    { "{DPAD_UP}", L"resource/texture/Xbox Series/Default/xbox_dpad_up.png",              -1 },
    { "{DPAD_DN}", L"resource/texture/Xbox Series/Default/xbox_dpad_down.png",            -1 },
    { "{DPAD_LR}", L"resource/texture/Xbox Series/Default/xbox_dpad_horizontal.png",      -1 },
};
static constexpr int ICON_COUNT = (int)(sizeof(s_Icons) / sizeof(s_Icons[0]));

//==============================================================================
// 内部状態
//==============================================================================
static InputDevice    s_Device      = InputDevice::KBMouse;
static int            s_WhiteTexID  = -1;
static DirectWrite*   s_pDW         = nullptr;   // ヒント本文
static DirectWrite*   s_pDWIcon     = nullptr;   // デバイスラベル

// 前フレームのキーボード状態
static BYTE s_PrevKeys[256] = {};

//==============================================================================
// トークン
//   ヒント文字列を「アイコン」または「テキスト」の列に分解して保持する
//==============================================================================
struct HintToken
{
    bool        isIcon;
    int         texID;   // isIcon == true のとき有効
    std::string text;    // isIcon == false のとき有効
    float       width;   // 占有幅（px）
};

// ヒント文字列 → HintToken 列に変換
//   {TAG} → アイコントークン（texID が有効な場合のみ幅を付与）
//   それ以外 → テキストトークン（文字数 × CHAR_W で幅を推定）
static std::vector<HintToken> ParseHint(const char* str)
{
    std::vector<HintToken> tokens;
    if (!str || !str[0]) return tokens;

    std::string buf;
    const char* p = str;

    // バッファに溜まったテキストをトークンとして push
    auto FlushText = [&]()
    {
        if (buf.empty()) return;
        HintToken t;
        t.isIcon = false;
        t.texID  = -1;
        t.text   = buf;
        t.width  = (float)buf.size() * CHAR_W;
        tokens.push_back(t);
        buf.clear();
    };

    while (*p)
    {
        if (*p == '{')
        {
            FlushText();

            const char* end = strchr(p + 1, '}');
            if (end)
            {
                // タグ名（"TAB" など）を抽出
                const std::string tagName(p + 1, end);

                int foundTex = -1;
                for (int i = 0; i < ICON_COUNT; ++i)
                {
                    const char* t  = s_Icons[i].tag;          // "{TAB}"
                    const size_t n = strlen(t) - 2;           // { と } を除いた長さ
                    if (tagName.size() == n &&
                        tagName.compare(0, n, t + 1, n) == 0)
                    {
                        foundTex = s_Icons[i].texID;
                        break;
                    }
                }

                HintToken tok;
                tok.isIcon = true;
                tok.texID  = foundTex;
                tok.text   = "";
                tok.width  = (foundTex >= 0) ? (BTN_SZ + BTN_GAP) : 0.0f;
                tokens.push_back(tok);
                p = end + 1;
            }
            else
            {
                buf += *p++;  // } なし → リテラルとして扱う
            }
        }
        else
        {
            buf += *p++;
        }
    }
    FlushText();
    return tokens;
}

//==============================================================================
// 初期化
//==============================================================================
void InputHint_Initialize()
{
    s_WhiteTexID = Texture_Load(L"resource/texture/white.png");

    // ボタンアイコンテクスチャを一括読み込み
    for (auto& e : s_Icons)
        e.texID = Texture_Load(e.path);

    // ヒント本文フォント（Arial 17pt）
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

    // デバイス種別ラベル（小さいフォント・水色）
    static FontData fdIcon;
    fdIcon.font          = Font::Arial;
    fdIcon.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
    fdIcon.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
    fdIcon.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
    fdIcon.fontSize      = 13.0f;
    fdIcon.localeName    = L"en-us";
    fdIcon.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
    fdIcon.Color         = D2D1::ColorF(0.5f, 0.8f, 1.0f, 1.0f);
    s_pDWIcon = new DirectWrite(&fdIcon);
    s_pDWIcon->Init();

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
//==============================================================================
void InputHint_Update()
{
    // ── ゲームパッド検出 ──────────────────────────────────────────────────
    static const WORD PAD_BUTTONS[] = {
        PAD_A, PAD_B, PAD_X, PAD_Y,
        PAD_DPAD_UP, PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_DPAD_RIGHT,
        PAD_START, PAD_BACK,
        PAD_LEFT_SHOULDER, PAD_RIGHT_SHOULDER,
        PAD_LEFT_THUMB, PAD_RIGHT_THUMB,
    };
    bool padTrig = false;
    for (WORD btn : PAD_BUTTONS)
        if (PadLogger_IsTrigger(btn)) { padTrig = true; break; }

    if (!padTrig)
    {
        float lx, ly, rx, ry;
        PadLogger_GetLeftStick(&lx, &ly);
        PadLogger_GetRightStick(&rx, &ry);
        const float lt   = PadLogger_GetLeftTrigger();
        const float rt   = PadLogger_GetRightTrigger();
        constexpr float DEAD = 0.25f;
        if (lx*lx + ly*ly > DEAD*DEAD || rx*rx + ry*ry > DEAD*DEAD ||
            lt > DEAD || rt > DEAD)
            padTrig = true;
    }
    if (padTrig) { s_Device = InputDevice::Gamepad; return; }

    // ── キーボード検出（全 256 キーをスキャン）───────────────────────────
    BYTE curKeys[256] = {};
    GetKeyboardState(curKeys);
    for (int i = 1; i < 256; ++i)
    {
        if ((curKeys[i] & 0x80) && !(s_PrevKeys[i] & 0x80))
        {
            s_Device = InputDevice::KBMouse;
            break;
        }
    }
    memcpy(s_PrevKeys, curKeys, sizeof(s_PrevKeys));

    // ── マウスボタン検出 ──────────────────────────────────────────────────
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
//
//   kbmText / padText 内の {TAG} プレースホルダーをボタンアイコンに置換して描画する。
//
//   使用可能タグ:
//     KB/Mouse  : {TAB} {ENTER} {W} {S} {UP} {DOWN} {MOUSE_L}
//     Gamepad   : {LB}  {RB}   {A} {DPAD_UP} {DPAD_DN}
//
//   例:
//     InputHint_Draw(
//         "{TAB} R/L ARM    {W}{S} Weapon    {MOUSE_L} Decide",
//         "{LB}{RB} R/L ARM    {DPAD_UP}{DPAD_DN} Weapon    {A} Decide");
//
//   描画順:
//     1. Sprite_Draw でバー背景 + アイコンスプライトを描く
//     2. Direct3D_BindMainRenderTarget() でRTVをリセット
//     3. DirectWrite でデバイスラベルとテキストセグメントを描く
//     4. Direct3D_BindMainRenderTarget() + Sprite_Begin() で後続スプライトに備える
//==============================================================================
void InputHint_Draw(const char* kbmText, const char* padText)
{
    if (s_WhiteTexID < 0) return;

    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();

    // ── バー背景 ────────────────────────────────────────────────────────
    const XMFLOAT4 colBg   = { 0.02f, 0.04f, 0.10f, 0.88f };  // 濃いネイビー
    const XMFLOAT4 colTop  = { 0.30f, 0.55f, 1.00f, 0.70f };  // 上辺アクセントライン
    const XMFLOAT4 colDev  = { 0.05f, 0.10f, 0.25f, 0.92f };  // デバイスラベル背景
    Sprite_Draw(s_WhiteTexID, 0.0f, BAR_Y, SW,     BAR_H, colBg);
    Sprite_Draw(s_WhiteTexID, 0.0f, BAR_Y, SW,     1.5f,  colTop);
    Sprite_Draw(s_WhiteTexID, 0.0f, BAR_Y, ICON_W, BAR_H, colDev);

    const float CY = BAR_Y + BAR_H * 0.5f + 1.0f;  // バー垂直中心

    // ── ヒント文字列をトークン列に変換 ──────────────────────────────────
    const char* hint   = (s_Device == InputDevice::KBMouse) ? kbmText : padText;
    const auto  tokens = ParseHint(hint);

    // ── 合計幅を計算してテキスト領域内でセンタリング ────────────────────
    float totalW = 0.0f;
    for (const auto& t : tokens) totalW += t.width;

    const float areaStart = ICON_W;
    const float areaW     = SW - ICON_W;
    const float startX    = areaStart + (areaW - totalW) * 0.5f;

    // ── Pass 1: ボタンアイコンをスプライトで描画 ─────────────────────────
    // スプライトは仮想 1600×900 座標系で描画されるのでスケール不要
    {
        const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
        float x = startX;
        for (const auto& t : tokens)
        {
            if (t.isIcon && t.texID >= 0)
            {
                const float iy = CY - BTN_SZ * 0.5f;
                Sprite_Draw(t.texID, x, iy, BTN_SZ, BTN_SZ, white);
            }
            x += t.width;
        }
    }

    // ── D2D 描画モードへ切り替え ──────────────────────────────────────
    Direct3D_BindMainRenderTarget();

    // フルスクリーン対応: 仮想 1600×900 → 実ピクセル座標へのスケールを設定
    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

    // デバイスラベル（左側帯）
    if (s_pDWIcon)
    {
        const char* label = (s_Device == InputDevice::KBMouse) ? "KB / Mouse" : "Gamepad";
        s_pDWIcon->SetScale(scaleX, scaleY);
        s_pDWIcon->BeginBatch();
        s_pDWIcon->DrawAt(label,
            ICON_W * 0.5f, CY,
            ICON_W * 0.5f - 4.0f,
            D2D1::ColorF(0.5f, 0.8f, 1.0f, 1.0f));
        s_pDWIcon->EndBatch();
        s_pDWIcon->SetScale(1.0f, 1.0f);
    }

    // Pass 2: テキストセグメントを DirectWrite で描画
    //   cx      = トークン中心 X（仮想 1600×900 座標）
    //   halfW   = 推定幅より大きく取り、折り返しを防ぐ
    if (s_pDW && !tokens.empty())
    {
        s_pDW->SetScale(scaleX, scaleY);
        s_pDW->BeginBatch();
        float x = startX;
        for (const auto& t : tokens)
        {
            if (!t.isIcon && !t.text.empty())
            {
                const float cx          = x + t.width * 0.5f;
                const float renderHalfW = t.width * 0.5f + 160.0f;
                s_pDW->DrawAt(t.text, cx, CY, renderHalfW,
                    D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f));
            }
            x += t.width;
        }
        s_pDW->EndBatch();
        s_pDW->SetScale(1.0f, 1.0f);
    }

    // ── 後続スプライト描画のために RTV を再バインド ─────────────────────
    Direct3D_BindMainRenderTarget();
    Sprite_Begin();
}
