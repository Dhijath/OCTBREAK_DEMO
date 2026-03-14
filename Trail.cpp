/*==============================================================================

   カメラ向きリボントレイル [Trail.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/20
--------------------------------------------------------------------------------

   ■レンダリングの仕組み
     ・各ポイントにカメラ向き（camera-facing）の左右頂点を生成して TRIANGLE_STRIP で描画
     ・サイド方向 = normalize( cross(toCam, tangent) )
     ・アルファ : 先端（age=0）が最大、末尾（age=maxAge）で 0
     ・加算ブレンド（SrcAlpha, One）でグロー感を演出

   ■シェーダーファイル
     resource/shader/shader_vertex_trail.cso
     resource/shader/shader_pixel_trail.cso

==============================================================================*/

#include "Trail.h"
#include "direct3d.h"
#include "Player_Camera.h"

#include <fstream>
#include <algorithm>

using namespace DirectX;

//==============================================================================
// 静的メンバ定義
//==============================================================================
ID3D11VertexShader*      Trail::s_pVS        = nullptr;
ID3D11PixelShader*       Trail::s_pPS        = nullptr;
ID3D11InputLayout*       Trail::s_pIL        = nullptr;
ID3D11BlendState*        Trail::s_pBlendAdd  = nullptr;
ID3D11DepthStencilState* Trail::s_pDSSNoWrite= nullptr;
ID3D11RasterizerState*   Trail::s_pRSNoCull  = nullptr;
ID3D11Buffer*            Trail::s_pCBView    = nullptr;
ID3D11Buffer*            Trail::s_pCBProj    = nullptr;
int                      Trail::s_RefCount   = 0;

//==============================================================================
// 内部ヘルパー
//==============================================================================
namespace
{
    // GPU 頂点レイアウト（POSITION float4 + COLOR float4）
    struct TrailVertex
    {
        XMFLOAT4 pos;   // ワールド座標（w=1）
        XMFLOAT4 color; // RGBA
    };

    static bool LoadCSO(const char* path, std::vector<unsigned char>& out)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return false;
        ifs.seekg(0, std::ios::end);
        const size_t sz = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0);
        out.resize(sz);
        ifs.read(reinterpret_cast<char*>(out.data()), sz);
        return true;
    }

    // XMVECTOR → XMFLOAT4 (w=1)
    inline XMFLOAT4 ToFloat4(XMVECTOR v)
    {
        XMFLOAT4 f;
        XMStoreFloat4(&f, XMVectorSetW(v, 1.0f));
        return f;
    }
}

//==============================================================================
// 共有リソース初期化
//==============================================================================
void Trail::SharedInit()
{
    if (++s_RefCount > 1) return; // 2 つ目以降はスキップ

    ID3D11Device* dev = Direct3D_GetDevice();
    std::vector<unsigned char> bc;

    //--------------------------------------------------------------------------
    // 頂点シェーダー + 入力レイアウト
    //--------------------------------------------------------------------------
    if (LoadCSO("resource/shader/shader_vertex_trail.cso", bc))
    {
        dev->CreateVertexShader(bc.data(), bc.size(), nullptr, &s_pVS);

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
              offsetof(TrailVertex, pos),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
              offsetof(TrailVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        dev->CreateInputLayout(layout, 2, bc.data(), bc.size(), &s_pIL);
    }

    //--------------------------------------------------------------------------
    // ピクセルシェーダー
    //--------------------------------------------------------------------------
    if (LoadCSO("resource/shader/shader_pixel_trail.cso", bc))
        dev->CreatePixelShader(bc.data(), bc.size(), nullptr, &s_pPS);

    //--------------------------------------------------------------------------
    // 加算ブレンドステート（SrcAlpha + One = グロー）
    //--------------------------------------------------------------------------
    {
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable           = TRUE;
        bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
        bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        dev->CreateBlendState(&bd, &s_pBlendAdd);
    }

    //--------------------------------------------------------------------------
    // 深度ステート（テスト ON / 書き込み OFF）
    //--------------------------------------------------------------------------
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable    = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        dev->CreateDepthStencilState(&dsd, &s_pDSSNoWrite);
    }

    //--------------------------------------------------------------------------
    // ラスタライザー（カリングなし：リボンを両面描画）
    // カメラ角度によって三角形のワインディングが反転するため必須
    //--------------------------------------------------------------------------
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode        = D3D11_FILL_SOLID;
        rd.CullMode        = D3D11_CULL_NONE;   // 両面描画
        rd.DepthClipEnable = TRUE;
        dev->CreateRasterizerState(&rd, &s_pRSNoCull);
    }

    //--------------------------------------------------------------------------
    // View / Proj 専用定数バッファ（b1 / b2）
    // 他シェーダー（Billboard 等）が b1/b2 を汚染しても Trail は自前で上書きする
    //--------------------------------------------------------------------------
    {
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth  = sizeof(XMFLOAT4X4);
        cbd.Usage      = D3D11_USAGE_DEFAULT;
        cbd.BindFlags  = D3D11_BIND_CONSTANT_BUFFER;
        dev->CreateBuffer(&cbd, nullptr, &s_pCBView);
        dev->CreateBuffer(&cbd, nullptr, &s_pCBProj);
    }
}

