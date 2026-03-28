/*==============================================================================

   �Ǖ`��p�s�N�Z���V�F�[�_�iLit�j [shader_pixel_wall_lit.hlsl]
                                                         Author : 51106
                                                         Date   : 2026/02/17
--------------------------------------------------------------------------------

   �E�ǐ�p Lit PS�i���C�e�B���O�Ή��Łj
   �EAmbient / Directional / Specular�iBlinn-Phong�j�Ή�
   �EWallLightData �\���̂� cbuffer �̃��C�A�E�g����v������

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

cbuffer PS_CONSTANT_BUFFER_LIGHT : register(b2)
{
    float3 ambient;
    float padding1;
    float4 directional_direction;
    float4 directional_color;
    float3 camera_position;
    float specular_power;
    float4 specular_color;
}

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float3 normal : TEXCOORD4;
};

Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

//----------------------------------------------------------
// Shadow map (ShadowMap::BindForMainPass)
//----------------------------------------------------------
cbuffer CB_SHADOW_PARAM : register(b5)
{
    float2 shadowMapSize;
    float  shadowDepthBias;
    float  shadowPad0;
    float  shadowStrength;
    float  shadowPCF;      // 1.0 = 3x3 PCF（高） / 0.0 = 1サンプル（中）
    float2 shadowPad1;
}
cbuffer CB_LIGHT_VP : register(b8)
{
    float4x4 lightViewProj;
}
Texture2D              shadowMap     : register(t7);
SamplerComparisonState shadowSampler : register(s1);

// 3x3 PCF：9点サンプル平均でジャギを低減
float ShadowPCF(float2 shadowUV, float cmpDepth)
{
    float2 texelSize = 1.0f / shadowMapSize;
    float shadow = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            shadow += shadowMap.SampleCmpLevelZero(
                shadowSampler,
                shadowUV + float2((float)x, (float)y) * texelSize,
                cmpDepth);
        }
    }
    return shadow / 9.0f;
}

float4 main(PS_IN pi) : SV_TARGET
{
    float2 uv = pi.uv * uvRepeat;
    float4 texColor = tex.Sample(samplerState, uv);

    float3 N = normalize(pi.normal);

    float3 lightDir = normalize(-directional_direction.xyz);

    // Lambert�f�B�t���[�Y
    float NdotL = max(0.0f, dot(N, lightDir));
    float3 diffuse = directional_color.rgb * NdotL;

    // Blinn-Phong�X�y�L����
    float3 viewDir = normalize(camera_position - pi.posW);
    float3 halfVec = normalize(lightDir + viewDir);
    float NdotH = max(0.0f, dot(N, halfVec));
    float3 specular = specular_color.rgb * pow(NdotH, specular_power);

    // �ŏI�J���[����
    float3 lighting = ambient + diffuse + specular;
    float3 finalColor = texColor.rgb * pi.color.rgb * diffuse_color.rgb * lighting;
    float alpha = texColor.a * pi.color.a * diffuse_color.a;

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
            float cmpDepth = ndc.z - shadowDepthBias;
            float shadowFactor = (shadowPCF > 0.5f)
                ? ShadowPCF(shadowUV, cmpDepth)
                : shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, cmpDepth);
            finalColor *= lerp(1.0f - shadowStrength, 1.0f, shadowFactor);
        }
    }

    return float4(finalColor, alpha);
}