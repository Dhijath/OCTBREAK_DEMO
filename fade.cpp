/*==============================================================================

   フェード [fade.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/15
--------------------------------------------------------------------------------
   実装。ヘッダに宣言したラッパーもここで定義しています。
==============================================================================*/
#include "fade.h"
using namespace DirectX;
#include <algorithm>
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"

// 内部状態（ファイル内静的）
static double     g_FadeTime{ 0.0 };
static double     g_FadeStartTime{ 0.0 };
static double     g_AccumulatedTime{ 0.0 };
static XMFLOAT3   g_Color{ 0.0f,0.0f, 0.0f };
static float      g_Alpha = 0.0f;
static FadeState  g_State = FADE_STATE_NONE;
static int        g_FadeTexID = -1;

void Fade_Initialize()
{
    g_FadeTime = 0.0;
    g_FadeStartTime = 0.0;
    g_AccumulatedTime = 0.0;
    g_Color = { 0.0f,0.0f, 0.0f };
    g_Alpha = 0.0f;
    g_State = FADE_STATE_NONE;

    // 白1pxなどの単色テクスチャを用意しておく（既存のパスに合わせてください）
    g_FadeTexID = Texture_Load(L"resource/texture/unnamed.jpg");
}

void Fade_Finalize()
{
    // 必要ならテクスチャ解放処理を書く（Texture モジュールで管理してるなら不要）
}

void Fade_Update(double elapsed_time)
{
    // Stateが「NONE or FINISHED_IN」なら更新不要
    if (g_State == FADE_STATE_NONE || g_State == FADE_STATE_FINISHED_IN) return;

    g_AccumulatedTime += elapsed_time;

    double ratio = 0.0;
    if (g_FadeTime > 0.0)
    {
        ratio = std::min((g_AccumulatedTime - g_FadeStartTime) / g_FadeTime, 1.0);
    }
    else
    {
        ratio = 1.0;
    }

    if (ratio >= 1.0)
    {
        g_State = (g_State == FADE_STATE_IN) ? FADE_STATE_FINISHED_IN : FADE_STATE_FINISHED_OUT;
    }

    g_Alpha = (float)(g_State == FADE_STATE_IN ? 1.0 - ratio : ratio);
}

void Fade_Draw()
{
    if (g_State == FADE_STATE_NONE) return;
    if (g_State == FADE_STATE_FINISHED_IN) return;
    if (g_FadeTexID < 0) return;  // テクスチャ未ロード時は描画しない

    // DirectWrite / MiniMap 等が RTV を切り離したまま戻さないケースがあるため
    // フェード描画前に必ずメインRTVを再バインドする（これで「偶に消える」問題を防ぐ）
    Direct3D_BindMainRenderTarget();

    // 深度テストを無効化してから描画（前段の処理で深度が戻っていても確実に最前面に出す）
    Direct3D_SetDepthEnable(false);
    // 2D正射影を確実にセット（フェードは常に最前面に描画する）
    Sprite_Begin();

    XMFLOAT4 color{ g_Color.x, g_Color.y, g_Color.z, g_Alpha };
    Sprite_Draw(
        g_FadeTexID,
        0.0f, 0.0f,
        (float)SPRITE_SCREEN_W,
        (float)SPRITE_SCREEN_H,
        color
    );
}

void Fade_Start(double time, bool isFadeOut, DirectX::XMFLOAT3 color)
{

    g_FadeStartTime = g_AccumulatedTime;
    g_FadeTime = time;
    g_State = isFadeOut ? FADE_STATE_OUT : FADE_STATE_IN;
    g_Color = color;
    g_Alpha = isFadeOut ? 0.0f : 1.0f;

}

// 現在のフェード状態を返す
FadeState Fade_GetState()
{
    return g_State;
}

/* ----------------------------
   便利ラッパー関数の実装
   ---------------------------- */

   // フェードアウト開始
void Fade_StartOut(double time, DirectX::XMFLOAT3 color)
{
    Fade_Start(time, true, color);

}

// フェードイン開始
void Fade_StartIn(double time, DirectX::XMFLOAT3 color)
{
    Fade_Start(time, false, color);
}

// フェードアウト（暗転）が完了したか
bool Fade_IsOutEnd()
{
    return Fade_GetState() == FADE_STATE_FINISHED_OUT;
}

// フェードイン（明転）が完了したか
bool Fade_IsInEnd()
{
    return Fade_GetState() == FADE_STATE_FINISHED_IN;
}

// フェードが進行中かどうか
bool Fade_IsActive()
{
    FadeState s = Fade_GetState();
    return !(s == FADE_STATE_NONE || s == FADE_STATE_FINISHED_IN);
}
