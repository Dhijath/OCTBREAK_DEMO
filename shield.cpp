/*==============================================================================

   シールドシステム [shield.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/21
--------------------------------------------------------------------------------

   ■描画方針
   ・D10（ペンタゴナル・トラペゾヘドロン）を CPU 側でワールド座標に変換し
     動的頂点バッファへアップロードして描画する
   ・Trail と同じ VS/PS（float4 pos + float4 color）を流用
   ・αブレンド / 深度書き込みOFF / 両面描画

==============================================================================*/
#include "shield.h"
#include "direct3d.h"
#include "player_camera.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <fstream>
#include <vector>
#include <cmath>

using namespace DirectX;

//==============================================================================
// 内部定数・型
//==============================================================================
namespace
{
    struct ShieldVertex
    {
        XMFLOAT4 pos;   // ワールド座標 w=1
        XMFLOAT4 color; // RGBA
    };

    static constexpr int   VERTEX_COUNT = 20;     // 正12面体（ドデカヘドロン）頂点数
    static constexpr int   INDEX_COUNT = 108;    // 12面 × 3三角形 × 3頂点
    static constexpr float SHIELD_RADIUS = 0.85f;  // シールド半径（m）
    static constexpr float DAMAGE_REDUCTION = 0.5f;   // 被ダメ軽減率
    static constexpr float FLASH_DURATION = 0.15f;  // フラッシュ持続（秒）

    //--------------------------------------------------------------------------
    // D10 ローカル頂点・インデックス（起動時に BuildGeometry() で生成）
    //--------------------------------------------------------------------------
    XMFLOAT3 s_LocalVerts[VERTEX_COUNT];
    UINT16   s_Indices[INDEX_COUNT];

    //--------------------------------------------------------------------------
    // 状態
    //--------------------------------------------------------------------------
    bool  g_Active = false;
    float g_FlashTimer = 0.0f;

    //--------------------------------------------------------------------------
    // GPU リソース
    //--------------------------------------------------------------------------
    ID3D11VertexShader* g_pVS = nullptr;
    ID3D11PixelShader* g_pPS = nullptr;
    ID3D11InputLayout* g_pIL = nullptr;
    ID3D11Buffer* g_pVB = nullptr;   // 動的頂点バッファ
    ID3D11Buffer* g_pIB = nullptr;   // 静的インデックスバッファ
    ID3D11Buffer* g_pCBView = nullptr;   // VS b1: View
    ID3D11Buffer* g_pCBProj = nullptr;   // VS b2: Proj
    ID3D11BlendState* g_pBS = nullptr;   // αブレンド
    ID3D11DepthStencilState* g_pDSS = nullptr;   // 深度テストON / 書き込みOFF
    ID3D11RasterizerState* g_pRS = nullptr;   // 両面描画

