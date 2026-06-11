/*==============================================================================

   スカイボックス用ピクセルシェーダー [shader_pixel_sky.hlsl]

 
   ・ライティング・シャドウ・丸影は一切なし
   ・テクスチャをそのままサンプルして返すだけ

==============================================================================*/

Texture2D    g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

float4 main(PS_IN pi) : SV_TARGET
{
    return g_Texture.Sample(g_Sampler, pi.uv);
}
