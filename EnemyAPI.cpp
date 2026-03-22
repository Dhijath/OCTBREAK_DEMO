/*==============================================================================

   エネミー API [EnemyAPI.cpp]
                                                         Author : 51106
                                                         Date   : 2026/01/13
--------------------------------------------------------------------------------
   Enemy クラスの生成・管理を C 関数として公開するラッパーモジュール。
   EnemyManager から呼ばれ、Enemy インスタンスのリストを内部で保持する。

   ■機能
     - Enemy_Initialize : エネミーリストをクリア
     - Enemy_Spawn      : 指定位置にエネミーを 1 体生成しインデックスを返す
     - Enemy_Update     : 全エネミーの AI・移動・衝突を更新
     - Enemy_Draw       : 全エネミーを描画

==============================================================================*/
#include "EnemyAPI.h"
#include "Enemy.h"

#include <vector>

namespace
{
    std::vector<Enemy> g_Enemies;
}

void Enemy_Initialize(const DirectX::XMFLOAT3&)
{
    g_Enemies.clear();

}

int Enemy_Spawn(const DirectX::XMFLOAT3& position)
{
    Enemy e;
    e.Initialize(position);
    g_Enemies.push_back(e);
    return static_cast<int>(g_Enemies.size() - 1);
}

void Enemy_Update(double elapsed_time)
{
    for (Enemy& e : g_Enemies)
        e.Update(elapsed_time);
}

void Enemy_Draw()
{
    for (Enemy& e : g_Enemies)
        e.Draw();
}

void Enemy_Finalize()
{
    for (Enemy& e : g_Enemies)
        e.Finalize();
    g_Enemies.clear();
}

int Enemy_GetCount()
{
    return static_cast<int>(g_Enemies.size());
}

AABB Enemy_GetAABBAt(int index)
{
    return g_Enemies[index].GetAABB();
}

const DirectX::XMFLOAT3& Enemy_GetPositionAt(int index)
{
    return g_Enemies[index].GetPosition();
}
