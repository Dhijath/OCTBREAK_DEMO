/*==============================================================================

   ブロックステージ描画 [BlockStageRender.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   概要・既存描画との共存方法は BlockStageRender.h を参照。

   ■色の扱い
     色はリニア値で指定し、ピクセルシェーダーの最後で ACES トーンマップと
     ガンマ補正を掛ける（バックバッファが sRGB ではないため、ここで補正する）。
     そのため「暗い地の色 ＋ 1.0 を超える自己発光」の組み合わせが
     白飛びせずネオン風に出る。

   ■法線
     頂点には位置しか持たせず、ピクセルシェーダーでワールド座標の微分から
     面法線を求める。非一様スケールや回転を掛けても正しい面法線になり、
     メッシュの巻き順にも依存しない（カリングなしで描く）。

==============================================================================*/
#include "BlockStageRender.h"
#include "direct3d.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <cmath>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    //==========================================================================
    // シェーダー
    //==========================================================================
    const char kShaderSource[] = R"HLSL(
cbuffer StageFrame : register(b10)
{
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4 gCamPos;       // xyz, time
    float4 gLightDir;     // xyz
    float4 gFogColor;     // rgb
    float4 gFogParams;    // start, end
    float4 gSkyTop;
    float4 gSkyBottom;
    float4 gNebula;       // rgb, intensity
    float4 gGridColor;    // rgb, cell size
    float4 gGroundColor;  // rgb
};
cbuffer StageObj : register(b11)
{
    float4x4 gWorld;
    float4 gColor;        // rgb
    float4 gParams;       // x: emissive / glow strength, z: grid plane Y, w: grid receives shadow
};

// ---- existing shadow resources (same layout as shader_pixel_3d.hlsl; bound by the game)
cbuffer CB_SHADOW_PARAM : register(b5)
{
    float2 shadowMapSize;
    float  shadowDepthBias;
    float  shadowPad0;
    float  shadowStrength;
    float  shadowPCF;
    float2 shadowPad1;
};
cbuffer BLOB_SHADOW : register(b6)
{
    float3 blobCenterW;
    float  blobRadius;
    float  blobSoftness;
    float  blobStrength;
    float2 blobPad;
};
cbuffer CB_LIGHT_VP : register(b8)
{
    float4x4 lightViewProj;
};
Texture2D              shadowMap     : register(t7);
SamplerComparisonState shadowSampler : register(s1);

// 1 = lit. Only up-facing surfaces receive the blob shadow
float ShadowAt(float3 wp, float3 n)
{
    float s = 1.0;

    if (n.y > 0.5)
    {
        float dist = length(wp.xz - blobCenterW.xz);
        float t = saturate((dist - blobRadius) / max(blobSoftness, 0.0001));
        float inside = (1.0 - t) * (1.0 - t);
        s *= lerp(1.0 - blobStrength, 1.0, 1.0 - inside);
    }

    if (shadowStrength > 0.0)
    {
        float4 posLight = mul(float4(wp, 1.0), lightViewProj);
        float3 ndc = posLight.xyz / posLight.w;
        float2 uv = ndc.xy * float2(0.5, -0.5) + 0.5;
        if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0 && ndc.z >= 0.0 && ndc.z <= 1.0)
        {
            float cmp = ndc.z - shadowDepthBias;
            float f = 0.0;
            if (shadowPCF > 0.5)
            {
                float2 texel = 1.0 / shadowMapSize;
                for (int y = -1; y <= 1; y++)
                    for (int x = -1; x <= 1; x++)
                        f += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texel, cmp);
                f /= 9.0;
            }
            else
            {
                f = shadowMap.SampleCmpLevelZero(shadowSampler, uv, cmp);
            }
            s *= lerp(1.0 - shadowStrength, 1.0, f);
        }
    }
    return s;
}

