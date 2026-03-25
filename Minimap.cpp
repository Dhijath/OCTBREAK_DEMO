/*==============================================================================

   ミニマップ制御 [Minimap.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/16
--------------------------------------------------------------------------------

==============================================================================*/

#include "player.h"
#include "direct3d.h"
#include "player_camera.h"
#include "map.h"
#include "sprite.h"
#include "Minimap.h"
#include "texture.h"


//==============================================================================
// ミニマップ：3D描画
//==============================================================================

int g_MinimapframeTexID = -1;  //ミニマップのフレーム用テクスチャ


void MiniMap_Render3D()
{
    Direct3D_BeginOffScreen();
    const DirectX::XMFLOAT3 center = Player_GetPosition();
    Player_Camera_SetMiniMapTopDown(center, 100.0f, 60.0f);

    Map_DrawForMinimap(); 
    Player_DrawMarker();

    Player_Camera_ApplyMainViewProj();
    Direct3D_EndOffScreen();
}


//==============================================================================
// ミニマップ：2D描画
//==============================================================================
void MiniMap_Draw2D()
{
    //============================================================
    // 2D描画用にレンダーステートを調整
    //============================================================

    // ミニマップはUIなので深度テストを無効化
    // → 3DオブジェクトとのZ競合を防ぐ
    Direct3D_SetDepthEnable(false);

    // PNGフレームやミニマップ表示のためαブレンドを有効化
    Direct3D_SetBlendState(true);

    //============================================================
    // ミニマップ描画元（オフスクリーン）のSRV取得
    //============================================================

    // 事前に描画してあるミニマップ用オフスクリーンテクスチャ
    ID3D11ShaderResourceView* srv = Direct3D_GetOffScreenSRV();

    // SRVが無効な場合は何も描画せず終了
    if (!srv) return;

    //============================================================
    // 画面サイズ・配置パラメータ計算
    //============================================================

    // バックバッファ横幅（右上配置用）
    const float screenW = (float)SPRITE_SCREEN_W;

    // ミニマップ本体の表示サイズ（正方形）
    const float mapSize = 270.0f;

    // 画面端からの余白
    const float margin = 20.0f;

    // ミニマップ本体の左上座標（右上配置）
    const float sx = screenW - mapSize - margin;
    const float sy = margin;

    // フレーム用の余白サイズ
    // フレームをミニマップより一回り大きくするための値
    const float fsxy = 100.0f;

    // フレーム全体の描画サイズ
    const float frameSize = mapSize + fsxy;

    // フレームの左上座標
    // ミニマップの中心にフレームが来るよう半分ずらす
    const float frameSx = sx - fsxy / 2;
    const float frameSy = sy - fsxy / 2;



    //============================================================
    // ミニマップ本体描画（SRV直接描画）
    //============================================================

    // 正方形用スプライト描画モード開始
    // ミニマップ背景（黒みがかった緑の板）
    Sprite_Begin();
    Sprite_Draw(
        Map_GetWiteTexID(),
        sx, sy,
        mapSize, mapSize,
        { 0.0f, 0.15f, 0.05f, 1.0f }
    );
    // → UV指定描画に対応した描画モード
    Sprite_BeginSquare();

    // オフスクリーンが16:9想定のため、
    // 横方向をトリミングして正方形に切り出す
    float uvLeft = (1.0f - 9.0f / 16.0f) * 0.5f;
    float uvRight = 1.0f - uvLeft;

    // SRVを指定UV範囲で描画
    // 黒を透過するため加算合成に切り替え
    ID3D11Device* dev = Direct3D_GetDevice();
    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* additiveBlend = nullptr;
    dev->CreateBlendState(&blendDesc, &additiveBlend);

    ID3D11BlendState* oldBlend = nullptr;
    FLOAT oldFactor[4];
    UINT  oldMask;
    ctx->OMGetBlendState(&oldBlend, oldFactor, &oldMask);

    ctx->OMSetBlendState(additiveBlend, nullptr, 0xFFFFFFFF);

    Sprite_DrawSRV_UV(
        srv,
        sx, sy,
        mapSize, mapSize,
        uvLeft, 0.0f,
        uvRight, 1.0f,
        { 1,1,1,1 }
    );

    // ブレンドステートを元に戻す
    ctx->OMSetBlendState(oldBlend, oldFactor, oldMask);
    SAFE_RELEASE(additiveBlend);
    SAFE_RELEASE(oldBlend);
    // 通常スプライト描画開始
    Sprite_Begin();


    //============================================================
    // レンダーステート復帰
    //============================================================

    // 以降の3D描画のため深度テストを再度有効化
    Direct3D_SetDepthEnable(true);
}
