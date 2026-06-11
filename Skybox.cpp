/*==============================================================================

   スカイボックス [Skybox.cpp]

   ■実装の概要
   ・球メッシュを緯度経度分割で生成（内側から見る）
   ・専用 VS/PS（shader_vertex_sky / shader_pixel_sky）を使用
   ・ビュー行列の平行移動行（row[3]）を除去してカメラ位置に追従
   ・描画時は深度書き込みOFF → 他のオブジェクトが必ず上に描かれる

   ■プロジェクト設定
   ・shader_vertex_sky.hlsl : シェーダーの種類 = 頂点シェーダー (vs_5_0)
                              出力ファイル = Resource/Shader/shader_vertex_sky.cso
   ・shader_pixel_sky.hlsl  : シェーダーの種類 = ピクセルシェーダー (ps_5_0)
                              出力ファイル = Resource/Shader/shader_pixel_sky.cso



==============================================================================*/

#include "Skybox.h"
#include "direct3d.h"
#include "texture.h"

#include <d3d11.h>
#include <DirectXMath.h>
#include <fstream>
#include <vector>
#include <cmath>

using namespace DirectX;

//==============================================================================
// 内部
//==============================================================================
namespace
{

//--------------------------------------------------------------------------
// 頂点構造体（shader3d と同じレイアウト）
//--------------------------------------------------------------------------
struct SkyVertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;   // 未使用
    XMFLOAT4 color;    // 未使用
    XMFLOAT2 uv;
};

//--------------------------------------------------------------------------
// 定数バッファ（行列用）
//--------------------------------------------------------------------------
struct MatrixCB { XMFLOAT4X4 m; };

//--------------------------------------------------------------------------
// D3D リソース
//--------------------------------------------------------------------------
ID3D11VertexShader* g_pVS          = nullptr;
ID3D11PixelShader*  g_pPS          = nullptr;
ID3D11InputLayout*  g_pLayout      = nullptr;
ID3D11Buffer*       g_pVB          = nullptr;
ID3D11Buffer*       g_pIB          = nullptr;
ID3D11Buffer*       g_pCBWorld     = nullptr;
ID3D11Buffer*       g_pCBView      = nullptr;
ID3D11Buffer*       g_pCBProj      = nullptr;
ID3D11DepthStencilState* g_pDSS_NoWrite = nullptr;  // 深度書き込みOFF
ID3D11RasterizerState*   g_pRS_CullNone = nullptr;  // カリングなし（巻き順不問）

int  g_TexId       = -1;
int  g_IndexCount  = 0;

//--------------------------------------------------------------------------
// .cso 読み込みヘルパ
//--------------------------------------------------------------------------
static std::vector<char> LoadCSO(const char* path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    ifs.seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    std::vector<char> buf(size);
    ifs.read(buf.data(), size);
    return buf;
}

//--------------------------------------------------------------------------
// 定数バッファ生成ヘルパ
//--------------------------------------------------------------------------
static ID3D11Buffer* CreateCB(ID3D11Device* dev, UINT byteWidth)
{
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth      = byteWidth;
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    ID3D11Buffer* buf = nullptr;
    dev->CreateBuffer(&bd, nullptr, &buf);
    return buf;
}

//--------------------------------------------------------------------------
// 球メッシュ生成
// ・stacks 段 × slices 列の緯度経度分割
// ・UV : u = 経度方向（0→1）, v = 緯度方向（0=上, 1=下）
// ・内側から見てフロントフェースになるよう巻き順を設定
//--------------------------------------------------------------------------
static void BuildSphere(float radius, int stacks, int slices,
                        std::vector<SkyVertex>& verts,
                        std::vector<UINT>& indices)
{
    verts.clear();
    indices.clear();

    for (int i = 0; i <= stacks; ++i)
    {
        const float v     = static_cast<float>(i) / stacks;
        const float theta = XM_PI * v;  // 0（北極）→ π（南極）

        for (int j = 0; j <= slices; ++j)
        {
            const float u   = static_cast<float>(j) / slices;
            const float phi = XM_2PI * u;  // 0 → 2π

            SkyVertex vtx{};
            vtx.position = {
                radius * sinf(theta) * cosf(phi),
                radius * cosf(theta),
                radius * sinf(theta) * sinf(phi)
            };
            vtx.normal = { 0.f, 0.f, 0.f };
            vtx.color  = { 1.f, 1.f, 1.f, 1.f };
            vtx.uv     = { u, v };
            verts.push_back(vtx);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            const UINT v00 =  i      * (slices + 1) + j;
            const UINT v10 = (i + 1) * (slices + 1) + j;
            const UINT v01 =  i      * (slices + 1) + j + 1;
            const UINT v11 = (i + 1) * (slices + 1) + j + 1;

            // 内側から見て CW（DirectX フロントフェース）になるよう逆順
            indices.push_back(v00); indices.push_back(v11); indices.push_back(v10);
            indices.push_back(v00); indices.push_back(v01); indices.push_back(v11);
        }
    }
}

} // namespace

