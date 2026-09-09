/*==============================================================================

   エネミーのパーツモデル共有 [EnemyParts.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03

==============================================================================*/
#include "EnemyParts.h"
#include "model.h"
#include <map>
#include <string>
#include <utility>

namespace
{
    // (パス, 倍率) → モデル
    std::map<std::pair<std::string, float>, MODEL*> g_Models;
}

MODEL* EnemyParts_Get(const char* path, float scale)
{
    const auto key = std::make_pair(std::string(path), scale);
    const auto it = g_Models.find(key);
    if (it != g_Models.end()) return it->second;

    MODEL* model = ModelLoad(path, scale);
    g_Models[key] = model;   // 読み込み失敗（nullptr）も記録して、毎回読み直さない
    return model;
}

void EnemyParts_Release()
{
    for (auto& m : g_Models)
        if (m.second) ModelRelease(m.second);
    g_Models.clear();
}