//==============================================================================
// 共有リソース終了
//==============================================================================
void Trail::SharedFinalize()
{
    if (--s_RefCount > 0) return;

    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    rel(s_pCBProj);
    rel(s_pCBView);
    rel(s_pRSNoCull);
    rel(s_pDSSNoWrite);
    rel(s_pBlendAdd);
    rel(s_pIL);
    rel(s_pPS);
    rel(s_pVS);
}

//==============================================================================
// インスタンス初期化
//==============================================================================
void Trail::Initialize(int maxPoints, float width, float maxAge, XMFLOAT4 color)
{
    m_MaxPoints = maxPoints > 2 ? maxPoints : 2;
    m_Width     = width;
    m_MaxAge    = maxAge > 0.0f ? maxAge : 0.01f;
    m_Color     = color;

    m_Points.reserve(m_MaxPoints);
    SharedInit();
}

//==============================================================================
// インスタンス終了
//==============================================================================
void Trail::Finalize()
{
    Clear();
    if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
    m_VBCapacity = 0;
    SharedFinalize();
}

//==============================================================================
// 全ポイントクリア
//==============================================================================
void Trail::Clear()
{
    m_Points.clear();
}

//==============================================================================
// 動的 VB のサイズ保証
//==============================================================================
void Trail::EnsureVB(int vertexCount)
{
    if (vertexCount <= m_VBCapacity) return;

    if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth      = static_cast<UINT>(sizeof(TrailVertex) * vertexCount);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (SUCCEEDED(Direct3D_GetDevice()->CreateBuffer(&bd, nullptr, &m_pVB)))
        m_VBCapacity = vertexCount;
}

//==============================================================================
// 更新
// ・末尾（古い）→ 先頭（新しい）順で m_Points を管理
// ・m_Points[0] = 最古（尾） / m_Points[n-1] = 最新（頭）
//==============================================================================
void Trail::Update(double dt, const XMFLOAT3& headPos)
{
    const float fdt = static_cast<float>(dt);

    // 全ポイントをエージング
    for (auto& p : m_Points)
        p.age += fdt;

    // 寿命切れを先頭から削除
    while (!m_Points.empty() && m_Points.front().age >= m_MaxAge)
        m_Points.erase(m_Points.begin());

    // 新ポイントを末尾に追加
    TrailPoint tp;
    tp.pos = headPos;
    tp.age = 0.0f;
    m_Points.push_back(tp);

    // 最大数超えなら先頭を削除
    if ((int)m_Points.size() > m_MaxPoints)
        m_Points.erase(m_Points.begin());
}

