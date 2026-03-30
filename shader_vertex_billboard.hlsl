/*==============================================================================

   ビルボード描画用頂点シェーダー [shader_vertex_billboard.hlsl]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;

};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;

};

cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;

};

cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float4 ambient_color;

};

cbuffer VS_CONSTANT_BUFFER : register(b4)
{
    float4 directional_world_vector;
    float4 directional_color;
    float3 eye_PosW;
    // float specular_power;
};

struct VS_IN
{
    float4 posL : POSITION;
    float4 color : COLOR0;
    float2 tex : TEXCOORD0;

};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 tex : TEXCOORD0;
};

//=============================================================================
// 頂点シェーダー
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;


// 簡易変換
    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    vo.posH = mul(vi.posL, mtxWVP);


    vo.color = vi.color;
    vo.tex = vi.tex;

    return vo;
}
