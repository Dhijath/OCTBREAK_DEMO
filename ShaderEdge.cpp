/*==============================================================================

   エッジ検出シェーダー管理 [ShaderEdge.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/11
--------------------------------------------------------------------------------
   - 法線書き出しパス + エッジ検出パスの2段構成
   - 法線バッファ : 独自に作成（オフスクリーンとは別）
   - 深度バッファ : バックバッファの深度テクスチャを流用
                   ※ DXGI_FORMAT_R24G8_TYPELESS で作り直しが必要な場合あり
                     （direct3d.cpp の深度フォーマットに依存）

==============================================================================*/
#include "ShaderEdge.h"
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

namespace
{
    ID3D11Device* g_pDevice = nullptr;
    ID3D11DeviceContext* g_pContext = nullptr;

    //--------------------------------------------------------------------------
    // 法線書き出し用バッファ
    //--------------------------------------------------------------------------
    ID3D11Texture2D* g_pNormalTex = nullptr;
    ID3D11RenderTargetView* g_pNormalRTV = nullptr;
    ID3D11ShaderResourceView* g_pNormalSRV = nullptr;

    //--------------------------------------------------------------------------
    // 深度読み取り用
    // direct3d.cpp の深度バッファを SRV として読むために
    // TYPELESS フォーマットで別途作成する
    //--------------------------------------------------------------------------
    ID3D11Texture2D* g_pDepthTex = nullptr;
    ID3D11DepthStencilView* g_pDepthDSV = nullptr;
    ID3D11ShaderResourceView* g_pDepthSRV = nullptr;

    //--------------------------------------------------------------------------
    // 法線書き出し VS/PS
    //--------------------------------------------------------------------------
    ID3D11VertexShader* g_pNormalVS = nullptr; // shader_vertex_3d.cso 流用
    ID3D11PixelShader* g_pNormalPS = nullptr; // shader_pixel_normal.cso
    ID3D11InputLayout* g_pLayout = nullptr;

    //--------------------------------------------------------------------------
    // エッジ検出 VS/PS
    //--------------------------------------------------------------------------
    ID3D11VertexShader* g_pEdgeVS = nullptr; // shader_vertex_edge.cso
    ID3D11PixelShader* g_pEdgePS = nullptr; // shader_pixel_edge.cso

    //--------------------------------------------------------------------------
    // 定数バッファ（エッジパラメータ）
    //--------------------------------------------------------------------------
    struct EdgeParam
    {
        float     depthThreshold;
        float     normalThreshold;
        XMFLOAT2  texelSize;
        XMFLOAT4  edgeColor;
    };
    ID3D11Buffer* g_pCB_EdgeParam = nullptr;

    //--------------------------------------------------------------------------
    // 法線書き出し用 VS 定数バッファ（World/View/Proj）
    //--------------------------------------------------------------------------
    ID3D11Buffer* g_pCB_World = nullptr;
    ID3D11Buffer* g_pCB_View = nullptr;
    ID3D11Buffer* g_pCB_Proj = nullptr;

    //--------------------------------------------------------------------------
    // サンプラー（ポイントサンプリング）
    //--------------------------------------------------------------------------
    ID3D11SamplerState* g_pSampler = nullptr;

    //--------------------------------------------------------------------------
    // 退避用（法線パス中に状態を保存）
    //--------------------------------------------------------------------------
    ID3D11RenderTargetView* g_pSavedRTV = nullptr;
    ID3D11DepthStencilView* g_pSavedDSV = nullptr;
    ID3D11VertexShader* g_pSavedVS = nullptr;
    ID3D11PixelShader* g_pSavedPS = nullptr;
    ID3D11InputLayout* g_pSavedLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY g_SavedTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ID3D11Buffer* g_pSavedVSCB[3] = { nullptr, nullptr, nullptr };

    //--------------------------------------------------------------------------
    // ブレンドステート（エッジをアルファ合成）
    //--------------------------------------------------------------------------
    ID3D11BlendState* g_pBlendState = nullptr;

