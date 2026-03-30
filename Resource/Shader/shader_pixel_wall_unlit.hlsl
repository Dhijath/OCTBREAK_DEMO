/*==============================================================================

   壁描画用ピクセルシェーダー（Unlit）[shader_pixel_wall_unlit.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/31
--------------------------------------------------------------------------------

   ・壁専用 Unlit PS
   ・UV ループ確認用
   ・影・ライトは未使用（完全確認（必要）

==============================================================================*/

cbuffer PS_CONSTANT_BUFFER_DIFFUSE : register(b0)
{
    float4 diffuse_color;
}

cbuffer PS_CONSTANT_BUFFER_UV_REPEAT : register(b1)
{
    float2 uvRepeat;
    float2 padding;
}

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

float4 main(PS_IN pi) : SV_TARGET
{
    float2 uv = pi.uv * uvRepeat;
    float4 t = tex.Sample(samplerState, uv);

    float3 color = t.rgb * pi.color.rgb * diffuse_color.rgb;
    float alpha = t.a * pi.color.a * diffuse_color.a;

    return float4(color, alpha);
}
