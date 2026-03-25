/*==============================================================================

   クリア画面 [Clear.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景＋「GAME CLEAR」表示＋スコア表示
   - 入力は見ない（Enter復帰は Game_Manager 側）
   - 下辺りの長方形の“上に”キャプション画像を配置可能
   - 　スコア数字を拡大表示（中央寄せ）
==============================================================================*/
#include "Clear.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "Score.h"
#include "text_logo.h"
#include <DirectXMath.h>
using namespace DirectX;

/* -----------------------------------------------------------------------------
   リソース
----------------------------------------------------------------------------- */
static int g_ClearBgTex = -1; // 背景
static int g_WhiteTex   = -1; // 1x1 白
static int g_DigitTex   = -1; // 数字（suji.png）

/* -----------------------------------------------------------------------------
   数字スプライトの元サイズ（Score.cpp と揃える）
----------------------------------------------------------------------------- */
static constexpr int   DIGIT_W = 32;
static constexpr int   DIGIT_H = 35;
static constexpr float DIGIT_SCALE = 1.6f; // 　拡大率（リザルトと同等感）
static constexpr float DIGIT_DST_W = DIGIT_W * DIGIT_SCALE;
static constexpr float DIGIT_DST_H = DIGIT_H * DIGIT_SCALE;

/* -----------------------------------------------------------------------------
   数字描画（中央寄せ・拡大版）
   - 1 桁ずつ src を切り出し、dest 側は DIGIT_DST_W/H で描画
   - Sprite_Draw( tex, x,y, dstW,dstH, srcX,srcY,srcW,srcH, color ) を使用
----------------------------------------------------------------------------- */
static void DrawNumberLineCenteredScaled(int texId, int value, float centerX, float baselineY)
{
    if (value < 0) value = 0;

    // 桁数
    int tmp = value;
    int digits = (tmp == 0) ? 1 : 0;
    while (tmp > 0) { tmp /= 10; ++digits; }

    // 横中央寄せ位置を決定（拡大後の幅で計算）
    const float totalW = DIGIT_DST_W * digits;
    float x = centerX - totalW * 0.5f;

    // 0 の描画（特例）
    if (value == 0)
    {
        Sprite_Draw(
            texId,
            x, baselineY,
            DIGIT_DST_W, DIGIT_DST_H,              // ← 拡大後の描画サイズ
            DIGIT_W * 0, 0, DIGIT_W, DIGIT_H,      // ← 元画像からの切り出し
            XMFLOAT4(1, 1, 1, 1)
        );
        return;
    }

    // 左から順に描くため、いったん桁を配列に格納（下位→上位）
    int buf[16] = { 0 };
    int n = 0;
    while (value > 0 && n < 16) { buf[n++] = value % 10; value /= 10; }

    // 上位桁から描画
    for (int i = n - 1; i >= 0; --i)
    {
        const int d = buf[i];
        Sprite_Draw(
            texId,
            x, baselineY,
            DIGIT_DST_W, DIGIT_DST_H,              // 拡大後の描画サイズ
            DIGIT_W * d, 0, DIGIT_W, DIGIT_H,      // 元の桁画像を切り出し
            XMFLOAT4(1, 1, 1, 1)
        );
        x += DIGIT_DST_W;
    }
}

/* -----------------------------------------------------------------------------
   初期化
----------------------------------------------------------------------------- */
void Clear_Initialize()
{
    g_ClearBgTex = Texture_Load(L"resource/texture/title_bg.png");
    g_WhiteTex   = Texture_Load(L"resource/texture/white.png");
    g_DigitTex   = Texture_Load(L"resource/texture/suji.png");
}

/* -----------------------------------------------------------------------------
   終了
----------------------------------------------------------------------------- */
void Clear_Finalize() {}

/* -----------------------------------------------------------------------------
   更新（入力は Game_Manager 側）
----------------------------------------------------------------------------- */
void Clear_Update(double) {}

/* -----------------------------------------------------------------------------
   描画
----------------------------------------------------------------------------- */
void Clear_Draw()
{
    Direct3D_SetBlendState(true);
    Direct3D_SetDepthEnable(false); // 　2Dスプライト中は深度を切る
    Sprite_Begin();
    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // ──────────────────────────────────────────
    // 1) スプライト描画（背景 + スコアパネル）
    // ──────────────────────────────────────────

    // 背景（全面フィット）
    if (g_ClearBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ClearBgTex);
        const float th = (float)Texture_Height(g_ClearBgTex);
        const float sx = (float)sw / (tw > 0 ? tw : 1.0f);
        const float sy = (float)sh / (th > 0 ? th : 1.0f);
        Sprite_Draw(g_ClearBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // スコアパネル（半透明＋枠）
    if (g_WhiteTex >= 0)
    {
        const float pw = 520.0f;
        const float ph = 120.0f;
        const float px = sw * 0.5f - pw * 0.5f;
        const float py = sh * 0.50f - ph * 0.5f;

        Sprite_Draw(g_WhiteTex, px, py, pw, ph, XMFLOAT4(0, 0, 0, 0.55f));
        Sprite_Draw(g_WhiteTex, px, py, pw, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px, py + ph - 3, pw, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px, py, 3, ph, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px + pw - 3, py, 3, ph, XMFLOAT4(1, 1, 1, 0.5f));

        // 拡大スコア（中央寄せ）
        if (g_DigitTex >= 0)
        {
            const int   score          = (int)Score_GetScore();
            const float digitsBaselineY = py + ph * 0.5f - DIGIT_DST_H * 0.5f;
            DrawNumberLineCenteredScaled(g_DigitTex, score, sw * 0.5f, digitsBaselineY);
        }
    }

    // ──────────────────────────────────────────
    // 2) TextLogo 描画（スプライトより後に D2D で描く）
    // ──────────────────────────────────────────

    // ロゴ（TextLogo: "GAME CLEAR"）
    {
        LogoStyle s;
        s.fontSize     = 100.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.50f, 1.00f, 0.90f, 1.0f); // シアン
        s.colorBottom  = D2D1::ColorF(0.05f, 0.60f, 0.80f, 1.0f); // 深い青
        s.outlineColor = D2D1::ColorF(0.00f, 0.10f, 0.15f, 1.0f);
        s.outlineWidth = 3.5f;
        TextLogo_Draw(L"GAME CLEAR", sw * 0.5f, sh * 0.22f, s);
    }

}