float FogAt(float3 wp)
{
    float d = distance(gCamPos.xyz, wp);
    return saturate((d - gFogParams.x) / (gFogParams.y - gFogParams.x));
}

float3 Tonemap(float3 x)
{
    float3 c = saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
    return pow(c, 1.0 / 2.2);
}

float hash3(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float vnoise(float3 x)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(hash3(i + float3(0, 0, 0)), hash3(i + float3(1, 0, 0)), f.x),
                     lerp(hash3(i + float3(0, 1, 0)), hash3(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(hash3(i + float3(0, 0, 1)), hash3(i + float3(1, 0, 1)), f.x),
                     lerp(hash3(i + float3(0, 1, 1)), hash3(i + float3(1, 1, 1)), f.x), f.y), f.z);
}
float fbm(float3 p)
{
    float a = 0.5, s = 0.0;
    for (int k = 0; k < 4; k++) { s += a * vnoise(p); p = p * 2.03 + 7.1; a *= 0.5; }
    return s;
}

// ---------------------------------------------------------------- mesh
struct MeshOut { float4 pos : SV_Position; float3 wpos : TEXCOORD0; };

MeshOut VSMesh(float3 pos : POSITION)
{
    MeshOut o;
    float4 wp = mul(float4(pos, 1.0), gWorld);
    o.pos = mul(wp, gViewProj);
    o.wpos = wp.xyz;
    return o;
}

// face normal from derivatives, always facing the viewer
float3 FaceNormal(float3 wpos, float3 V)
{
    float3 n = normalize(cross(ddx(wpos), ddy(wpos)));
    return (dot(n, V) < 0.0) ? -n : n;
}

float4 PSMesh(MeshOut i) : SV_Target
{
    float3 V = normalize(gCamPos.xyz - i.wpos);
    float3 n = FaceNormal(i.wpos, V);
    float3 L = normalize(-gLightDir.xyz);

    float  diff = saturate(dot(n, L));
    float3 amb  = lerp(gSkyBottom.rgb, gSkyTop.rgb, n.y * 0.5 + 0.5) * 0.7 + 0.2;
    float3 H    = normalize(L + V);
    float  spec = pow(saturate(dot(n, H)), 40.0) * 0.35;
    float  rim  = pow(1.0 - saturate(dot(n, V)), 3.0);
    float  sh   = ShadowAt(i.wpos, n);

    float3 c = gColor.rgb * (amb * (0.65 + 0.35 * sh) + diff * 0.95 * sh) + spec * sh + gColor.rgb * rim * 0.7;
    c += gColor.rgb * gParams.x;
    c = lerp(c, gFogColor.rgb, FogAt(i.wpos));
    return float4(Tonemap(c), 1.0);
}

// glow shell (additive): strongest when facing the viewer, fading toward the silhouette
float4 PSGlow(MeshOut i) : SV_Target
{
    float3 V = normalize(gCamPos.xyz - i.wpos);
    float3 n = FaceNormal(i.wpos, V);
    float  f = abs(dot(n, V));
    f = f * f;
    return float4(gColor.rgb * gParams.x * f * (1.0 - FogAt(i.wpos)), 1.0);
}

// ---------------------------------------------------------------- sky
struct FsOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

FsOut VSFull(uint id : SV_VertexID)
{
    FsOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 1.0, 1.0);
    return o;
}

