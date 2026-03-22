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
#include <DirectXMath.h>
using namespace DirectX;

//------------------------------------------------------------------------------
// リソース
//------------------------------------------------------------------------------
static int g_ResultBgTex = -1;  // 背景
static int g_ResultLogoTex = -1;  // 「GAME OVER」ロゴ
static int g_WhiteTex = -1;  // 1x1 白
static int g_DigitTex = -1;  // 数字スプライト（0～9 が横並び）
static int g_CaptionTex = -1;  // スコア下に置く任意のテキスト画像

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
    g_ResultLogoTex = Texture_Load(L"resource/texture/result_logo.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");
    g_DigitTex = Texture_Load(L"resource/texture/suji.png");

    g_CaptionTex = Texture_Load(L"resource/texture/enter_menu.png");
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

    // 背景（フィット）
    if (g_ResultBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ResultBgTex);
        const float th = (float)Texture_Height(g_ResultBgTex);
        const float sx = (float)sw / (tw > 0 ? tw : 1.0f);
        const float sy = (float)sh / (th > 0 ? th : 1.0f);
        Sprite_Draw(g_ResultBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // ロゴ
    if (g_ResultLogoTex >= 0)
    {
        const float lw = 720.0f;
        const float lh = 200.0f;
        const float lx = sw * 0.5f - lw * 0.5f;
        const float ly = sh * 0.18f;
        Sprite_Draw(g_ResultLogoTex, lx, ly, lw, lh, XMFLOAT4(1, 1, 1, 1));
    }

    // スコアパネル（半透明＋縁）
    float panelW = 680.0f;
    float panelH = 180.0f;
    float panelX = sw * 0.5f - panelW * 0.5f;
    float panelY = sh * 0.50f - panelH * 0.5f;

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
        const int score = (int)Score_GetScore();
        const float digitsY = panelY + panelH * 0.5f - (DIGIT_H * DIGIT_SCALE) * 0.6f;

        DrawNumberLineCenteredScaled(
            g_DigitTex, score, sw * 0.5f, digitsY, DIGIT_SCALE);
    }

    // スコア直下の任意テキスト画像（存在すれば描画）
// 
// 下辺長方形とキャプションを描画
    if (g_WhiteTex >= 0)
    {
        const float bw = 420.0f;
        const float bh = 36.0f;
        const float bx = sw * 0.5f - bw * 0.5f;
        const float by = sh * 0.82f;

        // 下辺長方形（半透明）
        Sprite_Draw(g_WhiteTex, bx, by, bw, bh, XMFLOAT4(1, 1, 1, 0.12f));

        // キャプションを中央に配置
        if (g_CaptionTex >= 0)
        {
            const float tw = (float)Texture_Width(g_CaptionTex);
            const float th = (float)Texture_Height(g_CaptionTex);

            // 長方形の中央に合わせる
            const float cx = bx + bw * 0.5f - tw * 0.5f;
            const float cy = by + bh * 0.5f - th * 0.5f;

            Sprite_Draw(g_CaptionTex, cx, cy, tw, th, XMFLOAT4(1, 1, 1, 1));
        }
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
