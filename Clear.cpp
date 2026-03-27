/*==============================================================================

   クリア画面 [Clear.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景＋「GAME CLEAR」（TextLogo）＋ スコア（DirectWrite）
   入力は Game_Manager 側。
==============================================================================*/
#include "Clear.h"
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
static int g_ClearBgTex = -1;
static int g_WhiteTex   = -1;

static DirectWrite* g_pDWLabel = nullptr;   // "SCORE" ラベル（22pt）
static DirectWrite* g_pDWScore = nullptr;   // スコア数値（52pt Bold）

//------------------------------------------------------------------------------
// レイアウト定数（仮想 1600×900）
//------------------------------------------------------------------------------
static constexpr float SCR_CX = 800.0f;
static constexpr float SCR_CY = 450.0f;
static constexpr float PNL_W = 680.0f;
static constexpr float PNL_H = 210.0f;
static constexpr float PNL_X = SCR_CX - PNL_W * 0.5f;
static constexpr float PNL_Y = SCR_CY - PNL_H * 0.5f;

//------------------------------------------------------------------------------
// 初期化
//------------------------------------------------------------------------------
void Clear_Initialize()
{
    g_ClearBgTex = Texture_Load(L"resource/texture/title_bg.png");
    g_WhiteTex   = Texture_Load(L"resource/texture/white.png");

    if (!g_pDWLabel)
    {
        static FontData fdLabel;
        fdLabel.font          = Font::Arial;
        fdLabel.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        fdLabel.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdLabel.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdLabel.fontSize      = 22.0f;
        fdLabel.localeName    = L"en-us";
        fdLabel.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdLabel.Color         = D2D1::ColorF(0.75f, 0.75f, 0.75f, 1.0f);
        g_pDWLabel = new DirectWrite(&fdLabel);
        g_pDWLabel->Init();
    }

    if (!g_pDWScore)
    {
        static FontData fdScore;
        fdScore.font          = Font::Arial;
        fdScore.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdScore.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdScore.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdScore.fontSize      = 80.0f;
        fdScore.localeName    = L"en-us";
        fdScore.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdScore.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWScore = new DirectWrite(&fdScore);
        g_pDWScore->Init();
    }
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void Clear_Finalize()
{
    if (g_pDWLabel) { g_pDWLabel->Release(); delete g_pDWLabel; g_pDWLabel = nullptr; }
    if (g_pDWScore) { g_pDWScore->Release(); delete g_pDWScore; g_pDWScore = nullptr; }
}

//------------------------------------------------------------------------------
// 更新（入力は Game_Manager 側）
//------------------------------------------------------------------------------
void Clear_Update(double) {}

//------------------------------------------------------------------------------
// 描画
//------------------------------------------------------------------------------
void Clear_Draw()
{
    Sprite_Begin();
    Direct3D_SetBlendState(true);
    Direct3D_SetDepthEnable(false);

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // 背景
    if (g_ClearBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ClearBgTex);
        const float th = (float)Texture_Height(g_ClearBgTex);
        const float sx = sw / (tw > 0.0f ? tw : 1.0f);
        const float sy = sh / (th > 0.0f ? th : 1.0f);
        Sprite_Draw(g_ClearBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // スコアパネル（半透明＋枠）
    if (g_WhiteTex >= 0)
    {
        Sprite_Draw(g_WhiteTex, PNL_X, PNL_Y, PNL_W, PNL_H, XMFLOAT4(0, 0, 0, 0.55f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y,             PNL_W, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y + PNL_H - 3, PNL_W, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, PNL_X,             PNL_Y,             3, PNL_H, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, PNL_X + PNL_W - 3, PNL_Y,            3, PNL_H, XMFLOAT4(1, 1, 1, 0.5f));
    }

    // TextLogo: "GAME CLEAR"
    {
        LogoStyle s;
        s.fontSize     = 100.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.50f, 1.00f, 0.90f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.05f, 0.60f, 0.80f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.00f, 0.10f, 0.15f, 1.0f);
        s.outlineWidth = 3.5f;
        TextLogo_Draw(L"GAME CLEAR", sw * 0.5f, sh * 0.22f, s);
    }

    // DirectWrite: "SCORE" ラベル + スコア数値
    if (g_pDWLabel && g_pDWScore)
    {
        const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
        const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

        char scoreBuf[32];
        snprintf(scoreBuf, sizeof(scoreBuf), "%d", static_cast<int>(Score_GetScore()));

        const D2D1_COLOR_F dGRAY  = D2D1::ColorF(0.70f, 0.70f, 0.70f, 1.0f);
        const D2D1_COLOR_F dWHITE = D2D1::ColorF(1.0f,  1.0f,  1.0f,  1.0f);

        g_pDWLabel->SetScale(scaleX, scaleY);
        g_pDWLabel->BeginBatch();
        g_pDWLabel->DrawAt("SCORE", SCR_CX, PNL_Y + 42.0f, 200.0f, dGRAY, 1.0f);
        g_pDWLabel->EndBatch();
        g_pDWLabel->SetScale(1.0f, 1.0f);

        g_pDWScore->SetScale(scaleX, scaleY);
        g_pDWScore->BeginBatch();
        g_pDWScore->DrawAt(scoreBuf, SCR_CX, PNL_Y + 105.0f, 600.0f, dWHITE, 1.0f);
        g_pDWScore->EndBatch();
        g_pDWScore->SetScale(1.0f, 1.0f);
    }
}
