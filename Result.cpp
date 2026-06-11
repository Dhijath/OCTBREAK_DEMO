/*==============================================================================

   リザルト（ゲームオーバー）画面 [Result.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景 + "GAME OVER"（TextLogo）+ スコア・ダメージ統計（DirectWrite）
   入力は Game_Manager 側。
==============================================================================*/
#include "Result.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "Score.h"
#include "text_logo.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <cstdio>
using namespace DirectX;

//------------------------------------------------------------------------------
// リソース
//------------------------------------------------------------------------------
static int g_ResultBgTex = -1;
static int g_WhiteTex    = -1;

static DirectWrite* g_pDWLabel = nullptr;   // ラベル（22pt）
static DirectWrite* g_pDWValue = nullptr;   // 数値（46pt Bold）

//------------------------------------------------------------------------------
// レイアウト定数（仮想 1600×900）
//------------------------------------------------------------------------------
static constexpr float SCR_CX  = 800.0f;
static constexpr float SCR_CY  = 450.0f;
static constexpr float PNL_W   = 700.0f;
static constexpr float PNL_H   = 260.0f;
static constexpr float PNL_X   = SCR_CX - PNL_W * 0.5f;
static constexpr float PNL_Y   = SCR_CY - PNL_H * 0.5f + 30.0f;

// 行ピッチ
static constexpr float ROW_H   = 76.0f;

//------------------------------------------------------------------------------
// 初期化
//------------------------------------------------------------------------------
void Result_Initialize()
{
    g_ResultBgTex = Texture_Load(L"resource/texture/title_bg.png");
    g_WhiteTex    = Texture_Load(L"resource/texture/white.png");

    if (!g_pDWLabel)
    {
        static FontData fdLabel;
        fdLabel.font          = Font::Arial;
        fdLabel.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        fdLabel.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdLabel.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdLabel.fontSize      = 28.0f;
        fdLabel.localeName    = L"en-us";
        fdLabel.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdLabel.Color         = D2D1::ColorF(0.70f, 0.70f, 0.70f, 1.0f);
        g_pDWLabel = new DirectWrite(&fdLabel);
        g_pDWLabel->Init();
    }

    if (!g_pDWValue)
    {
        static FontData fdValue;
        fdValue.font          = Font::Arial;
        fdValue.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdValue.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdValue.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdValue.fontSize      = 42.0f;
        fdValue.localeName    = L"en-us";
        fdValue.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdValue.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWValue = new DirectWrite(&fdValue);
        g_pDWValue->Init();
    }
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void Result_Finalize()
{
    if (g_pDWLabel) { g_pDWLabel->Release(); delete g_pDWLabel; g_pDWLabel = nullptr; }
    if (g_pDWValue) { g_pDWValue->Release(); delete g_pDWValue; g_pDWValue = nullptr; }
}

//------------------------------------------------------------------------------
// 更新（入力は Game_Manager 側）
//------------------------------------------------------------------------------
void Result_Update(double) {}

//------------------------------------------------------------------------------
// 共通リザルトパネル描画（Result / Clear 両方から使用）
//------------------------------------------------------------------------------
void ResultPanel_Draw()
{
    if (!g_pDWLabel || !g_pDWValue) return;

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

    // パネル背景・枠
    if (g_WhiteTex >= 0)
    {
        Sprite_Draw(g_WhiteTex, PNL_X, PNL_Y, PNL_W, PNL_H, XMFLOAT4(0, 0, 0, 0.60f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y,             PNL_W, 2, XMFLOAT4(1, 1, 1, 0.4f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y + PNL_H - 2, PNL_W, 2, XMFLOAT4(1, 1, 1, 0.4f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y,             2, PNL_H, XMFLOAT4(1, 1, 1, 0.4f));
        Sprite_Draw(g_WhiteTex, PNL_X + PNL_W - 2, PNL_Y,            2, PNL_H, XMFLOAT4(1, 1, 1, 0.4f));
    }

    // 行データ
    struct Row { const char* label; int value; D2D1_COLOR_F color; };
    const D2D1_COLOR_F WHITE  = D2D1::ColorF(1.0f,  1.0f,  1.0f,  1.0f);
    const D2D1_COLOR_F ORANGE = D2D1::ColorF(1.0f,  0.75f, 0.20f, 1.0f);
    const D2D1_COLOR_F RED    = D2D1::ColorF(1.0f,  0.35f, 0.35f, 1.0f);

    Row rows[] =
    {
        { "SCORE",         (int)Score_GetScore(),   WHITE  },
        { "DAMAGE DEALT",  Score_GetDamageDealt(), ORANGE },
        { "DAMAGE TAKEN",  Score_GetDamageTaken(), RED    },
    };
    constexpr int ROW_COUNT = 3;

    // 横線：パネルを3等分した境界（固定）
    const float LINE_PITCH = PNL_H / (float)ROW_COUNT;
    if (g_WhiteTex >= 0)
    {
        for (int i = 1; i < ROW_COUNT; ++i)
        {
            const float lineY = PNL_Y + LINE_PITCH * i;
            Sprite_Draw(g_WhiteTex, PNL_X + 20, lineY, PNL_W - 40, 1, XMFLOAT4(1,1,1,0.15f));
        }
    }

    // テキスト：各行の中心Y（横線とは独立）
    const float LABEL_CX = PNL_X + PNL_W * 0.28f;
    const float VALUE_CX = PNL_X + PNL_W * 0.72f;
    const float LABEL_HW = PNL_W * 0.26f;
    const float VALUE_HW = PNL_W * 0.24f;

    g_pDWLabel->SetScale(scaleX, scaleY);
    g_pDWValue->SetScale(scaleX, scaleY);

    for (int i = 0; i < ROW_COUNT; ++i)
    {
        // 各行の中心Y
        const float rowY = PNL_Y + LINE_PITCH * i + LINE_PITCH * 0.5f;
        const D2D1_COLOR_F GRAY = D2D1::ColorF(0.65f, 0.65f, 0.65f, 1.0f);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", rows[i].value);

        g_pDWLabel->BeginBatch();
        g_pDWLabel->DrawAt(rows[i].label, LABEL_CX, rowY, LABEL_HW, GRAY, 1.0f);
        g_pDWLabel->EndBatch();

        g_pDWValue->BeginBatch();
        g_pDWValue->DrawAt(buf, VALUE_CX, rowY, VALUE_HW, rows[i].color, 1.0f);
        g_pDWValue->EndBatch();
    }

    g_pDWLabel->SetScale(1.0f, 1.0f);
    g_pDWValue->SetScale(1.0f, 1.0f);
}

//------------------------------------------------------------------------------
// 描画
//------------------------------------------------------------------------------
void Result_Draw()
{
    Sprite_Begin();
    Direct3D_SetBlendState(true);
    Direct3D_SetDepthEnable(false);

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // 背景
    if (g_ResultBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ResultBgTex);
        const float th = (float)Texture_Height(g_ResultBgTex);
        const float sx = sw / (tw > 0.0f ? tw : 1.0f);
        const float sy = sh / (th > 0.0f ? th : 1.0f);
        Sprite_Draw(g_ResultBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // タイトル
    {
        LogoStyle s;
        s.fontSize     = 100.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(1.00f, 0.30f, 0.10f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.60f, 0.05f, 0.00f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.15f, 0.00f, 0.00f, 1.0f);
        s.outlineWidth = 3.5f;
        TextLogo_Draw(L"GAME OVER", sw * 0.5f, sh * 0.18f, s);
    }

    ResultPanel_Draw();
}
