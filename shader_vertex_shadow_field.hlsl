/*==============================================================================

   シャドウ用頂点シェーダー [shader_vertex_shadow_field.hlsl]
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
    float4 posL : POSITION0;
    float3 normalL : NORMAL0;
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posW = mul(vi.posL, world);
    vo.posH = mul(posW, lightViewProj);
    return vo;
}