    //--------------------------------------------------------------------------
    // ラスタライザーステート（深度クリップ無効・カリングなし）
    //--------------------------------------------------------------------------
    ID3D11RasterizerState* g_pRSNoCull = nullptr;

    //--------------------------------------------------------------------------
    // ヘルパー：CSO ロード
    //--------------------------------------------------------------------------
    static bool LoadCSO(const char* path, unsigned char** ppData, size_t* pSize)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return false;
        ifs.seekg(0, std::ios::end);
        *pSize = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0, std::ios::beg);
        *ppData = new unsigned char[*pSize];
        ifs.read(reinterpret_cast<char*>(*ppData), *pSize);
        return true;
    }

    //--------------------------------------------------------------------------
    // ヘルパー：定数バッファ作成
    //--------------------------------------------------------------------------
    static ID3D11Buffer* CreateCB(UINT byteWidth)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = byteWidth;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.Usage = D3D11_USAGE_DEFAULT;
        ID3D11Buffer* pBuf = nullptr;
        g_pDevice->CreateBuffer(&desc, nullptr, &pBuf);
        return pBuf;
    }

    ID3D11DepthStencilState* g_pDSSDepthDisable = nullptr;
}

//==============================================================================
// 初期化
//==============================================================================
bool ShaderEdge_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) return false;
    g_pDevice = pDevice;
    g_pContext = pContext;

    const UINT W = Direct3D_GetBackBufferWidth();
    const UINT H = Direct3D_GetBackBufferHeight();

    HRESULT hr;
    unsigned char* pData = nullptr;
    size_t         size = 0;



    //--------------------------------------------------------------------------
    // 法線テクスチャ（RTV + SRV）
    //--------------------------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = W;
        desc.Height = H;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;



        hr = g_pDevice->CreateTexture2D(&desc, nullptr, &g_pNormalTex);
        if (FAILED(hr)) return false;

        hr = g_pDevice->CreateRenderTargetView(g_pNormalTex, nullptr, &g_pNormalRTV);
        if (FAILED(hr)) return false;

        hr = g_pDevice->CreateShaderResourceView(g_pNormalTex, nullptr, &g_pNormalSRV);
        if (FAILED(hr)) return false;
    }

    //--------------------------------------------------------------------------
    // 深度テクスチャ（DSV + SRV）
    // R24G8_TYPELESS で作成してシェーダーから読めるようにする
    //--------------------------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = W;
        desc.Height = H;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        hr = g_pDevice->CreateTexture2D(&desc, nullptr, &g_pDepthTex);
        if (FAILED(hr)) return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        hr = g_pDevice->CreateDepthStencilView(g_pDepthTex, &dsvDesc, &g_pDepthDSV);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = g_pDevice->CreateShaderResourceView(g_pDepthTex, &srvDesc, &g_pDepthSRV);
        if (FAILED(hr)) return false;
    }

    //--------------------------------------------------------------------------
    // 法線書き出し VS（shader_vertex_3d.cso 流用）
    //--------------------------------------------------------------------------
    if (!LoadCSO("resource/shader/shader_vertex_3d.cso", &pData, &size))
    {
        MessageBox(nullptr, "shader_vertex_3d.cso not found (ShaderEdge)", "error", MB_OK);
        return false;
    }
    hr = g_pDevice->CreateVertexShader(pData, size, nullptr, &g_pNormalVS);
    if (FAILED(hr)) { delete[] pData; return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pData, size, &g_pLayout);
    delete[] pData;
    if (FAILED(hr)) return false;

    //--------------------------------------------------------------------------
    // 法線書き出し PS（shader_pixel_normal.cso）
    //--------------------------------------------------------------------------
    if (!LoadCSO("resource/shader/shader_pixel_normal.cso", &pData, &size))
    {
        MessageBox(nullptr, "shader_pixel_normal.cso not found", "error", MB_OK);
        return false;
    }
    hr = g_pDevice->CreatePixelShader(pData, size, nullptr, &g_pNormalPS);
    delete[] pData;
    if (FAILED(hr)) return false;

    //--------------------------------------------------------------------------
    // エッジ検出 VS（shader_vertex_edge.cso）
    //--------------------------------------------------------------------------
    if (!LoadCSO("resource/shader/shader_vertex_edge.cso", &pData, &size))
    {
        MessageBox(nullptr, "shader_vertex_edge.cso not found", "error", MB_OK);
        return false;
    }
    hr = g_pDevice->CreateVertexShader(pData, size, nullptr, &g_pEdgeVS);
    delete[] pData;
    if (FAILED(hr)) return false;

    //--------------------------------------------------------------------------
    // エッジ検出 PS（shader_pixel_edge.cso）
    //--------------------------------------------------------------------------
    if (!LoadCSO("resource/shader/shader_pixel_edge.cso", &pData, &size))
    {
        MessageBox(nullptr, "shader_pixel_edge.cso not found", "error", MB_OK);
        return false;
    }
    hr = g_pDevice->CreatePixelShader(pData, size, nullptr, &g_pEdgePS);
    delete[] pData;
    if (FAILED(hr)) return false;

    //--------------------------------------------------------------------------
    // 定数バッファ
    //--------------------------------------------------------------------------
    g_pCB_EdgeParam = CreateCB(sizeof(EdgeParam));
    g_pCB_World = CreateCB(sizeof(XMFLOAT4X4));
    g_pCB_View = CreateCB(sizeof(XMFLOAT4X4));
    g_pCB_Proj = CreateCB(sizeof(XMFLOAT4X4));

    //--------------------------------------------------------------------------
    // ポイントサンプラー
    //--------------------------------------------------------------------------
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        g_pDevice->CreateSamplerState(&sd, &g_pSampler);
    }

    //--------------------------------------------------------------------------
    // ブレンドステート（アルファ合成）
    //--------------------------------------------------------------------------
    {
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        g_pDevice->CreateBlendState(&bd, &g_pBlendState);
    }

    //--------------------------------------------------------------------------
    // ラスタライザー（カリングなし・深度クリップ無効）
    //--------------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = FALSE;
        g_pDevice->CreateRasterizerState(&rd, &g_pRSNoCull);
    }

    //--------------------------------------------------------------------------
    // 深度テスト無効DSS（エッジ描画用）
    //--------------------------------------------------------------------------
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = FALSE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        g_pDevice->CreateDepthStencilState(&dsd, &g_pDSSDepthDisable);
    }



    //--------------------------------------------------------------------------
    // デフォルトパラメータ
    //--------------------------------------------------------------------------
    ShaderEdge_SetParam(0.01f, 0.4f, { 0.0f, 0.0f, 0.0f, 1.0f });

    return true;
}

