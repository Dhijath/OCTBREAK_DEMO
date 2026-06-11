/*==============================================================================
   屋外マップ生成 [map_outdoor.cpp]
   Author : 51106
   Date   : 2026/06/12
--------------------------------------------------------------------------------
   天井なし・広いフラット空間の屋外アリーナ。
   map.cpp の内部API（Map_Internal_*）経由でデータを設定する。
==============================================================================*/
#include "map.h"
#include "cube.h"
#include "MapPatrolAI.h"
#include <vector>
#include <random>
#include <DirectXMath.h>
using namespace DirectX;

void Map_GenerateOutdoor(std::uint32_t seed)
{
    constexpr int   W   = 61;
    constexpr int   H   = 61;
    constexpr float OX  = 0.5f;
    constexpr float OZ  = 0.5f;

    const float FLOOR_Y = Map_Internal_GetFloorY();
    const float WALL_H  = Map_Internal_GetWallH();

    constexpr int TILE_FLOOR = 0;
    constexpr int TILE_WALL  = 1;
    constexpr int TILE_EMPTY = 2;

    std::mt19937 rng(seed);

    //------------------------------------------------------------------
    // タイル初期化：全床、外周壁
    //------------------------------------------------------------------
    std::vector<int> tiles(W * H, TILE_FLOOR);
    for (int x = 0; x < W; ++x)
    {
        tiles[0 * W + x]     = TILE_WALL;
        tiles[(H-1)*W + x]   = TILE_WALL;
    }
    for (int y = 0; y < H; ++y)
    {
        tiles[y*W + 0]       = TILE_WALL;
        tiles[y*W + (W-1)]   = TILE_WALL;
    }

    //------------------------------------------------------------------
    // 正規化
    //------------------------------------------------------------------
    {
        std::vector<int> out(W * H, TILE_EMPTY);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (tiles[y*W+x] == TILE_FLOOR) out[y*W+x] = TILE_FLOOR;

        for (int y = 1; y < H-1; ++y)
            for (int x = 1; x < W-1; ++x)
            {
                if (tiles[y*W+x] != TILE_FLOOR) continue;
                int nb[4] = { (y-1)*W+x, (y+1)*W+x, y*W+(x-1), y*W+(x+1) };
                for (int n : nb) if (out[n] != TILE_FLOOR) out[n] = TILE_WALL;
            }

        for (int x = 0; x < W; ++x) { out[0*W+x] = TILE_WALL; out[(H-1)*W+x] = TILE_WALL; }
        for (int y = 0; y < H; ++y) { out[y*W+0]  = TILE_WALL; out[y*W+(W-1)] = TILE_WALL; }
        tiles.swap(out);
    }

    //------------------------------------------------------------------
    // スポーン・ゴール設定
    //------------------------------------------------------------------
    Map_Internal_SetSpawnPos    (Map_Internal_TileToWorld(W/2, 4,   W, H, 1.5f));
    Map_Internal_SetBossSpawnPos(Map_Internal_TileToWorld(W/2, H-5, W, H, 1.5f));
    Map_Internal_SetGoalInvalid();

    //------------------------------------------------------------------
    // エネミースポーン散布
    //------------------------------------------------------------------
    Map_Internal_ClearEnemySpawns();
    std::uniform_int_distribution<int> dist(5, W - 6);
    for (int i = 0; i < 20; ++i)
    {
        int tx = dist(rng), ty = dist(rng);
        if (tiles[ty*W+tx] != TILE_FLOOR) continue;
        Map_Internal_AddEnemySpawn(Map_Internal_TileToWorld(tx, ty, W, H, 1.5f));
    }

    //------------------------------------------------------------------
    // MapObjects（床のみ・天井なし）
    //------------------------------------------------------------------
    Map_Internal_ClearObjects();
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
        {
            if (tiles[ty*W+tx] != TILE_FLOOR) continue;
            const XMFLOAT3 p = Map_Internal_TileToWorld(tx, ty, W, H, FLOOR_Y);
            Map_Internal_AddObject(Map_Internal_KindFloor(), p, Cube_CreateAABB(p));
        }

    //------------------------------------------------------------------
    // 壁 Plane
    //------------------------------------------------------------------
    Map_Internal_BuildWalls(tiles, W, H, OX, OZ, FLOOR_Y, WALL_H);

    //------------------------------------------------------------------
    // パトロール AI
    //------------------------------------------------------------------
    {
        std::vector<XMFLOAT3> patrol;
        for (int ty = 1; ty < H-1; ++ty)
            for (int tx = 1; tx < W-1; ++tx)
            {
                if (tiles[ty*W+tx] != TILE_FLOOR) continue;
                bool ok = true;
                int nb8[8] = {
                    (ty-1)*W+(tx-1),(ty-1)*W+tx,(ty-1)*W+(tx+1),
                     ty   *W+(tx-1),             ty   *W+(tx+1),
                    (ty+1)*W+(tx-1),(ty+1)*W+tx,(ty+1)*W+(tx+1)
                };
                for (int n : nb8) if (tiles[n] != TILE_FLOOR) { ok=false; break; }
                if (!ok) continue;
                patrol.push_back(Map_Internal_TileToWorld(tx, ty, W, H, 1.0f));
            }
        MapPatrolAI_Initialize(patrol);
    }

    //------------------------------------------------------------------
    // ミニマップ用タイル
    //------------------------------------------------------------------
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
        {
            if (tiles[ty*W+tx] == TILE_EMPTY) continue;
            const int kind = (tiles[ty*W+tx] == TILE_FLOOR)
                             ? Map_Internal_KindMinimapFloor()
                             : Map_Internal_KindMinimapWall();
            const XMFLOAT3 p = Map_Internal_TileToWorld(tx, ty, W, H, 10.0f);
            Map_Internal_AddObject(kind, p, Cube_CreateAABB(p));
        }
}
