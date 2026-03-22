/*==============================================================================

   Direct3D 初期化まわり [direct3d.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/12
--------------------------------------------------------------------------------

==============================================================================*/

#include <d3d11.h>
#include "direct3d.h"
#include "debug_ostream.h"

#pragma comment(lib, "d3d11.lib")

// デバッグビルド時は DirectXTex のデバッグ版を使う
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment(lib, "DirectXTex_Debug.lib")
#else
#pragma comment(lib, "DirectXTex_Release.lib")
#endif

/* D3D 系のインターフェース */
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11BlendState* g_pBlendStateMultiply = nullptr;     // αブレンド用
static ID3D11BlendState* g_pBlendStateAdditive = nullptr;     // 加算合成用
static ID3D11DepthStencilState* g_pDepthStencilStateDepthDisable = nullptr; // 深度テスト無効
static ID3D11DepthStencilState* g_pDepthStencilStateDepthEnable = nullptr; // 深度テスト有効
static ID3D11DepthStencilState* g_pDepthStencilStateDepthWriteDisable = nullptr; // 深度書き込み禁止
static ID3D11RasterizerState* g_pRasterizerState = nullptr;   // ラスタライザーステート

/* バックバッファ関連 */
static ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
static ID3D11Texture2D* g_pDepthStencilBuffer = nullptr;
static ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
static D3D11_TEXTURE2D_DESC    g_BackBufferDesc{};
static D3D11_VIEWPORT          g_Viewport{};

// バックバッファ生成/解放（内部用）
static bool configureBackBuffer();
static void releaseBackBuffer();

/*========================================
    オフスクリーンレンダリング（ミニマップ等）
  ========================================*/
static ID3D11Texture2D* g_pOffScreenColorTexture = nullptr; // カラー
static ID3D11RenderTargetView* g_pOffScreenRenderTargetView = nullptr;
static ID3D11ShaderResourceView* g_pOffScreenSRV = nullptr;
static ID3D11Texture2D* g_pOffScreenDepthStencilBuffer = nullptr;
static ID3D11DepthStencilView* g_pOffScreenDepthStencilView = nullptr;
static D3D11_TEXTURE2D_DESC      g_OffScreenDesc{};
static D3D11_VIEWPORT            g_Viewport2{};

/*========================================
    シャドウマップ用
  ========================================*/
static ID3D11Texture2D* g_pShadowMapTexture = nullptr;
static ID3D11DepthStencilView* g_pShadowMapDSV = nullptr;
static ID3D11ShaderResourceView* g_pShadowMapSRV = nullptr;
static D3D11_VIEWPORT            g_ShadowViewport{};


