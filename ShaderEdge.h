/*==============================================================================

   エッジ検出シェーダー管理 [ShaderEdge.h]
                                                         Author : 51106
                                                         Date   : 2026/03/11
--------------------------------------------------------------------------------
   - 深度 + 法線 によるエッジ検出（ポストエフェクト）
   - トゥーンモデルの輪郭線をバックフェース法より高品質に描画する

   【描画フロー】
     1. ShaderEdge_BeginNormalPass()
        → 法線をオフスクリーンバッファに書き出す
        → ModelDraw() でトゥーン対象モデルを描画
     2. ShaderEdge_EndNormalPass()
        → バックバッファに戻す
     3. 通常シーン描画（Shader3d など）
     4. ShaderEdge_DrawEdge()
        → 法線テクスチャ + 深度テクスチャからエッジを検出して重ねる

==============================================================================*/
#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

//==============================================================================
// 初期化 / 終了
//==============================================================================
bool ShaderEdge_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void ShaderEdge_Finalize();

// バックバッファリサイズ時にノーマル/深度テクスチャを再生成する
void ShaderEdge_ResizeBuffers();

//==============================================================================
// パラメータ設定
//==============================================================================

/// <summary>
/// エッジ検出パラメータを設定する
/// </summary>
/// <param name="depthThreshold">深度差の閾値（例: 0.001f）</param>
/// <param name="normalThreshold">法線差の閾値（例: 0.3f）</param>
/// <param name="color">エッジの色（通常は黒）</param>
void ShaderEdge_SetParam(
    float depthThreshold,
    float normalThreshold,
    const DirectX::XMFLOAT4& color
);

//==============================================================================
// 行列設定（Player_Camera.cpp の ApplyViewProjToShaders から呼ぶ）
//==============================================================================
void ShaderEdge_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void ShaderEdge_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void ShaderEdge_SetProjectMatrix(const DirectX::XMMATRIX& matrix);

//==============================================================================
// 描画
//==============================================================================

/// <summary>
/// 法線書き出しパス開始
/// オフスクリーンバッファに切り替えて法線を書き出す準備をする
/// この後 ModelDraw() でトゥーン対象モデルを描画する
/// </summary>
void ShaderEdge_BeginNormalPass();

/// <summary>
/// 法線書き出しパス終了
/// バックバッファに戻す
/// </summary>
void ShaderEdge_EndNormalPass();

/// <summary>
/// エッジ検出を実行してバックバッファに重ねる
/// 通常シーン描画後に呼ぶ
/// </summary>
void ShaderEdge_DrawEdge();