float4 PSSky(FsOut i) : SV_Target
{
    float2 ndc = float2(i.uv.x * 2.0 - 1.0, 1.0 - i.uv.y * 2.0);
    float4 wp = mul(float4(ndc, 1.0, 1.0), gInvViewProj);
    float3 dir = normalize(wp.xyz / wp.w - gCamPos.xyz);

    float3 col = lerp(gSkyBottom.rgb, gSkyTop.rgb, smoothstep(-0.05, 0.6, dir.y));
    col += gFogColor.rgb * pow(saturate(1.0 - abs(dir.y)), 10.0) * 0.8;

    // nebula
    float n1 = fbm(dir * 2.5 + float3(0.0, 0.0, gCamPos.w * 0.01));
    float n2 = fbm(dir * 5.0 + 11.3);
    float neb = saturate(n1 * 1.6 - 0.35);
    col += gNebula.rgb * gNebula.a * neb * neb * (0.5 + n2);

    // stars
    float3 sp = dir * 110.0;
    float3 ip = floor(sp);
    float3 fp = frac(sp) - 0.5;
    float h = hash3(ip);
    float star = step(0.985, h) * smoothstep(0.3, 0.0, length(fp));
    col += star * (0.5 + 1.2 * frac(h * 91.7));

    // sun in the light direction
    float3 sunDir = normalize(-gLightDir.xyz);
    float sd = saturate(dot(dir, sunDir));
    col += (gNebula.rgb * 0.6 + 0.4) * (pow(sd, 1200.0) * 6.0 + pow(sd, 30.0) * 0.3);

    return float4(Tonemap(col), 1.0);
}

// ---------------------------------------------------------------- grid plane
struct GridOut { float4 pos : SV_Position; float3 wpos : TEXCOORD0; };

GridOut VSGrid(uint id : SV_VertexID)
{
    GridOut o;
    float2 q = float2(id & 1, id >> 1);
    float3 wp = float3(lerp(-700.0, 700.0, q.x), gParams.z, lerp(-700.0, 700.0, q.y));
    o.wpos = wp;
    o.pos = mul(float4(wp, 1.0), gViewProj);
    return o;
}

