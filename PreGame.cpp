/*==============================================================================
   中間メニュー [PreGame.cpp]
   タイトルで START を選んだ後に表示
   選択肢: アセンブリ / スコア確認
   ESC / Bボタン → タイトルへ戻る
==============================================================================*/
#include "PreGame.h"
#include "UIInput.h"
#include "audio.h"
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include "text_logo.h"
#include "input_hint.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
using namespace DirectX;

//------------------------------------------------------------------------------
// 定数
//------------------------------------------------------------------------------
static constexpr int   ITEM_COUNT = 3;
static const wchar_t*  ITEM_LABELS[ITEM_COUNT] = { L"QUICK START", L"ASSEMBLY", L"SCOREBOARD" };

//------------------------------------------------------------------------------
// 状態
//------------------------------------------------------------------------------
static int           g_Selected = 0;
static float         g_Time     = 0.0f;
static PreGameResult g_Result   = PreGameResult::None;

// テクスチャ
static int g_BgTex    = -1;
static int g_WhiteTex = -1;

// SE
static int g_SeCursorMove = -1;
static int g_SeSelect     = -1;
static int g_SeCancel     = -1;

//------------------------------------------------------------------------------
void PreGame_Initialize()
{
    g_BgTex    = Texture_Load(L"resource/texture/titleBg.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    g_Selected = 0;
    g_Time     = 0.0f;
    g_Result   = PreGameResult::None;
}

//------------------------------------------------------------------------------
void PreGame_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;
}

//------------------------------------------------------------------------------
void PreGame_Update(double elapsed_time)
{
    g_Time += static_cast<float>(elapsed_time);

    if (UI_IsMoveUp())
    {
        g_Selected = (g_Selected + ITEM_COUNT - 1) % ITEM_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsMoveDown())
    {
        g_Selected = (g_Selected + 1) % ITEM_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }

    if (UI_IsConfirm())
    {
        PlayAudio(g_SeSelect, false);
        if      (g_Selected == 0) g_Result = PreGameResult::QuickStart;
        else if (g_Selected == 1) g_Result = PreGameResult::Assembly;
        else                      g_Result = PreGameResult::ScoreCheck;
    }

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Result = PreGameResult::Back;
    }
}

//------------------------------------------------------------------------------
void PreGame_Draw()
{
    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    static constexpr float BOX_W  = 260.0f;   // Title と同じ
    static constexpr float BOX_H  = 82.0f;
    const float baseX = sw * 0.5f;
    const float baseY = sh * 0.55f;           // Title と同じ
    const float gapY  = 100.0f;               // Title と同じ

    // ── スプライト ─────────────────────────────────
    Sprite_Begin();

    // 背景
    if (g_BgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_BgTex);
        const float th = (float)Texture_Height(g_BgTex);
        const float sx = sw / std::max(1.0f, tw);
        const float sy = sh / std::max(1.0f, th);
        Sprite_Draw(g_BgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // ボタン枠
    if (g_WhiteTex >= 0)
    {
        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            const bool  sel   = (i == g_Selected);
            const float scale = sel ? 1.1f : 1.0f;
            const float bob   = sel ? std::sin(g_Time * 6.0f) * 5.0f : 0.0f;
            const float bw    = BOX_W * scale;
            const float bh    = BOX_H * scale;
            const float bx    = baseX - bw * 0.5f;
            const float by    = baseY + i * gapY - bh * 0.5f + bob;

            Sprite_Draw(g_WhiteTex, bx - 5, by - 5, bw + 10, bh + 10,
                        XMFLOAT4(1, 1, 1, 0.3f));
            if (sel)
            {
                const float pulse = std::sin(g_Time * 8.0f) * 0.5f + 0.5f;
                Sprite_Draw(g_WhiteTex, bx - 12, by - 12, bw + 24, bh + 24,
                            XMFLOAT4(1, 1, 1, 0.25f + 0.25f * pulse));
            }
        }
    }

    // ── TextLogo ────────────────────────────────────

    // タイトルロゴ（Title と同じスタイル）
    {
        LogoStyle s;
        s.fontSize     = 148.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.95f, 0.95f, 1.00f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.35f, 0.35f, 0.40f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.06f, 0.06f, 0.08f, 1.0f);
        s.outlineWidth = 5.0f;
        TextLogo_Draw(L"Oct Break", baseX, (float)sh * 0.20f, s);
    }

    // メニュー項目（Title と同じスタイル）
    {
        LogoStyle s;
        s.fontSize     = 68.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(1.0f, 0.92f, 0.70f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.85f, 0.55f, 0.10f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.05f, 0.02f, 0.00f, 1.0f);
        s.outlineWidth = 2.5f;

        for (int i = 0; i < ITEM_COUNT; ++i)
        {
            const bool  sel = (i == g_Selected);
            const float sc  = sel ? 1.1f : 1.0f;
            const float bob = sel ? std::sin(g_Time * 6.0f) * 5.0f : 0.0f;
            TextLogo_Draw(ITEM_LABELS[i], baseX, baseY + i * gapY + bob, s, sc);
        }
    }

    // フッター（InputHint バー）
    Direct3D_BindMainRenderTarget();
    static const wchar_t* itemDesc[ITEM_COUNT] = {
        L"ダンジョン最奥にいるボスの討伐が目的です",
        L"武器の組み合わせを変更します",
        L"過去のスコアと順位を確認します",
    };
    InputHint_Draw(
        "{UP}{DOWN} Move    {ENTER} Select    {ESC} Back",
        "{DPAD_UP}{DPAD_DN} Move    {A} Select    {B} Back",
        itemDesc[g_Selected]);
}

//------------------------------------------------------------------------------
PreGameResult PreGame_GetResult()
{
    PreGameResult r = g_Result;
    g_Result = PreGameResult::None;
    return r;
}
