/*==============================================================================

   クリア画面 [Clear.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景＋「GAME CLEAR」（TextLogo）＋ リザルトパネル（ResultPanel_Draw）
   入力は Game_Manager 側。
==============================================================================*/
#include "Clear.h"
#include "Result.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "text_logo.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
using namespace DirectX;

static int g_ClearBgTex = -1;

void Clear_Initialize()
{
    g_ClearBgTex = Texture_Load(L"resource/texture/title_bg.png");
    Result_Initialize(); // パネル用リソースを共用
}

void Clear_Finalize()
{
    // パネルリソースは Result_Finalize に委譲（Game_Manager が呼ぶ）
}

void Clear_Update(double) {}

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

    // タイトル
    {
        LogoStyle s;
        s.fontSize     = 100.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.50f, 1.00f, 0.90f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.05f, 0.60f, 0.80f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.00f, 0.10f, 0.15f, 1.0f);
        s.outlineWidth = 3.5f;
        TextLogo_Draw(L"GAME CLEAR", sw * 0.5f, sh * 0.18f, s);
    }

    ResultPanel_Draw();
}
