/*==============================================================================

   マップ制御 [map.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/16
--------------------------------------------------------------------------------

   ■このファイルがやること
   ・タイル（床/壁）ベースで「部屋＋通路」のダンジョンを自動生成する
   ・床/天井のみ MapObject（AABB）を作る（衝突・床判定用）
   ・壁の見た目は TileWall + WallPlaneRenderer に委譲する（Plane方式）

   ■壁の方針（重要）
   ・壁テクスチャを揃えるため、Cube ではなく Plane を使用
   ・map.cpp は壁の連結/向き推測を一切しない（TileWall側の責務）
   ■追記
   ・床と天井をメッシュフィールドにすることで頂点数の削減、大幅に軽量化
   ・現在はエネミーの出現位置やタイルのシェーダーをライティング対応できるよう
==============================================================================*/

#include "map.h"
#include "collision.h"
#include "cube.h"
#include "texture.h"
#include <DirectXMath.h>
#include "Light.h"
#include "Meshfield.h"
#include "Player_Camera.h"
#include "shader3d.h"
#include "FloorRegistry.h"
#include "blob_shadow.h"
#include "direct3d.h"
#include "player.h"
#include "billboard.h"

#include "TileWall.h"
#include "WallPlaneRenderer.h"
#include "MapPatrolAI.h"

#include <vector>
#include <random>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <chrono>
#include "WallShader.h"

using namespace DirectX;

namespace
{
    //==========================================================================
    // MapObject.KindId
    //==========================================================================
    enum : int
    {
        KIND_GROUND = 0, // 未使用（見た目は MeshField）
        KIND_FLOOR = 1, // 床（FloorRegistryに登録する）
        KIND_WALL = 2, // 旧：Cube壁（現在は生成しない）
        KIND_TREE = 3, // 未使用オブジェクト用に作成したもの
        KIND_CEILING = 4, // 天井（床の上だけ生成）
        KIND_GOAL = 5, // ゴール（当たり判定＋ビルボード表示）
        KIND_MINIMAP_FLOOR = 101,  // ミニマップ用床
        KIND_MINIMAP_WALL = 102,   // ミニマップ用壁
        KIND_MINIMAP_GOAL = 103,   // ミニマップ用ゴール

    };

    //==========================================================================
    // タイル種別（このcpp内だけで使用）
    // ※TileWall側の識別子と衝wall3しないよう、MAP_ 接頭辞を付与
    //==========================================================================
    enum : int
    {
        MAP_TILE_FLOOR = 0,
        MAP_TILE_WALL = 1,
        MAP_TILE_EMPTY = 2,
    };


    struct Room
    {
        int x, y, w, h;
        int cx() const { return x + w / 2; }
        int cy() const { return y + h / 2; }
    };

    //==========================================================================
    // 内部状態
    //==========================================================================
    std::vector<MapObject> g_MapObjects;
    // エネミースポーン候補（ワールド座標）
    std::vector<XMFLOAT3> g_EnemySpawnPositions;
    std::vector<WallPlane> g_WallPlanes;
    WallPlaneRenderer      g_WallRenderer;

    //==============================
    // エネミーの出現量調整
    //==============================
    int enemy_spawnrate = 10;

    // テクスチャ
    int g_FloorTexID = -1;
    int g_WallTexID = -1;
    int g_CeilingTexID = -1;
    int g_TreeTexID = -1;
    int g_GoalTexID = -1;
    int g_WhiteTexID = -1;


    // スポーン位置
    XMFLOAT3 g_SpawnPos = { 0.0f, 1.0f, 0.0f };
    // ゴール位置（中心）
    XMFLOAT3 g_GoalPos = { -1.0f, 1.0f, 0.0f };
    // ボスのスポーン位置（ゴール部屋の中心）
    XMFLOAT3 g_BossSpawnPos = { 0.0f, 1.5f, 0.0f };

    // ゴールの当たり判定（AABB） ※簡単にするなら中心＋固定サイズ
    AABB g_GoalAabb{};

    // ゴール到達回数管理 
    int g_GoalReachCount = 0;              // 現在の到達回数
    constexpr int GOAL_REQUIRED_COUNT = 2; // クリアに必要な回数


    //==========================================================================
    // 生成パラメータ
    //==========================================================================
    constexpr float CELL_SIZE = 1.0f;  // 1タイル=1m
    constexpr float FLOOR_Y = 0.5f;  // 床中心（底面がY=0）

    constexpr float WALL_HEIGHT = 5.0f;

    // 天井の中心Y（壁の上に1枚）
    // 天井は1m厚のCubeを1枚置く想定なので +0.5＜現在メッシュフィールドへの以降に成功し削除
    constexpr float CEILING_CENTER_Y = FLOOR_Y + WALL_HEIGHT;

    // 丸影用の高さ判定（影が小さくなる高さ）
    constexpr float SHADOW_MAX_HEIGHT = 3.0f;

    //==========================================================================
    // タイル座標 → ワールド座標（中心配置）
    //
    // ■役割
    // ・タイル(tx,ty) を「ワールド座標の中心位置」に変換する
    //
    // ■引数
    // ・tx,ty : タイル座標
    // ・w,h   : タイル配列の幅/高さ（中心合わせに使用）
    // ・cellSize : 1タイルのメートル換算
    // ・originX,originZ : マップ全体の原点オフセット
    // ・y : 返したいY（床中心や天井中心などを呼び出し側で指定）
    //
    // ■戻り値
    // ・指定タイルの「中心点」ワールド座標
    //==========================================================================
    static XMFLOAT3 TileCoordToWorldCenter(
        int tx, int ty, int w, int h,
        float cellSize,
        float originX, float originZ,
        float y)
    {
        const float x = originX + (tx - w * 0.5f + 0.5f) * cellSize;
        const float z = originZ + (ty - h * 0.5f + 0.5f) * cellSize;
        return { x, y, z };
    }
    //==========================================================================
    // MapObject追加
    //
    // ■役割
    // ・衝突/床判定用の MapObject を g_MapObjects に追加する
    //
    // ■副作用
    // ・g_MapObjects が増える（描画・衝突判定側が参照）
    //==========================================================================
    static void AddMapObject(int kindId, const XMFLOAT3& pos, const AABB& aabb)
    {
        MapObject obj{};
        obj.KindId = kindId;
        obj.Position = pos;
        obj.Aabb = aabb;
        g_MapObjects.push_back(obj);
    }

    //==========================================================================
    // 通路を幅付きで掘る（水平）※外周は壊さない
    //
    // ■役割
    // ・tiles 上で x1～x2 の範囲を床にする（横方向の通路）
    // ・corridorWidth 分だけ太さを持たせる
    //
    // ■注意
    // ・外周(0 と w-1 / 0 と h-1) は必ず壁のままにする
    //==========================================================================
    static void CarveCorridorHorizontal(
        std::vector<int>& tiles, int w, int h,
        int x1, int x2, int y, int corridorWidth)
    {
        if (x1 > x2) std::swap(x1, x2);

        const int half = corridorWidth / 2;

        for (int x = x1; x <= x2; ++x)
        {
            for (int offsetY = -half; offsetY <= half; ++offsetY)
            {
                const int carveY = y + offsetY;
                if (carveY <= 0 || carveY >= h - 1) continue;
                if (x <= 0 || x >= w - 1) continue;

                tiles[carveY * w + x] = MAP_TILE_FLOOR;
            }
        }
    }

    //==========================================================================
    // 通路を幅付きで掘る（垂直）※外周は壊さない
    //
    // ■役割
    // ・tiles 上で y1～y2 の範囲を床にする（縦方向の通路）
    // ・corridorWidth 分だけ太さを持たせる
    //
    // ■注意
    // ・外周は必ず壁のまま
    //==========================================================================
    static void CarveCorridorVertical(
        std::vector<int>& tiles, int w, int h,
        int y1, int y2, int x, int corridorWidth)
    {
        if (y1 > y2) std::swap(y1, y2);

        const int half = corridorWidth / 2;

        for (int y = y1; y <= y2; ++y)
        {
            for (int offsetX = -half; offsetX <= half; ++offsetX)
            {
                const int carveX = x + offsetX;
                if (carveX <= 0 || carveX >= w - 1) continue;
                if (y <= 0 || y >= h - 1) continue;

                tiles[y * w + carveX] = MAP_TILE_FLOOR;
            }
        }
    }