/*==============================================================================

   Direct3D 初期化

==============================================================================*/
bool Direct3D_Initialize(HWND hWnd)
{
    // デバイス・スワップチェーン・コンテキストの生成設定
    DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
    swap_chain_desc.Windowed = TRUE;
    swap_chain_desc.BufferCount = 2;        // ダブルバッファ
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desc.OutputWindow = hWnd;

    UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;  // D2D1 interop に必須

#if defined(DEBUG) || defined(_DEBUG)
    device_flags |= D3D11_CREATE_DEVICE_DEBUG; // デバッグレイヤーON
#endif

    // 対応したい D3D の機能レベル一覧
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    // デバイスとスワップチェーン生成
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_flags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &g_pSwapChain,
        &g_pDevice,
        &feature_level,
        &g_pDeviceContext
    );

    if (FAILED(hr)) {
        MessageBox(hWnd, "Direct3D の初期化に失敗しました", "エラー", MB_OK);
        return false;
    }

    // DXGI のデフォルト Alt+Enter フルスクリーン処理を無効化
    // （ゲーム側でボーダーレスフルスクリーンを独自実装するため競合を防ぐ）
    {
        IDXGIFactory* pFactory = nullptr;
        if (SUCCEEDED(g_pSwapChain->GetParent(__uuidof(IDXGIFactory), (void**)&pFactory)))
        {
            pFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
            pFactory->Release();
        }
    }

    // バックバッファ設定
    if (!configureBackBuffer()) {
        MessageBox(hWnd, "バックバッファの設定に失敗しました", "エラー", MB_OK);
        return false;
    }

    // αブレンドの設定
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;

    // RGB の合成方法（一般的な透過合成）
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    // αチャンネルの合成
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    // カラー書き込みマスク
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = g_pDevice->CreateBlendState(&bd, &g_pBlendStateMultiply);
    if (FAILED(hr)) {
        MessageBox(hWnd, "ブレンドステートの作成に失敗しました", "エラー", MB_OK);
        return false;
    }

    // αブレンドをひとまず有効化
    float blend_factor[4] = { 0, 0, 0, 0 };
    g_pDeviceContext->OMSetBlendState(g_pBlendStateMultiply, blend_factor, 0xffffffff);

    D3D11_BLEND_DESC bdAdditive = {};  //  ゼロクリア
    bdAdditive.AlphaToCoverageEnable = FALSE;
    bdAdditive.IndependentBlendEnable = FALSE;
    bdAdditive.RenderTarget[0].BlendEnable = TRUE;
    bdAdditive.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;         //  ONE
    bdAdditive.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;        //  ONE（加算）
    bdAdditive.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bdAdditive.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bdAdditive.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bdAdditive.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bdAdditive.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = g_pDevice->CreateBlendState(&bdAdditive, &g_pBlendStateAdditive);
    if (FAILED(hr)) {
        MessageBox(hWnd, "加算合成ブレンドステートの作成に失敗しました", "エラー", MB_OK);
        return false;
    }

    // 深度ステート設定（有効／無効）
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.StencilEnable = FALSE;

    // 深度テスト無効
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthDisable);

    // 深度テスト有効（通常）
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthEnable);

    // 深度テストするけど書き込まない（パーティクル用とか）
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    g_pDevice->CreateDepthStencilState(&dsd, &g_pDepthStencilStateDepthWriteDisable);

    Direct3D_SetDepthEnable(true);

    // ラスタライザステート
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;

    g_pDevice->CreateRasterizerState(&rd, &g_pRasterizerState);
    g_pDeviceContext->RSSetState(g_pRasterizerState);

    //  オフスクリーン＆シャドウマップは必要になったタイミングで
    // ゲーム側から Direct3D_ConfigureOffScreenBuffer / Direct3D_ConfigureShadowMap を呼んでもらう想定。
    // ここで作りたい場合は、下を解禁してサイズを決めればOK。
    /*
    Direct3D_ConfigureOffScreenBuffer();
    Direct3D_ConfigureShadowMap(1024);
    */

    return true;
}


/*==============================================================================

   Direct3D 解放

==============================================================================*/
void Direct3D_Finalize()
{

    Direct3D_ReleaseShadowMap();
    Direct3D_ReleaseOffScreenBuffer();

    SAFE_RELEASE(g_pDepthStencilStateDepthDisable);
    SAFE_RELEASE(g_pDepthStencilStateDepthEnable);
    SAFE_RELEASE(g_pDepthStencilStateDepthWriteDisable);
    SAFE_RELEASE(g_pBlendStateMultiply);
    SAFE_RELEASE(g_pBlendStateAdditive);
    SAFE_RELEASE(g_pRasterizerState);

    releaseBackBuffer();

    SAFE_RELEASE(g_pSwapChain);
    SAFE_RELEASE(g_pDeviceContext);
    SAFE_RELEASE(g_pDevice);
}


/*==============================================================================

   バッファクリア

==============================================================================*/
void Direct3D_Clear()
{
    float clear_color[4] = { 0.2f, 0.4f, 0.8f, 1.0f };

    g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear_color);
    g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // レンダーターゲットと深度バッファをセット
    g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    //  重要：通常のビューポートを毎回強制でセット（ミニマップで変わっても戻す）
    g_pDeviceContext->RSSetViewports(1, &g_Viewport);
}



/*==============================================================================

   メインRTV+DSV+ビューポートの復元
   シャドウパス後に呼ぶことでバックバッファ描画に戻す

==============================================================================*/
void Direct3D_BindMainRenderTarget()
{
    g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
    g_pDeviceContext->RSSetViewports(1, &g_Viewport);
}

/*==============================================================================

   画面更新（Present）

==============================================================================*/
void Direct3D_Present()
{
    // 画面表示（VSync あり）
    g_pSwapChain->Present(1, 0);
}


/*==============================================================================

   各種 Getter

==============================================================================*/
unsigned int Direct3D_GetBackBufferWidth()
{
    return g_BackBufferDesc.Width;
}

unsigned int Direct3D_GetBackBufferHeight()
{
    return g_BackBufferDesc.Height;
}

ID3D11Device* Direct3D_GetDevice()
{
    return g_pDevice;
}

