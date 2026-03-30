/*==============================================================================

   2D描画用頂点シェーダー [shader_vertex_2d.hlsl]
                                                         Author : 51106
                                                         Date   : 2026/02/15
--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 proj;

};

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 world;

};

struct VS_IN
{
    float4 posL  : POSITION0;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH  : SV_POSITION;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

//=============================================================================
// 頂点シェーダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4x4 mtx = mul(world, proj);
    vo.posH  = mul(vi.posL, mtx); // 座標変換

    vo.color = vi.color;
    vo.uv    = vi.uv;

	return vo;
}