    //==========================================================================
    // tiles から床/天井だけ MapObject を作る
    //
    // ■役割
    // ・タイルが床(MAP_TILE_FLOOR) の場所にだけ、床と天井のAABB(Cube)を作る
    // ・壁は描画を TileWall に委譲するため、この関数では作らない
    //
    // ■結果
    // ・g_MapObjects に KIND_FLOOR / KIND_CEILING が追加される
    //==========================================================================
    static void BuildFloorCeilingCollidersFromTiles(
        const std::vector<int>& tiles,
        int w, int h,
        float cellSize,
        float originX, float originZ)
    {
        for (int ty = 0; ty < h; ++ty)
        {
            for (int tx = 0; tx < w; ++tx)
            {
                const int t = tiles[ty * w + tx];
                if (t != MAP_TILE_FLOOR) continue;

                // 床（1m厚）
                {
                    const XMFLOAT3 p = TileCoordToWorldCenter(tx, ty, w, h, cellSize, originX, originZ, FLOOR_Y);
                    const AABB a = Cube_CreateAABB(p);
                    AddMapObject(KIND_FLOOR, p, a);
                }

                // 天井（1m厚、1枚）
                {
                    const XMFLOAT3 p = TileCoordToWorldCenter(tx, ty, w, h, cellSize, originX, originZ, CEILING_CENTER_Y);
                    const AABB a = Cube_CreateAABB(p);
                    AddMapObject(KIND_CEILING, p, a);
                }
            }
        }
    }

    //==========================================================================
    // WallPlane から「薄い壁AABB」を作って MapObject に積む
    //
    // ■役割
    // ・壁の見た目は Plane で描画しているが、プレイヤー等の衝突判定は AABB のみ参照する
    // ・そのため、WallPlane の位置/幅/高さ/向きから「衝突用の薄板AABB」を生成する
    //
    // ■引数
    // ・planes    : TileWall が生成した壁パネル群（描画用データ）
    // ・thickness : 当たり判定の厚み（例：0.10f～0.20f）
    //
    // ■結果
    // ・g_MapObjects に KIND_WALL（衝突用）が追加される
    //==========================================================================
    static void BuildWallCollidersFromWallPlanes(
        const std::vector<WallPlane>& planes,
        float thickness)
    {
        const float halfT = thickness * 0.5f;

        for (const WallPlane& pl : planes)
        {
            const XMFLOAT3 c = pl.center;

            const float halfH = pl.height * 0.5f;
            const float minY = c.y - halfH;
            const float maxY = c.y + halfH;

            const bool isNormalX = (fabsf(pl.normal.x) > 0.5f);

            float minX, maxX, minZ, maxZ;

            if (isNormalX)
            {
                minX = c.x - halfT;
                maxX = c.x + halfT;

                const float halfW = pl.width * 0.5f;
                minZ = c.z - halfW;
                maxZ = c.z + halfW;
            }
            else
            {
                minZ = c.z - halfT;
                maxZ = c.z + halfT;

                const float halfW = pl.width * 0.5f;
                minX = c.x - halfW;
                maxX = c.x + halfW;
            }

            AABB a{};
            a.min = { minX, minY, minZ };
            a.max = { maxX, maxY, maxZ };

            AddMapObject(KIND_WALL, c, a);
        }
    }

    //==========================================================================
    // 点(x,z)の真下にある床の天面Yを探す（丸影など）
    //
    // ■役割
    // ・BlobShadow のために「床の上面Y」を取得する
    // ・KIND_FLOOR の AABB だけを対象にする（壁/天井は無視）
    //
    // ■戻り値
    // ・床が見つかれば true + outTopY に最も高い床の天面Y
    // ・見つからなければ false
    //==========================================================================
    static bool TryGetHighestFloorSurfaceY(const XMFLOAT3& pos, float* outTopY)
    {
        float best = -FLT_MAX;
        bool found = false;

        for (const MapObject& obj : g_MapObjects)
        {
            if (obj.KindId != KIND_FLOOR) continue;

            const AABB& a = obj.Aabb;

            if (pos.x < a.min.x || pos.x > a.max.x) continue;
            if (pos.z < a.min.z || pos.z > a.max.z) continue;

            const float topY = a.max.y;

            if (!found || topY > best)
            {
                best = topY;
                found = true;
            }
        }

        if (!found) return false;
        *outTopY = best;
        return true;
    }
}

std::uint32_t Map_GenerateRandomSeed()
{
    // 現在時刻とランダムデバイスを組み合わせてシード生成
    std::random_device rd;
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    // 時刻とランダムデバイスをXORで組み合わせ
    return static_cast<std::uint32_t>(millis) ^ rd();
}




//==============================================================================
// 初期化
//
// ■役割
// ・各種テクスチャをロード
// ・ダンジョン生成を行い、MapObject/WallPlane を構築する
//==============================================================================
void Map_Initialize()
{
    g_FloorTexID = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_WallTexID = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_CeilingTexID = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_TreeTexID = Texture_Load(L"Resource/Texture/tree.png");
    g_GoalTexID = Texture_Load(L"Resource/Texture/goal.png");
    g_WhiteTexID = Texture_Load(L"Resource/Texture/white.png");



    // ランダムなシード値でダンジョン生成デバッグのため現在停止
    Map_GenerateDungeon(Map_GenerateRandomSeed());

    // 乱数seed固定：再現性のあるダンジョン
    //Map_GenerateDungeon(12345u);

}

//==============================================================================
// 終了処理
//
// ■役割
// ・終了時に必要な解放処理を行う
// ・WallPlaneRenderer に Release 相当があればここで呼ぶ
//==============================================================================
void Map_Finalize()
{
    // Renderer側に Release があるなら呼ぶ（無ければ消してOK）
    // g_WallRenderer.Release();
}

//==============================================================================
// 天井テクスチャ差し替え
//
// ■役割
// ・外部から天井のテクスチャIDを差し替える
//==============================================================================
void Map_SetCeilingTexture(int texId)
{
    g_CeilingTexID = texId;
}

const std::vector<DirectX::XMFLOAT3>& Map_GetEnemySpawnPositions()
{
    return g_EnemySpawnPositions;
}


bool Map_IsPlayerReachedGoal()
{
    const AABB p = Player_GetAABB();

    return Collision_IsOverLapAABB(p, g_GoalAabb);
}

DirectX::XMFLOAT3 Map_GetGoalPosition()
{
    return g_GoalPos;
}


//==============================================================================
// スポーン位置取得
//
// ■役割
// ・ダンジョン生成時に決定されたスポーン位置を返す
//==============================================================================
XMFLOAT3 Map_GetSpawnPosition()
{
    return g_SpawnPos;
}

//==============================================================================
// ボスルーム中心座標取得
//
// ■役割
// ・ダンジョン生成時に確定したボスのスポーン位置を返す
// ・game.cpp から EnemyBoss をスポーンするために使用する
//==============================================================================
XMFLOAT3 Map_GetBossSpawnPosition()
{
    return g_BossSpawnPos;
}

