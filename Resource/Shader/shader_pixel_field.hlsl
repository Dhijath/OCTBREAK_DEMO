/*==============================================================================

   �t�B�[���h�p�s�N�Z���V�F�[�_�[ [shader_pixel_field.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   �E�n�ʃe�N�X�`���� blend(R/G) �ō�����
   �EBlob Shadow�i�ۉe�j���ȈՓI�ɍ���
     - �v���C���[�ʒu�iblobCenterW�j�����XZ�����ŉe�����
     - ShadowMap�{�����̑O�i�Ƃ��āA�����ڊm�F�ɍœK

==============================================================================*/

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1; //  ���[���h���W
    float3 normalW : TEXCOORD2;
    float4 blend : COLOR0; // R/G ���u�����h�W���Ɏg�p
    float2 uv : TEXCOORD0;
};

Texture2D tex0 : register(t0); // ��
Texture2D tex1 : register(t1); // �y
SamplerState samp : register(s0);

//----------------------------------------------------------
// Blob Shadow�i�ۉe�j�p�萔
//  �� C++���� PS b6 �ɃZ�b�g���Ă�������
//----------------------------------------------------------
cbuffer BLOB_SHADOW : register(b6)
{
    float3 blobCenterW; // �v���C���[�ʒu�i���[���h�j
    float blobRadius; // ���a�im�j

    float blobSoftness; // �ڂ������im�j
    float blobStrength; // �Z���i0..1�j
    float2 pad;
};

float BlobShadowFactor(float3 posW)
{
    // XZ ���ʋ����Ŋۉe
    float2 d = posW.xz - blobCenterW.xz;
    float dist = length(d);

    // dist <= radius �ŉe�B���E�� softness �Ńt�F�[�h
    float t = saturate((dist - blobRadius) / max(blobSoftness, 0.0001f));
    float inside = 1.0f - t; // 1:���S, 0:�O
    inside = inside * inside; // �������R�Ɂi�D�݁j

    // �e�W���F���S�قǈÂ��i1-strength�j��
    return lerp(1.0f - blobStrength, 1.0f, 1.0f - inside);
}

//----------------------------------------------------------
// Shadow Map (ShadowMap::BindForMainPass)
//----------------------------------------------------------
cbuffer CB_SHADOW_PARAM : register(b5)
{
    float2 shadowMapSize;
    float  shadowDepthBias;
    float  shadowPad0;
    float  shadowStrength;
    float3 shadowPad1;
};

cbuffer CB_LIGHT_VP : register(b8)
{
    float4x4 lightViewProj;
};

Texture2D              shadowMap     : register(t7);
SamplerComparisonState shadowSampler : register(s1);

float ShadowMapFactor(float3 posW)
{
    if (shadowStrength <= 0.0f) return 1.0f;

    float4 posLight = mul(float4(posW, 1.0f), lightViewProj);
    float3 ndc = posLight.xyz / posLight.w;

    float2 shadowUV = ndc.xy * float2(0.5f, -0.5f) + 0.5f;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f ||
        ndc.z < 0.0f || ndc.z > 1.0f)
        return 1.0f;

    float cmpDepth = ndc.z - shadowDepthBias;
    return shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, cmpDepth);
}

float4 main(PS_IN pi) : SV_TARGET
{
    float4 c0 = tex0.Sample(samp, pi.uv); // ��
    float4 c1 = tex1.Sample(samp, pi.uv); // �y

    // blend�iR/G�j�ō����i���̃��W�b�N�ێ��j
    float r = pi.blend.r;
    float g = pi.blend.g;

    float4 tex_color = c0 * g + c1 * r;

    // Blob shadow (丸影)
    float shadow = BlobShadowFactor(pi.posW);
    tex_color.rgb *= shadow;

    // Shadow map (directional shadow)
    float smFactor = ShadowMapFactor(pi.posW);
    tex_color.rgb *= lerp(1.0f - shadowStrength, 1.0f, smFactor);

    return tex_color;
}