float4 PSGrid(GridOut i) : SV_Target
{
    float cell = gGridColor.w;
    float2 uv = i.wpos.xz / cell;
    float2 fw = max(fwidth(uv), 1e-4);
    float2 g  = abs(frac(uv - 0.5) - 0.5);

    // lines are at least 6cm wide; far away they shrink to 1px and fade out
    float  halfWidth = 0.03 / cell;
    float2 a  = saturate(1.2 - (g - halfWidth) / fw);
    float  ln = max(a.x, a.y) * saturate(1.4 - 2.0 * max(fw.x, fw.y));
    float  glow = exp(-min(g.x, g.y) * 16.0);

    float3 c = gGroundColor.rgb + gGridColor.rgb * (0.03 + ln * 1.6 + glow * 0.35);
    if (gParams.w > 0.5)
        c *= ShadowAt(i.wpos, float3(0.0, 1.0, 0.0));

    c = lerp(c, gFogColor.rgb, FogAt(i.wpos));
    return float4(Tonemap(c), 1.0);
}
)HLSL";

    //==========================================================================
    // 定数バッファ
    //==========================================================================
    constexpr UINT SLOT_FRAME = 10;
    constexpr UINT SLOT_OBJ   = 11;

    struct CbFrame
    {
        XMFLOAT4X4 viewProj;
        XMFLOAT4X4 invViewProj;
        XMFLOAT4 camPos;
        XMFLOAT4 lightDir;
        XMFLOAT4 fogColor;
        XMFLOAT4 fogParams;
        XMFLOAT4 skyTop;
        XMFLOAT4 skyBottom;
        XMFLOAT4 nebula;
        XMFLOAT4 gridColor;
        XMFLOAT4 groundColor;
    };

    struct CbObj
    {
        XMFLOAT4X4 world;
        XMFLOAT4 color;
        XMFLOAT4 params;
    };

    // シャドウマップと同じ光の向き（game.cpp のシャドウパスに合わせる）
    constexpr XMFLOAT4 LIGHT_DIR = { 1.0f, -0.8f, 0.5f, 0.0f };

    //==========================================================================
    // リソース
    //==========================================================================
    enum class InitState { NotYet, Ready, Failed };
    InitState g_Init = InitState::NotYet;

    ComPtr<ID3D11VertexShader> g_VsMesh, g_VsFull, g_VsGrid;
    ComPtr<ID3D11PixelShader>  g_PsMesh, g_PsGlow, g_PsSky, g_PsGrid;
    ComPtr<ID3D11InputLayout>  g_Layout;
    ComPtr<ID3D11Buffer>       g_CbFrame, g_CbObj;
    ComPtr<ID3D11RasterizerState>   g_RsNoCull;
    ComPtr<ID3D11DepthStencilState> g_DsDefault, g_DsNoWrite, g_DsNone;
    ComPtr<ID3D11BlendState>        g_BsOpaque, g_BsAdd;

    constexpr int MESH_COUNT = static_cast<int>(StageMesh::Count);
    ComPtr<ID3D11Buffer> g_MeshVb[MESH_COUNT];
    UINT                 g_MeshVertexCount[MESH_COUNT] = {};

    // Begin で退避する描画状態
    ID3D11RasterizerState*   g_OldRs = nullptr;
    ID3D11DepthStencilState* g_OldDs = nullptr;
    UINT                     g_OldStencilRef = 0;
    ID3D11BlendState*        g_OldBlend = nullptr;
    FLOAT                    g_OldBlendFactor[4] = {};
    UINT                     g_OldSampleMask = 0xffffffff;

    //==========================================================================
    // メッシュ生成（位置のみ・三角形リスト）
    //==========================================================================
    using Verts = std::vector<XMFLOAT3>;

    void Tri(Verts& v, const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
    {
        v.push_back(a); v.push_back(b); v.push_back(c);
    }

    void Quad(Verts& v, const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d)
    {
        Tri(v, a, b, c);
        Tri(v, a, c, d);
    }

    // 上下で大きさの違う四角柱（top = 上面の倍率。1 で立方体、0 で四角錐）
    Verts MakeTaperedBox(float top)
    {
        Verts v;
        const float b = 0.5f;
        const float t = 0.5f * top;
        const XMFLOAT3 lo[4] = { { -b, -0.5f, -b }, { b, -0.5f, -b }, { b, -0.5f, b }, { -b, -0.5f, b } };
        const XMFLOAT3 hi[4] = { { -t,  0.5f, -t }, { t,  0.5f, -t }, { t,  0.5f, t }, { -t,  0.5f, t } };

        for (int i = 0; i < 4; ++i)
        {
            const int j = (i + 1) % 4;
            if (top > 0.0f) Quad(v, lo[i], lo[j], hi[j], hi[i]);
            else            Tri(v, lo[i], lo[j], hi[0]);
        }
        Quad(v, lo[0], lo[1], lo[2], lo[3]);
        if (top > 0.0f) Quad(v, hi[0], hi[1], hi[2], hi[3]);
        return v;
    }

    Verts MakeOcta()
    {
        Verts v;
        const XMFLOAT3 top = { 0.0f, 0.5f, 0.0f }, bottom = { 0.0f, -0.5f, 0.0f };
        const XMFLOAT3 ring[4] = { { 0.5f, 0, 0 }, { 0, 0, 0.5f }, { -0.5f, 0, 0 }, { 0, 0, -0.5f } };
        for (int i = 0; i < 4; ++i)
        {
            const int j = (i + 1) % 4;
            Tri(v, ring[i], ring[j], top);
            Tri(v, ring[j], ring[i], bottom);
        }
        return v;
    }

    // n角柱
    Verts MakePrism(int sides)
    {
        Verts v;
        const XMFLOAT3 top = { 0.0f, 0.5f, 0.0f }, bottom = { 0.0f, -0.5f, 0.0f };
        for (int i = 0; i < sides; ++i)
        {
            const float a0 = XM_2PI * i / sides;
            const float a1 = XM_2PI * (i + 1) / sides;
            const XMFLOAT3 p0 = { cosf(a0) * 0.5f, -0.5f, sinf(a0) * 0.5f };
            const XMFLOAT3 p1 = { cosf(a1) * 0.5f, -0.5f, sinf(a1) * 0.5f };
            const XMFLOAT3 q0 = { p0.x, 0.5f, p0.z };
            const XMFLOAT3 q1 = { p1.x, 0.5f, p1.z };
            Quad(v, p0, p1, q1, q0);
            Tri(v, q0, q1, top);
            Tri(v, p1, p0, bottom);
        }
        return v;
    }

    // 球。roughness > 0 で半径を乱して岩にする
    Verts MakeSphere(int slices, int stacks, float roughness)
    {
        // 格子点ごとの半径（極と継ぎ目で値が食い違わないように先に決める）
        std::vector<float> radius((stacks + 1) * slices, 0.5f);
        if (roughness > 0.0f)
        {
            unsigned int state = 12345u;
            for (int s = 1; s < stacks; ++s)
                for (int i = 0; i < slices; ++i)
                {
                    state = state * 1664525u + 1013904223u;
                    const float r01 = static_cast<float>((state >> 8) & 0xffff) / 65535.0f;
                    radius[s * slices + i] = 0.5f * (1.0f - roughness + roughness * r01);
                }
        }

        auto point = [&](int s, int i) -> XMFLOAT3
        {
            const int   ii  = i % slices;
            const float phi = XM_PI * s / stacks;          // 0 = 上端
            const float th  = XM_2PI * ii / slices;
            const float r   = (s == 0 || s == stacks) ? 0.5f * (1.0f - roughness * 0.5f) : radius[s * slices + ii];
            return { r * sinf(phi) * cosf(th), r * cosf(phi), r * sinf(phi) * sinf(th) };
        };

        Verts v;
        for (int s = 0; s < stacks; ++s)
            for (int i = 0; i < slices; ++i)
            {
                const XMFLOAT3 a = point(s, i),     b = point(s, i + 1);
                const XMFLOAT3 c = point(s + 1, i), d = point(s + 1, i + 1);
                if (s > 0)          Tri(v, a, b, d);
                if (s < stacks - 1) Tri(v, a, d, c);
            }
        return v;
    }

    // 輪（Y軸まわり。外径1）
    Verts MakeTorus(int segments, int sides)
    {
        constexpr float MAJOR = 0.39f;
        constexpr float MINOR = 0.11f;

        auto point = [&](int seg, int side) -> XMFLOAT3
        {
            const float u = XM_2PI * seg / segments;
            const float w = XM_2PI * side / sides;
            const float r = MAJOR + MINOR * cosf(w);
            return { r * cosf(u), MINOR * sinf(w), r * sinf(u) };
        };

        Verts v;
        for (int seg = 0; seg < segments; ++seg)
            for (int side = 0; side < sides; ++side)
                Quad(v, point(seg, side), point(seg + 1, side), point(seg + 1, side + 1), point(seg, side + 1));
        return v;
    }

    bool CreateMesh(ID3D11Device* dev, StageMesh id, const Verts& verts)
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = static_cast<UINT>(sizeof(XMFLOAT3) * verts.size());
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = verts.data();

        const int index = static_cast<int>(id);
        g_MeshVertexCount[index] = static_cast<UINT>(verts.size());
        return SUCCEEDED(dev->CreateBuffer(&bd, &sd, g_MeshVb[index].ReleaseAndGetAddressOf()));
    }

    //==========================================================================
    // 初期化
    //==========================================================================
    bool Compile(const char* entry, const char* target, ComPtr<ID3DBlob>& outBlob)
    {
        ComPtr<ID3DBlob> error;
        const HRESULT hr = D3DCompile(kShaderSource, strlen(kShaderSource), "BlockStageRender", nullptr, nullptr,
                                      entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                                      outBlob.ReleaseAndGetAddressOf(), error.GetAddressOf());
        if (FAILED(hr) && error)
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        return SUCCEEDED(hr);
    }

    bool Initialize()
    {
        ID3D11Device* dev = Direct3D_GetDevice();
        if (!dev) return false;

        // シェーダー
        ComPtr<ID3DBlob> blob;
        if (!Compile("VSMesh", "vs_5_0", blob)) return false;
        if (FAILED(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_VsMesh.ReleaseAndGetAddressOf()))) return false;

        const D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(dev->CreateInputLayout(layout, 1, blob->GetBufferPointer(), blob->GetBufferSize(), g_Layout.ReleaseAndGetAddressOf()))) return false;

        if (!Compile("VSFull", "vs_5_0", blob)) return false;
        if (FAILED(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_VsFull.ReleaseAndGetAddressOf()))) return false;
        if (!Compile("VSGrid", "vs_5_0", blob)) return false;
        if (FAILED(dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_VsGrid.ReleaseAndGetAddressOf()))) return false;

        if (!Compile("PSMesh", "ps_5_0", blob)) return false;
        if (FAILED(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_PsMesh.ReleaseAndGetAddressOf()))) return false;
        if (!Compile("PSGlow", "ps_5_0", blob)) return false;
        if (FAILED(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_PsGlow.ReleaseAndGetAddressOf()))) return false;
        if (!Compile("PSSky", "ps_5_0", blob)) return false;
        if (FAILED(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_PsSky.ReleaseAndGetAddressOf()))) return false;
        if (!Compile("PSGrid", "ps_5_0", blob)) return false;
        if (FAILED(dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, g_PsGrid.ReleaseAndGetAddressOf()))) return false;

        // 定数バッファ
        {
            D3D11_BUFFER_DESC bd{};
            bd.Usage     = D3D11_USAGE_DEFAULT;
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.ByteWidth = sizeof(CbFrame);
            if (FAILED(dev->CreateBuffer(&bd, nullptr, g_CbFrame.ReleaseAndGetAddressOf()))) return false;
            bd.ByteWidth = sizeof(CbObj);
            if (FAILED(dev->CreateBuffer(&bd, nullptr, g_CbObj.ReleaseAndGetAddressOf()))) return false;
        }

        // 描画状態
        {
            D3D11_RASTERIZER_DESC rd{};
            rd.FillMode        = D3D11_FILL_SOLID;
            rd.CullMode        = D3D11_CULL_NONE;
            rd.DepthClipEnable = TRUE;
            if (FAILED(dev->CreateRasterizerState(&rd, g_RsNoCull.ReleaseAndGetAddressOf()))) return false;

            D3D11_DEPTH_STENCIL_DESC dd{};
            dd.DepthEnable    = TRUE;
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            dd.DepthFunc      = D3D11_COMPARISON_LESS;
            if (FAILED(dev->CreateDepthStencilState(&dd, g_DsDefault.ReleaseAndGetAddressOf()))) return false;
            dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            dd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
            if (FAILED(dev->CreateDepthStencilState(&dd, g_DsNoWrite.ReleaseAndGetAddressOf()))) return false;
            dd.DepthEnable    = FALSE;
            if (FAILED(dev->CreateDepthStencilState(&dd, g_DsNone.ReleaseAndGetAddressOf()))) return false;

            D3D11_BLEND_DESC bl{};
            bl.RenderTarget[0].BlendEnable           = FALSE;
            bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (FAILED(dev->CreateBlendState(&bl, g_BsOpaque.ReleaseAndGetAddressOf()))) return false;
            bl.RenderTarget[0].BlendEnable    = TRUE;
            bl.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
            bl.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
            bl.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
            bl.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ZERO;
            bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
            bl.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
            if (FAILED(dev->CreateBlendState(&bl, g_BsAdd.ReleaseAndGetAddressOf()))) return false;
        }

        // メッシュ
        if (!CreateMesh(dev, StageMesh::Cube,     MakeTaperedBox(1.0f)))      return false;
        if (!CreateMesh(dev, StageMesh::Octa,     MakeOcta()))                return false;
        if (!CreateMesh(dev, StageMesh::Pyramid,  MakeTaperedBox(0.0f)))      return false;
        if (!CreateMesh(dev, StageMesh::Cylinder, MakePrism(16)))             return false;
        if (!CreateMesh(dev, StageMesh::Hex,      MakePrism(6)))              return false;
        if (!CreateMesh(dev, StageMesh::Frustum,  MakeTaperedBox(0.6f)))      return false;
        if (!CreateMesh(dev, StageMesh::Sphere,   MakeSphere(16, 10, 0.0f)))  return false;
        if (!CreateMesh(dev, StageMesh::Torus,    MakeTorus(24, 8)))          return false;
        if (!CreateMesh(dev, StageMesh::Rock,     MakeSphere(9, 6, 0.38f)))   return false;

        return true;
    }

    void SetObj(const XMMATRIX& world, const XMFLOAT3& color, const XMFLOAT4& params)
    {
        CbObj cb{};
        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
        cb.color  = { color.x, color.y, color.z, 1.0f };
        cb.params = params;
        Direct3D_GetContext()->UpdateSubresource(g_CbObj.Get(), 0, nullptr, &cb, 0, 0);
    }

    void DrawMeshBuffer(StageMesh mesh)
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        const int index = static_cast<int>(mesh);
        UINT stride = sizeof(XMFLOAT3);
        UINT offset = 0;
        ctx->IASetInputLayout(g_Layout.Get());
        ctx->IASetVertexBuffers(0, 1, g_MeshVb[index].GetAddressOf(), &stride, &offset);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->Draw(g_MeshVertexCount[index], 0);
    }
}