//==============================================================================
// Skybox_Initialize
//==============================================================================
void Skybox_Initialize()
{
    ID3D11Device*        dev = Direct3D_GetDevice();
    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    //--------------------------------------------------------------------------
    // 頂点シェーダー
    //--------------------------------------------------------------------------
    auto vsBytes = LoadCSO("Resource/Shader/shader_vertex_sky.cso");
    if (vsBytes.empty())
    {
        MessageBoxW(nullptr, L"shader_vertex_sky.cso が見つかりません", L"Skybox Error", MB_OK);
        return;
    }
    dev->CreateVertexShader(vsBytes.data(), vsBytes.size(), nullptr, &g_pVS);

    //--------------------------------------------------------------------------
    // 入力レイアウト（shader3d と同じ構成）
    //--------------------------------------------------------------------------
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    dev->CreateInputLayout(layout, ARRAYSIZE(layout),
                           vsBytes.data(), vsBytes.size(), &g_pLayout);

    //--------------------------------------------------------------------------
    // ピクセルシェーダー
    //--------------------------------------------------------------------------
    auto psBytes = LoadCSO("Resource/Shader/shader_pixel_sky.cso");
    if (psBytes.empty())
    {
        MessageBoxW(nullptr, L"shader_pixel_sky.cso が見つかりません", L"Skybox Error", MB_OK);
        return;
    }
    dev->CreatePixelShader(psBytes.data(), psBytes.size(), nullptr, &g_pPS);

    //--------------------------------------------------------------------------
    // 定数バッファ（World / View / Proj）
    //--------------------------------------------------------------------------
    g_pCBWorld = CreateCB(dev, sizeof(MatrixCB));
    g_pCBView  = CreateCB(dev, sizeof(MatrixCB));
    g_pCBProj  = CreateCB(dev, sizeof(MatrixCB));

    //--------------------------------------------------------------------------
    // 球メッシュ
    //--------------------------------------------------------------------------
    std::vector<SkyVertex> verts;
    std::vector<UINT>      inds;
    BuildSphere(500.f, 16, 32, verts, inds);
    g_IndexCount = static_cast<int>(inds.size());

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(sizeof(SkyVertex) * verts.size());
        bd.Usage     = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd{ verts.data() };
        dev->CreateBuffer(&bd, &sd, &g_pVB);
    }
    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(sizeof(UINT) * inds.size());
        bd.Usage     = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd{ inds.data() };
        dev->CreateBuffer(&bd, &sd, &g_pIB);
    }

    //--------------------------------------------------------------------------
    // 深度書き込みOFF ステート
    //--------------------------------------------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // 書き込みOFF
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    dsd.StencilEnable  = FALSE;
    dev->CreateDepthStencilState(&dsd, &g_pDSS_NoWrite);

    //--------------------------------------------------------------------------
    // カリングなし ラスタライザー（巻き順を問わず描画）
    //--------------------------------------------------------------------------
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode              = D3D11_FILL_SOLID;
    rd.CullMode              = D3D11_CULL_NONE;  // 両面描画
    rd.DepthClipEnable       = FALSE;            // 深度クリップOFF（球の端が切れないように）
    rd.FrontCounterClockwise = FALSE;
    dev->CreateRasterizerState(&rd, &g_pRS_CullNone);

    //--------------------------------------------------------------------------
    // 初期化チェック
    //--------------------------------------------------------------------------
    if (!g_pVS || !g_pPS || !g_pLayout || !g_pVB || !g_pIB || !g_pDSS_NoWrite || !g_pRS_CullNone)
    {
        MessageBoxW(nullptr, L"Skybox: D3Dリソースの生成に失敗しました", L"Skybox Error", MB_OK);
    }

    //--------------------------------------------------------------------------
    // デフォルトテクスチャ（宇宙背景）
    //--------------------------------------------------------------------------
    g_TexId = Texture_Load(L"Resource/Texture/haikeiutyu.png");
}

