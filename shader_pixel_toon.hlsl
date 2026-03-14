/*==============================================================================

   トゥーン用ピクセルシェーダー [shader_pixel_toon.hlsl]
                                                         Author : 51106
                                                         Date   : 2026/03/11
--------------------------------------------------------------------------------
   ・shader_pixel_3d.hlsl をベースにトゥーン（セルシェーディング）化
   ・定数バッファ構成は shader_pixel_3d.hlsl と完全に同一
     → ShaderToon は Shader3d と同じ CB バインドで動作する
   ・トゥーン制御パラメータのみ b7 に追加

==============================================================================*/
//=============================================================================
// トゥーン用 ピクセルシェーダー (shader_pixel_toon.hlsl)
// - b0 : diffuse_color
// - b1 : ambient_color
// - b2 : directional_vector / directional_color
// - b3 : eye_posW / specular_power / specular_color
// - b4 : Point_light[]
// - b6 : Blob Shadow
// - b7 : Toon パラメータ（新規）
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
    float4 directional_vector;
    float4 directional_color;
}

cbuffer PS_CONSTANT_BUFFER_SPECULAR : register(b3)
{
    float3 eye_posW;
    float  specular_power;
    float4 specular_color;
}

struct PointLight
{
    float3 posW;
    float  range;
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER_POINT : register(b4)
{
    PointLight Point_light[4];
    int        Point_light_count;
    float3     point_light_dummy;
}

//----------------------------------------------------------
// Blob Shadow（丸影）  ※ shader_pixel_3d.hlsl と同一
//----------------------------------------------------------
cbuffer BLOB_SHADOW : register(b6)
{
    float3 blobCenterW;
    float  blobRadius;
    float  blobSoftness;
    float  blobStrength;
    float2 pad;
}

//----------------------------------------------------------
// トゥーンパラメータ（新規）
//----------------------------------------------------------
cbuffer TOON_PARAM : register(b7)
{
    int   toonSteps;        // 階調数（2〜4 推奨）
    float toonSpecularSize; // ハイライト閾値（0.0〜1.0）
    float toonRimPower;     // リムライト強度（0.0 で無効）
    float toonRimThreshold; // リムライト閾値
}

struct PS_IN
{
    float4 posH    : SV_POSITION;
    float3 posW    : TEXCOORD1;
    float3 normalW : TEXCOORD2;
    float4 color   : COLOR0;
    float2 uv      : TEXCOORD0;
};

Texture2D    tex          : register(t0);
SamplerState samplerState : register(s0);

//----------------------------------------------------------
// シャドウマップ（ShadowMap::BindForMainPass でバインド）
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

//----------------------------------------------------------
// 丸影（shader_pixel_3d.hlsl と同一）
//----------------------------------------------------------
float BlobShadowFactor(float3 posW)
{
    float2 d    = posW.xz - blobCenterW.xz;
    float  dist = length(d);
    float  t    = saturate((dist - blobRadius) / max(blobSoftness, 0.0001f));
    float  inside = 1.0f - t;
    inside = inside * inside;
    return lerp(1.0f - blobStrength, 1.0f, 1.0f - inside);
}

//----------------------------------------------------------
// トゥーン階調化：NdotL を steps 段に丸める
//----------------------------------------------------------
float ToonStep(float value, int steps)
{
    // 0〜1 を steps 段に量子化
    return floor(saturate(value) * float(steps)) / float(steps - 1);
}

float4 main(PS_IN pi) : SV_TARGET
{
    // --- 材質色
    float4 texSample     = tex.Sample(samplerState, pi.uv);
    float3 material_color = texSample.rgb * pi.color.rgb * diffuse_color.rgb;

    // --- 法線・視線
    float3 N      = normalize(pi.normalW);
    float3 toEye  = normalize(eye_posW - pi.posW);
    float3 Ld     = -normalize(directional_vector.xyz);

    // --- トゥーンディフューズ（階調化）
    float  NdotL     = dot(Ld, N) * 0.5f + 0.5f; // half-Lambert
    float  toonDiff  = ToonStep(NdotL, toonSteps);
    float3 diffuse   = material_color * directional_color.rgb * toonDiff;

    // --- 環境光（そのまま）
    float3 ambient = material_color * ambient_color.rgb;

    // --- トゥーンスペキュラ（パキッとしたハイライト）
    float3 r          = reflect(-Ld, N);
    float  specRaw    = pow(max(dot(r, toEye), 0.0f), specular_power);
    float  specToon   = specRaw > toonSpecularSize ? 1.0f : 0.0f;
    float3 specular   = specular_color.rgb * specToon;

    // --- リムライト（輪郭を明るくする効果）
    float  rim        = 1.0f - saturate(dot(toEye, N));
    rim               = pow(rim, toonRimPower);
    float  rimFactor  = rim > toonRimThreshold ? rim : 0.0f;
    float3 rimLight   = material_color * rimFactor;

    float3 color = ambient + diffuse + specular + rimLight;

    // --- 点光源（トゥーン化）
    for (int i = 0; i < Point_light_count; i++)
    {
        PointLight pl = Point_light[i];

        float3 lightToPixel    = pi.posW - pl.posW;
        float  distance        = length(lightToPixel);
        float  A               = max(1.0f - distance / pl.range, 0.0f);
        A = A * A;

        float3 Lp              = -normalize(lightToPixel);
        float  point_NdotL     = dot(Lp, N) * 0.5f + 0.5f;
        float  point_toon      = ToonStep(point_NdotL, toonSteps);

        color += material_color * pl.color.rgb * A * point_toon;

        // 点光源スペキュラもトゥーン化
        float3 point_r         = reflect(normalize(lightToPixel), N);
        float  point_specRaw   = pow(max(dot(point_r, toEye), 0.0f), specular_power);
        float  point_specToon  = point_specRaw > toonSpecularSize ? 1.0f : 0.0f;
        color += pl.color.rgb * point_specToon * A;
    }

    // --- 丸影（shader_pixel_3d.hlsl と同一）
    float shadow = BlobShadowFactor(pi.posW);
    color *= shadow;

    // --- シャドウマップ（ShadowMap::BindForMainPass でバインドされたとき有効）
    if (shadowStrength > 0.0f)
    {
        // ワールド座標からライト空間のクリップ座標を計算
        float4 posLight = mul(float4(pi.posW, 1.0f), lightViewProj);
        float3 ndc = posLight.xyz / posLight.w;

        // NDC → UV（Y反転、Z は [0,1] 範囲チェック）
        float2 shadowUV = ndc.xy * float2(0.5f, -0.5f) + 0.5f;

        // UV が有効範囲内かつ深度が有効な場合のみサンプル
        if (shadowUV.x >= 0.0f && shadowUV.x <= 1.0f &&
            shadowUV.y >= 0.0f && shadowUV.y <= 1.0f &&
            ndc.z >= 0.0f && ndc.z <= 1.0f)
        {
            float cmpDepth   = ndc.z - shadowDepthBias;
            float shadowFactor = shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, cmpDepth);
            // shadowFactor: 1=日当たり / 0=影　→ 影の濃さ(shadowStrength)で補間
            float dimFactor = lerp(1.0f - shadowStrength, 1.0f, shadowFactor);
            color *= dimFactor;
        }
    }

    float alpha = texSample.a * pi.color.a * diffuse_color.a;
    return float4(color, alpha);
}