ID3D11DeviceContext* Direct3D_GetContext()
{
    return g_pDeviceContext;
}

IDXGISwapChain* Direct3D_GetSwapChain()
{
    return g_pSwapChain;
}


/*==============================================================================

   深度テスト / ブレンドステート

==============================================================================*/
void Direct3D_SetDepthEnable(bool enable)
{
    if (enable)
        g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthEnable, 0);
    else
        g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthDisable, 0);
}

// αブレンドのオン・オフ
void Direct3D_SetBlendState(bool enable)
{
    float blend_factor[4] = { 0,0,0,0 };

    if (enable)
        g_pDeviceContext->OMSetBlendState(g_pBlendStateMultiply, blend_factor, 0xffffffff);
    else
        g_pDeviceContext->OMSetBlendState(nullptr, blend_factor, 0xffffffff);
}

// 加算合成のオン・オフ
void Direct3D_SetBlendStateAdditive(bool enable)
{
    float blend_factor[4] = { 0, 0, 0, 0 };

    if (enable)
        g_pDeviceContext->OMSetBlendState(g_pBlendStateAdditive, blend_factor, 0xffffffff);
    else
        g_pDeviceContext->OMSetBlendState(g_pBlendStateMultiply, blend_factor, 0xffffffff);
}


// 深度テストはするけど深度書き込みだけ止める
void Direct3D_SetDepthStencilStateDepthWriteDisable(bool enable)
{
    // 
    if (enable)
        g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthEnable, 0);
    else
        g_pDeviceContext->OMSetDepthStencilState(g_pDepthStencilStateDepthWriteDisable, 0);
}



/*==============================================================================

   バックバッファ生成＆解放（内部用）

==============================================================================*/
bool configureBackBuffer()
{
    HRESULT hr;
    ID3D11Texture2D* back_buffer_pointer = nullptr;

    // バックバッファの取り出し
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer_pointer);
    if (FAILED(hr)) {
        hal::dout << "バックバッファ取得に失敗" << std::endl;
        return false;
    }

    // RTV 作成
    hr = g_pDevice->CreateRenderTargetView(back_buffer_pointer, nullptr, &g_pRenderTargetView);
    if (FAILED(hr)) {
        back_buffer_pointer->Release();
        hal::dout << "レンダーターゲットビュー作成に失敗" << std::endl;
        return false;
    }

    // バックバッファの情報を読む
    back_buffer_pointer->GetDesc(&g_BackBufferDesc);

    back_buffer_pointer->Release();

    // 深度バッファ作成
    D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.Width = g_BackBufferDesc.Width;
    depth_desc.Height = g_BackBufferDesc.Height;
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = g_pDevice->CreateTexture2D(&depth_desc, nullptr, &g_pDepthStencilBuffer);
    if (FAILED(hr)) {
        hal::dout << "深度バッファ作成に失敗" << std::endl;
        return false;
    }

    // 深度ビュー作成
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = depth_desc.Format;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = 0;

    hr = g_pDevice->CreateDepthStencilView(g_pDepthStencilBuffer, &dsv_desc, &g_pDepthStencilView);
    if (FAILED(hr)) {
        hal::dout << "深度ビュー作成に失敗" << std::endl;
        return false;
    }

    // ビューポート設定
    g_Viewport.TopLeftX = 0.0f;
    g_Viewport.TopLeftY = 0.0f;
    g_Viewport.Width = static_cast<float>(g_BackBufferDesc.Width);
    g_Viewport.Height = static_cast<float>(g_BackBufferDesc.Height);
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;

    g_pDeviceContext->RSSetViewports(1, &g_Viewport);

    return true;
}

void releaseBackBuffer()
{
    SAFE_RELEASE(g_pRenderTargetView);
    SAFE_RELEASE(g_pDepthStencilBuffer);
    SAFE_RELEASE(g_pDepthStencilView);
}

