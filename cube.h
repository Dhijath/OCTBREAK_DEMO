/*==============================================================================

   キューブ描画 [cube.h]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   1×1×1 の単位キューブ描画モジュールのパブリック API。
   迷路の壁・床タイルの描画ベースとして使用する。

   ■AABB 生成
     Cube_CreateAABB          : 中心位置から 1×1×1 の AABB を生成
     Cube_CreateAABBFromWorld : ワールド行列の 8 頂点を変換して AABB を包括生成

==============================================================================*/
#pragma once
#ifndef CUBE_H
#define CUBE_H

#include <d3d11.h>
#include <DirectXMath.h>
#include "collision.h"

// キューブ描画用のリソース初期化
void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

// キューブ描画用リソース解放
void Cube_Finalize(void);

// 指定テクスチャ＆ワールド行列でキューブを1つ描画
void Cube_Draw(int texID, const DirectX::XMMATRIX mtxW);

// 中心 position を持つ 1x1x1 の AABB を生成（ローカル基準）
AABB Cube_CreateAABB(const DirectX::XMFLOAT3& position);

//==============================================================================
// ワールド行列から「回転込み」のAABBを生成
// ・Cube(ローカル -0.5～+0.5) の8頂点を world で変換してAABB化
// ・床レジストリ登録に使う
//==============================================================================
AABB Cube_CreateAABBFromWorld(const DirectX::XMMATRIX& world);

void Cube_DrawWall(
    int texID,
    const DirectX::XMMATRIX& mtxW,
    const DirectX::XMFLOAT2& uvRepeat);


#endif


