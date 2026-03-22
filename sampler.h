/*==============================================================================

   サンプラーステート管理 [sampler.h]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   テクスチャのフィルタリング方式（Point / Linear / Anisotropic）を
   切り替えるモジュールのパブリック API。

==============================================================================*/
#ifndef SAMPLER_H
#define SAMPLER_H

#include <d3d11.h>

void Sampler_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sampler_Finalize();
void Sampler_SetFilterPoint();

void Sampler_SetFilterLinear();

void Sampler_SetFilterAnisotropic();

#endif // !SAMPLER_H

