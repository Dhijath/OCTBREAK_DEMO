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
#include "BlockStage.h"
#include <vector>
#include <random>
#include <DirectXMath.h>
using namespace DirectX;

void Map_GenerateOutdoor(std::uint32_t seed)
{
    constexpr int   W   = 49;   // アリーナ縮小（旧61）で敵密度アップ
    constexpr int   H   = 49;
    constexpr float OX  = 0.5f;
    constexpr float OZ  = 0.5f;

    const float FLOOR_Y = Map_Internal_GetFloorY();
    const float WALL_H  = Map_Internal_GetWallH();

    BlockStage_Deactivate();   // タイル式マップに戻すので、描画の委譲を解除する

    // 屋外アリーナは天井なし（ダンジョンに戻るときは Game_Initialize が true に戻す）
    Map_SetCeilingVisible(false);

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
    // 遮蔽物散布：矩形ブロック（柱・バリケード）をランダム配置
    // ・プレイヤースポーン（ショップ）とボススポーン周辺は空ける
    // ・他の壁と2タイル以上の間隔を保証（袋小路・狭路を作らない）
    //------------------------------------------------------------------
    const int spawnTX = W / 2, spawnTY = 4;
    const int bossTX  = W / 2, bossTY  = H - 5;

    // ブロックの矩形からタイル(px,py)までのチェビシェフ距離
    auto rectDist = [](int bx, int by, int bw, int bh, int px, int py)
    {
        const int dx = (px < bx) ? bx - px : (px > bx + bw - 1) ? px - (bx + bw - 1) : 0;
        const int dy = (py < by) ? by - py : (py > by + bh - 1) ? py - (by + bh - 1) : 0;
        return (dx > dy) ? dx : dy;
    };

    {
        // 遮蔽物サイズのバリエーション（柱〜横長バリケード）
        constexpr int kBlockSizes[][2] = {
            {1,1}, {2,2}, {3,3}, {2,3}, {3,2}, {1,4}, {4,1}, {1,3}, {3,1},
        };
        constexpr int kBlockSizeCount = sizeof(kBlockSizes) / sizeof(kBlockSizes[0]);
        constexpr int kBlockTarget    = 30;   // 配置数
        constexpr int kSpawnClear     = 7;    // スポーン周辺の禁止半径（タイル）
        constexpr int kBossClear      = 5;    // ボススポーン周辺の禁止半径

        std::uniform_int_distribution<int> posDist (3, W - 4);
        std::uniform_int_distribution<int> sizeDist(0, kBlockSizeCount - 1);

        int placed = 0;
        for (int attempt = 0; attempt < 600 && placed < kBlockTarget; ++attempt)
        {
            const int* sz = kBlockSizes[sizeDist(rng)];
            const int bw = sz[0], bh = sz[1];
            const int bx = posDist(rng);
            const int by = posDist(rng);
            if (bx + bw > W - 3 || by + bh > H - 3) continue;

            // スポーン保護領域
            if (rectDist(bx, by, bw, bh, spawnTX, spawnTY) < kSpawnClear) continue;
            if (rectDist(bx, by, bw, bh, bossTX,  bossTY)  < kBossClear)  continue;

            // 周囲2タイルを含め全て床なら配置可（他ブロック・外周との間隔確保）
            bool ok = true;
            for (int y = by - 2; y <= by + bh + 1 && ok; ++y)
                for (int x = bx - 2; x <= bx + bw + 1 && ok; ++x)
                    if (tiles[y*W + x] != TILE_FLOOR) ok = false;
            if (!ok) continue;

            for (int y = by; y < by + bh; ++y)
                for (int x = bx; x < bx + bw; ++x)
                    tiles[y*W + x] = TILE_WALL;
            ++placed;
        }
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
    Map_Internal_SetSpawnPos    (Map_Internal_TileToWorld(spawnTX, spawnTY, W, H, 1.5f));
    Map_Internal_SetBossSpawnPos(Map_Internal_TileToWorld(bossTX,  bossTY,  W, H, 1.5f));
    Map_Internal_SetGoalInvalid();

    //------------------------------------------------------------------
    // エネミースポーン散布
    // ・遮蔽物の上は不可、プレイヤースポーンから8タイル以上離す
    //------------------------------------------------------------------
    Map_Internal_ClearEnemySpawns();
    {
        constexpr int kEnemySpawnTarget = 30;  // 湧き点を増やして3倍の敵を散らす（旧20）
        constexpr int kEnemySpawnClear  = 8;   // プレイヤースポーンからの最低距離

        std::uniform_int_distribution<int> dist(5, W - 6);
        int placed = 0;
        for (int attempt = 0; attempt < 400 && placed < kEnemySpawnTarget; ++attempt)
        {
            const int tx = dist(rng), ty = dist(rng);
            if (tiles[ty*W+tx] != TILE_FLOOR) continue;
            if (rectDist(tx, ty, 1, 1, spawnTX, spawnTY) < kEnemySpawnClear) continue;
            Map_Internal_AddEnemySpawn(Map_Internal_TileToWorld(tx, ty, W, H, 1.5f));
            ++placed;
        }
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
