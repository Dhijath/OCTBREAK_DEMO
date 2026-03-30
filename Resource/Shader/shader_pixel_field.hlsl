/*==============================================================================

   フィールド用ピクセルシェーダー [shader_pixel_field.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   ・地面テクスチャを blend(R/G) で合成する
   ・Blob Shadow（丸影）を簡易的に適用
     - プレイヤー位置（blobCenterW）からのXZ距離で影をつける
     - ShadowMap本実装の前（として、テスト的確認に最適

==============================================================================*/

struct PS_IN
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1; // ワールド座標
    float3 normalW : TEXCOORD2;
    float4 blend : COLOR0; // R/G をブレンド重みに使用
    float2 uv : TEXCOORD0;
};

Texture2D tex0 : register(t0); // 草
Texture2D tex1 : register(t1); // 土
SamplerState samp : register(s0);

//----------------------------------------------------------
// Blob Shadow（丸影）用定数
//  ← C++側で PS b6 にセットしてください
//----------------------------------------------------------
cbuffer BLOB_SHADOW : register(b6)
{
    float3 blobCenterW; // プレイヤー位置（ワールド）
    float blobRadius; // 半径（m）

    float blobSoftness; // ぼかし幅（m）
    float blobStrength; // 強さ（0..1）
    float2 pad;
};

float BlobShadowFactor(float3 posW)
{
    // XZ 平面距離で丸影
    float2 d = posW.xz - blobCenterW.xz;
    float dist = length(d);

    // dist <= radius で影。周辺は softness でフェード
    float t = saturate((dist - blobRadius) / max(blobSoftness, 0.0001f));
    float inside = 1.0f - t; // 1:中心, 0:外
    inside = inside * inside; // 滑らかに減衰（好み）

    // 影の色：中心ほど暗い（1-strength）に
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
    float4 c0 = tex0.Sample(samp, pi.uv); // 草
    float4 c1 = tex1.Sample(samp, pi.uv); // 土

    // blend（R/G）で合成（既存のロジック維持）
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
