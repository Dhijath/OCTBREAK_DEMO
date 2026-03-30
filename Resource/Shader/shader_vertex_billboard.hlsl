/*==============================================================================

   Billboard描画用頂点シェーダ [shader_vertex_billboard.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/11/14
--------------------------------------------------------------------------------

==============================================================================*/

// 定数バッファ（World 行列）
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;
}

// 定数バッファ（View 行列）
cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
}

// 定数バッファ（Projection 行列）
cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
}

// UV変換用パラメータ（スケールとオフセット）
cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float2 scale;
    float2 translation;
}

// ※ 色用の定数バッファを追加
cbuffer VS_CONSTANT_BUFFER : register(b4)
{
    float4 materialColor; // ※ Shader_Billboard_SetColor() から渡される色
}

// 頂点入力
struct VS_IN
{
    float4 posL : POSITION0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// 頂点出力
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);

    vo.posH = mul(vi.posL, mtxWVP);

    // ※ 頂点カラーとマテリアルカラーを乗算
    vo.color = vi.color * materialColor;

    vo.uv = vi.uv * scale + translation;

    return vo;
}
