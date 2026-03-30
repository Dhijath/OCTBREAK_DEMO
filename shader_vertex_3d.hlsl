/*==============================================================================

   3D描画用頂点シェーダー [shader_vertex_3d.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   ・Shader3d 用 VS
   ・PS へワールド座標(posW) と 法線(normalW) を渡す
   ・丸影（Blob Shadow）用の posW は必須（PSで参照）

==============================================================================*/
//=============================================================================
// 3D 用 頂点シェーダー (shader_vertex_3d.hlsl)
// - b0 : World 行列
// - b1 : View  行列
// - b2 : Proj  行列
//=============================================================================

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

// VS → PS
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1; //  POSITION0 だと衝突するのでTEXCOORDに移動
    float3 normalW : TEXCOORD2;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    // local -> world
    float4 posW4 = mul(float4(vi.position, 1.0f), world);
    vo.posW = posW4.xyz;

    // world -> view -> proj
    float4 posV = mul(posW4, view);
    vo.posH = mul(posV, proj);

    // normal (w=0)
    float3 nW = mul(float4(vi.normal, 0.0f), world).xyz;
    vo.normalW = normalize(nW);

    vo.color = vi.color;
    vo.uv = vi.uv;

    return vo;
}