//==============================================================================
// バックバッファリサイズ時にテクスチャを再生成
//==============================================================================
void ShaderEdge_ResizeBuffers()
{
    if (!g_pDevice) return;

    // サイズ依存リソースを解放
    SAFE_RELEASE(g_pNormalSRV);
    SAFE_RELEASE(g_pNormalRTV);
    SAFE_RELEASE(g_pNormalTex);
    SAFE_RELEASE(g_pDepthSRV);
    SAFE_RELEASE(g_pDepthDSV);
    SAFE_RELEASE(g_pDepthTex);

    const UINT W = Direct3D_GetBackBufferWidth();
    const UINT H = Direct3D_GetBackBufferHeight();

    // 法線テクスチャ再生成
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = W; desc.Height = H;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        g_pDevice->CreateTexture2D(&desc, nullptr, &g_pNormalTex);
        g_pDevice->CreateRenderTargetView(g_pNormalTex, nullptr, &g_pNormalRTV);
        g_pDevice->CreateShaderResourceView(g_pNormalTex, nullptr, &g_pNormalSRV);
    }

    // 深度テクスチャ再生成
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = W; desc.Height = H;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        g_pDevice->CreateTexture2D(&desc, nullptr, &g_pDepthTex);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        g_pDevice->CreateDepthStencilView(g_pDepthTex, &dsvDesc, &g_pDepthDSV);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        g_pDevice->CreateShaderResourceView(g_pDepthTex, &srvDesc, &g_pDepthSRV);
    }

    // texelSize を更新
    ShaderEdge_SetParam(0.01f, 0.4f, { 0.0f, 0.0f, 0.0f, 1.0f });
}

