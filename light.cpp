/*==============================================================================

   ライト制御 [Light.cpp]
														 Author : 51106
														 Date   : 2026/02/16
--------------------------------------------------------------------------------

   ■このファイルがやること
   ・環境光（アンビエント）の設定
   ・平行光源（ディレクショナルライト）の設定
   ・鏡面反射光（スペキュラライト）の設定
   ・点光源（ポイントライト）の設定
   ・シーン全体のライティングを一括設定
   ・床・天井・壁すべてに同じライト設定を自動反映

==============================================================================*/

#include "direct3d.h"
#include "Light.h"
using namespace DirectX;
#include "shader3d.h"
#include "shader_field.h"
#include "WallShader.h"

struct DirectionalLight
{
	XMFLOAT4 Directional;
	XMFLOAT4 Color;
};

struct SpecularLight
{
	XMFLOAT3 CameraPosition;
	float    SpecularPower;
	XMFLOAT4 SpecularColor;
};

struct PointLight
{
	DirectX::XMFLOAT3 LightPosition;
	float             range;
	DirectX::XMFLOAT4 Color;
};

struct PointLightList
{
	PointLight pointLights[4];
	int        numPointLights;
	DirectX::XMFLOAT3 padding;
};

namespace {
	ID3D11Buffer* g_pPSConstantBuffer1 = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	ID3D11Device* g_pDevice = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer2 = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer3 = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer4 = nullptr;

	PointLightList g_pointLights{};

    // Light_GetAmbient() のために現在値をCPU側でも保持する
    XMFLOAT3 g_currentAmbient = { 1.0f, 1.0f, 1.0f };
}

//==============================================================================
// 初期化
//==============================================================================
void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.CPUAccessFlags = 0;

	buffer_desc.ByteWidth = sizeof(XMFLOAT4);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1);

	buffer_desc.ByteWidth = sizeof(DirectionalLight);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer2);

	buffer_desc.ByteWidth = sizeof(SpecularLight);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer3);

	buffer_desc.ByteWidth = sizeof(PointLightList);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer4);
}

//==============================================================================
// 終了処理
//==============================================================================
void Light_Finalize(void)
{
	SAFE_RELEASE(g_pPSConstantBuffer4)
		SAFE_RELEASE(g_pPSConstantBuffer3)
		SAFE_RELEASE(g_pPSConstantBuffer2)
		SAFE_RELEASE(g_pPSConstantBuffer1)
}

//==============================================================================
// 環境光設定
//
// ■役割
// ・シーン全体の基礎照明を設定する
// ・床・天井シェーダー（Shader3d系）と壁シェーダー（WallShader）の両方に反映
//
// ■引数
// ・color : 環境光の色（RGB）
//==============================================================================
void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
	g_currentAmbient = color;   // CPU側に保存（GetAmbient 用）
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &color, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);

	WallShader_SetAmbient(color);
}

XMFLOAT3 Light_GetAmbient()
{
	return g_currentAmbient;
}

//==============================================================================
// 平行光源設定
//
// ■役割
// ・太陽光のような一定方向から差す光を設定する
// ・床・天井シェーダーと壁シェーダーの両方に反映
//
// ■引数
// ・direction : 光の向き（ベクトル）
// ・color     : 光の色（RGBA）
//==============================================================================
void Light_SetDirectionalWorld(
	const DirectX::XMFLOAT4& direction,
	const DirectX::XMFLOAT4& color)
{
	DirectionalLight light{ direction, color };

	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);

	WallShader_SetDirectional(direction, color);
}

//==============================================================================
// 鏡面反射光設定
//
// ■役割
// ・光沢のある表面に現れるハイライト（テカリ）を設定する
// ・床・天井シェーダーと壁シェーダーの両方に反映
//
// ■引数
// ・camera_position : カメラの位置
// ・specular_power  : ハイライトの鋭さ（数値が大きいほど鋭い）
// ・specular_color  : 鏡面反射の色（RGBA）
//==============================================================================
void Light_SetSpecularWorld(
	const DirectX::XMFLOAT3& camera_position,
	const float& specular_power,
	const DirectX::XMFLOAT4& specular_color)
{
	SpecularLight light{
		camera_position,
		specular_power,
		specular_color,
	};

	g_pContext->UpdateSubresource(g_pPSConstantBuffer3, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(3, 1, &g_pPSConstantBuffer3);

	WallShader_SetSpecular(camera_position, specular_power, specular_color);
}

//==============================================================================
// 点光源数設定
//
// ■役割
// ・使用する点光源の数を指定する（最大4つ）
//
// ■引数
// ・count : 有効な点光源の数
//==============================================================================
void Light_SetPointLightCount(int count)
{
	g_pointLights.numPointLights = count;

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_pointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);
}

//==============================================================================
// 点光源個別設定
//
// ■役割
// ・指定インデックスの点光源の位置・範囲・色を設定する
//
// ■引数
// ・index    : 点光源の番号（0～3）
// ・position : 点光源の位置
// ・range    : 光の届く範囲（半径）
// ・color    : 光の色（RGB）
//==============================================================================
void Light_SetPointLightWorldByCount(
	const int        index,
	const XMFLOAT3& position,
	float            range,
	const XMFLOAT3& color)
{
	if (index >= g_pointLights.numPointLights)
	{
		return;
	}

	g_pointLights.pointLights[index].LightPosition = position;
	g_pointLights.pointLights[index].range = range;
	g_pointLights.pointLights[index].Color = { color.x, color.y, color.z, 1.0f };

	g_pContext->UpdateSubresource(g_pPSConstantBuffer4, 0, nullptr, &g_pointLights, 0, 0);
	g_pContext->PSSetConstantBuffers(4, 1, &g_pPSConstantBuffer4);
}

//==============================================================================
// シーン全体のライト一括設定
//
// ■役割
// ・環境光・平行光源・鏡面反射光を一度に設定する
// ・床・天井シェーダーと壁シェーダーの両方に反映
//
// ■引数
// ・ambient                : 環境光の色（RGB）
// ・directional_direction  : 平行光源の向き
// ・directional_color      : 平行光源の色
// ・camera_position        : カメラ位置
// ・specular_power         : 鏡面反射の鋭さ
// ・specular_color         : 鏡面反射の色
//==============================================================================
void Light_SetScene(
	const DirectX::XMFLOAT3& ambient,
	const DirectX::XMFLOAT4& directional_direction,
	const DirectX::XMFLOAT4& directional_color,
	const DirectX::XMFLOAT3& camera_position,
	float specular_power,
	const DirectX::XMFLOAT4& specular_color)
{
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &ambient, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);

	DirectionalLight dirLight{ directional_direction, directional_color };
	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &dirLight, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);

	SpecularLight specLight{ camera_position, specular_power, specular_color };
	g_pContext->UpdateSubresource(g_pPSConstantBuffer3, 0, nullptr, &specLight, 0, 0);
	g_pContext->PSSetConstantBuffers(3, 1, &g_pPSConstantBuffer3);

	WallShader_SetAmbient(ambient);
	WallShader_SetDirectional(directional_direction, directional_color);
	WallShader_SetSpecular(camera_position, specular_power, specular_color);
}