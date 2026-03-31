/*==============================================================================

   Direct3D 周りの処理まとめ [direct3d.h]
                                                         Author : 51106
                                                         Date   : 2026/02/12
--------------------------------------------------------------------------------

==============================================================================*/

#ifndef DIRECT3D_H
#define DIRECT3D_H

#include <d3d11.h>
#include <Windows.h>

// 安全に Release するためのマクロ
#define SAFE_RELEASE(o) if (o) { (o)->Release(); o = NULL; }

// Direct3D の初期化
bool Direct3D_Initialize(HWND hWnd);

// Direct3D の終了処理
void Direct3D_Finalize();

// バックバッファのクリア
void Direct3D_Clear();

// バックバッファの画面表示
void Direct3D_Present();

// バックバッファのリサイズ（フルスクリーン切替・WM_SIZE 時に呼ぶ）
// width / height が 0 の場合（最小化など）は何もしない
bool Direct3D_ResizeBackBuffer(unsigned int width, unsigned int height);

// バックバッファの横幅を返す
unsigned int Direct3D_GetBackBufferWidth();

// バックバッファの高さを返す
unsigned int Direct3D_GetBackBufferHeight();

// D3D デバイス取得
ID3D11Device* Direct3D_GetDevice();

// D3D デバイスコンテキスト取得
ID3D11DeviceContext* Direct3D_GetContext();

// スワップチェーン取得
IDXGISwapChain* Direct3D_GetSwapChain();

// 深度テストの ON / OFF
void Direct3D_SetDepthEnable(bool enable);

// ブレンドステートの ON / OFF（αブレンド）
void Direct3D_SetBlendState(bool enable);

//  加算合成
void Direct3D_SetBlendStateAdditive(bool enable);

// ミニマップマーカー用（αチャンネル非書き込み・RGB のみαブレンド）
void Direct3D_SetBlendStateMarker();

// 深度書き込みだけ禁止するステートの切り替え
void Direct3D_SetDepthStencilStateDepthWriteDisable(bool enable);



// ==================================================
// オフスクリーン（ミニマップ用）
// ==================================================

// オフスクリーン用のバッファ生成・解放
bool Direct3D_ConfigureOffScreenBuffer();
void Direct3D_ReleaseOffScreenBuffer();

// ミニマップとして貼るテクスチャ（SRV）を取得
ID3D11ShaderResourceView* Direct3D_GetOffScreenSRV();

// 必要なら、オフスクリーン描画用ビューポートも取得
const D3D11_VIEWPORT* Direct3D_GetOffScreenViewport();


// ==================================================
// シャドウマップ用
// ==================================================

// シャドウマップの作成・解放
bool Direct3D_ConfigureShadowMap(unsigned int size);
void Direct3D_ReleaseShadowMap();

// シャドウマップをサンプリングする SRV
ID3D11ShaderResourceView* Direct3D_GetShadowMapSRV();

// シャドウマップ描画用ビューポート取得
const D3D11_VIEWPORT* Direct3D_GetShadowMapViewport();

// オフスクリーン描画の開始／終了
void Direct3D_BeginOffScreen();
void Direct3D_EndOffScreen();

// オフスクリーンのSRV取得（ミニマップ貼り付け用）
ID3D11ShaderResourceView* Direct3D_GetOffScreenSRV();

// シャドウパス後にメインRTV+DSV+ビューポートを復元する
void Direct3D_BindMainRenderTarget();

// 深度バッファ全体をクリア（HUDミニプレビュー描画前に呼ぶ）
void Direct3D_ClearDepth();


#endif // DIRECT3D_H

