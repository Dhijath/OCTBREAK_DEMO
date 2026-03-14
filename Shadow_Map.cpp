/*==============================================================================

   シャドウマップ管理 [shadow_map.cpp]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   ・Depth Texture(typeless) を作って DSV + SRV を生成
   ・影生成パスでは RTV を NULL、DSV のみで描画
   ・通常描画では Shadow SRV を PS(t7) に、LightViewProj を VS(b3) に渡す
   ・DepthBias 用 RasterizerState を影生成パス中だけ有効化

==============================================================================*/
#include "shadow_map.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <algorithm>
#include <fstream>
#include "direct3d.h" // ← デバイス/コンテキスト取得に使う想定
#include "model.h"    // ← DrawModel で MODEL* を使う

using namespace DirectX;

namespace
{
    bool g_Enable = true;
    bool g_InShadowPass = false;

    int g_Size = 2048;

    ID3D11Texture2D* g_pShadowTex = nullptr;
    ID3D11DepthStencilView* g_pShadowDSV = nullptr;
    ID3D11ShaderResourceView* g_pShadowSRV = nullptr;

    ID3D11SamplerState* g_pShadowCmpSampler = nullptr;
    ID3D11RasterizerState* g_pRSDepthBias = nullptr;

    D3D11_VIEWPORT              g_ShadowVP{};

    // VS b3: lightViewProj
    ID3D11Buffer* g_pCBLightVP = nullptr;

    // PS b5: shadow params
    ID3D11Buffer* g_pCBShadowParam = nullptr;

    // 深度のみ描画用シェーダー
    ID3D11VertexShader*  g_pShadowVS    = nullptr;
    ID3D11PixelShader*   g_pShadowPS    = nullptr;
    ID3D11InputLayout*   g_pShadowIL    = nullptr;
    ID3D11Buffer*        g_pCBWorld     = nullptr; // VS b0: world

    // 保存（戻す用）
    ID3D11RasterizerState* g_pRSPrev = nullptr;
    D3D11_VIEWPORT              g_PrevVP{};
    UINT                        g_NumPrevVP = 1;

    struct CBLightVP
    {
        XMFLOAT4X4 lightViewProj;
    };

    struct CBShadowParam
    {
        XMFLOAT2 shadowMapSize; // (w,h)
        float    depthBias;     // receiver bias (clip space)
        float    pad0;
        float    strength;      // 0..1
        float    pad1[3];
    };

    void SafeRelease(IUnknown*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }

    ID3D11Device* GetDevice()
    {
          return Direct3D_GetDevice();
    }

    ID3D11DeviceContext* GetContext()
    {
          return Direct3D_GetContext();
    }

    // CSO ファイル読み込みヘルパー
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

    XMFLOAT3 Normalize(const XMFLOAT3& v)
    {
        XMVECTOR x = XMLoadFloat3(&v);
        x = XMVector3Normalize(x);
        XMFLOAT3 o{};
        XMStoreFloat3(&o, x);
        return o;
    }
}

