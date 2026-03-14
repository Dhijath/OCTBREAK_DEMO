/*==============================================================================

   カメラ向きリボントレイル [Trail.h]
                                                         Author : 51106
                                                         Date   : 2026/03/20
--------------------------------------------------------------------------------

   ■概要
     エミッター位置の履歴をリボン状の頂点列で描画するトレイルクラス。
     複数インスタンス間で VS/PS/ブレンドステート等のGPUリソースを共有する。

   ■使い方
     Trail m_Trail;
     // 初期化
     m_Trail.Initialize(48, 0.12f, 0.35f, {1.0f, 0.6f, 0.1f, 1.0f});
     // 毎フレーム（Update で位置を追加）
     m_Trail.Update(dt, emitterWorldPos);
     // 描画パスで（不透明物の後、加算ブレンドで描画）
     m_Trail.Draw();
     // 終了時
     m_Trail.Finalize();

   ■パラメータ
     maxPoints : 保持するポイント数（長さ）
     width     : リボン幅（m）
     maxAge    : ポイントの寿命（秒）―これ以上古いものは消える
     color     : ベースカラー RGBA（A=先端の最大アルファ）

==============================================================================*/
#pragma once
#ifndef TRAIL_H
#define TRAIL_H

#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>

class Trail
{
public:
    //==========================================================================
    // 初期化
    //==========================================================================
    void Initialize(int           maxPoints,
                    float         width,
                    float         maxAge,
                    DirectX::XMFLOAT4 color);

    //==========================================================================
    // 終了処理（デストラクタからも自動呼び出し）
    //==========================================================================
    ~Trail() { Finalize(); }
    void Finalize();

    //==========================================================================
    // 毎フレーム更新
    // ・headPos : エミッターのワールド座標（弾の先端、プレイヤー位置など）
    // ・dt      : 経過時間（秒）
    //==========================================================================
    void Update(double dt, const DirectX::XMFLOAT3& headPos);

    //==========================================================================
    // 描画
    // ・加算ブレンド / 深度テスト ON / 深度書き込み OFF
    // ・不透明オブジェクト描画後に呼ぶこと
    //==========================================================================
    void Draw();

    //==========================================================================
    // 全ポイントをクリア（瞬間テレポート時など）
    //==========================================================================
    void Clear();

    bool IsEmpty() const { return m_Points.empty(); }

private:
    //----------------------------------------------------------------------
    // CPU 側ポイント
    //----------------------------------------------------------------------
    struct TrailPoint
    {
        DirectX::XMFLOAT3 pos;
        float             age; // 経過時間（秒）
    };

    std::vector<TrailPoint> m_Points;
    int                     m_MaxPoints = 32;
    float                   m_Width     = 0.1f;
    float                   m_MaxAge    = 0.5f;
    DirectX::XMFLOAT4       m_Color     = { 1, 1, 1, 1 };

    //----------------------------------------------------------------------
    // GPU 側（インスタンスごとの動的 VB）
    //----------------------------------------------------------------------
    ID3D11Buffer* m_pVB       = nullptr;
    int           m_VBCapacity = 0;

    void EnsureVB(int vertexCount); // 容量不足なら再生成

    //----------------------------------------------------------------------
    // 共有 GPU リソース（全インスタンスで 1 セット）
    //----------------------------------------------------------------------
    static ID3D11VertexShader*      s_pVS;
    static ID3D11PixelShader*       s_pPS;
    static ID3D11InputLayout*       s_pIL;
    static ID3D11BlendState*        s_pBlendAdd;    // 加算ブレンド
    static ID3D11DepthStencilState* s_pDSSNoWrite;  // 深度テストON/書き込みOFF
    static ID3D11RasterizerState*   s_pRSNoCull;    // カリングなし（リボン両面描画）
    static ID3D11Buffer*            s_pCBView;      // 独自 View 行列 CB (b1)
    static ID3D11Buffer*            s_pCBProj;      // 独自 Proj 行列 CB (b2)
    static int                      s_RefCount;

    static void SharedInit();
    static void SharedFinalize();
};

#endif // TRAIL_H
