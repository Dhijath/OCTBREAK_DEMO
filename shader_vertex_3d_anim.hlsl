/*==============================================================================

   3D描画用頂点シェーダー（アニメーション対応版）[shader_vertex_3d_anim.hlsl]
                                                         Author : 51106
                                                         Date   : 2025/01/27
--------------------------------------------------------------------------------
   ・スケルタルアニメーション対応
   ・ボーン行列によるスキニング計算を追加
   ・最大4つのボーンの影響を受ける頂点変形に対応
   ・PS へワールド座標(posW) と 法線(normalW) を渡す

==============================================================================*/
//=============================================================================
// 3D アニメーション用 頂点シェーダー (shader_vertex_3d_anim.hlsl)
// - b0 : World 行列
// - b1 : View  行列
// - b2 : Proj  行列
// - b3 : Bone  行列配列（最大256）
//=============================================================================

cbuffer VS_CONSTANT_BUFFER_WORLD : register(b0)
{
    float4x4 world;
}

cbuffer VS_CONSTANT_BUFFER_VIEW : register(b1)
{
    float4x4 view;
}

cbuffer VS_CONSTANT_BUFFER_PROJ : register(b2)
{
    float4x4 proj;
}

cbuffer VS_CONSTANT_BUFFER_BONES : register(b3)
{
    float4x4 bones[256]; // ボーン行列配列
}

struct VS_IN
{
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES; // ボーンインデックス（最大4）
    float4 boneWeights : BLENDWEIGHT; // ボーンウェイト（合計1.0）
};

// VS → PS
struct VS_OUT
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD1;
    float3 normalW : TEXCOORD2;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    //=========================================================================
    // スキニング計算（ボーン行列による頂点変形）
    //=========================================================================
    float4 animatedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 animatedNormal = float3(0.0f, 0.0f, 0.0f);

    // 各ボーンの影響を重み付き加算
    for (int i = 0; i < 4; i++)
    {
        uint boneIndex = vi.boneIndices[i];
        float weight = vi.boneWeights[i];

        // 位置をボーン空間に変換して加算
        animatedPos += weight * mul(float4(vi.position, 1.0f), bones[boneIndex]);

        // 法線も同様に変換（w=0で平行移動成分を無視）
        animatedNormal += weight * mul(float4(vi.normal, 0.0f), bones[boneIndex]).xyz;
    }

    //=========================================================================
    // ワールド座標変換（スキニング後の座標を使用）
    //=========================================================================
    float4 posW4 = mul(animatedPos, world);
    vo.posW = posW4.xyz;

    //=========================================================================
    // ビュー・プロジェクション変換
    //=========================================================================
    float4 posV = mul(posW4, view);
    vo.posH = mul(posV, proj);

    //=========================================================================
    // 法線変換（スキニング後の法線を使用）
    //=========================================================================
    float3 nW = mul(float4(animatedNormal, 0.0f), world).xyz;
    vo.normalW = normalize(nW);

    //=========================================================================
    // 色とUVはそのまま渡す
    //=========================================================================
    vo.color = vi.color;
    vo.uv = vi.uv;

    return vo;
}
