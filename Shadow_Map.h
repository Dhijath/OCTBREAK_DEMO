/*==============================================================================

   シャドウマップ管理 [shadow_map.h]
                                                         Author : 51106
                                                         Date   : 2025/12/17
--------------------------------------------------------------------------------

   ・Directional Light 前提の ShadowMap（Depth のみ）
   ・RenderPass で ShadowMap を生成し、通常描画で SRV として参照する
   ・enable / cast / receive を後で個別に切れる設計

==============================================================================*/
#pragma once
#include <DirectXMath.h>

struct ID3D11ShaderResourceView;
struct MODEL; // 前方宣言（DrawModel の引数用）

namespace ShadowMap
{
    // 初期化／終了
    bool Initialize(int size = 2048);
    void Finalize();

    // 影のON/OFF（全体）
    void SetEnabled(bool enable);
    bool IsEnabled();

    // 影生成パス開始／終了
    // lightDirW : 「ライトが向く方向」(例: directional_vector.xyz)
    // focusPosW : 影中心（例: Player位置）
    void BeginPass(const DirectX::XMFLOAT3& lightDirW,
        const DirectX::XMFLOAT3& focusPosW,
        float area = 40.0f,
        float nearZ = 1.0f,
        float farZ = 120.0f);
    void EndPass();

    // 影参照（通常描画パス）に必要なものをバインド
    // VS b3 : LightViewProj（影生成 VS 用）
    // PS b8 : LightViewProj（PS で shadowUV 計算用）
    // PS t7 : ShadowMap SRV, PS s1 : ComparisonSampler, PS b5 : ShadowParam
    // pcfEnable = true  → 3x3 PCF（高品質・ソフト）
    // pcfEnable = false → 1サンプル（中品質・ハード）
    void BindForMainPass(bool pcfEnable = true);

    // シャドウパス中にモデルを深度のみ描画する
    // BeginPass() ～ EndPass() の間で呼ぶこと
    void DrawModel(struct MODEL* model, const DirectX::XMMATRIX& world);

    // 「いま影生成中か？」（描画側で分岐したい時に使う）
    bool IsRenderingShadow();

    // SRV（デバッグ表示や Sprite_DrawSRV で貼りたい時）
    ID3D11ShaderResourceView* GetShadowSRV();
}
