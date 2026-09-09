/*==============================================================================

   ブロックステージ描画 [BlockStageRender.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ブロックステージ専用の描画器。テクスチャを一切使わず、
   ・手続き生成の空（グラデーション＋星雲＋星）
   ・発光グリッドの地面 / 天井
   ・単色メッシュ（半球環境光＋リムライト＋距離フォグ＋自己発光）
   ・発光体のまわりの光のにじみ（加算の外殻）
   を描く。

   ■既存の描画との共存
     ・シェーダーは起動時に D3DCompile でコンパイルする（.cso を増やさない）
     ・定数バッファは b10 / b11 を使い、既存シェーダーのスロットに触れない
     ・既存のシャドウマップ（t7 / s1 / b5 / b8）と丸影（b6）はそのまま参照する
       → プレイヤーや敵の影がステージにも落ちる
     ・Begin で退避したラスタライザ / 深度 / ブレンド状態を End で元に戻す

==============================================================================*/
#pragma once
#include <DirectXMath.h>

// メッシュの種類。どれも原点中心・一辺1の箱に収まる大きさで作ってある
enum class StageMesh
{
    Cube,
    Octa,       // 正八面体
    Pyramid,    // 四角錐（頂点が上）
    Cylinder,   // 16角柱
    Hex,        // 六角柱
    Frustum,    // 上がすぼまった四角柱（上面は底面の0.6倍）
    Sphere,
    Torus,      // 輪（Y軸まわり）
    Rock,       // いびつな岩
    Count
};

// 1フレームぶんの環境設定
struct StageFrame
{
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT3   camPos;
    float               time;

    DirectX::XMFLOAT3 skyTop;      // 空の上の色
    DirectX::XMFLOAT3 skyBottom;   // 空の地平線側の色
    DirectX::XMFLOAT4 nebula;      // 星雲の色（a = 濃さ）
    DirectX::XMFLOAT3 fog;         // フォグの色
    float             fogStart;
    float             fogEnd;
    DirectX::XMFLOAT3 grid;        // グリッド線の色
    float             gridCell;    // グリッドの1マス（m）
    DirectX::XMFLOAT3 ground;      // 地面の地の色
};

namespace StageRender
{
    // 描画開始。初期化（シェーダーのコンパイル）に失敗している場合は false を返す
    bool Begin(const StageFrame& frame);

    void DrawSky();                                   // 画面全体に空を描く（深度は書かない）
    void DrawGrid(float y, bool receiveShadow);       // 高さ y の水平なグリッド面（天井は false）

    // 不透明メッシュ。emissive は自己発光の強さ（0 = 発光なし）
    void DrawMesh(StageMesh mesh, const DirectX::XMMATRIX& world,
                  const DirectX::XMFLOAT3& color, float emissive);

    // 光のにじみ（加算）。BeginGlow 〜 EndGlow の間に DrawGlow を呼ぶ
    void BeginGlow();
    void DrawGlow(StageMesh mesh, const DirectX::XMMATRIX& world,
                  const DirectX::XMFLOAT3& color, float strength);
    void EndGlow();

    // 描画終了。退避しておいた描画状態を元に戻す
    void End();

    // リソース解放（アプリ終了時）
    void Finalize();
}
