/*==============================================================================

   ItemManager [ItemManager.h]
                                                         Author : 51106
                                                         Date   : 2026/02/18
--------------------------------------------------------------------------------
==============================================================================*/

#ifndef ITEMMANAGER_H
#define ITEMMANAGER_H

#include "Item.h"
#include <DirectXMath.h>

static const int ITEM_MAX = 64; // アイテムの最大同時存在数

void ItemManager_Initialize();
void ItemManager_Finalize();
void ItemManager_Update();
void ItemManager_Draw();

// 指定座標にアイテムをスポーンする
// type     : アイテム種類
// position : スポーン座標
void ItemManager_Spawn(ItemType type, const DirectX::XMFLOAT3& position);

// ランダムな種類のアイテムをスポーンする（エネミードロップ用）
// position : スポーン座標
void ItemManager_SpawnRandom(const DirectX::XMFLOAT3& position);

// 全アイテムを消去（ルーム遷移用、SEは解放しない）
void ItemManager_ClearAll();

#endif // ITEMMANAGER_H