//==============================================================================
// 描画
//==============================================================================
void Trail::Draw()
{
    const int n = static_cast<int>(m_Points.size());
    if (n < 2) return;
    if (!s_pVS || !s_pPS || !s_pIL) return;

    const int numVerts = n * 2;
    EnsureVB(numVerts);
    if (!m_pVB) return;

    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    //--------------------------------------------------------------------------
    // CPU 側頂点生成
    //--------------------------------------------------------------------------
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    TrailVertex* verts = reinterpret_cast<TrailVertex*>(mapped.pData);

    const XMFLOAT3& camPosF = 
        Player_Camera_GetPosition();
    const XMVECTOR  camPosV = XMLoadFloat3(&camPosF);
    const float     halfW   = m_Width * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        XMVECTOR pos = XMLoadFloat3(&m_Points[i].pos);

        // ── タンジェント（リボンの進行方向）─────────────────────────────────
        XMVECTOR tangent;
        if (i == 0)
            tangent = XMVectorSubtract(XMLoadFloat3(&m_Points[1].pos), pos);
        else if (i == n - 1)
            tangent = XMVectorSubtract(pos, XMLoadFloat3(&m_Points[n - 2].pos));
        else
            tangent = XMVectorSubtract(XMLoadFloat3(&m_Points[i + 1].pos),
                                       XMLoadFloat3(&m_Points[i - 1].pos));

        tangent = XMVector3NormalizeEst(tangent);

        // ── カメラ向き垂直方向（リボン幅方向）───────────────────────────────
        XMVECTOR toCam = XMVector3NormalizeEst(XMVectorSubtract(camPosV, pos));

        // toCam と tangent が平行に近い場合はワールドUpをフォールバックにする
        float para = XMVectorGetX(XMVector3Dot(toCam, tangent));
        XMVECTOR side;
        if (fabsf(para) > 0.99f)
            side = XMVector3NormalizeEst(XMVector3Cross(XMVectorSet(0, 1, 0, 0), tangent));
        else
            side = XMVector3NormalizeEst(XMVector3Cross(toCam, tangent));

        // 前フレームの side と向きが逆なら反転してよじれを防ぐ
        if (i > 0)
        {
            XMVECTOR prevLeft  = XMLoadFloat4(reinterpret_cast<XMFLOAT4*>(&verts[(i-1)*2+0].pos));
            XMVECTOR prevRight = XMLoadFloat4(reinterpret_cast<XMFLOAT4*>(&verts[(i-1)*2+1].pos));
            XMVECTOR prevSide  = XMVector3NormalizeEst(XMVectorSubtract(prevRight, prevLeft));
            if (XMVectorGetX(XMVector3Dot(side, prevSide)) < 0.0f)
                side = XMVectorNegate(side);
        }

        // ── アルファ：新しいほど不透明（ヘッドが先端 = 最新 = i が大きい）──
        const float t     = static_cast<float>(i) / static_cast<float>(n - 1);
        const float alpha = t * m_Color.w; // 0→tail, 1→head
        const XMFLOAT4 col = { m_Color.x, m_Color.y, m_Color.z, alpha };

        // ── 左右頂点────────────────────────────────────────────────────────
        XMVECTOR left  = XMVectorSubtract(pos, XMVectorScale(side, halfW));
        XMVECTOR right = XMVectorAdd     (pos, XMVectorScale(side, halfW));

        verts[i * 2 + 0] = { ToFloat4(left),  col };
        verts[i * 2 + 1] = { ToFloat4(right), col };
    }

    ctx->Unmap(m_pVB, 0);

    //--------------------------------------------------------------------------
    // シェーダー / ステート設定
    //--------------------------------------------------------------------------
    ctx->VSSetShader(s_pVS, nullptr, 0);
    ctx->PSSetShader(s_pPS, nullptr, 0);
    ctx->IASetInputLayout(s_pIL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // View/Proj を Trail 専用 CB に書き込んで b1/b2 にバインド
    // （Billboard 等が b1/b2 を空バッファで上書きしても影響を受けないようにする）
    if (s_pCBView && s_pCBProj)
    {
        XMFLOAT4X4 vT, pT;
        XMStoreFloat4x4(&vT, XMMatrixTranspose(XMLoadFloat4x4(&Player_Camera_GetViewMatrix())));
        XMStoreFloat4x4(&pT, XMMatrixTranspose(XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix())));
        ctx->UpdateSubresource(s_pCBView, 0, nullptr, &vT, 0, 0);
        ctx->UpdateSubresource(s_pCBProj, 0, nullptr, &pT, 0, 0);
        ctx->VSSetConstantBuffers(1, 1, &s_pCBView);
        ctx->VSSetConstantBuffers(2, 1, &s_pCBProj);
    }

    // ステート保存
    const float bf[4] = {};
    ID3D11BlendState*        prevBS  = nullptr;
    ID3D11DepthStencilState* prevDSS = nullptr;
    ID3D11RasterizerState*   prevRS  = nullptr;
    UINT                     prevRef = 0;
    FLOAT                    prevBF[4] = {};
    ctx->OMGetBlendState(&prevBS, prevBF, nullptr);
    ctx->OMGetDepthStencilState(&prevDSS, &prevRef);
    ctx->RSGetState(&prevRS);

    // 加算ブレンド / 深度テストON・書き込みOFF / カリングなし
    ctx->OMSetBlendState(s_pBlendAdd,   bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(s_pDSSNoWrite, 0);
    ctx->RSSetState(s_pRSNoCull);

    //--------------------------------------------------------------------------
    // 描画
    //--------------------------------------------------------------------------
    const UINT stride = sizeof(TrailVertex);
    const UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
    ctx->Draw(static_cast<UINT>(numVerts), 0);

    //--------------------------------------------------------------------------
    // ステート復元
    //--------------------------------------------------------------------------
    ctx->OMSetBlendState(prevBS,  prevBF, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(prevDSS, prevRef);
    ctx->RSSetState(prevRS);
    if (prevBS)  prevBS->Release();
    if (prevDSS) prevDSS->Release();
    if (prevRS)  prevRS->Release();
}