    //--------------------------------------------------------------------------
    // 正12面体（ドデカヘドロン）ジオメトリ生成
    // 頂点 20 個、五角形面 12 枚（各 3 三角形に分割）= 36 三角形 = 108 インデックス
    //
    // 頂点グループ（全頂点の外接球半径 = √3、s = 1/√3 で正規化）:
    //   G1: (±1, ±1, ±1)         → ×s  → indices  0〜 7
    //   G2: (0, ±φ, ±1/φ)       → ×s  → indices  8〜11
    //   G3: (±1/φ, 0, ±φ)       → ×s  → indices 12〜15
    //   G4: (±φ, ±1/φ, 0)       → ×s  → indices 16〜19
    //--------------------------------------------------------------------------
    void BuildGeometry()
    {
        const float phi = (1.0f + sqrtf(5.0f)) / 2.0f; // 黄金比 φ ≈ 1.618
        const float r = 1.0f / phi;                   // 1/φ ≈ 0.618
        const float s = 1.0f / sqrtf(3.0f);           // 正規化スケール（外接球半径 → 1）
        const float sp = phi * s;
        const float sr = r * s;

        // ── 頂点 ─────────────────────────────────────────────────────────────
        // G1: (±1, ±1, ±1)
        s_LocalVerts[0] = { s,  s,  s };
        s_LocalVerts[1] = { s,  s, -s };
        s_LocalVerts[2] = { s, -s,  s };
        s_LocalVerts[3] = { s, -s, -s };
        s_LocalVerts[4] = { -s,  s,  s };
        s_LocalVerts[5] = { -s,  s, -s };
        s_LocalVerts[6] = { -s, -s,  s };
        s_LocalVerts[7] = { -s, -s, -s };
        // G2: (0, ±φ, ±1/φ)
        s_LocalVerts[8] = { 0,  sp,  sr };
        s_LocalVerts[9] = { 0,  sp, -sr };
        s_LocalVerts[10] = { 0, -sp,  sr };
        s_LocalVerts[11] = { 0, -sp, -sr };
        // G3: (±1/φ, 0, ±φ)
        s_LocalVerts[12] = { sr, 0,  sp };
        s_LocalVerts[13] = { -sr, 0,  sp };
        s_LocalVerts[14] = { sr, 0, -sp };
        s_LocalVerts[15] = { -sr, 0, -sp };
        // G4: (±φ, ±1/φ, 0)
        s_LocalVerts[16] = { sp,  sr, 0 };
        s_LocalVerts[17] = { sp, -sr, 0 };
        s_LocalVerts[18] = { -sp,  sr, 0 };
        s_LocalVerts[19] = { -sp, -sr, 0 };

        // ── 12 面（各五角形をファン分割で 3 三角形に）────────────────────────
        static const UINT16 faces[12][5] =
        {
            {  8,  0, 16,  1,  9 },  // F0
            {  0, 12,  2, 17, 16 },  // F1
            {  0,  8,  4, 13, 12 },  // F2
            {  1,  9,  5, 15, 14 },  // F3
            {  1, 14,  3, 17, 16 },  // F4
            {  8,  9,  5, 18,  4 },  // F5
            {  4, 18, 19,  6, 13 },  // F6
            {  7, 15, 14,  3, 11 },  // F7
            {  3, 17,  2, 10, 11 },  // F8
            {  2, 12, 13,  6, 10 },  // F9
            {  5, 15,  7, 19, 18 },  // F10
            { 11, 10,  6, 19,  7 },  // F11
        };

        int idx = 0;
        for (int f = 0; f < 12; ++f)
        {
            const UINT16* p = faces[f];
            // 五角形をファン分割: (p0,p1,p2), (p0,p2,p3), (p0,p3,p4)
            s_Indices[idx++] = p[0]; s_Indices[idx++] = p[1]; s_Indices[idx++] = p[2];
            s_Indices[idx++] = p[0]; s_Indices[idx++] = p[2]; s_Indices[idx++] = p[3];
            s_Indices[idx++] = p[0]; s_Indices[idx++] = p[3]; s_Indices[idx++] = p[4];
        }
    }

    //--------------------------------------------------------------------------
    // CSO バイナリ読み込み
    //--------------------------------------------------------------------------
    static bool LoadCso(const char* path, std::vector<char>& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        out.resize(static_cast<size_t>(f.tellg()));
        f.seekg(0);
        f.read(out.data(), out.size());
        return !out.empty();
    }
}


//==============================================================================
// Shield_Initialize
//==============================================================================
void Shield_Initialize()
{
    BuildGeometry();

    auto* dev = Direct3D_GetDevice();
    auto* ctx = Direct3D_GetContext();

    // ── VS/PS（Trail シェーダーを流用）─────────────────────────────────────
    std::vector<char> vsBin, psBin;
    if (!LoadCso("resource/shader/shader_vertex_trail.cso", vsBin))
    {
        MessageBox(nullptr, "shield: shader_vertex_trail.cso が見つかりません", "Shield Error", MB_OK);
        return;
    }
    if (!LoadCso("resource/shader/shader_pixel_trail.cso", psBin))
    {
        MessageBox(nullptr, "shield: shader_pixel_trail.cso が見つかりません", "Shield Error", MB_OK);
        return;
    }

    dev->CreateVertexShader(vsBin.data(), vsBin.size(), nullptr, &g_pVS);
    dev->CreatePixelShader(psBin.data(), psBin.size(), nullptr, &g_pPS);

    // ── 入力レイアウト（Trail と同じ: float4 POSITION + float4 COLOR0）──
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    dev->CreateInputLayout(layout, 2, vsBin.data(), vsBin.size(), &g_pIL);

    // ── 動的頂点バッファ──────────────────────────────────────────────────
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(ShieldVertex) * VERTEX_COUNT;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    dev->CreateBuffer(&vbDesc, nullptr, &g_pVB);

    // ── 静的インデックスバッファ──────────────────────────────────────────
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(UINT16) * INDEX_COUNT;
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{ s_Indices };
    dev->CreateBuffer(&ibDesc, &ibData, &g_pIB);

    // ── View/Proj 定数バッファ────────────────────────────────────────────
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(XMFLOAT4X4);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&cbDesc, nullptr, &g_pCBView);
    dev->CreateBuffer(&cbDesc, nullptr, &g_pCBProj);

    // ── αブレンドステート────────────────────────────────────────────────
    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&blendDesc, &g_pBS);

    // ── 深度ステート（テストON / 書き込みOFF）────────────────────────────
    D3D11_DEPTH_STENCIL_DESC dssDesc{};
    dssDesc.DepthEnable = TRUE;
    dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dev->CreateDepthStencilState(&dssDesc, &g_pDSS);

    // ── ラスタライザ（両面描画）──────────────────────────────────────────
    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rsDesc, &g_pRS);
}


