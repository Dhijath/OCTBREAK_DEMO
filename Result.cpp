/*==============================================================================

   リザルト（ゲームオーバー）画面 [Result.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景＋「GAME OVER」表示＋スコア表示（拡大）＋スコア下に任意テキスト画像
   - 入力は見ない（Enter復帰は Game_Manager 側）
==============================================================================*/
#include "Result.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "Score.h"
#include "text_logo.h"
#include <DirectXMath.h>
using namespace DirectX;

//------------------------------------------------------------------------------
// リソース
//------------------------------------------------------------------------------
static int g_ResultBgTex = -1;  // 背景
static int g_WhiteTex    = -1;  // 1x1 白
static int g_DigitTex    = -1;  // 数字スプライト（0～9 が横並び）

// 数字1桁あたりの切り出しサイズ（suji.png 想定）
static constexpr int DIGIT_W = 32;
static constexpr int DIGIT_H = 35;

// 数字の拡大倍率（ここだけ変えればOK）
static constexpr float DIGIT_SCALE = 2.5f;

// 前方宣言
static void DrawNumberLineCenteredScaled(
    int texId, int value, float centerX, float y, float scale);

//------------------------------------------------------------------------------
// 初期化
//------------------------------------------------------------------------------
void Result_Initialize()
{
    g_ResultBgTex = Texture_Load(L"resource/texture/title_bg.png");
    g_WhiteTex    = Texture_Load(L"resource/texture/white.png");
    g_DigitTex    = Texture_Load(L"resource/texture/suji.png");
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void Result_Finalize()
{
    // 個別解放は Texture 系のFinalize側でまとめる想定
}

//------------------------------------------------------------------------------
// 更新（入力は Game_Manager 側）
//------------------------------------------------------------------------------
void Result_Update(double)
{
}

//------------------------------------------------------------------------------
// 描画
//------------------------------------------------------------------------------
void Result_Draw()
{
    Sprite_Begin();
    Direct3D_SetBlendState(true);
    Direct3D_SetDepthEnable(false); // 　2Dスプライト中は深度を切る
    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // ──────────────────────────────────────────
    // 1) スプライト描画（背景 + スコアパネル）
    // ──────────────────────────────────────────

    // 背景（フィット）
    if (g_ResultBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ResultBgTex);
        const float th = (float)Texture_Height(g_ResultBgTex);
        const float sx = (float)sw / (tw > 0 ? tw : 1.0f);
        const float sy = (float)sh / (th > 0 ? th : 1.0f);
        Sprite_Draw(g_ResultBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // スコアパネル（半透明＋縁）
    const float panelW = 680.0f;
    const float panelH = 180.0f;
    const float panelX = sw * 0.5f - panelW * 0.5f;
    const float panelY = sh * 0.50f - panelH * 0.5f;

    if (g_WhiteTex >= 0)
    {
        Sprite_Draw(g_WhiteTex, panelX, panelY, panelW, panelH, XMFLOAT4(0, 0, 0, 0.55f));
        Sprite_Draw(g_WhiteTex, panelX, panelY, panelW, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, panelX, panelY + panelH - 3, panelW, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, panelX, panelY, 3, panelH, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, panelX + panelW - 3, panelY, 3, panelH, XMFLOAT4(1, 1, 1, 0.5f));
    }

    // スコア（中央に大きく）
    if (g_DigitTex >= 0)
    {
        const int   score   = (int)Score_GetScore();
        const float digitsY = panelY + panelH * 0.5f - (DIGIT_H * DIGIT_SCALE) * 0.6f;
        DrawNumberLineCenteredScaled(g_DigitTex, score, sw * 0.5f, digitsY, DIGIT_SCALE);
    }

    // ──────────────────────────────────────────
    // 2) TextLogo 描画（スプライトより後に D2D で描く）
    // ──────────────────────────────────────────

    // ロゴ（TextLogo: "GAME OVER"）
    {
        LogoStyle s;
        s.fontSize     = 100.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(1.00f, 0.30f, 0.10f, 1.0f); // 明るい赤
        s.colorBottom  = D2D1::ColorF(0.60f, 0.05f, 0.00f, 1.0f); // 暗い赤
        s.outlineColor = D2D1::ColorF(0.15f, 0.00f, 0.00f, 1.0f);
        s.outlineWidth = 3.5f;
        TextLogo_Draw(L"GAME OVER", sw * 0.5f, sh * 0.22f, s);
    }

}

//------------------------------------------------------------------------------
// 数字列（int）を拡大スケーリングして中央揃え表示
//  Sprite_Draw の「dstW,dstH,srcX,srcY,srcW,srcH,color」オーバーロードを使用
//------------------------------------------------------------------------------
static void DrawNumberLineCenteredScaled(
    int texId, int value, float centerX, float y, float scale)
{
    if (value < 0) value = 0;

    // 桁数
    int tmp = value;
    int digits = (tmp == 0) ? 1 : 0;
    while (tmp > 0) { tmp /= 10; digits++; }

    const float glyphW = DIGIT_W * scale;
    const float glyphH = DIGIT_H * scale;
    float totalW = glyphW * digits;
    float x = centerX - totalW * 0.5f;

    // 0 の特別処理
    if (value == 0)
    {
        // dstW, dstH を指定しつつ、src は「0」の部分を切り出し
        Sprite_Draw(texId, x, y,
            glyphW, glyphH,
            DIGIT_W * 0, 0, DIGIT_W, DIGIT_H,
            XMFLOAT4(1, 1, 1, 1));
        return;
    }

    // 下位桁からバッファに詰める
    int buf[16] = { 0 };
    int n = 0;
    while (value > 0 && n < 16) { buf[n++] = value % 10; value /= 10; }

    // 上位桁 → 下位桁の順に描画
    for (int i = n - 1; i >= 0; --i)
    {
        const int d = buf[i];
        Sprite_Draw(texId, x, y,
            glyphW, glyphH,                  // 宛先サイズ（拡大）
            DIGIT_W * d, 0, DIGIT_W, DIGIT_H,// 切り出し矩形
            XMFLOAT4(1, 1, 1, 1));           // 色
        x += glyphW;
    }
}
