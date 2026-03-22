/*==============================================================================

   サンプラーステート管理 [sampler.cpp]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   テクスチャのフィルタリング方式を管理するモジュール。

   ■フィルタリング種別
     Point       : 最近傍補間（ドット絵・ピクセルアート向け）
     Linear      : 線形補間（一般的な 3D テクスチャ向け）
     Anisotropic : 異方性フィルタリング（斜め面のテクスチャをきれいに表示）

   ■使い方
     描画直前に Sampler_SetFilter○○() を呼んでフィルタを切り替える。

==============================================================================*/
#include "Sampler.h"
#include "direct3d.h"

namespace 
{
	ID3D11SamplerState* g_pSamplerSetFilterPoint = nullptr;
	ID3D11SamplerState* g_pSamplerSetFilterLinear = nullptr;
	ID3D11SamplerState* g_pSamplerSetFilterAnisotropic = nullptr;
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
}

void Sampler_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // 点フィルタリング
	//sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;

	//UV参照外のテクスチャのアドレスモードを設定
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.BorderColor[0] = 0.0f;
	sampler_desc.BorderColor[1] = 0.0f;
	sampler_desc.BorderColor[2] = 0.0f;
	sampler_desc.BorderColor[3] = 0.0f;


	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = 16;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.MinLOD = 0;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;


	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerSetFilterPoint);

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerSetFilterLinear);

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerSetFilterAnisotropic);

}


void Sampler_Finalize()
{
	SAFE_RELEASE(g_pSamplerSetFilterPoint);
	SAFE_RELEASE(g_pSamplerSetFilterLinear);
	SAFE_RELEASE(g_pSamplerSetFilterAnisotropic);
}


void Sampler_SetFilterPoint()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerSetFilterPoint);
}


void Sampler_SetFilterLinear()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerSetFilterLinear);
}


void Sampler_SetFilterAnisotropic()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerSetFilterAnisotropic);
}
