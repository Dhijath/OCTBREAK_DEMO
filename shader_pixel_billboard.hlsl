/*==============================================================================

   ビルボード描画用ピクセルシェーダー [shader_pixel_billboard.h]
														 Author : 51106
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/
struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex; //テクスチャ
SamplerState samp; //テクスチャサンプラ

float4 main(PS_IN pi) : SV_TARGET
{
    return tex.Sample(samp, pi.uv) * pi.color;
}
