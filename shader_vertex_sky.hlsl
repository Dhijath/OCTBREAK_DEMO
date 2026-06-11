/*==============================================================================

   スカイボックス用頂点シェーダー [shader_vertex_sky.hlsl]

  
   ・ビュー行列の平行移動成分をゼロにして球をカメラに追従させる
   ・posH.z = posH.w にすることで深度が常に最大値（1.0）になり
     他のオブジェクトより必ず後ろに描画される

==============================================================================*/

cbuffer VS_CONSTANT_BUFFER_WORLD : register(b0) { float4x4 world; }
cbuffer VS_CONSTANT_BUFFER_VIEW  : register(b1) { float4x4 view; }
cbuffer VS_CONSTANT_BUFFER_PROJ  : register(b2) { float4x4 proj; }

struct VS_IN
{
    float3 position : POSITION0;
    float3 normal   : NORMAL0;    // 未使用（レイアウト共通化のため残す）
    float4 color    : COLOR0;     // 未使用（同上）
    float2 uv       : TEXCOORD0;
};

struct VS_OUT
{
    float4 posH : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4 posW = mul(float4(vi.position, 1.0f), world);
    float4 posV = mul(posW, view);
    float4 posH = mul(posV, proj);

    // 深度を最大値に固定（すべてのオブジェクトより奥に描画される）
    posH.z = posH.w;

    vo.posH = posH;
    vo.uv   = vi.uv;

    return vo;
}
