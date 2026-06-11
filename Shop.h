/*==============================================================================
   ショップ [Shop.h]
   Author : 51106
   Date   : 2026/06/12
--------------------------------------------------------------------------------
   サバイバルモードのウェーブ間に使える武器購入ショップ。
   スポーン地点に配置し、近づいてEキー/Aボタンで開く。
==============================================================================*/
#pragma once
#include <DirectXMath.h>

void Shop_Initialize(const DirectX::XMFLOAT3& pos);
void Shop_Finalize();
void Shop_Update(double elapsed_time);
void Shop_Draw();       // 3Dオブジェクト（ビルボード）
void Shop_DrawUI();     // 購入UIオーバーレイ

bool Shop_IsOpen();
