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
#include <DirectXMath.h>
using namespace DirectX;

/* -----------------------------------------------------------------------------
   リソース
----------------------------------------------------------------------------- */
static int g_ClearBgTex = -1; // 背景
static int g_ClearLogoTex = -1; // 「GAME CLEAR」ロゴ
static int g_WhiteTex = -1; // 1x1 白
static int g_DigitTex = -1; // 数字（suji.png）
static int g_CaptionTex = -1; // 下部帯の上に載せるキャプション画像

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
    g_ClearLogoTex = Texture_Load(L"resource/texture/clear_logo.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");
    g_DigitTex = Texture_Load(L"resource/texture/suji.png");
    g_CaptionTex = Texture_Load(L"resource/texture/enter_menu.png"); // 任意のキャプション画像
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

    // 背景（全面フィット）
    if (g_ClearBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_ClearBgTex);
        const float th = (float)Texture_Height(g_ClearBgTex);
        const float sx = (float)sw / (tw > 0 ? tw : 1.0f);
        const float sy = (float)sh / (th > 0 ? th : 1.0f);
        Sprite_Draw(g_ClearBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // ロゴ（中央やや上）
    if (g_ClearLogoTex >= 0)
    {
        const float lw = 720.0f;
        const float lh = 200.0f;
        const float lx = sw * 0.5f - lw * 0.5f;
        const float ly = sh * 0.18f;
        Sprite_Draw(g_ClearLogoTex, lx, ly, lw, lh, XMFLOAT4(1, 1, 1, 1));
    }

    // スコアパネル（半透明＋枠）
    float px = 0, py = 0, pw = 0, ph = 0;
    if (g_WhiteTex >= 0)
    {
        pw = 520.0f;
        ph = 120.0f;
        px = sw * 0.5f - pw * 0.5f;
        py = sh * 0.50f - ph * 0.5f;

        Sprite_Draw(g_WhiteTex, px, py, pw, ph, XMFLOAT4(0, 0, 0, 0.55f));
        Sprite_Draw(g_WhiteTex, px, py, pw, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px, py + ph - 3, pw, 3, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px, py, 3, ph, XMFLOAT4(1, 1, 1, 0.5f));
        Sprite_Draw(g_WhiteTex, px + pw - 3, py, 3, ph, XMFLOAT4(1, 1, 1, 0.5f));

        // 　拡大スコア（中央寄せ）
        if (g_DigitTex >= 0)
        {
            const int score = (int)Score_GetScore();
            const float digitsBaselineY = py + ph * 0.5f - DIGIT_DST_H * 0.5f; // パネル中央に拡大後の高さで合わせる
            DrawNumberLineCenteredScaled(g_DigitTex, score, sw * 0.5f, digitsBaselineY);
        }
    }

    // 下辺の帯
    float bx = 0, by = 0, bw = 0, bh = 0;
    if (g_WhiteTex >= 0)
    {
        bw = 420.0f;
        bh = 36.0f;
        bx = sw * 0.5f - bw * 0.5f;
        by = sh * 0.82f;

        Sprite_Draw(g_WhiteTex, bx, by, bw, bh, XMFLOAT4(1, 1, 1, 0.12f));

        // 帯の「上」にキャプション画像
        if (g_CaptionTex >= 0)
        {
            const float tw = (float)Texture_Width(g_CaptionTex);
            const float th = (float)Texture_Height(g_CaptionTex);

            const float margin = 10.0f;                  // 帯からの隙間
            const float cx = bx + bw * 0.5f - tw * 0.5f; // 帯中央に合わせる
            const float cy = by - th - margin;           // 上に浮かせる

            Sprite_Draw(g_CaptionTex, cx, cy, tw, th, XMFLOAT4(1, 1, 1, 1));
        }
    }
}
