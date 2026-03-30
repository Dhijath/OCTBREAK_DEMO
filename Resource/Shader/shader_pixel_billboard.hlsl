/*==============================================================================

   Billboard描画用ピクセルシェーダ [shader_pixel_billboard.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/11/14
--------------------------------------------------------------------------------

==============================================================================*/

struct PS_IN
{
    float4 posH : SV_POSITION; // クリップ座標
    float4 color : COLOR0; // 頂点カラー（加算用）
    float2 uv : TEXCOORD0; // UV座標
};

Texture2D tex; // ビルボードに貼るテクスチャ
SamplerState samplerState; // サンプラーステート

// ピクセルシェーダ本体
float4 main(PS_IN ps_in) : SV_TARGET
{
    // テクスチャをサンプリングして、頂点カラーで色を乗算
    return tex.Sample(samplerState, ps_in.uv) * ps_in.color;
}