namespace ShadowMap
{
    bool Initialize(int size)
    {
        g_Size = size;
        if (g_Size < 256)  g_Size = 256;
        if (g_Size > 8192) g_Size = 8192;
        //<algorithm>使って変更も視野


        ID3D11Device* dev = GetDevice();
        if (!dev) return false;

        // --- Shadow Depth Texture (typeless)
        D3D11_TEXTURE2D_DESC td{};
        td.Width = (UINT)g_Size;
        td.Height = (UINT)g_Size;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_pShadowTex)))
            return false;

        // --- DSV
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

        if (FAILED(dev->CreateDepthStencilView(g_pShadowTex, &dsvd, &g_pShadowDSV)))
            return false;

        // --- SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = DXGI_FORMAT_R32_FLOAT;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;

        if (FAILED(dev->CreateShaderResourceView(g_pShadowTex, &srvd, &g_pShadowSRV)))
            return false;

        // --- Comparison Sampler
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MaxLOD = D3D11_FLOAT32_MAX;

        if (FAILED(dev->CreateSamplerState(&sd, &g_pShadowCmpSampler)))
            return false;

        // --- DepthBias Rasterizer
        D3D11_RASTERIZER_DESC rs{};
        rs.FillMode = D3D11_FILL_SOLID;
        rs.CullMode = D3D11_CULL_BACK;
        rs.DepthClipEnable = TRUE;

        // 影のアクネ対策（まずはこの値で開始）
        rs.DepthBias = 1500;              // integer bias
        rs.SlopeScaledDepthBias = 2.0f;   // slope bias
        rs.DepthBiasClamp = 0.0f;

        if (FAILED(dev->CreateRasterizerState(&rs, &g_pRSDepthBias)))
            return false;

        // --- Viewport
        g_ShadowVP.TopLeftX = 0.0f;
        g_ShadowVP.TopLeftY = 0.0f;
        g_ShadowVP.Width = (FLOAT)g_Size;
        g_ShadowVP.Height = (FLOAT)g_Size;
        g_ShadowVP.MinDepth = 0.0f;
        g_ShadowVP.MaxDepth = 1.0f;

        // --- CB LightVP (VS b3)
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(CBLightVP);
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(dev->CreateBuffer(&cbd, nullptr, &g_pCBLightVP)))
            return false;

        // --- CB ShadowParam (PS b5)
        D3D11_BUFFER_DESC spd{};
        spd.ByteWidth = sizeof(CBShadowParam);
        spd.Usage = D3D11_USAGE_DEFAULT;
        spd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(dev->CreateBuffer(&spd, nullptr, &g_pCBShadowParam)))
            return false;

        // --- Shadow VS (shader_vertex_shadow_3d.cso)
        {
            unsigned char* pData = nullptr;
            size_t sz = 0;
            if (LoadCSO("resource/shader/shader_vertex_shadow_3d.cso", &pData, &sz))
            {
                dev->CreateVertexShader(pData, sz, nullptr, &g_pShadowVS);

                // 入力レイアウト（Vertex3D と同じ）
                D3D11_INPUT_ELEMENT_DESC layout[] = {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };
                dev->CreateInputLayout(layout, ARRAYSIZE(layout), pData, sz, &g_pShadowIL);
                delete[] pData;
            }
        }

        // --- Shadow PS (shader_pixel_shadow.cso)
        {
            unsigned char* pData = nullptr;
            size_t sz = 0;
            if (LoadCSO("resource/shader/shader_pixel_shadow.cso", &pData, &sz))
            {
                dev->CreatePixelShader(pData, sz, nullptr, &g_pShadowPS);
                delete[] pData;
            }
        }

        // --- CB World (VS b0)
        {
            D3D11_BUFFER_DESC wbd{};
            wbd.ByteWidth      = sizeof(XMFLOAT4X4);
            wbd.Usage          = D3D11_USAGE_DEFAULT;
            wbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            dev->CreateBuffer(&wbd, nullptr, &g_pCBWorld);
        }

        return true;
    }

    void Finalize()
    {
        SafeRelease((IUnknown*&)g_pCBWorld);
        SafeRelease((IUnknown*&)g_pShadowIL);
        SafeRelease((IUnknown*&)g_pShadowPS);
        SafeRelease((IUnknown*&)g_pShadowVS);
        SafeRelease((IUnknown*&)g_pCBShadowParam);
        SafeRelease((IUnknown*&)g_pCBLightVP);
        SafeRelease((IUnknown*&)g_pRSDepthBias);
        SafeRelease((IUnknown*&)g_pShadowCmpSampler);
        SafeRelease((IUnknown*&)g_pShadowSRV);
        SafeRelease((IUnknown*&)g_pShadowDSV);
        SafeRelease((IUnknown*&)g_pShadowTex);
    }

    void SetEnabled(bool enable) { g_Enable = enable; }
    bool IsEnabled() { return g_Enable; }
    bool IsRenderingShadow() { return g_InShadowPass; }

    void BeginPass(const XMFLOAT3& lightDirW, const XMFLOAT3& focusPosW,
        float area, float nearZ, float farZ)
    {
        if (!g_Enable) return;

        ID3D11DeviceContext* ctx = GetContext();
        if (!ctx) return;

        g_InShadowPass = true;

        // ---- 退避（Viewport / RS）
        g_NumPrevVP = 1;
        ctx->RSGetViewports(&g_NumPrevVP, &g_PrevVP);
        ctx->RSGetState(&g_pRSPrev);

        // ---- ShadowViewport / DepthBiasRS
        ctx->RSSetViewports(1, &g_ShadowVP);
        ctx->RSSetState(g_pRSDepthBias);

        // ---- RTVなし、DSVのみ
        ctx->OMSetRenderTargets(0, nullptr, g_pShadowDSV);

        // ---- Depth Clear
        ctx->ClearDepthStencilView(g_pShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // ---- Light ViewProj を作る（Directional）
        XMFLOAT3 dirN = Normalize(lightDirW);

        // ライトは「逆方向」から当てるイメージ：位置を focus から -dir * dist
        const float dist = std::max(area, 10.0f);
        XMFLOAT3 lightPos{
            focusPosW.x - dirN.x * dist,
            focusPosW.y - dirN.y * dist,
            focusPosW.z - dirN.z * dist
        };

        XMVECTOR eye = XMLoadFloat3(&lightPos);
        XMVECTOR at = XMLoadFloat3(&focusPosW);

        // up が dir と平行だと壊れるので簡易回避
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);
        if (fabsf(dirN.y) > 0.95f) up = XMVectorSet(0, 0, 1, 0);

        XMMATRIX V = XMMatrixLookAtLH(eye, at, up);

        // まずは「正方形の直交投影」：エリア内を影にする
        float half = area * 0.5f;
        XMMATRIX P = XMMatrixOrthographicLH(area, area, nearZ, farZ);

        XMMATRIX VP = XMMatrixMultiply(V, P);

        CBLightVP cb{};
        XMStoreFloat4x4(&cb.lightViewProj, XMMatrixTranspose(VP)); // HLSL は列/行の都合で Transpose

        ctx->UpdateSubresource(g_pCBLightVP, 0, nullptr, &cb, 0, 0);

        // VS(b3) に設定（影生成パスの VS が使う）
        ctx->VSSetConstantBuffers(3, 1, &g_pCBLightVP);
    }

    void EndPass()
    {
        if (!g_Enable) return;

        ID3D11DeviceContext* ctx = GetContext();
        if (!ctx) return;

        // ---- 戻す（Viewport / RS）
        ctx->RSSetViewports(1, &g_PrevVP);
        ctx->RSSetState(g_pRSPrev);
        SafeRelease((IUnknown*&)g_pRSPrev);

        // RTV/DSV は呼び元（Direct3D_Clear 等）が貼り直す前提
        g_InShadowPass = false;
    }

    void BindForMainPass()
    {
        if (!g_Enable) return;

        ID3D11DeviceContext* ctx = GetContext();
        if (!ctx) return;

        // VS(b3): LightViewProj（影生成 VS が参照する）
        ctx->VSSetConstantBuffers(3, 1, &g_pCBLightVP);

        // PS(b8): LightViewProj（PS が posW から shadowUV を計算する用）
        ctx->PSSetConstantBuffers(8, 1, &g_pCBLightVP);

        // PS(t7): Shadow SRV
        ctx->PSSetShaderResources(7, 1, &g_pShadowSRV);

        // PS(s1): Comparison Sampler
        ctx->PSSetSamplers(1, 1, &g_pShadowCmpSampler);

        // PS(b5): shadow param
        CBShadowParam sp{};
        sp.shadowMapSize = XMFLOAT2((float)g_Size, (float)g_Size);
        sp.depthBias = 0.001f;
        sp.strength = 0.6f; // 影の濃さ（0=影なし / 1=真っ暗）

        ctx->UpdateSubresource(g_pCBShadowParam, 0, nullptr, &sp, 0, 0);
        ctx->PSSetConstantBuffers(5, 1, &g_pCBShadowParam);
    }

    void DrawModel(MODEL* model, const XMMATRIX& world)
    {
        if (!g_InShadowPass || !model || !model->AiScene) return;
        if (!g_pShadowVS || !g_pShadowPS || !g_pShadowIL || !g_pCBWorld) return;

        ID3D11DeviceContext* ctx = GetContext();
        if (!ctx) return;

        // シェーダーセット
        ctx->VSSetShader(g_pShadowVS, nullptr, 0);
        ctx->PSSetShader(g_pShadowPS, nullptr, 0);
        ctx->IASetInputLayout(g_pShadowIL);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // ワールド行列を VS b0 に渡す
        XMFLOAT4X4 wt;
        XMStoreFloat4x4(&wt, XMMatrixTranspose(world));
        ctx->UpdateSubresource(g_pCBWorld, 0, nullptr, &wt, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &g_pCBWorld);
        // VS b3 は BeginPass で設定済みの lightViewProj

        // 全メッシュ描画（深度のみ書き込み）
        const UINT stride = sizeof(float) * (3 + 3 + 4 + 2); // Vertex3D のサイズ
        const UINT offset = 0;
        for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
        {
            aiMesh* mesh = model->AiScene->mMeshes[m];
            if (!mesh || mesh->mNumFaces == 0) continue;
            if (!model->VertexBuffer[m] || !model->IndexBuffer[m]) continue;

            ctx->IASetVertexBuffers(0, 1, &model->VertexBuffer[m], &stride, &offset);
            ctx->IASetIndexBuffer(model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);
            ctx->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
        }
    }

    ID3D11ShaderResourceView* GetShadowSRV()
    {
        return g_pShadowSRV;
    }
}
