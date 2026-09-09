/*==============================================================================

   マップ巡回AI [MapPatrolAI.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
   ■最適化（2026/10）
   ・巡回点を 4m 四方の格子に振り分けておき、近距離の目的地探しは
     周囲の格子だけを調べる（以前は毎回すべての巡回点を走査していた）
   ・目的地は「同じ階」の巡回点からだけ選ぶ（上下に分かれるステージ対応）
   ・視線判定は双方の実際の高さ（足元＋0.5m）で行う
     （平面のステージでは従来の固定高さ 1.5m と同じ結果になる）
==============================================================================*/

#include "MapPatrolAI.h"
#include "map.h"
#include <random>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace
{
    //==========================================================================
    // 床座標リスト（マップ生成時に設定）
    //==========================================================================
    std::vector<DirectX::XMFLOAT3> g_FloorPositions;

    //==========================================================================
    // 近傍検索用の格子（キー → 巡回点の添字）
    //==========================================================================
    constexpr float CELL = 4.0f;
    std::unordered_map<long long, std::vector<int>> g_Grid;

    long long CellKey(int cx, int cz)
    {
        return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned int>(cz);
    }

    int CellOf(float v) { return static_cast<int>(std::floor(v / CELL)); }

    //==========================================================================
    // 乱数生成器（目的地選択用）
    //==========================================================================
    std::mt19937 g_Rng;

    // 視線の高さ（足元からのオフセット）
    constexpr float EYE_HEIGHT = 0.5f;

    // 同じ階とみなす高さの差
    constexpr float SAME_LEVEL = 1.2f;

    bool SameLevel(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        return std::fabs(a.y - b.y) < SAME_LEVEL;
    }

    // from から to へ壁越しでなく見通せるか（双方の足元＋EYE_HEIGHT の高さで判定）
    bool ClearLine(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to)
    {
        const DirectX::XMFLOAT3 a = { from.x, from.y + EYE_HEIGHT, from.z };
        const DirectX::XMFLOAT3 b = { to.x,   to.y   + EYE_HEIGHT, to.z };
        DirectX::XMFLOAT3 hitPos{};
        return !Map_RaycastWalls(a, b, &hitPos, /*wallsOnly=*/true);
    }
}

//==============================================================================
// 床座標リスト初期化
//==============================================================================
void MapPatrolAI_Initialize(const std::vector<DirectX::XMFLOAT3>& floorPositions)
{
    g_FloorPositions = floorPositions;

    g_Grid.clear();
    for (int i = 0; i < static_cast<int>(g_FloorPositions.size()); ++i)
    {
        const DirectX::XMFLOAT3& p = g_FloorPositions[i];
        g_Grid[CellKey(CellOf(p.x), CellOf(p.z))].push_back(i);
    }

    // 乱数生成器をシード初期化（時刻ベース）
    g_Rng.seed(static_cast<unsigned int>(time(nullptr)));
}

//==============================================================================
// ランダムな巡回目的地を取得
//==============================================================================
DirectX::XMFLOAT3 MapPatrolAI_GetRandomDestination()
{
    if (g_FloorPositions.empty())
    {
        // 床がない場合はデフォルト位置
        return { 0.0f, 1.0f, 0.0f };
    }

    // ランダムにインデックスを選択
    std::uniform_int_distribution<size_t> dist(0, g_FloorPositions.size() - 1);
    size_t randomIndex = dist(g_Rng);

    return g_FloorPositions[randomIndex];
}

//==============================================================================
// 床座標リストのクリア
//==============================================================================
void MapPatrolAI_Clear()
{
    g_FloorPositions.clear();
    g_Grid.clear();
}

//==============================================================================
// 視線遮蔽判定
//
// ■役割
// ・from → to にレイを飛ばし、壁AABBに当たるか確認する
// ・双方の足元から 0.5m の高さで判定する（屋上・上の階にいる相手は建物や床板で遮られる）
//==============================================================================
bool MapPatrolAI_HasLineOfSight(
    const DirectX::XMFLOAT3& from,
    const DirectX::XMFLOAT3& to)
{
    return ClearLine(from, to);
}

//==============================================================================
// 到達可能な巡回目的地を取得
//
// ■役割
// ・同じ階の巡回点からランダムに選び、from から直線で行けるか確認する
// ・壁に遮られた座標は引き直す（maxTry 回まで）
// ・見つからなければその場にとどまる（別の階の点へ向かって壁に張り付かないように）
//==============================================================================
DirectX::XMFLOAT3 MapPatrolAI_GetReachableDestination(
    const DirectX::XMFLOAT3& from,
    int maxTry)
{
    if (g_FloorPositions.empty())
    {
        return { 0.0f, 1.0f, 0.0f };
    }

    std::uniform_int_distribution<size_t> dist(0, g_FloorPositions.size() - 1);

    // 別の階の点を引いた分は試行回数に数えない（ただし無限ループはしない）
    int attempts = 0;
    for (int tried = 0; tried < maxTry && attempts < maxTry * 6; ++attempts)
    {
        const DirectX::XMFLOAT3& candidate = g_FloorPositions[dist(g_Rng)];
        if (!SameLevel(from, candidate)) continue;
        ++tried;

        if (ClearLine(from, candidate))
            return candidate;
    }

    // 見つからなければ近場の同じ階の点（それも無ければ現在地）
    return MapPatrolAI_GetNearbyDestination(from, 6.0f, 4);
}

//==============================================================================
// 近距離の巡回目的地を取得
//
// ■役割
// ・現在位置から searchRadius 以内・同じ階の巡回点を、周囲の格子だけから集める
// ・候補の中から視線が通る座標を返す
//==============================================================================
DirectX::XMFLOAT3 MapPatrolAI_GetNearbyDestination(
    const DirectX::XMFLOAT3& from,
    float searchRadius,
    int maxTry)
{
    if (g_FloorPositions.empty())
    {
        return { 0.0f, 1.0f, 0.0f };
    }

    // 半径以内の候補を、周囲の格子から収集する
    std::vector<int> candidates;
    candidates.reserve(32);

    const float radiusSq = searchRadius * searchRadius;
    const int cx0 = CellOf(from.x - searchRadius), cx1 = CellOf(from.x + searchRadius);
    const int cz0 = CellOf(from.z - searchRadius), cz1 = CellOf(from.z + searchRadius);

    for (int cz = cz0; cz <= cz1; ++cz)
    {
        for (int cx = cx0; cx <= cx1; ++cx)
        {
            auto it = g_Grid.find(CellKey(cx, cz));
            if (it == g_Grid.end()) continue;

            for (int idx : it->second)
            {
                const DirectX::XMFLOAT3& pos = g_FloorPositions[idx];
                const float dx = pos.x - from.x;
                const float dz = pos.z - from.z;
                if (dx * dx + dz * dz > radiusSq) continue;
                if (!SameLevel(from, pos)) continue;
                candidates.push_back(idx);
            }
        }
    }

    // 候補がなければ現在地にとどまる
    if (candidates.empty())
    {
        return from;
    }

    // 候補をシャッフルして視線が通るものを返す
    std::shuffle(candidates.begin(), candidates.end(), g_Rng);

    const int tryCount = std::min(maxTry, static_cast<int>(candidates.size()));
    for (int i = 0; i < tryCount; ++i)
    {
        const DirectX::XMFLOAT3& candidate = g_FloorPositions[candidates[i]];
        if (ClearLine(from, candidate))
            return candidate;
    }

    // 視線が通るものが無ければ、いちばん近い候補
    return g_FloorPositions[candidates[0]];
}