//==============================================================================
// 描画開始
//==============================================================================
bool StageRender::Begin(const StageFrame& frame)
{
    if (g_Init == InitState::NotYet)
        g_Init = Initialize() ? InitState::Ready : InitState::Failed;
    if (g_Init != InitState::Ready) return false;

    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    // 描画状態を退避（End で戻す）
    ctx->RSGetState(&g_OldRs);
    ctx->OMGetDepthStencilState(&g_OldDs, &g_OldStencilRef);
    ctx->OMGetBlendState(&g_OldBlend, g_OldBlendFactor, &g_OldSampleMask);

    // フレーム定数
    const XMMATRIX view     = XMLoadFloat4x4(&frame.view);
    const XMMATRIX proj     = XMLoadFloat4x4(&frame.proj);
    const XMMATRIX viewProj = view * proj;

    CbFrame cb{};
    XMStoreFloat4x4(&cb.viewProj,    XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&cb.invViewProj, XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj)));
    cb.camPos      = { frame.camPos.x, frame.camPos.y, frame.camPos.z, frame.time };
    cb.lightDir    = LIGHT_DIR;
    cb.fogColor    = { frame.fog.x, frame.fog.y, frame.fog.z, 1.0f };
    cb.fogParams   = { frame.fogStart, frame.fogEnd, 0.0f, 0.0f };
    cb.skyTop      = { frame.skyTop.x, frame.skyTop.y, frame.skyTop.z, 1.0f };
    cb.skyBottom   = { frame.skyBottom.x, frame.skyBottom.y, frame.skyBottom.z, 1.0f };
    cb.nebula      = frame.nebula;
    cb.gridColor   = { frame.grid.x, frame.grid.y, frame.grid.z, frame.gridCell };
    cb.groundColor = { frame.ground.x, frame.ground.y, frame.ground.z, 1.0f };
    ctx->UpdateSubresource(g_CbFrame.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbs[2] = { g_CbFrame.Get(), g_CbObj.Get() };
    ctx->VSSetConstantBuffers(SLOT_FRAME, 2, cbs);
    ctx->PSSetConstantBuffers(SLOT_FRAME, 2, cbs);

    const FLOAT blendFactor[4] = { 0, 0, 0, 0 };
    ctx->RSSetState(g_RsNoCull.Get());
    ctx->OMSetDepthStencilState(g_DsDefault.Get(), 0);
    ctx->OMSetBlendState(g_BsOpaque.Get(), blendFactor, 0xffffffff);
    return true;
}