//==============================================================================
// Shield_Finalize
//==============================================================================
void Shield_Finalize()
{
    SAFE_RELEASE(g_pRS);
    SAFE_RELEASE(g_pDSS);
    SAFE_RELEASE(g_pBS);
    SAFE_RELEASE(g_pCBProj);
    SAFE_RELEASE(g_pCBView);
    SAFE_RELEASE(g_pIB);
    SAFE_RELEASE(g_pVB);
    SAFE_RELEASE(g_pIL);
    SAFE_RELEASE(g_pPS);
    SAFE_RELEASE(g_pVS);
}


//==============================================================================
// Shield_Update
//==============================================================================
void Shield_Update(double dt, bool guarding)
{
    g_Active = guarding;
    if (g_FlashTimer > 0.0f)
        g_FlashTimer -= static_cast<float>(dt);
}


//==============================================================================
// Shield_Draw
//==============================================================================
void Shield_Draw(const XMFLOAT3& center)
{
    // デバッグ：初期化チェック
    if (!g_pVB)
    {
        MessageBox(nullptr, "Shield: g_pVB が null（Initialize 失敗）", "Shield Debug", MB_OK);
        return;
    }
    if (!g_Active) return;

    auto* ctx = Direct3D_GetContext();

    // ── フラッシュ係数（0.0〜1.0）────────────────────────────────────────
    const float flash = (g_FlashTimer > 0.0f)
        ? (g_FlashTimer / FLASH_DURATION)
        : 0.0f;

    const XMFLOAT4 color =
    {
        0.2f + flash * 0.8f,
        0.6f + flash * 0.4f,
        1.0f,
        0.25f + flash * 0.4f
    };

    // ── CPU 側でワールド座標に変換して頂点バッファへ書き込み──────────────
    ShieldVertex verts[VERTEX_COUNT];
    for (int i = 0; i < VERTEX_COUNT; ++i)
    {
        verts[i].pos =
        {
            s_LocalVerts[i].x * SHIELD_RADIUS + center.x,
            s_LocalVerts[i].y * SHIELD_RADIUS + center.y,
            s_LocalVerts[i].z * SHIELD_RADIUS + center.z,
            1.0f
        };
        verts[i].color = color;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, verts, sizeof(verts));
        ctx->Unmap(g_pVB, 0);
    }

    // ── View/Proj を定数バッファへ──────────────────────────────────────────
    {
        XMFLOAT4X4 vt, pt;
        XMStoreFloat4x4(&vt, XMMatrixTranspose(XMLoadFloat4x4(&Player_Camera_GetViewMatrix())));
        XMStoreFloat4x4(&pt, XMMatrixTranspose(XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix())));
        ctx->UpdateSubresource(g_pCBView, 0, nullptr, &vt, 0, 0);
        ctx->UpdateSubresource(g_pCBProj, 0, nullptr, &pt, 0, 0);
    }

    // ── パイプラインセット──────────────────────────────────────────────────
    ctx->VSSetShader(g_pVS, nullptr, 0);
    ctx->PSSetShader(g_pPS, nullptr, 0);
    ctx->IASetInputLayout(g_pIL);

    UINT stride = sizeof(ShieldVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
    ctx->IASetIndexBuffer(g_pIB, DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetConstantBuffers(1, 1, &g_pCBView);
    ctx->VSSetConstantBuffers(2, 1, &g_pCBProj);

    const float blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(g_pBS, blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_pDSS, 0);
    ctx->RSSetState(g_pRS);

    ctx->DrawIndexed(INDEX_COUNT, 0, 0);

    // ── ステート復元────────────────────────────────────────────────────────
    ctx->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(nullptr, 0);
    ctx->RSSetState(nullptr);
}


//==============================================================================
// その他 API
//==============================================================================
bool  Shield_IsActive() { return g_Active; }
void  Shield_NotifyHit() { g_FlashTimer = FLASH_DURATION; }
float Shield_GetDamageReduction() { return DAMAGE_REDUCTION; }