/*==============================================================================

   フィールド用頂点シェーダー [shader_vertex_field.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   ・地面（MeshField）用の頂点シェーダ
   ・ワールド座標(posW) と 法線(normalW) を PS へ渡す
   ・blend(R/G) と UV はそのまま渡す

==============================================================================*/

cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
}

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
}

cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
}

struct VS_IN
{
    float4 posL : POSITION0; // ローカル座標
    float3 normalL : NORMAL0; // ローカル法線
    float4 blend : COLOR0; // ブレンド用（R=土, G=草など）
    float2 uv : TEXCOORD0; // UV
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // クリップ座標
    float3 posW : TEXCOORD1; // ワールド座標（丸影用）
    float3 normalW : TEXCOORD2; // ワールド法線（後のライティング用）
    float4 blend : COLOR0; // ブレンド値
    float2 uv : TEXCOORD0; // UV
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4 posW4 = mul(vi.posL, world);
    vo.posW = posW4.xyz;

    float4 posV = mul(posW4, view);
    vo.posH = mul(posV, proj);

    float3 nW = mul(float4(vi.normalL, 0.0f), world).xyz;
    vo.normalW = normalize(nW);

    vo.blend = vi.blend;
    vo.uv = vi.uv;

    return vo;
}