//==============================================================================
// 終了
//==============================================================================
void ShaderEdge_Finalize()
{
    SAFE_RELEASE(g_pRSNoCull);
    SAFE_RELEASE(g_pBlendState);
    SAFE_RELEASE(g_pSampler);
    SAFE_RELEASE(g_pCB_Proj);
    SAFE_RELEASE(g_pCB_View);
    SAFE_RELEASE(g_pCB_World);
    SAFE_RELEASE(g_pCB_EdgeParam);
    SAFE_RELEASE(g_pEdgePS);
    SAFE_RELEASE(g_pEdgeVS);
    SAFE_RELEASE(g_pNormalPS);
    SAFE_RELEASE(g_pLayout);
    SAFE_RELEASE(g_pNormalVS);
    SAFE_RELEASE(g_pDepthSRV);
    SAFE_RELEASE(g_pDepthDSV);
    SAFE_RELEASE(g_pDepthTex);
    SAFE_RELEASE(g_pNormalSRV);
    SAFE_RELEASE(g_pNormalRTV);
    SAFE_RELEASE(g_pNormalTex);

    SAFE_RELEASE(g_pDSSDepthDisable);
}

//==============================================================================
// パラメータ設定
//==============================================================================
void ShaderEdge_SetParam(float depthThreshold, float normalThreshold, const XMFLOAT4& color)
{
    const UINT W = Direct3D_GetBackBufferWidth();
    const UINT H = Direct3D_GetBackBufferHeight();

    EdgeParam param;
    param.depthThreshold = depthThreshold;
    param.normalThreshold = normalThreshold;
    param.texelSize = { 1.0f / W, 1.0f / H };
    param.edgeColor = color;

    g_pContext->UpdateSubresource(g_pCB_EdgeParam, 0, nullptr, &param, 0, 0);
}

//==============================================================================
// View/Proj を法線書き出しVSに渡す（Player_Camera.cpp から呼ばれる）
//==============================================================================
void ShaderEdge_SetViewMatrix(const XMMATRIX& m)
{
    XMFLOAT4X4 t; XMStoreFloat4x4(&t, XMMatrixTranspose(m));
    g_pContext->UpdateSubresource(g_pCB_View, 0, nullptr, &t, 0, 0);
}

void ShaderEdge_SetProjectMatrix(const XMMATRIX& m)
{
    XMFLOAT4X4 t; XMStoreFloat4x4(&t, XMMatrixTranspose(m));
    g_pContext->UpdateSubresource(g_pCB_Proj, 0, nullptr, &t, 0, 0);
}

void ShaderEdge_SetWorldMatrix(const XMMATRIX& m)
{
    XMFLOAT4X4 t; XMStoreFloat4x4(&t, XMMatrixTranspose(m));
    g_pContext->UpdateSubresource(g_pCB_World, 0, nullptr, &t, 0, 0);
}

