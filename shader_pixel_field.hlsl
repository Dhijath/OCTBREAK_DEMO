/*==============================================================================

   フィールド用ピクセルシェーダー [shader_pixel_field.hlsl]
														 Author : 51106
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

/*==============================================================================
    フィールド用ピクセルシェーダー
    Ambient + Directional
==============================================================================*/

//=========================
// Light.cpp と一致させる cbuffer
//=========================
cbuffer DiffuseBuffer : register(b0)
{
    float4 diffuse_color;
};

cbuffer AmbientBuffer : register(b1)
{
    float4 ambient_color;
};

cbuffer DirectionalBuffer : register(b2)
{
    float4 directional_vector;
    float4 directional_color;
};

//=========================
// 入力
//=========================
struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : POSITION0;
    float3 normalW : NORMAL0;
    float4 blend : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex_grass : register(t0);
Texture2D tex_dirt : register(t1);

SamplerState samp : register(s0);

//----------------------------------------------------------
// Shadow map (ShadowMap::BindForMainPass)
//----------------------------------------------------------
cbuffer CB_SHADOW_PARAM : register(b5)
{
    float2 shadowMapSize;
    float  shadowDepthBias;
    float  shadowPad0;
    float  shadowStrength;
    float3 shadowPad1;
}
cbuffer CB_LIGHT_VP : register(b8)
{
    float4x4 lightViewProj;
}
Texture2D              shadowMap     : register(t7);
SamplerComparisonState shadowSampler : register(s1);


//=========================
// Main
//=========================
float4 main(PS_IN pi) : SV_TARGET
{
    float4 col_grass = tex_grass.Sample(samp, pi.uv);
    float4 col_dirt = tex_dirt.Sample(samp, pi.uv);

    float4 tex_color = col_grass * pi.blend.g +
                       col_dirt * pi.blend.r;

    float3 base = tex_color.rgb * diffuse_color.rgb;

    float3 N = normalize(pi.normalW);
    float3 L = normalize(-directional_vector.xyz);

    float diff = max(dot(N, L), 0.0f);

    float3 diffuse = base * directional_color.rgb * diff;
    float3 ambient = base * ambient_color.rgb;

    float3 result = diffuse + ambient;

    // --- Shadow map
    if (shadowStrength > 0.0f)
    {
        float4 posLight = mul(float4(pi.posW, 1.0f), lightViewProj);
        float3 ndc = posLight.xyz / posLight.w;
        float2 shadowUV = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
        if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
            shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
            ndc.z >= 0.0f && ndc.z <= 1.0f)
        {
            float cmpDepth   = ndc.z - shadowDepthBias;
            float shadowFactor = shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, cmpDepth);
            result *= lerp(1.0f - shadowStrength, 1.0f, shadowFactor);
        }
    }

    return float4(result, tex_color.a);
}