//==============================================================================
// Skybox_Finalize
//==============================================================================
void Skybox_Finalize()
{
    SAFE_RELEASE(g_pRS_CullNone);
    SAFE_RELEASE(g_pDSS_NoWrite);
    SAFE_RELEASE(g_pIB);
    SAFE_RELEASE(g_pVB);
    SAFE_RELEASE(g_pCBProj);
    SAFE_RELEASE(g_pCBView);
    SAFE_RELEASE(g_pCBWorld);
    SAFE_RELEASE(g_pLayout);
    SAFE_RELEASE(g_pPS);
    SAFE_RELEASE(g_pVS);
}

//==============================================================================
// Skybox_Draw
//==============================================================================
void Skybox_Draw(const XMMATRIX& view, const XMMATRIX& proj)
{
    if (g_TexId < 0) return;
    if (!g_pVS || !g_pPS || !g_pVB || !g_pIB) return;

    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    //--------------------------------------------------------------------------
    // ビュー行列から平行移動を除去（球がカメラ位置に追従するため）
    //--------------------------------------------------------------------------
    XMMATRIX skyView = view;
    skyView.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);

    //--------------------------------------------------------------------------
    // 深度・ラスタライザーの退避と上書き
    //--------------------------------------------------------------------------
    ID3D11DepthStencilState* prevDSS = nullptr;
    UINT prevStencilRef = 0;
    ctx->OMGetDepthStencilState(&prevDSS, &prevStencilRef);
    ctx->OMSetDepthStencilState(g_pDSS_NoWrite, 0);

    ID3D11RasterizerState* prevRS = nullptr;
    ctx->RSGetState(&prevRS);
    ctx->RSSetState(g_pRS_CullNone);

    //--------------------------------------------------------------------------
    // シェーダー・レイアウトをバインド
    //--------------------------------------------------------------------------
    ctx->VSSetShader(g_pVS, nullptr, 0);
    ctx->PSSetShader(g_pPS, nullptr, 0);
    ctx->IASetInputLayout(g_pLayout);

    //--------------------------------------------------------------------------
    // 定数バッファ更新（World=Identity / View=回転のみ / Proj）
    //--------------------------------------------------------------------------
    {
        MatrixCB cb;
        XMStoreFloat4x4(&cb.m, XMMatrixTranspose(XMMatrixIdentity()));
        ctx->UpdateSubresource(g_pCBWorld, 0, nullptr, &cb, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &g_pCBWorld);
    }
    {
        MatrixCB cb;
        XMStoreFloat4x4(&cb.m, XMMatrixTranspose(skyView));
        ctx->UpdateSubresource(g_pCBView, 0, nullptr, &cb, 0, 0);
        ctx->VSSetConstantBuffers(1, 1, &g_pCBView);
    }
    {
        MatrixCB cb;
        XMStoreFloat4x4(&cb.m, XMMatrixTranspose(proj));
        ctx->UpdateSubresource(g_pCBProj, 0, nullptr, &cb, 0, 0);
        ctx->VSSetConstantBuffers(2, 1, &g_pCBProj);
    }

    //--------------------------------------------------------------------------
    // テクスチャをスロット0にセット
    //--------------------------------------------------------------------------
    Set_Texture(g_TexId, 0);

    //--------------------------------------------------------------------------
    // 描画
    //--------------------------------------------------------------------------
    const UINT stride = sizeof(SkyVertex);
    const UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
    ctx->IASetIndexBuffer(g_pIB, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->DrawIndexed(g_IndexCount, 0, 0);

    //--------------------------------------------------------------------------
    // 深度・ラスタライザーを元に戻す
    //--------------------------------------------------------------------------
    ctx->OMSetDepthStencilState(prevDSS, prevStencilRef);
    SAFE_RELEASE(prevDSS);

    ctx->RSSetState(prevRS);
    SAFE_RELEASE(prevRS);
}

//==============================================================================
// Skybox_SetTexture
//==============================================================================
void Skybox_SetTexture(int texId)
{
    g_TexId = texId;
}