//==============================================================================
// 法線書き出しパス開始
//==============================================================================
void ShaderEdge_BeginNormalPass()
{
    // 現在の状態を退避
    SAFE_RELEASE(g_pSavedRTV);
    SAFE_RELEASE(g_pSavedDSV);
    SAFE_RELEASE(g_pSavedVS);
    SAFE_RELEASE(g_pSavedPS);
    SAFE_RELEASE(g_pSavedLayout);
    for (auto& cb : g_pSavedVSCB) { SAFE_RELEASE(cb); }
    g_pContext->OMGetRenderTargets(1, &g_pSavedRTV, &g_pSavedDSV);
    g_pContext->VSGetShader(&g_pSavedVS, nullptr, nullptr);
    g_pContext->PSGetShader(&g_pSavedPS, nullptr, nullptr);
    g_pContext->IAGetInputLayout(&g_pSavedLayout);
    g_pContext->IAGetPrimitiveTopology(&g_SavedTopo);
    g_pContext->VSGetConstantBuffers(0, 3, g_pSavedVSCB);

    // 法線バッファ + 独自深度バッファ にセット
    g_pContext->OMSetRenderTargets(1, &g_pNormalRTV, g_pDepthDSV);

    // クリア
    float clearColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // 法線なし = (0,0,0) -> 0.5,0.5,0.5
    g_pContext->ClearRenderTargetView(g_pNormalRTV, clearColor);
    g_pContext->ClearDepthStencilView(g_pDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 法線書き出しシェーダーをセット
    g_pContext->VSSetShader(g_pNormalVS, nullptr, 0);
    g_pContext->PSSetShader(g_pNormalPS, nullptr, 0);
    g_pContext->IASetInputLayout(g_pLayout);

    // VS 定数バッファ
    g_pContext->VSSetConstantBuffers(0, 1, &g_pCB_World);
    g_pContext->VSSetConstantBuffers(1, 1, &g_pCB_View);
    g_pContext->VSSetConstantBuffers(2, 1, &g_pCB_Proj);


}

//==============================================================================
// 法線書き出しパス終了
//==============================================================================
void ShaderEdge_EndNormalPass()
{
    // 状態を復元
    if (g_pSavedRTV || g_pSavedDSV)
        g_pContext->OMSetRenderTargets(1, &g_pSavedRTV, g_pSavedDSV);
    g_pContext->VSSetShader(g_pSavedVS, nullptr, 0);
    g_pContext->PSSetShader(g_pSavedPS, nullptr, 0);
    g_pContext->IASetInputLayout(g_pSavedLayout);
    g_pContext->IASetPrimitiveTopology(g_SavedTopo);
    g_pContext->VSSetConstantBuffers(0, 3, g_pSavedVSCB);

    SAFE_RELEASE(g_pSavedRTV);
    SAFE_RELEASE(g_pSavedDSV);
    SAFE_RELEASE(g_pSavedVS);
    SAFE_RELEASE(g_pSavedPS);
    SAFE_RELEASE(g_pSavedLayout);
    for (auto& cb : g_pSavedVSCB) { SAFE_RELEASE(cb); }
}

//==============================================================================
// エッジ検出描画
//==============================================================================
void ShaderEdge_DrawEdge()
{
    // 退避変数に追加
    ID3D11DepthStencilState* savedDSS = nullptr;  
    UINT savedStencilRef = 0;


    //--------------------------------------------------------------------------
    // 現在のパイプライン状態を退避
    //--------------------------------------------------------------------------
    ID3D11BlendState* savedBlend = nullptr;
    ID3D11RasterizerState* savedRS = nullptr;
    ID3D11VertexShader* savedVS = nullptr;
    ID3D11PixelShader* savedPS = nullptr;
    ID3D11InputLayout* savedLayout = nullptr;
    ID3D11SamplerState* savedSampler = nullptr;
    ID3D11Buffer* savedPSCB = nullptr;
    ID3D11Buffer* savedVSCB[3] = { nullptr, nullptr, nullptr };
    float savedBF[4] = {};
    UINT  savedMask = 0;
    D3D11_PRIMITIVE_TOPOLOGY savedTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;



    g_pContext->OMGetBlendState(&savedBlend, savedBF, &savedMask);
    g_pContext->OMGetDepthStencilState(&savedDSS, &savedStencilRef);
    g_pContext->RSGetState(&savedRS);
    g_pContext->VSGetShader(&savedVS, nullptr, nullptr);
    g_pContext->PSGetShader(&savedPS, nullptr, nullptr);
    g_pContext->IAGetInputLayout(&savedLayout);
    g_pContext->IAGetPrimitiveTopology(&savedTopo);
    g_pContext->PSGetSamplers(0, 1, &savedSampler);
    g_pContext->PSGetConstantBuffers(0, 1, &savedPSCB);
    g_pContext->VSGetConstantBuffers(0, 3, savedVSCB);

    //--------------------------------------------------------------------------
    // SRV として使うので RTV を一度セットし直す
    //--------------------------------------------------------------------------
    ID3D11RenderTargetView* currentRTV = nullptr;
    ID3D11DepthStencilView* currentDSV = nullptr;
    g_pContext->OMGetRenderTargets(1, &currentRTV, &currentDSV);
    g_pContext->OMSetRenderTargets(1, &currentRTV, currentDSV);

    //--------------------------------------------------------------------------
    // エッジ描画
    //--------------------------------------------------------------------------
    g_pContext->VSSetShader(g_pEdgeVS, nullptr, 0);
    g_pContext->PSSetShader(g_pEdgePS, nullptr, 0);
    g_pContext->IASetInputLayout(nullptr);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pContext->PSSetShaderResources(0, 1, &g_pNormalSRV);
    g_pContext->PSSetShaderResources(1, 1, &g_pDepthSRV);
    g_pContext->PSSetSamplers(0, 1, &g_pSampler);
    g_pContext->PSSetConstantBuffers(0, 1, &g_pCB_EdgeParam);

    float bf[4] = {};
    g_pContext->OMSetBlendState(g_pBlendState, bf, 0xffffffff);
    g_pContext->RSSetState(g_pRSNoCull);
    g_pContext->OMSetBlendState(g_pBlendState, bf, 0xffffffff);
    g_pContext->RSSetState(g_pRSNoCull);
    g_pContext->OMSetDepthStencilState(g_pDSSDepthDisable, 0); // ★セット

    g_pContext->Draw(3, 0); // 

    //--------------------------------------------------------------------------
    // SRV を外す（次フレームで RTV として使うため）
    //--------------------------------------------------------------------------
    ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
    g_pContext->PSSetShaderResources(0, 2, nullSRV);

    //--------------------------------------------------------------------------
    // パイプライン状態を復元
    //--------------------------------------------------------------------------
    g_pContext->OMSetBlendState(savedBlend, savedBF, savedMask);
    g_pContext->OMSetDepthStencilState(savedDSS, savedStencilRef);
    g_pContext->RSSetState(savedRS);
    g_pContext->VSSetShader(savedVS, nullptr, 0);
    g_pContext->PSSetShader(savedPS, nullptr, 0);
    g_pContext->IASetInputLayout(savedLayout);
    g_pContext->IASetPrimitiveTopology(savedTopo);
    g_pContext->PSSetSamplers(0, 1, &savedSampler);
    g_pContext->PSSetConstantBuffers(0, 1, &savedPSCB);
    g_pContext->VSSetConstantBuffers(0, 3, savedVSCB);

    //g_pContext->OMSetRenderTargets(1, &currentRTV, 0);

    SAFE_RELEASE(savedBlend);
    SAFE_RELEASE(savedRS);
    SAFE_RELEASE(savedVS);
    SAFE_RELEASE(savedPS);
    SAFE_RELEASE(savedLayout);
    SAFE_RELEASE(savedSampler);
    SAFE_RELEASE(savedPSCB);
    SAFE_RELEASE(savedVSCB[0]);
    SAFE_RELEASE(savedVSCB[1]);
    SAFE_RELEASE(savedVSCB[2]);
    SAFE_RELEASE(currentRTV);
    SAFE_RELEASE(currentDSV);
    SAFE_RELEASE(savedDSS);
}