//==============================================================================
// 空
//==============================================================================
void StageRender::DrawSky()
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    ctx->OMSetDepthStencilState(g_DsNone.Get(), 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_VsFull.Get(), nullptr, 0);
    ctx->PSSetShader(g_PsSky.Get(), nullptr, 0);
    ctx->Draw(3, 0);
    ctx->OMSetDepthStencilState(g_DsDefault.Get(), 0);
}

//==============================================================================
// グリッド面
//==============================================================================
void StageRender::DrawGrid(float y, bool receiveShadow)
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    SetObj(XMMatrixIdentity(), { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, y, receiveShadow ? 1.0f : 0.0f });
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g_VsGrid.Get(), nullptr, 0);
    ctx->PSSetShader(g_PsGrid.Get(), nullptr, 0);
    ctx->Draw(4, 0);
}

//==============================================================================
// メッシュ
//==============================================================================
void StageRender::DrawMesh(StageMesh mesh, const XMMATRIX& world, const XMFLOAT3& color, float emissive)
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    SetObj(world, color, { emissive, 0.0f, 0.0f, 0.0f });
    ctx->VSSetShader(g_VsMesh.Get(), nullptr, 0);
    ctx->PSSetShader(g_PsMesh.Get(), nullptr, 0);
    DrawMeshBuffer(mesh);
}

