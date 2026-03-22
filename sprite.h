/*==============================================================================

   2Dスプライト描画 [Sprite.h]
														 Author : 51106
														 Date   : 2025/11/12
--------------------------------------------------------------------------------

   ・2Dスプライトの初期化/終了、描画開始(Sprite_Begin)を提供
   ・テクスチャ全表示／部分表示（UVカット）／サイズ変更／回転に対応
   ・color は乗算カラー（RGBA）

   ※座標系メモ
	 sx, sy : 画面の左上を原点(0,0)とするスクリーン座標（px）
	 sw, sh : 画面上での表示サイズ（px）
	 pixx, pixy, pixw, pixh : テクスチャ内の切り出し矩形（px）
	 angle : 回転角（ラジアン想定／実装依存で中心回転か基準点回転）

==============================================================================*/

#pragma once
#ifndef SPRITE_H
#define SPRITE_H

#include <d3d11.h>
#include <DirectXMath.h>

// スプライト座標系の仮想解像度（常に1600×900で描画→画面全体に引き伸ばし）
static constexpr int SPRITE_SCREEN_W = 1600;
static constexpr int SPRITE_SCREEN_H = 900;

//====================================
// 初期化／終了
//====================================
void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);

//====================================
// パイプライン開始
//====================================
void Sprite_Begin();//長方形

void Sprite_BeginSquare();//正方形


//====================================
// 描画API
//====================================

// テクスチャ全表示
void Sprite_Draw(
	int texid, float sx, float sy,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f }
);

// テクスチャ全表示（サイズ変更）
void Sprite_Draw(
	int texid, float sx, float sy, float sw, float sh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f }
);

// UVカット（部分表示）
void Sprite_Draw(
	int texid, float sx, float sy,
	int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f }
);

// UVカット（サイズ変更）
void Sprite_Draw(
	int texid, float sx, float sy, float sw, float sh,
	int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f }
);

// UVカット（サイズ変更、回転）
void Sprite_Draw(
	int texid, float sx, float sy, float sw, float sh,
	int pixx, int pixy, int pixw, int pixh, float angle,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f }
);

void Sprite_DrawSRV(
	ID3D11ShaderResourceView* srv,
	float sx, float sy, float sw, float sh,
	const DirectX::XMFLOAT4& color);

void Sprite_DrawSRV_UV(
	ID3D11ShaderResourceView* srv,
	float sx, float sy, float sw, float sh,
	float u0, float v0, float u1, float v1,  // UV座標
	const DirectX::XMFLOAT4& color);


#endif // SPRITE_H
