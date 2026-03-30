/*==============================================================================

   シャドウ用頂点シェーダー [shader_vertex_shadow_3d.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

==============================================================================*/
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
}
cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float4x4 lightViewProj;
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
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = mul(float4(vi.position, 1.0f), world);
    vo.posH = mul(posW, lightViewProj);
    return vo;
}