/*==============================================================================

   バックバッファ リサイズ（フルスクリーン切替・WM_SIZE 時）

==============================================================================*/
bool Direct3D_ResizeBackBuffer(unsigned int width, unsigned int height)
{
    if (!g_pSwapChain)         return true;  // 初期化前は無視
    if (width == 0 || height == 0) return true;  // 最小化は無視

    // RTV / DSV をコンテキストからアンバインドしてから解放
    // （バインドしたまま ResizeBuffers を呼ぶと D3D 警告が出る）
    g_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    releaseBackBuffer();

    // GPU コマンドキューを強制フラッシュ
    // （ResizeBuffers 前に GPU 側の参照を確実に終わらせる）
    g_pDeviceContext->Flush();

    // バッファ数・フォーマットはそのまま（0 / UNKNOWN で引き継ぎ）
    HRESULT hr = g_pSwapChain->ResizeBuffers(
        0,                       // バッファ数（現在値を維持）
        width, height,           // 新しいサイズ
        DXGI_FORMAT_UNKNOWN,     // フォーマット（現在値を維持）
        0                        // フラグなし
    );

    if (FAILED(hr))
    {
        hal::dout << "ResizeBuffers 失敗: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // RTV / DSV / Viewport を再生成
    return configureBackBuffer();
}


/*==============================================================================

   オフスクリーン（ミニマップ）用

==============================================================================*/

// バッファ生成
bool Direct3D_ConfigureOffScreenBuffer()
{
    HRESULT hr;

    // すでに存在していたら一度解放
    Direct3D_ReleaseOffScreenBuffer();

    // ---------- カラーテクスチャ ----------
    ZeroMemory(&g_OffScreenDesc, sizeof(g_OffScreenDesc));

    // ミニマップ用：正方形に固定（512×512 or 1024×1024）
    const unsigned int minimapSize = 512;  // 解像度を固定

    g_OffScreenDesc.Width = minimapSize;
    g_OffScreenDesc.Height = minimapSize;
    g_OffScreenDesc.MipLevels = 1;
    g_OffScreenDesc.ArraySize = 1;
    g_OffScreenDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_OffScreenDesc.SampleDesc.Count = 1;
    g_OffScreenDesc.SampleDesc.Quality = 0;
    g_OffScreenDesc.Usage = D3D11_USAGE_DEFAULT;
    g_OffScreenDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    g_OffScreenDesc.CPUAccessFlags = 0;
    g_OffScreenDesc.MiscFlags = 0;

    hr = g_pDevice->CreateTexture2D(&g_OffScreenDesc, nullptr, &g_pOffScreenColorTexture);
    if (FAILED(hr)) {
        hal::dout << "オフスクリーン用カラーTexture2D作成に失敗" << std::endl;
        return false;
    }

    // RTV
    hr = g_pDevice->CreateRenderTargetView(g_pOffScreenColorTexture, nullptr, &g_pOffScreenRenderTargetView);
    if (FAILED(hr)) {
        hal::dout << "オフスクリーン用RTV作成に失敗" << std::endl;
        return false;
    }

    // SRV（ミニマップとして貼る用）
    hr = g_pDevice->CreateShaderResourceView(g_pOffScreenColorTexture, nullptr, &g_pOffScreenSRV);
    if (FAILED(hr)) {
        hal::dout << "オフスクリーン用SRV作成に失敗" << std::endl;
        return false;
    }

    // ---------- 深度バッファ ----------
    D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.Width = minimapSize;
    depth_desc.Height = minimapSize;
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_desc.CPUAccessFlags = 0;
    depth_desc.MiscFlags = 0;

    hr = g_pDevice->CreateTexture2D(&depth_desc, nullptr, &g_pOffScreenDepthStencilBuffer);
    if (FAILED(hr)) {
        hal::dout << "オフスクリーン用深度バッファ作成に失敗" << std::endl;
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = depth_desc.Format;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = 0;

    hr = g_pDevice->CreateDepthStencilView(g_pOffScreenDepthStencilBuffer, &dsv_desc, &g_pOffScreenDepthStencilView);
    if (FAILED(hr)) {
        hal::dout << "オフスクリーン用DSV作成に失敗" << std::endl;
        return false;
    }

    // ビューポート（正方形）
    g_Viewport2.TopLeftX = 0.0f;
    g_Viewport2.TopLeftY = 0.0f;
    g_Viewport2.Width = static_cast<float>(minimapSize);
    g_Viewport2.Height = static_cast<float>(minimapSize);
    g_Viewport2.MinDepth = 0.0f;
    g_Viewport2.MaxDepth = 1.0f;

    return true;
}

// オフスクリーン解放
void Direct3D_ReleaseOffScreenBuffer()
{
    SAFE_RELEASE(g_pOffScreenSRV);
    SAFE_RELEASE(g_pOffScreenRenderTargetView);
    SAFE_RELEASE(g_pOffScreenColorTexture);
    SAFE_RELEASE(g_pOffScreenDepthStencilView);
    SAFE_RELEASE(g_pOffScreenDepthStencilBuffer);
}

// オフスクリーン SRV を返す（ミニマップ用）
ID3D11ShaderResourceView* Direct3D_GetOffScreenSRV()
{
    return g_pOffScreenSRV;
}

// オフスクリーン用ビューポート
const D3D11_VIEWPORT* Direct3D_GetOffScreenViewport()
{
    return &g_Viewport2;
}


/*==============================================================================

   シャドウマップ用

==============================================================================*/

// シャドウマップ生成
bool Direct3D_ConfigureShadowMap(unsigned int size)
{
    HRESULT hr;

    // 既存があれば一度解放
    Direct3D_ReleaseShadowMap();

    // typeless depth texture
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = size;
    texDesc.Height = size;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    hr = g_pDevice->CreateTexture2D(&texDesc, nullptr, &g_pShadowMapTexture);
    if (FAILED(hr)) {
        hal::dout << "シャドウマップ用Texture2D作成に失敗" << std::endl;
        return false;
    }

    // DSV（ライト視点描画用）
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = g_pDevice->CreateDepthStencilView(g_pShadowMapTexture, &dsvDesc, &g_pShadowMapDSV);
    if (FAILED(hr)) {
        hal::dout << "シャドウマップ用DSV作成に失敗" << std::endl;
        return false;
    }

    // SRV（PS でサンプルする用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_pDevice->CreateShaderResourceView(g_pShadowMapTexture, &srvDesc, &g_pShadowMapSRV);
    if (FAILED(hr)) {
        hal::dout << "シャドウマップ用SRV作成に失敗" << std::endl;
        return false;
    }

    // ビューポート（ライト視点）
    g_ShadowViewport.TopLeftX = 0.0f;
    g_ShadowViewport.TopLeftY = 0.0f;
    g_ShadowViewport.Width = static_cast<float>(size);
    g_ShadowViewport.Height = static_cast<float>(size);
    g_ShadowViewport.MinDepth = 0.0f;
    g_ShadowViewport.MaxDepth = 1.0f;

    return true;
}

// シャドウマップ解放
void Direct3D_ReleaseShadowMap()
{
    SAFE_RELEASE(g_pShadowMapSRV);
    SAFE_RELEASE(g_pShadowMapDSV);
    SAFE_RELEASE(g_pShadowMapTexture);
}

// シャドウマップ SRV
ID3D11ShaderResourceView* Direct3D_GetShadowMapSRV()
{
    return g_pShadowMapSRV;
}

// シャドウマップ用ビューポート
const D3D11_VIEWPORT* Direct3D_GetShadowMapViewport()
{
    return &g_ShadowViewport;
}

// 保存用（一時的に現在のRTV/DSVとビューポートを退避）
static ID3D11RenderTargetView* g_pSavedRTV = nullptr;
static ID3D11DepthStencilView* g_pSavedDSV = nullptr;
static D3D11_VIEWPORT          g_SavedViewport{};



void Direct3D_BeginOffScreen()
{
    if (!g_pDeviceContext || !g_pOffScreenRenderTargetView || !g_pOffScreenDepthStencilView)
        return;

    SAFE_RELEASE(g_pSavedRTV);
    SAFE_RELEASE(g_pSavedDSV);

    g_pDeviceContext->OMGetRenderTargets(1, &g_pSavedRTV, &g_pSavedDSV);

    UINT n = 1;
    g_pDeviceContext->RSGetViewports(&n, &g_SavedViewport);

    g_pDeviceContext->OMSetRenderTargets(1, &g_pOffScreenRenderTargetView, g_pOffScreenDepthStencilView);
    g_pDeviceContext->RSSetViewports(1, &g_Viewport2);

    float clear_color[4] = { 0,0,0,1 };
    g_pDeviceContext->ClearRenderTargetView(g_pOffScreenRenderTargetView, clear_color);
    g_pDeviceContext->ClearDepthStencilView(g_pOffScreenDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Direct3D_EndOffScreen()
{
    if (!g_pDeviceContext) return;

    if (g_pSavedRTV || g_pSavedDSV)
    {
        g_pDeviceContext->OMSetRenderTargets(1, &g_pSavedRTV, g_pSavedDSV);
        g_pDeviceContext->RSSetViewports(1, &g_SavedViewport);
    }

    SAFE_RELEASE(g_pSavedRTV);
    SAFE_RELEASE(g_pSavedDSV);
}



    