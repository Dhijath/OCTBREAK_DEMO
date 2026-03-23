/*==============================================================================

   ライティング制御 [Light.h]
														 Author : 51106
														 Date   : 2025/11/12
--------------------------------------------------------------------------------

   ・環境光（アンビエントライト）
   ・並行光（ディレクショナルライト）
   ・鏡面反射光（スペキュラライト）
   ・点光源（ポイントライト）

   各ライトの情報をGPUへ送るための定数バッファを管理

==============================================================================*/

#pragma once
#ifndef LIGHT_H
#define LIGHT_H

#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

//====================================
// 初期化／終了処理
//====================================

//------------------------------------
// Light_Initialize
// ライト関連の初期化処理
// ・各種定数バッファの生成
//------------------------------------
void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

//------------------------------------
// Light_Finalize
// ライト関連の終了処理
// ・定数バッファの解放
//------------------------------------
void Light_Finalize(void);

//====================================
// ライト設定系
//====================================

//------------------------------------
// Light_SetAmbient
// アンビエント（環境光）の設定
// color : 環境光のRGB値
//------------------------------------
void Light_SetAmbient(const DirectX::XMFLOAT3& color);
XMFLOAT3 Light_GetAmbient();   // 現在のアンビエント値を取得

//------------------------------------
// Light_SetDirectionalWorld
// 並行光（ディレクショナルライト）の設定
// direction : 光の向き
// color     : 光の色（RGBA）
//------------------------------------
void Light_SetDirectionalWorld(
	const DirectX::XMFLOAT4& direction,
	const DirectX::XMFLOAT4& color
);

//------------------------------------
// Light_SetSpecularWorld
// 鏡面反射光（スペキュラライト）の設定
// camera_position : カメラ位置
// specular_power  : ハイライトの鋭さ（指数）
// specular_color  : 鏡面反射光の色
//------------------------------------
void Light_SetSpecularWorld(
	const DirectX::XMFLOAT3& camera_position,
	const float& specular_power,
	const DirectX::XMFLOAT4& specular_color
);

//------------------------------------
// Light_SetPointLightCount
// 使用する点光源の数を設定
// count : 有効な点光源の数
//------------------------------------


void Light_SetPointLightCount(int count);

//------------------------------------
// Light_SetPointLightWorldByCount
// 点光源の位置・範囲・色を個別に設定
// index    : 設定対象の点光源インデックス
// position : 光源の位置
// range    : 光の届く範囲
// color    : 光の色（RGB）
//------------------------------------


void Light_SetPointLightWorldByCount(
	int index,
	const XMFLOAT3& position,
	float range,
	const XMFLOAT3& color);

void Light_SetScene(
	const DirectX::XMFLOAT3& ambient,
	const DirectX::XMFLOAT4& directional_direction,
	const DirectX::XMFLOAT4& directional_color,
	const DirectX::XMFLOAT3& camera_position,
	float specular_power,
	const DirectX::XMFLOAT4& specular_color
);

#endif // LIGHT_H