//==============================================================================
// ダンジョン生成
//
// ■役割（全体）
// ・tiles を「壁で埋める」→「部屋配置」→「通路掘り」→「外周壁固定」
// ・tiles から床/天井の MapObject(AABB) を生成
// ・tiles から壁Plane（見た目）を生成し、さらに衝突用薄板AABBを生成
//
// ■結果
// ・g_MapObjects（床/天井/壁の衝突AABB）
// ・g_WallPlanes（壁の見た目）
// ・g_SpawnPos（最初の部屋中心）
//==============================================================================
void Map_GenerateDungeon(std::uint32_t seed)
{
    //==============================
    // マップ全体サイズ（奇数推奨）
    //==============================
    const int dungeonWidth =  79;
    const int dungeonHeight = 79;

    //==============================
    // 部屋生成の試行回数
    //==============================
    const int roomTryCount = 120;

    //==============================
    // ワールド座標変換用の基準位置
    //==============================
    const float originX = 0.5f;
    const float originZ = 0.5f;

    //==============================
    // 部屋サイズ（奇数推奨）
    //==============================
    const int roomMinSize = 5;
    const int roomMaxSize = 7;

    //const int roomMinSize = 73;
    //const int roomMaxSize = 73;
    //

    //==============================
    // 通路の幅（奇数推奨）
    //==============================
    const int corridorWidth = 5;


    //==============================
    // 乱数生成器（シード指定で再現可能）
    //==============================
    std::mt19937 rng(seed);

    std::uniform_int_distribution<int> roomWDist(roomMinSize, roomMaxSize);
    std::uniform_int_distribution<int> roomHDist(roomMinSize, roomMaxSize);

    std::uniform_int_distribution<int> roomXDist(2, dungeonWidth - roomMaxSize - 3);
    std::uniform_int_distribution<int> roomYDist(2, dungeonHeight - roomMaxSize - 3);

    //==============================
    // タイル配列を「すべて壁」で初期化
    //==============================
    std::vector<int> tiles(dungeonWidth * dungeonHeight, MAP_TILE_WALL);

    //==============================
    // 配置済み部屋
    //==============================
    std::vector<Room> rooms;
    rooms.reserve(64);

    //==============================
    // 部屋同士の重なり判定（marginで離す）
    //==============================
    auto overlaps = [&](const Room& a, const Room& b) -> bool
        {
            const int margin = 4; // 近すぎる配置を避ける（壁欠け/通路干渉も起きにくい）

            return (a.x - margin < b.x + b.w &&
                a.x + a.w + margin > b.x &&
                a.y - margin < b.y + b.h &&
                a.y + a.h + margin > b.y);
        };

    //==============================
    // 部屋のランダム配置（採用された部屋は床に塗る）
    //==============================
    for (int i = 0; i < roomTryCount; ++i)
    {
        Room r{};

        // 奇数サイズに寄せる（中心/接続が安定しやすい）
        r.w = roomWDist(rng) | 1;
        r.h = roomHDist(rng) | 1;

        r.x = roomXDist(rng);
        r.y = roomYDist(rng);

        // 端に食い込む配置は弾く（外周壁固定のため）
        if (r.x + r.w >= dungeonWidth - 2) continue;
        if (r.y + r.h >= dungeonHeight - 2) continue;

        // 既存部屋と重なる/近いなら弾く
        bool ok = true;
        for (const Room& e : rooms)
        {
            if (overlaps(r, e)) { ok = false; break; }
        }
        if (!ok) continue;

        // 採用
        rooms.push_back(r);

        // 部屋範囲を床にする
        for (int y = r.y; y < r.y + r.h; ++y)
        {
            for (int x = r.x; x < r.x + r.w; ++x)
            {
                tiles[y * dungeonWidth + x] = MAP_TILE_FLOOR;
            }
        }
    }

    //==============================
    // 部屋が0の場合の保険
    //==============================
    if (rooms.empty())
    {
        Room r
        {
            dungeonWidth / 2 - roomMinSize / 2,
            dungeonHeight / 2 - roomMinSize / 2,
            roomMinSize | 1,
            roomMinSize | 1
        };

        rooms.push_back(r);

        for (int y = r.y; y < r.y + r.h; ++y)
        {
            for (int x = r.x; x < r.x + r.w; ++x)
            {
                tiles[y * dungeonWidth + x] = MAP_TILE_FLOOR;
            }
        }
    }

    //==============================
    // ボスルームの確定と拡張
    // ・rooms[0]（プレイヤースポーン）から最遠の部屋を 13×13 タイルに拡張する
    // ・この処理は通路接続・正規化より前に行う（正規化で壁が自然につく）
    //==============================
    int bossRoomIdx = 0;
    if (rooms.size() > 1)
    {
        constexpr int BOSS_HALF = 6; // 6*2+1 = 13×13 タイル

        // 最遠部屋を探す（rooms[0] = プレイヤースポーン基準）
        {
            int bestD2 = 0;
            for (int i = 1; i < (int)rooms.size(); ++i)
            {
                const int dx = rooms[i].cx() - rooms[0].cx();
                const int dy = rooms[i].cy() - rooms[0].cy();
                const int d2 = dx * dx + dy * dy;
                if (d2 > bestD2) { bestD2 = d2; bossRoomIdx = i; }
            }
        }

        // ボスルームを 13×13 タイルに拡張して掘り込む
        const int bx = rooms[bossRoomIdx].cx();
        const int by = rooms[bossRoomIdx].cy();

        for (int ry = by - BOSS_HALF; ry <= by + BOSS_HALF; ++ry)
        {
            for (int rx = bx - BOSS_HALF; rx <= bx + BOSS_HALF; ++rx)
            {
                if (rx > 0 && rx < dungeonWidth - 1 &&
                    ry > 0 && ry < dungeonHeight - 1)
                {
                    tiles[ry * dungeonWidth + rx] = MAP_TILE_FLOOR;
                }
            }
        }

        // Room 構造体を更新（cx()/cy() の整合性を維持するため）
        rooms[bossRoomIdx].x = bx - BOSS_HALF;
        rooms[bossRoomIdx].y = by - BOSS_HALF;
        rooms[bossRoomIdx].w = BOSS_HALF * 2 + 1;
        rooms[bossRoomIdx].h = BOSS_HALF * 2 + 1;
    }

    //==============================
    // 接続ヘルパ（L字通路：順序をランダム化）
    //==============================
    auto ConnectRooms_L = [&](int x1, int y1, int x2, int y2)
        {
            // 0/1で横→縦 or 縦→横を切り替える
            const bool hv = (std::uniform_int_distribution<int>(0, 1)(rng) == 0);

            if (hv)
            {
                CarveCorridorHorizontal(tiles, dungeonWidth, dungeonHeight, x1, x2, y1, corridorWidth);
                CarveCorridorVertical(tiles, dungeonWidth, dungeonHeight, y1, y2, x2, corridorWidth);
            }
            else
            {
                CarveCorridorVertical(tiles, dungeonWidth, dungeonHeight, y1, y2, x1, corridorWidth);
                CarveCorridorHorizontal(tiles, dungeonWidth, dungeonHeight, x1, x2, y2, corridorWidth);
            }
        };

    //==============================
    // 1) 枝分かれ：各部屋を「近い既存部屋」へ接続
    //==============================
    // ・直前固定をやめることで自然に分岐が増える
    // ・距離は中心同士の平方距離でOK（軽い）
    for (size_t i = 1; i < rooms.size(); ++i)
    {
        const int x2 = rooms[i].cx();
        const int y2 = rooms[i].cy();

        int bestIdx = 0;
        int bestD2 = INT_MAX;

        for (size_t j = 0; j < i; ++j)
        {
            const int x1 = rooms[j].cx();
            const int y1 = rooms[j].cy();

            const int dx = x2 - x1;
            const int dy = y2 - y1;
            const int d2 = dx * dx + dy * dy;

            if (d2 < bestD2)
            {
                bestD2 = d2;
                bestIdx = static_cast<int>(j);
            }
        }

        const int x1 = rooms[bestIdx].cx();
        const int y1 = rooms[bestIdx].cy();

        ConnectRooms_L(x1, y1, x2, y2);
    }

    //==============================
    // 2) 追加接続（ループ生成）
    //==============================
    // ・枝だけだと木構造になりやすいので、少数だけ"輪"を入れる
    // ・近い部屋同士を繋ぐと通路が暴れにくい
    const int extraLinks = std::max(1, static_cast<int>(rooms.size() / 4)); // 目安：部屋数の25%
    std::uniform_int_distribution<int> roomPick(0, static_cast<int>(rooms.size() - 1));

    // 既存接続の重複を避けるため、簡易にペア記録（小規模ならこれで十分）
    auto MakeKey = [](int a, int b) -> std::uint32_t
        {
            if (a > b) std::swap(a, b);
            return (static_cast<std::uint32_t>(a) << 16) | static_cast<std::uint32_t>(b);
        };

    std::vector<std::uint32_t> usedPairs;
    usedPairs.reserve(static_cast<size_t>(rooms.size() * 2));

    // 既に作った「枝分かれ」接続もペアとして登録しておく（重複掘りを減らす）
    for (size_t i = 1; i < rooms.size(); ++i)
    {
        // i は nearest に繋いだが、nearestIdx を保持していないので完全追跡はしない
        // ここでは"追加接続の重複回避"を軽くするため、usedPairs を過信しない方針
        // （同じ通路を2回掘っても致命的ではないため）
    }

    for (int k = 0; k < extraLinks; ++k)
    {
        const int a = roomPick(rng);

        // a に近い別部屋 b を探す（既に同じペアを作っていたら次候補へ）
        int bestB = -1;
        int bestD2 = INT_MAX;

        for (int b = 0; b < static_cast<int>(rooms.size()); ++b)
        {
            if (b == a) continue;

            const std::uint32_t key = MakeKey(a, b);
            bool used = false;
            for (std::uint32_t u : usedPairs)
            {
                if (u == key) { used = true; break; }
            }
            if (used) continue;

            const int ax = rooms[a].cx();
            const int ay = rooms[a].cy();
            const int bx = rooms[b].cx();
            const int by = rooms[b].cy();

            const int dx = bx - ax;
            const int dy = by - ay;
            const int d2 = dx * dx + dy * dy;

            if (d2 < bestD2)
            {
                bestD2 = d2;
                bestB = b;
            }
        }

        if (bestB < 0) continue;

        usedPairs.push_back(MakeKey(a, bestB));

        ConnectRooms_L(
            rooms[a].cx(), rooms[a].cy(),
            rooms[bestB].cx(), rooms[bestB].cy()
        );
    }

    //==============================
    // 外周を必ず壁に固定
    //==============================
    for (int x = 0; x < dungeonWidth; ++x)
    {
        tiles[0 * dungeonWidth + x] = MAP_TILE_WALL;
        tiles[(dungeonHeight - 1) * dungeonWidth + x] = MAP_TILE_WALL;
    }
    for (int y = 0; y < dungeonHeight; ++y)
    {
        tiles[y * dungeonWidth + 0] = MAP_TILE_WALL;
        tiles[y * dungeonWidth + (dungeonWidth - 1)] = MAP_TILE_WALL;
    }


    //==============================
    // タイル整理（壁を1層に正規化）
    // ・床の周囲1マスだけを壁にする
    // ・それより外側は空間（MAP_TILE_EMPTY）に落とす
    // ・床は絶対に壊さない
    // ・外周は最後に必ず壁へ固定する
    //==============================
    {
        std::vector<int> out = tiles; // 参照元を保持しながら書き換える

        // いったん全て「空間」へ（床は維持）
        for (int y = 0; y < dungeonHeight; ++y)
        {
            for (int x = 0; x < dungeonWidth; ++x)
            {
                const int idx = y * dungeonWidth + x;
                if (tiles[idx] == MAP_TILE_FLOOR)
                    out[idx] = MAP_TILE_FLOOR;  // 床は維持
                else
                    out[idx] = MAP_TILE_EMPTY;  // それ以外は空間へ
            }
        }

        // 床の4近傍だけ「壁」にする（= 壁は1層だけ残る）
        for (int y = 1; y < dungeonHeight - 1; ++y)
        {
            for (int x = 1; x < dungeonWidth - 1; ++x)
            {
                const int idx = y * dungeonWidth + x;
                if (tiles[idx] != MAP_TILE_FLOOR) continue;

                // 4方向（壁は外皮として1層だけ）
                const int n = (y - 1) * dungeonWidth + x;
                const int s = (y + 1) * dungeonWidth + x;
                const int w = y * dungeonWidth + (x - 1);
                const int e = y * dungeonWidth + (x + 1);

                if (out[n] != MAP_TILE_FLOOR) out[n] = MAP_TILE_WALL;
                if (out[s] != MAP_TILE_FLOOR) out[s] = MAP_TILE_WALL;
                if (out[w] != MAP_TILE_FLOOR) out[w] = MAP_TILE_WALL;
                if (out[e] != MAP_TILE_FLOOR) out[e] = MAP_TILE_WALL;
            }
        }

        //========================================================
        // 追加：1マス壁（柱）を除去（床に戻す）
        // ・床に挟まれている壁 / 床に囲まれた壁は「柱」扱いで床へ
        //========================================================
        for (int y = 1; y < dungeonHeight - 1; ++y)
        {
            for (int x = 1; x < dungeonWidth - 1; ++x)
            {
                const int idx = y * dungeonWidth + x;
                if (out[idx] != MAP_TILE_WALL) continue;

                //＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
                // 東西南北にタイルがあるか判定
                //＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
                const int n = out[(y - 1) * dungeonWidth + x];
                const int s = out[(y + 1) * dungeonWidth + x];
                const int w_ = out[y * dungeonWidth + (x - 1)];
                const int e = out[y * dungeonWidth + (x + 1)];

                const bool fn = (n == MAP_TILE_FLOOR);
                const bool fs = (s == MAP_TILE_FLOOR);
                const bool fw = (w_ == MAP_TILE_FLOOR);
                const bool fe = (e == MAP_TILE_FLOOR);

                const int floorCount =
                    (fn ? 1 : 0) + (fs ? 1 : 0) + (fw ? 1 : 0) + (fe ? 1 : 0);

                const bool sandwichedX = (fw && fe); // 東西に挟まれている
                const bool sandwichedZ = (fn && fs); // 南北に挟まれている

                // 除去条件：
                //   ・対面2方向に床（東西 or 南北）→ 廊下内の柱
                //   ・3方向以上に床              → 完全孤立柱
                // 隣接2方向のみ（NE/NW/SE/SW）は部屋コーナー壁なので維持
                if (sandwichedX || sandwichedZ || floorCount >= 3)
                {
                    out[idx] = MAP_TILE_FLOOR;
                }

            }
        }

        //========================================================
        // 柱除去後の再壁付け（バグ修正）
        // ・柱除去で WALL→FLOOR になったタイルの X/Z 隣接が
        //   EMPTY のまま残ると TileWall が壁 Plane を生成しない
        // ・EMPTY になっている隣接タイルを WALL に昇格させる
        //========================================================
        for (int y = 1; y < dungeonHeight - 1; ++y)
        {
            for (int x = 1; x < dungeonWidth - 1; ++x)
            {
                const int idx = y * dungeonWidth + x;
                if (out[idx] != MAP_TILE_FLOOR) continue;

                const int n = (y - 1) * dungeonWidth + x;
                const int s = (y + 1) * dungeonWidth + x;
                const int w_ = y * dungeonWidth + (x - 1);
                const int e_ = y * dungeonWidth + (x + 1);

                if (out[n] == MAP_TILE_EMPTY) out[n] = MAP_TILE_WALL;
                if (out[s] == MAP_TILE_EMPTY) out[s] = MAP_TILE_WALL;
                if (out[w_] == MAP_TILE_EMPTY) out[w_] = MAP_TILE_WALL;
                if (out[e_] == MAP_TILE_EMPTY) out[e_] = MAP_TILE_WALL;
            }
        }

        // 外周は必ず壁に固定（空間落ちを防ぐ）
        for (int x = 0; x < dungeonWidth; ++x)
        {
            out[0 * dungeonWidth + x] = MAP_TILE_WALL;
            out[(dungeonHeight - 1) * dungeonWidth + x] = MAP_TILE_WALL;
        }
        for (int y = 0; y < dungeonHeight; ++y)
        {
            out[y * dungeonWidth + 0] = MAP_TILE_WALL;
            out[y * dungeonWidth + (dungeonWidth - 1)] = MAP_TILE_WALL;
        }

        tiles.swap(out);
    }





    //==============================
    // ゴール位置（スポーンから最も遠い部屋の中心）
    // CELL_SIZE = 1.0f なのでタイル距離 = ワールド距離（メートル）
    // 目標: プレイヤースポーンから 30m 以上離れた部屋を優先
    //==============================
    {
        // スポーン地点（rooms[0]）のタイル座標
        const int spawnTileX = rooms[0].cx();
        const int spawnTileY = rooms[0].cy();

        // 全部屋の中で最も遠い部屋をゴールに選ぶ
        int gx = rooms.back().cx();   // フォールバック（部屋が1つの場合など）
        int gy = rooms.back().cy();
        float bestDistSq = -1.0f;

        for (const auto& room : rooms)
        {
            const float dx = static_cast<float>(room.cx() - spawnTileX) * CELL_SIZE;
            const float dz = static_cast<float>(room.cy() - spawnTileY) * CELL_SIZE;
            const float distSq = dx * dx + dz * dz;

            if (distSq > bestDistSq)
            {
                bestDistSq = distSq;
                gx = room.cx();
                gy = room.cy();
            }
        }

        // Map_Draw と一致
        const float billboardW = 1.0f;
        const float billboardH = 1.5f;
        const float pivotX = 0.5f;
        const float pivotY = 0.0f;

        // 見た目は少し高く（埋まり防止）
        const float goalVisualY = 1.6f;

        // 判定は床付近（触りやすさ優先）
        const float goalHitY = 1.0f;

        // 表示用（Billboard_Draw の位置）
        g_GoalPos = TileCoordToWorldCenter(
            gx, gy,
            dungeonWidth, dungeonHeight,
            CELL_SIZE,
            originX, originZ,
            goalVisualY
        );

        // 判定用AABBの中心点（XZは同じ、Yだけ別）
        const XMFLOAT3 hitPos = TileCoordToWorldCenter(
            gx, gy,
            dungeonWidth, dungeonHeight,
            CELL_SIZE,
            originX, originZ,
            goalHitY
        );

        // 見た目サイズに合わせたAABB（XZは薄板）
        const float minX = hitPos.x - billboardW * pivotX;
        const float maxX = hitPos.x + billboardW * (1.0f - pivotX);

        const float minY = hitPos.y - billboardH * pivotY;
        const float maxY = hitPos.y + billboardH * (1.0f - pivotY);

        const float halfZ = 0.5f; // 触りやすさ優先（0.25→0.5）
        const float minZ = hitPos.z - halfZ;
        const float maxZ = hitPos.z + halfZ;

        g_GoalAabb.min = { minX, minY, minZ };
        g_GoalAabb.max = { maxX, maxY, maxZ };

        // ボスのスポーン位置（ゴール部屋の中心、プレイヤーと同じ高さ）
        g_BossSpawnPos = TileCoordToWorldCenter(
            gx, gy,
            dungeonWidth, dungeonHeight,
            CELL_SIZE,
            originX, originZ,
            1.5f
        );
    }



    //==============================
    // 床/天井 MapObject 再構築
    //==============================
    g_MapObjects.clear();
    g_MapObjects.reserve(
        static_cast<size_t>(dungeonWidth) *
        static_cast<size_t>(dungeonHeight) * 2
    );

    BuildFloorCeilingCollidersFromTiles(
        tiles,
        dungeonWidth,
        dungeonHeight,
        CELL_SIZE,
        originX,
        originZ
    );

    //==============================
    // 壁Plane生成（TileWallへ委譲：見た目）
    //==============================
    {
        TileWallParams par{};
        par.cellSize = CELL_SIZE;
        par.floorY = FLOOR_Y;
        par.wallHeight = WALL_HEIGHT;
        par.panelHeight = WALL_HEIGHT;
        par.uvUnitMeter = 1.0f;

        TileWall_BuildFromTiles(
            tiles,
            dungeonWidth, dungeonHeight,
            originX, originZ,
            par,
            g_WallPlanes
        );

        g_WallRenderer.Build(g_WallPlanes);

        // 壁の衝突AABB（薄板）を生成して MapObject に積む
        BuildWallCollidersFromWallPlanes(g_WallPlanes, 0.15f);
        // ゴールを MapObject として追加（当たり判定用）
        {
            AddMapObject(KIND_GOAL, g_GoalPos, g_GoalAabb);
        }

        {
            std::vector<DirectX::XMFLOAT3> floorPositions;
            floorPositions.reserve(dungeonWidth * dungeonHeight);

            for (int ty = 0; ty < dungeonHeight; ++ty)
            {
                for (int tx = 0; tx < dungeonWidth; ++tx)
                {
                    const int t = tiles[ty * dungeonWidth + tx];
                    if (t != MAP_TILE_FLOOR) continue;

                    const XMFLOAT3 p = TileCoordToWorldCenter(
                        tx, ty,
                        dungeonWidth, dungeonHeight,
                        CELL_SIZE,
                        originX, originZ,
                        1.0f
                    );

                    floorPositions.push_back(p);
                }
            }

            // スポーン位置（最初の部屋の中心）を先に確定する
            {
                const int sx = rooms[0].cx();
                const int sy = rooms[0].cy();

                g_SpawnPos = TileCoordToWorldCenter(
                    sx, sy,
                    dungeonWidth, dungeonHeight,
                    CELL_SIZE,
                    originX, originZ,
                    1.5f
                );
            }

            // エネミースポーン生成（g_SpawnPos確定後に行う）
            {
                g_EnemySpawnPositions.clear();

                if (!floorPositions.empty())
                {
                    const size_t totalFloors = floorPositions.size();
                    const size_t playerSpawnFloorCount = 15;

                    std::vector<XMFLOAT3> candidatePositions;
                    candidatePositions.reserve(totalFloors);

                    const XMFLOAT3 playerSpawn = g_SpawnPos;
                    for (int ty = 1; ty < dungeonHeight - 1; ++ty)
                    {
                        for (int tx = 1; tx < dungeonWidth - 1; ++tx)
                        {
                            if (tiles[ty * dungeonWidth + tx] != MAP_TILE_FLOOR) continue;

                            // 4近傍がすべて床のタイルのみをスポーン候補にする
                            const bool n = (tiles[(ty - 1) * dungeonWidth + tx] == MAP_TILE_FLOOR);
                            const bool s = (tiles[(ty + 1) * dungeonWidth + tx] == MAP_TILE_FLOOR);
                            const bool w = (tiles[ty * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                            const bool e = (tiles[ty * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);
                            const bool nw = (tiles[(ty - 1) * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                            const bool ne = (tiles[(ty - 1) * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);
                            const bool sw = (tiles[(ty + 1) * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                            const bool se = (tiles[(ty + 1) * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);

                            if (!n || !s || !w || !e || !nw || !ne || !sw || !se) continue;

                            const XMFLOAT3 pos = TileCoordToWorldCenter(tx, ty, dungeonWidth, dungeonHeight, CELL_SIZE, originX, originZ, 1.0f);

                            const float dx = pos.x - playerSpawn.x;
                            const float dz = pos.z - playerSpawn.z;
                            const float distSq = dx * dx + dz * dz;

                            if (distSq > playerSpawnFloorCount * playerSpawnFloorCount)
                            {
                                // ボスルーム内はスポーン対象外（ボス専用エリアを確保）
                                const float bossDx = pos.x - g_BossSpawnPos.x;
                                const float bossDz = pos.z - g_BossSpawnPos.z;
                                constexpr float BOSS_EXCLUDE_R = 7.0f; // BOSS_HALF+1 タイル相当
                                if (bossDx * bossDx + bossDz * bossDz < BOSS_EXCLUDE_R * BOSS_EXCLUDE_R)
                                    continue;

                                candidatePositions.push_back(pos);
                            }
                        }
                    }

                    if (!candidatePositions.empty())
                    {
                        const int spawnCount = std::min(
                            static_cast<int>(candidatePositions.size()) / enemy_spawnrate,
                            static_cast<int>(rooms.size()) * 2
                        );

                        // candidatePositions をシャッフルして先頭からspawnCount個取る
                        std::shuffle(candidatePositions.begin(), candidatePositions.end(), rng);

                        for (int n = 0; n < spawnCount; ++n)
                        {
                            g_EnemySpawnPositions.push_back(candidatePositions[n]);
                        }
                    }
                }
            }

            // 4近傍がすべて床のタイルのみをAI巡回候補として渡す
            std::vector<DirectX::XMFLOAT3> patrolPositions;
            patrolPositions.reserve(floorPositions.size());

            for (int ty = 1; ty < dungeonHeight - 1; ++ty)
            {
                for (int tx = 1; tx < dungeonWidth - 1; ++tx)
                {
                    if (tiles[ty * dungeonWidth + tx] != MAP_TILE_FLOOR) continue;

                    const bool n = (tiles[(ty - 1) * dungeonWidth + tx] == MAP_TILE_FLOOR);
                    const bool s = (tiles[(ty + 1) * dungeonWidth + tx] == MAP_TILE_FLOOR);
                    const bool w = (tiles[ty * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                    const bool e = (tiles[ty * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);
                    const bool nw = (tiles[(ty - 1) * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                    const bool ne = (tiles[(ty - 1) * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);
                    const bool sw = (tiles[(ty + 1) * dungeonWidth + (tx - 1)] == MAP_TILE_FLOOR);
                    const bool se = (tiles[(ty + 1) * dungeonWidth + (tx + 1)] == MAP_TILE_FLOOR);

                    if (!n || !s || !w || !e || !nw || !ne || !sw || !se) continue;


                    const XMFLOAT3 p = TileCoordToWorldCenter(
                        tx, ty,
                        dungeonWidth, dungeonHeight,
                        CELL_SIZE,
                        originX, originZ,
                        1.0f
                    );

                    patrolPositions.push_back(p);
                }
            }

            MapPatrolAI_Initialize(patrolPositions);

            // ミニマップ用タイル層（上空に配置）
            {
                const float minimapY = 10.0f;

                for (int ty = 0; ty < dungeonHeight; ++ty)
                {
                    for (int tx = 0; tx < dungeonWidth; ++tx)
                    {
                        const int t = tiles[ty * dungeonWidth + tx];

                        if (t == MAP_TILE_FLOOR)
                        {
                            const XMFLOAT3 pos = TileCoordToWorldCenter(tx, ty, dungeonWidth, dungeonHeight, CELL_SIZE, originX, originZ, minimapY);
                            const AABB a = Cube_CreateAABB(pos);
                            AddMapObject(KIND_MINIMAP_FLOOR, pos, a);
                        }
                    }
                }

                {
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const XMFLOAT3 pos = {
                                g_GoalPos.x + dx * CELL_SIZE,
                                minimapY,
                                g_GoalPos.z + dy * CELL_SIZE
                            };
                            const AABB a = Cube_CreateAABB(pos);
                            AddMapObject(KIND_MINIMAP_GOAL, pos, a);
                        }
                }
            }
        }



    }

}

//==============================================================================
// 床登録（床だけ）
//
// ■役割
// ・FloorRegistry（床判定用）に床AABBだけ登録する
// ・Player 等の「接地判定」がこの床リストを参照する想定
//==============================================================================
void Map_RegisterFloors()
{
    FloorRegistry::Clear();

    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_FLOOR)
            FloorRegistry::Add(o.Aabb);
    }
}

//==============================================================================
// 描画
//
// ■役割（ざっくり順）
// 1) 丸影（床が見つからない場合は無効化）
// 2) AABBデバッグ描画
// 3) 地面（MeshField）
// 4) 3D描画状態 + ライティング
// 5) 床/天井（Cube）
// 6) 壁（PlaneRenderer）
// 7) 丸影解除
//==============================================================================
void Map_Draw()
{
    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    // 丸影（b6）は呼び元（game.cpp）が設定済みの値をそのまま使う

    //--------------------------------------------------------------------------
    // AABBデバッグ（床/天井のみ）
    //--------------------------------------------------------------------------
    // ※壁の衝突AABBも存在するが、表示が煩雑になりやすいのでここでは一律緑表示
    //for (const MapObject& o : g_MapObjects)
    //{
    //    Collision_DebugDraw(o.Aabb, { 0.2f, 0.8f, 0.2f, 1.0f });
    //}

   ////--------------------------------------------------------------------------
   //// 地面（見た目）
   ////--------------------------------------------------------------------------
   //{
   //    XMMATRIX mtxWorldField = XMMatrixIdentity();
   //    MeshField_Draw(mtxWorldField);
   //}

    //--------------------------------------------------------------------------
    // 3D描画 状態
    //--------------------------------------------------------------------------
    Map_Light_Reset();

    //--------------------------------------------------------------------------
    // 床をメッシュフィールドで描画、天井はCube
    //--------------------------------------------------------------------------
    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_FLOOR)
        {
            // 床：メッシュフィールドで描画
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_FloorTexID, CELL_SIZE);
        }
        else if (o.KindId == KIND_CEILING)
        {
            // 天井：メッシュフィールド（法線下向き）
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTileCeiling(world, g_CeilingTexID, CELL_SIZE);
        }

        // ミニマップ描画
        else if (o.KindId == KIND_MINIMAP_FLOOR)
        {

            // ミニマップ用床：緑
            Shader3d_SetColor({ 0.5f, 0.5f, 0.5f, 0.5f });  // 濃いグレー
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
            Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
        else if (o.KindId == KIND_MINIMAP_GOAL)
        {
            // ゴール：黄色
            Shader3d_SetColor({ 0.0f, 0.0f, 0.7f, 1.0f });
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y + 1, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
            Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
















    }

    //--------------------------------------------------------------------------
    // 壁はPlane方式で描画（向き固定・UVループ・パネル積み済み）
    //--------------------------------------------------------------------------
    g_WallRenderer.Draw(g_WallTexID);

    // 丸影（b6）のクリアは呼び元（game.cpp）が行う

    //-------------------------------------------------------------------------
    // ビルボード
    //--------------------------------------------------------------------------
    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId != KIND_TREE) continue;

        const XMFLOAT2 scale = { 2.0f, 3.0f };
        const XMFLOAT2 pivot = { 0.5f, 1.0f };
        Billboard_Draw(g_TreeTexID, o.Position, scale, pivot);


    }
    // map.cpp の Map_Draw() 内、ゴール描画の直前に追加

    //--------------------------------------------------------------------------
    // 状態復帰（Wall側がPS定数を上書きする場合の保険）
    //--------------------------------------------------------------------------
    Map_Light_Reset();
}

//==============================================================================
// ゴールビルボード描画（モデル類の描画後に呼ぶこと）
//
// ■なぜ Map_Draw() から分離するか
//   ゴールは半透明ビルボード。Map_Draw() 内で先に描くと透明ピクセルが深度バッファに
//   書き込まれ、後から描くエネミー・プレイヤーがその画素で深度テストに落ちて
//   「透過越しにモデルが映らない」問題が起きる。
//   → 全不透明モデルを描いた後に呼ぶことで正しく透過合成される。
//==============================================================================
void Map_DrawGoal()
{
    if (g_GoalTexID < 0) return;

    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    // 既存のブレンドステートを退避
    ID3D11BlendState* oldBlendState = nullptr;
    FLOAT oldBlendFactor[4];
    UINT oldSampleMask;
    ctx->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

    // アルファブレンドを設定
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* alphaBlend = nullptr;
    Direct3D_GetDevice()->CreateBlendState(&blendDesc, &alphaBlend);
    ctx->OMSetBlendState(alphaBlend, nullptr, 0xFFFFFFFF);

    // ビルボード描画（モデル後なので透過部分に正しくモデルが映る）
    const XMFLOAT2 scale = { 1.0f, 1.5f };
    const XMFLOAT2 pivot = { 0.5f, 0.0f };
    Billboard_Draw(g_GoalTexID, g_GoalPos, scale, pivot);

    // ブレンドステートを元に戻す
    ctx->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
    SAFE_RELEASE(alphaBlend);
    SAFE_RELEASE(oldBlendState);
}

//==============================================================================
// MapObject数
//
// ■役割
// ・現在保持している MapObject 数を返す
//==============================================================================
int Map_GetObjectsCount()
{
    return static_cast<int>(g_MapObjects.size());
}

//==============================================================================
// MapObject取得
//
// ■役割
// ・index 番目の MapObject を返す（ポインタ）
// ■注意
// ・範囲外チェックは呼び出し側責任
//==============================================================================
const MapObject* Map_GetObject(int index)
{
    return &g_MapObjects[index];
}


//==============================================================================
// ゴール到達回数を加算
//==============================================================================
void Map_AddGoalReachCount()
{
    g_GoalReachCount++;
    enemy_spawnrate -= 2;

    // 最低値の制限（エネミーが多すぎないように）
    if (enemy_spawnrate < 3)
    {
        enemy_spawnrate = 3;
    }
}

//==============================================================================
// ゴール到達回数を取得
//==============================================================================
int Map_GetGoalReachCount()
{
    return g_GoalReachCount;
}

//==============================================================================
// ゴール到達回数をリセット
//==============================================================================
void Map_ResetGoalReachCount()
{
    g_GoalReachCount = 0;
}

//==============================================================================
// クリア判定
//==============================================================================
bool Map_IsClearConditionMet()
{
    return g_GoalReachCount >= GOAL_REQUIRED_COUNT;
}

//==============================================================================
// ボス部屋専用マップ生成
//
// ■役割
// ・2回ゴール到達後に遷移するボス専用アリーナ（単一の大部屋）を生成する
// ・通常ダンジョンの代わりに呼ぶ
// ・ゴールは無効化、通常エネミースポーンなし
//==============================================================================
void Map_GenerateBossRoom(std::uint32_t /*seed*/)
{
    //==============================
    // マップサイズ（単一アリーナ）
    //==============================
    const int mapW = 29;
    const int mapH = 29;
    const float originX = 0.5f;
    const float originZ = 0.5f;

    //==============================
    // 全て壁で初期化
    //==============================
    std::vector<int> tiles(mapW * mapH, MAP_TILE_WALL);

    //==============================
    // 中央に大部屋を掘る（外周2マスを壁として残す）
    //==============================
    const int roomX1 = 2, roomX2 = mapW - 3; // 2..18 → 17タイル幅
    const int roomY1 = 2, roomY2 = mapH - 3; // 2..18 → 17タイル奥行き

    for (int y = roomY1; y <= roomY2; ++y)
        for (int x = roomX1; x <= roomX2; ++x)
            tiles[y * mapW + x] = MAP_TILE_FLOOR;

    //==============================
    // 外周を必ず壁に固定
    //==============================
    for (int x = 0; x < mapW; ++x)
    {
        tiles[0 * mapW + x] = MAP_TILE_WALL;
        tiles[(mapH - 1) * mapW + x] = MAP_TILE_WALL;
    }
    for (int y = 0; y < mapH; ++y)
    {
        tiles[y * mapW + 0] = MAP_TILE_WALL;
        tiles[y * mapW + (mapW - 1)] = MAP_TILE_WALL;
    }

    //==============================
    // 壁を1層に正規化
    //==============================
    {
        std::vector<int> out(mapW * mapH, MAP_TILE_EMPTY);
        for (int y = 0; y < mapH; ++y)
            for (int x = 0; x < mapW; ++x)
                if (tiles[y * mapW + x] == MAP_TILE_FLOOR)
                    out[y * mapW + x] = MAP_TILE_FLOOR;

        for (int y = 1; y < mapH - 1; ++y)
            for (int x = 1; x < mapW - 1; ++x)
            {
                if (tiles[y * mapW + x] != MAP_TILE_FLOOR) continue;
                const int n = (y - 1) * mapW + x;
                const int s = (y + 1) * mapW + x;
                const int w_ = y * mapW + (x - 1);
                const int e_ = y * mapW + (x + 1);
                if (out[n] != MAP_TILE_FLOOR) out[n] = MAP_TILE_WALL;
                if (out[s] != MAP_TILE_FLOOR) out[s] = MAP_TILE_WALL;
                if (out[w_] != MAP_TILE_FLOOR) out[w_] = MAP_TILE_WALL;
                if (out[e_] != MAP_TILE_FLOOR) out[e_] = MAP_TILE_WALL;
            }

        for (int x = 0; x < mapW; ++x)
        {
            out[0 * mapW + x] = MAP_TILE_WALL;
            out[(mapH - 1) * mapW + x] = MAP_TILE_WALL;
        }
        for (int y = 0; y < mapH; ++y)
        {
            out[y * mapW + 0] = MAP_TILE_WALL;
            out[y * mapW + (mapW - 1)] = MAP_TILE_WALL;
        }

        tiles.swap(out);
    }

    //==============================
    // スポーン位置（部屋の手前側）
    //==============================
    g_SpawnPos = TileCoordToWorldCenter(
        mapW / 2, roomY1 + 2,
        mapW, mapH, CELL_SIZE, originX, originZ, 1.5f);

    //==============================
    // ボスのスポーン位置（部屋の奥側）
    //==============================
    g_BossSpawnPos = TileCoordToWorldCenter(
        mapW / 2, roomY2 - 2,
        mapW, mapH, CELL_SIZE, originX, originZ, 1.5f);

    //==============================
    // ゴールを無効化（地下に沈めて当たり判定・ビルボードを隠す）
    //==============================
    g_GoalPos = { 0.0f, -100.0f, 0.0f };
    g_GoalAabb = { { -0.01f, -101.0f, -0.01f }, { 0.01f, -99.0f, 0.01f } };

    //==============================
    // 通常エネミースポーンなし（ボス専用アリーナ）
    //==============================
    g_EnemySpawnPositions.clear();

    //==============================
    // MapObjects 再構築
    //==============================
    g_MapObjects.clear();
    BuildFloorCeilingCollidersFromTiles(tiles, mapW, mapH, CELL_SIZE, originX, originZ);

    //==============================
    // 壁 Plane（見た目）生成
    //==============================
    {
        TileWallParams par{};
        par.cellSize = CELL_SIZE;
        par.floorY = FLOOR_Y;
        par.wallHeight = WALL_HEIGHT;
        par.panelHeight = WALL_HEIGHT;
        par.uvUnitMeter = 1.0f;

        g_WallPlanes.clear();
        TileWall_BuildFromTiles(tiles, mapW, mapH, originX, originZ, par, g_WallPlanes);
        g_WallRenderer.Build(g_WallPlanes);
        BuildWallCollidersFromWallPlanes(g_WallPlanes, 0.15f);
    }

    //==============================
    // パトロール AI（8近傍がすべて床のタイルのみ）
    //==============================
    {
        std::vector<XMFLOAT3> patrolPositions;
        for (int ty = 1; ty < mapH - 1; ++ty)
        {
            for (int tx = 1; tx < mapW - 1; ++tx)
            {
                if (tiles[ty * mapW + tx] != MAP_TILE_FLOOR) continue;

                const bool n = (tiles[(ty - 1) * mapW + tx] == MAP_TILE_FLOOR);
                const bool s = (tiles[(ty + 1) * mapW + tx] == MAP_TILE_FLOOR);
                const bool w = (tiles[ty * mapW + (tx - 1)] == MAP_TILE_FLOOR);
                const bool e = (tiles[ty * mapW + (tx + 1)] == MAP_TILE_FLOOR);
                const bool nw = (tiles[(ty - 1) * mapW + (tx - 1)] == MAP_TILE_FLOOR);
                const bool ne = (tiles[(ty - 1) * mapW + (tx + 1)] == MAP_TILE_FLOOR);
                const bool sw = (tiles[(ty + 1) * mapW + (tx - 1)] == MAP_TILE_FLOOR);
                const bool se = (tiles[(ty + 1) * mapW + (tx + 1)] == MAP_TILE_FLOOR);
                if (!n || !s || !w || !e || !nw || !ne || !sw || !se) continue;

                patrolPositions.push_back(TileCoordToWorldCenter(
                    tx, ty, mapW, mapH, CELL_SIZE, originX, originZ, 1.0f));
            }
        }
        MapPatrolAI_Initialize(patrolPositions);
    }

    //==============================
    // ミニマップ用タイル
    //==============================
    {
        const float minimapY = 10.0f;
        for (int ty = 0; ty < mapH; ++ty)
            for (int tx = 0; tx < mapW; ++tx)
            {
                if (tiles[ty * mapW + tx] != MAP_TILE_FLOOR) continue;
                const XMFLOAT3 pos = TileCoordToWorldCenter(
                    tx, ty, mapW, mapH, CELL_SIZE, originX, originZ, minimapY);
                AddMapObject(KIND_MINIMAP_FLOOR, pos, Cube_CreateAABB(pos));
            }
        // ボス部屋にゴールマーカーは不要
    }
}

void Map_DrawForMinimap()
{
    Shader3d_Begin();
    Light_SetAmbient({ 1.0f, 1.0f, 1.0f });

    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_MINIMAP_FLOOR)
        {
            Shader3d_SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
        }
        else if (o.KindId == KIND_MINIMAP_GOAL)
        {
            Shader3d_SetColor({ 0.0f, 0.0f, 0.7f, 1.0f });
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y + 1, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
        }
    }

    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}

void Map_Light_Reset()
{

    //===================================================
    //ライト設定　(色を変えないので一括に)
    //===================================================
    float Ambient_power = 0.6f;
    float Directional_power = 0.3f;
    Shader3d_Begin();

    Light_SetAmbient({ Ambient_power, Ambient_power, Ambient_power });

    //Light_SetDirectionalWorld({ 0.0f, 1.0f, 0.0f, 0.0f }, { Directional_power, Directional_power, Directional_power, Directional_power });
    Light_SetDirectionalWorld({ 0.0f, -1.0f, 0.0f, 0.0f }, { Directional_power, Directional_power, Directional_power, Directional_power });

    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}

/*==============================================================================

   Map_DrawMinimap 修正版（ライト完全OFF）

   ■変更点
   ・Ambient（環境光）を1.0fにして完全に明るく
   ・Directional（平行光源）を完全に0に
   ・これで真っ暗にならない

==============================================================================*/

//==============================================================================
// テクスチャID取得
//==============================================================================
int Map_GetFloorTexID()
{
    return g_FloorTexID;
}

int Map_GetWiteTexID()
{
    return g_WhiteTexID;
}

/*==============================================================================

   マップ制御 [map.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/16
--------------------------------------------------------------------------------

==============================================================================*/

//==============================================================================
// 壁レイキャスト
//
// ■役割
// ・start-end 線分と KIND_WALL の AABB をスラブ法で交差判定する
// ・複数ヒットした場合は最も start に近い交差点を返す
//
// ■引数
// ・start     : 線分の始点（カメラ基準点 pivot）
// ・end       : 線分の終点（カメラの理想位置）
// ・outHitPos : 最近交差点の受け取りポインタ
//
// ■戻り値
// ・true  : 交差あり
// ・false : 交差なし
//==============================================================================
bool Map_RaycastWalls(
    const DirectX::XMFLOAT3& start,
    const DirectX::XMFLOAT3& end,
    DirectX::XMFLOAT3* outHitPos,
    bool wallsOnly)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float dz = end.z - start.z;

    float nearestT = FLT_MAX;
    bool  hit = false;

    for (const MapObject& obj : g_MapObjects)
    {
        if (wallsOnly)
        {
            if (obj.KindId != KIND_WALL) continue;
        }
        else
        {
            if (obj.KindId != KIND_WALL &&
                obj.KindId != KIND_FLOOR &&
                obj.KindId != KIND_CEILING) continue;
        }

        const AABB& a = obj.Aabb;

        float tMin = 0.0f;
        float tMax = 1.0f;

        // X軸スラブ
        if (fabsf(dx) < 1e-6f)
        {
            if (start.x < a.min.x || start.x > a.max.x) continue;
        }
        else
        {
            float t1 = (a.min.x - start.x) / dx;
            float t2 = (a.max.x - start.x) / dx;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) continue;
        }

        // Y軸スラブ
        if (fabsf(dy) < 1e-6f)
        {
            if (start.y < a.min.y || start.y > a.max.y) continue;
        }
        else
        {
            float t1 = (a.min.y - start.y) / dy;
            float t2 = (a.max.y - start.y) / dy;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) continue;
        }

        // Z軸スラブ
        if (fabsf(dz) < 1e-6f)
        {
            if (start.z < a.min.z || start.z > a.max.z) continue;
        }
        else
        {
            float t1 = (a.min.z - start.z) / dz;
            float t2 = (a.max.z - start.z) / dz;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) continue;
        }

        if (tMin < nearestT)
        {
            nearestT = tMin;
            hit = true;
        }
    }

    if (!hit) return false;

    outHitPos->x = start.x + dx * nearestT;
    outHitPos->y = start.y + dy * nearestT;
    outHitPos->z = start.z + dz * nearestT;

    return true;
}

//==============================================================================
// 壁越しでないか判定
//
// ■役割
// ・from から to へ壁レイキャストを飛ばし、途中に壁/床/天井があれば false
// ・ロックオン継続判定などに使用する
//
// ■引数
// ・from : 判定開始位置
// ・to   : 判定終了位置
//
// ■戻り値
// ・true  : 視線が通っている
// ・false : 途中に遮蔽物がある
//==============================================================================
bool Map_HasLineOfSight(
    const DirectX::XMFLOAT3& from,
    const DirectX::XMFLOAT3& to,
    bool wallsOnly)
{
    DirectX::XMFLOAT3 hitPos{};
    if (!Map_RaycastWalls(from, to, &hitPos, wallsOnly))
        return true;

    const float dxHit = hitPos.x - from.x;
    const float dyHit = hitPos.y - from.y;
    const float dzHit = hitPos.z - from.z;
    const float hitDistSq = dxHit * dxHit + dyHit * dyHit + dzHit * dzHit;

    const float dxTarget = to.x - from.x;
    const float dyTarget = to.y - from.y;
    const float dzTarget = to.z - from.z;
    const float targetDistSq = dxTarget * dxTarget + dyTarget * dyTarget + dzTarget * dzTarget;

    const float margin = 0.05f;

    return hitDistSq + margin * margin >= targetDistSq;
}


int Map_GetWallTexID()
{
    return g_WallTexID;
}

int Map_GetCeilingTexID()
{
    return g_CeilingTexID;
}

void Map_DrawMinimap()
{
    Shader3d_Begin();

    // ライトを明るめに
    Light_SetAmbient({ 0.8f, 0.8f, 0.8f });
    Light_SetDirectionalWorld({ 0.0f, -1.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f, 1.0f });
    Light_SetSpecularWorld({ 0,0,0 }, 0.0f, { 0,0,0,0 });

    // 床：緑色
    Shader3d_SetColor({ 0.3f, 1.0f, 0.3f, 1.0f });
    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_FLOOR)
        {
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, Map_GetFloorTexID(), CELL_SIZE);
        }
    }

    // 壁：濃いグレー
    Shader3d_SetColor({ 0.2f, 0.2f, 0.2f, 1.0f });
    g_WallRenderer.Draw(Map_GetWallTexID());

    // ゴール：黄色
    Shader3d_SetColor({ 1.0f, 1.0f, 0.2f, 1.0f });
    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_MINIMAP_GOAL)
        {
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
        }
    }

    // 色を元に戻す
    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}