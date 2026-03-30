/*==============================================================================

   壁描画用頂点シェーダー [shader_vertex_wall.hlsl]
                                                         Author : 51106
                                                         Date   : 2026/02/17
--------------------------------------------------------------------------------

   ・WallPlaneRenderer の Vtx 構造体に対応
   ・入力：POSITION / NORMAL / COLOR / TEXCOORD
   ・World / View / Proj 行列でクリップ座標へ変換

==============================================================================*/
cbuffer VS_CONSTANT_BUFFER_WORLD : register(b0)
{
    float4x4 world;
}
cbuffer VS_CONSTANT_BUFFER_VIEW : register(b1)
{
    float4x4 view;
}
cbuffer VS_CONSTANT_BUFFER_PROJ : register(b2)
{
    float4x4 proj;
}

struct VS_IN
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD4;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4 posW4 = mul(float4(vi.position, 1.0f), world);
    vo.posW = posW4.xyz;

    float4 posV = mul(posW4, view);
    vo.posH = mul(posV, proj);

    vo.color = vi.color;
    vo.uv = vi.uv;
    vo.normal = vi.normal;

    return vo;
}
