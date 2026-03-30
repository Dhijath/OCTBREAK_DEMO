/*==============================================================================

   3D描画用ピクセルシェーダー（Unlit）[shader_pixel_unlit.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/03
--------------------------------------------------------------------------------

==============================================================================*/

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
}

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D tex;
SamplerState samplerState;

float4 main(PS_IN ps_in) : SV_TARGET
{
    return tex.Sample(samplerState, ps_in.uv) * diffuse_color;
}

