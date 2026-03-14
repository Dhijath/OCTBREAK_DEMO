/*==============================================================================

   3D�`��p�s�N�Z���V�F�[�_�[ [shader_pixel_3d.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   �E�]���� Ambient + Directional + Specular + PointLight �ɉ�����
     Blob Shadow�i�ۉe�j������
   �EBlob Shadow �� PS register(b6) �Ő���
     �� C++���� BlobShadow::SetToPixelShader() �ɂ�� b6 ���Z�b�g����

==============================================================================*/
//=============================================================================
// 3D �p �s�N�Z���V�F�[�_�[ (shader_pixel_3d.hlsl)
// - b0 : diffuse_color
// - b1 : ambient_color
// - b2 : directional_vector / directional_color
// - b3 : eye_posW / specular_power / specular_color
// - b4 : Point_light[]
// - b6 : Blob Shadow
//=============================================================================

cbuffer PS_CONSTANT_BUFFER_DIFFUSE : register(b0)
{
    float4 diffuse_color;
}

cbuffer PS_CONSTANT_BUFFER_AMBIENT : register(b1)
{
    float4 ambient_color;
}

cbuffer PS_CONSTANT_BUFFER_DIRECTIONAL : register(b2)
{
    float4 directional_vector; // xyz = ���C�g����
    float4 directional_color = { 1, 1, 1, 1 };
}

cbuffer PS_CONSTANT_BUFFER_SPECULAR : register(b3)
{
    float3 eye_posW;
    float specular_power = 30.0f;
    float4 specular_color = { 0.1f, 0.01f, 0.1f, 1.0f };
}

struct PointLight
{
    float3 posW;
    float range;
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER_POINT : register(b4)
{
    PointLight Point_light[4];
    int Point_light_count;
    float3 point_light_dummy;
};

//----------------------------------------------------------
// Blob Shadow�i�ۉe�j�p�萔�iMeshField �Ɠ����j
// PS register(b6)
//----------------------------------------------------------
cbuffer BLOB_SHADOW : register(b6)
{
    float3 blobCenterW; // �e���S�i�v���C���[�ʒu�Ȃǁj
    float blobRadius; // ���a�im�j

    float blobSoftness; // �ڂ������im�j
    float blobStrength; // �Z���i0..1�j
    float2 pad;
}

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1; // VS�ƈ�v
    float3 normalW : TEXCOORD2;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
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
    float3 shadowPad1;
}
cbuffer CB_LIGHT_VP : register(b8)
{
    float4x4 lightViewProj;
}
Texture2D              shadowMap     : register(t7);
SamplerComparisonState shadowSampler : register(s1);

float BlobShadowFactor(float3 posW)
{
    // XZ ���ʋ����Ŋۉe
    float2 d = posW.xz - blobCenterW.xz;
    float dist = length(d);

    // dist <= radius �ŉe�A���E�� softness �Ńt�F�[�h
    float t = saturate((dist - blobRadius) / max(blobSoftness, 0.0001f));
    float inside = 1.0f - t; // 1:���S, 0:�O
    inside = inside * inside; // ���傢���R��

    // �e�W���F���S�قǈÂ��i1-strength�j
    return lerp(1.0f - blobStrength, 1.0f, 1.0f - inside);
}

float4 main(PS_IN pi) : SV_TARGET
{
    // --- �ގ��F�i�e�N�X�`�� �~ ���_�J���[ �~ �f�B�t���[�Y�F�j
    float4 texSample = tex.Sample(samplerState, pi.uv);
    float3 material_color = texSample.rgb * pi.color.rgb * diffuse_color.rgb;

    // --- �@��������
    float3 N = normalize(pi.normalW);
    float3 toEye = normalize(eye_posW - pi.posW);

    // --- ���s���f�B�t���[�Y
    float3 Ld = -normalize(directional_vector.xyz);
    float dl = dot(Ld, N + 1.0f) * 0.5f;
    dl = max(dl, 0.0f);

    float3 diffuse = material_color * directional_color.rgb * dl;

    // --- ����
    float3 ambient = material_color * ambient_color.rgb;

    // --- ���s���X�y�L����
    float3 r = reflect(-Ld, N);
    float t = pow(max(dot(r, toEye), 0.0f), specular_power);
    float3 specular = specular_color.rgb * t;

    // --- �A���t�@
    float alpha = texSample.a * pi.color.a * diffuse_color.a;

    float3 color = ambient + diffuse + specular;

    // --- �_����
    for (int i = 0; i < Point_light_count; i++)
    {
        PointLight pl = Point_light[i];

        float3 lightToPixel = pi.posW - pl.posW;
        float distance = length(lightToPixel);

        float A = max(1.0f - distance / pl.range, 0.0f);
        A = A * A;

        float3 Lp = -normalize(lightToPixel);
        float point_light_dl = max(dot(Lp, N), 0.0f);

        color += material_color * pl.color.rgb * A * point_light_dl;

        float3 point_light_r = reflect(normalize(lightToPixel), N);
        float point_light_t = pow(max(dot(point_light_r, toEye), 0.0f), specular_power);

        color += pl.color.rgb * point_light_t * A;
    }

    // --- �ۉe�������i�󂯂鑤�j
    // �� �e���󂯂����Ȃ����̂�����ꍇ�́AC++���� strength=0 �ɂ��邩
    //  ���̕`��̒��O���� b6 ���O���^�p�ɂ��Ă�������
    float shadow = BlobShadowFactor(pi.posW);
    color *= shadow;

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
            color *= lerp(1.0f - shadowStrength, 1.0f, shadowFactor);
        }
    }

    return float4(color, alpha);
}