//==============================================================================
// 光のにじみ（加算・深度書き込みなし）
//==============================================================================
void StageRender::BeginGlow()
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    const FLOAT blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetDepthStencilState(g_DsNoWrite.Get(), 0);
    ctx->OMSetBlendState(g_BsAdd.Get(), blendFactor, 0xffffffff);
}

void StageRender::DrawGlow(StageMesh mesh, const XMMATRIX& world, const XMFLOAT3& color, float strength)
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    SetObj(world, color, { strength, 0.0f, 0.0f, 0.0f });
    ctx->VSSetShader(g_VsMesh.Get(), nullptr, 0);
    ctx->PSSetShader(g_PsGlow.Get(), nullptr, 0);
    DrawMeshBuffer(mesh);
}

void StageRender::EndGlow()
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    const FLOAT blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetDepthStencilState(g_DsDefault.Get(), 0);
    ctx->OMSetBlendState(g_BsOpaque.Get(), blendFactor, 0xffffffff);
}

//==============================================================================
// 描画終了
//==============================================================================
void StageRender::End()
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    ctx->RSSetState(g_OldRs);
    ctx->OMSetDepthStencilState(g_OldDs, g_OldStencilRef);
    ctx->OMSetBlendState(g_OldBlend, g_OldBlendFactor, g_OldSampleMask);

    SAFE_RELEASE(g_OldRs);
    SAFE_RELEASE(g_OldDs);
    SAFE_RELEASE(g_OldBlend);
}

//==============================================================================
// 解放
//==============================================================================
void StageRender::Finalize()
{
    g_VsMesh.Reset(); g_VsFull.Reset(); g_VsGrid.Reset();
    g_PsMesh.Reset(); g_PsGlow.Reset(); g_PsSky.Reset(); g_PsGrid.Reset();
    g_Layout.Reset();
    g_CbFrame.Reset(); g_CbObj.Reset();
    g_RsNoCull.Reset();
    g_DsDefault.Reset(); g_DsNoWrite.Reset(); g_DsNone.Reset();
    g_BsOpaque.Reset(); g_BsAdd.Reset();
    for (int i = 0; i < MESH_COUNT; ++i) g_MeshVb[i].Reset();
    g_Init = InitState::NotYet;
}
