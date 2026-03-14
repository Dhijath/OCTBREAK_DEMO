/*==============================================================================

   トレイル用頂点シェーダー [shader_vertex_trail.hlsl]
                                                         Author : 51106
                                                         Date   : 2026/03/20
--------------------------------------------------------------------------------

   ・world 変換なし（Trail.cpp 側でワールド座標を直接渡す）
   ・b0 : View + Proj を結合した CB_VP

==============================================================================*/

// 既存パイプラインが b1/b2 にグローバルバインドしている View/Proj をそのまま使う
cbuffer CB_VIEW : register(b1) { float4x4 g_View; }
cbuffer CB_PROJ : register(b2) { float4x4 g_Proj; }

struct VS_IN
{
    float4 pos   : POSITION; // ワールド座標（w=1）
    float4 color : COLOR0;   // RGBA（A=フェード済みアルファ）
};

struct VS_OUT
{
    float4 posH  : SV_POSITION;
    float4 color : COLOR0;
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    float4 posV = mul(vi.pos, g_View);
    vo.posH     = mul(posV,   g_Proj);
    vo.color    = vi.color;
    return vo;
}
