/*==============================================================================

   アイテム制御 [ItemManager.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/18
--------------------------------------------------------------------------------
==============================================================================*/

#include "ItemManager.h"
#include "Item.h"
#include <cstdlib>
#include <ctime>

static Item s_Items[ITEM_MAX];

//==============================================================================
// Initialize
// マップ上の固定座標にアイテムを配置する
//==============================================================================
void ItemManager_Initialize()
{
    //for (int i = 0; i < ITEM_MAX; ++i)
    //    s_Items[i] = Item{};
    //
    //// マップ固定配置（座標はマップに合わせて調整する）
    //ItemManager_Spawn(ItemType::HP_HEAL, { -3.0f, 1.0f,  3.0f });
    //ItemManager_Spawn(ItemType::ENERGY_HEAL, { 3.0f, 1.0f,  3.0f });
    //ItemManager_Spawn(ItemType::ATK_UP, { -3.0f, 1.0f, -3.0f });
    //ItemManager_Spawn(ItemType::SPEED_UP, { 3.0f, 1.0f, -3.0f });
}


//==============================================================================
// Finalize
//==============================================================================
void ItemManager_Finalize()
{
    for (int i = 0; i < ITEM_MAX; ++i)
        s_Items[i] = Item{};

    Item_UnloadSE();  // SE解放
}

//==============================================================================
// Update
//==============================================================================
void ItemManager_Update()
{
    for (int i = 0; i < ITEM_MAX; ++i)
        s_Items[i].Update();
}

//==============================================================================
// Draw
//==============================================================================
void ItemManager_Draw()
{
    for (int i = 0; i < ITEM_MAX; ++i)
        s_Items[i].Draw();
}

//==============================================================================
// Spawn
// 空きスロットにアイテムを生成する
//==============================================================================
void ItemManager_Spawn(ItemType type, const DirectX::XMFLOAT3& position)
{
    for (int i = 0; i < ITEM_MAX; ++i)
    {
        if (!s_Items[i].IsAlive())
        {
            s_Items[i].Initialize(type, position);
            return;
        }
    }
}

//==============================================================================
// ClearAll
// 全アイテムをリセット（ルーム遷移用、SEは解放しない）
//==============================================================================
void ItemManager_ClearAll()
{
    for (int i = 0; i < ITEM_MAX; ++i)
        s_Items[i] = Item{};
}

//==============================================================================
// SpawnRandom
// 4種類の中からランダムにアイテムを生成する（エネミードロップ用）
//==============================================================================
void ItemManager_SpawnRandom(const DirectX::XMFLOAT3& position)
{
    static bool seeded = false;
    if (!seeded)
    {
        srand(static_cast<unsigned int>(time(nullptr)));
        seeded = true;
    }

    const int typeCount = 4; // ItemType の種類数
    const ItemType type = static_cast<ItemType>(rand() % typeCount);
    ItemManager_Spawn(type, position);
}