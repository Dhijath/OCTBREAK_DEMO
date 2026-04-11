/*==============================================================================

   マップ制御 新実装 [map2.cpp]

   ■map.cpp との違い
   ・タイル生成を DungeonGen に委譲（GenerateDungeon / GenerateBossRoom が短い）
   ・g_MinimapObjects を g_MapObjects から完全分離
   ・8近傍チェックのコピペを DungeonGen_IsOpenTile に一本化
   ・Map_Draw() にミニマップ描画を混ぜない

   ■切り替え方
   ・プロジェクトから map.cpp を除外して map2.cpp を追加するだけ
   ・map.h は変更なし

==============================================================================*/

#include "map.h"
#include "DungeonGen.h"

#include "collision.h"
#include "cube.h"
#include "texture.h"
#include "Light.h"
#include "Meshfield.h"
#include "Player_Camera.h"
#include "shader3d.h"
#include "FloorRegistry.h"
#include "direct3d.h"
#include "player.h"
#include "billboard.h"
#include "TileWall.h"
#include "WallPlaneRenderer.h"
#include "MapPatrolAI.h"
#include "WallShader.h"

#include <DirectXMath.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <chrono>

using namespace DirectX;

//==============================================================================
// 内部
//==============================================================================
namespace
{

//--------------------------------------------------------------------------
// MapObject.KindId
//--------------------------------------------------------------------------
enum : int
{
    KIND_FLOOR   = 1,
    KIND_WALL    = 2,
    KIND_TREE    = 3,
    KIND_CEILING = 4,
    KIND_GOAL    = 5,

    KIND_MINIMAP_FLOOR = 101,
    KIND_MINIMAP_GOAL  = 103,
};

//--------------------------------------------------------------------------
// 内部状態
//--------------------------------------------------------------------------
std::vector<MapObject> g_MapObjects;      // 床/天井/壁/ゴール（衝突・描画）
std::vector<MapObject> g_MinimapObjects;  // ミニマップ専用（g_MapObjects に混ぜない）
std::vector<WallPlane> g_WallPlanes;
WallPlaneRenderer      g_WallRenderer;

std::vector<XMFLOAT3>  g_EnemySpawnPositions;

int g_FloorTexID   = -1;
int g_WallTexID    = -1;
int g_CeilingTexID = -1;
int g_GoalTexID    = -1;
int g_WhiteTexID   = -1;
int g_TreeTexID    = -1;

XMFLOAT3 g_SpawnPos     = { 0.f, 1.5f, 0.f };
XMFLOAT3 g_GoalPos      = { -1.f, 1.f, 0.f };
XMFLOAT3 g_BossSpawnPos = { 0.f, 1.5f, 0.f };
AABB     g_GoalAabb     = {};

int g_GoalReachCount = 0;
int g_EnemySpawnRate = 10;

constexpr float CELL_SIZE        = 1.0f;
constexpr float FLOOR_Y          = 0.5f;
constexpr float WALL_HEIGHT      = 5.0f;
constexpr float CEILING_CENTER_Y = FLOOR_Y + WALL_HEIGHT;
constexpr int   GOAL_REQUIRED    = 2;

constexpr float ORIGIN_X = 0.5f;
constexpr float ORIGIN_Z = 0.5f;

//--------------------------------------------------------------------------
// タイル座標 → ワールド中心座標
//--------------------------------------------------------------------------
static XMFLOAT3 TileToWorld(int tx, int ty, int W, int H, float y)
{
    return {
        ORIGIN_X + (tx - W * 0.5f + 0.5f) * CELL_SIZE,
        y,
        ORIGIN_Z + (ty - H * 0.5f + 0.5f) * CELL_SIZE
    };
}

//--------------------------------------------------------------------------
// MapObject / MinimapObject 追加
//--------------------------------------------------------------------------
static void AddMapObj(int kind, const XMFLOAT3& pos, const AABB& aabb)
{
    g_MapObjects.push_back({ kind, pos, aabb });
}
static void AddMinimapObj(int kind, const XMFLOAT3& pos, const AABB& aabb)
{
    g_MinimapObjects.push_back({ kind, pos, aabb });
}

//--------------------------------------------------------------------------
// 床・天井のAABB構築
//--------------------------------------------------------------------------
static void BuildFloorCeiling(const DungeonData& d)
{
    const int W = d.width, H = d.height;
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
        {
            if (d.tiles[ty*W + tx] != DGEN_FLOOR) continue;

            const XMFLOAT3 fp = TileToWorld(tx, ty, W, H, FLOOR_Y);
            AddMapObj(KIND_FLOOR, fp, Cube_CreateAABB(fp));

            const XMFLOAT3 cp = TileToWorld(tx, ty, W, H, CEILING_CENTER_Y);
            AddMapObj(KIND_CEILING, cp, Cube_CreateAABB(cp));
        }
}

//--------------------------------------------------------------------------
// 壁Planeから衝突AABB構築
//--------------------------------------------------------------------------
static void BuildWallColliders(const std::vector<WallPlane>& planes, float thickness)
{
    const float halfT = thickness * 0.5f;
    for (const WallPlane& pl : planes)
    {
        const XMFLOAT3 c   = pl.center;
        const float halfH  = pl.height * 0.5f;
        const float halfW  = pl.width  * 0.5f;
        const bool normalX = (fabsf(pl.normal.x) > 0.5f);

        AABB a{};
        a.min.y = c.y - halfH;
        a.max.y = c.y + halfH;

        if (normalX)
        {
            a.min.x = c.x - halfT; a.max.x = c.x + halfT;
            a.min.z = c.z - halfW; a.max.z = c.z + halfW;
        }
        else
        {
            a.min.z = c.z - halfT; a.max.z = c.z + halfT;
            a.min.x = c.x - halfW; a.max.x = c.x + halfW;
        }
        AddMapObj(KIND_WALL, c, a);
    }
}

//--------------------------------------------------------------------------
// 壁Plane生成 + 衝突AABB生成（通常/ボス共通）
//--------------------------------------------------------------------------
static void BuildWallGeometry(const DungeonData& d)
{
    TileWallParams par{};
    par.cellSize    = CELL_SIZE;
    par.floorY      = FLOOR_Y;
    par.wallHeight  = WALL_HEIGHT;
    par.panelHeight = WALL_HEIGHT;
    par.uvUnitMeter = 1.0f;

    g_WallPlanes.clear();
    TileWall_BuildFromTiles(
        d.tiles, d.width, d.height,
        ORIGIN_X, ORIGIN_Z, par, g_WallPlanes);
    g_WallRenderer.Build(g_WallPlanes);
    BuildWallColliders(g_WallPlanes, 0.15f);
}

//--------------------------------------------------------------------------
// ゴール位置・AABB・ボス位置を確定する
// ・gx,gy: ゴールを置くタイル座標
//--------------------------------------------------------------------------
static void ComputeGoal(int gx, int gy, int W, int H)
{
    constexpr float billW  = 1.0f, billH  = 1.5f;
    constexpr float pivotX = 0.5f, pivotY = 0.0f;

    g_GoalPos      = TileToWorld(gx, gy, W, H, 1.6f);
    g_BossSpawnPos = TileToWorld(gx, gy, W, H, 1.5f);

    const XMFLOAT3 hit = TileToWorld(gx, gy, W, H, 1.0f);
    g_GoalAabb.min = {
        hit.x - billW * pivotX,
        hit.y - billH * pivotY,
        hit.z - 0.5f
    };
    g_GoalAabb.max = {
        hit.x + billW * (1.f - pivotX),
        hit.y + billH * (1.f - pivotY),
        hit.z + 0.5f
    };
}

//--------------------------------------------------------------------------
// エネミースポーン・パトロール位置計算
// ・g_SpawnPos / g_BossSpawnPos が確定している前提で呼ぶ
//--------------------------------------------------------------------------
static void ComputeSpawns(const DungeonData& d, std::mt19937& rng)
{
    const int W = d.width, H = d.height;
    g_EnemySpawnPositions.clear();

    std::vector<XMFLOAT3> candidates;
    std::vector<XMFLOAT3> patrolPos;

    for (int ty = 1; ty < H-1; ++ty)
        for (int tx = 1; tx < W-1; ++tx)
        {
            if (d.tiles[ty*W + tx] != DGEN_FLOOR) continue;
            if (!DungeonGen_IsOpenTile(d.tiles, W, tx, ty)) continue;

            const XMFLOAT3 pos = TileToWorld(tx, ty, W, H, 1.0f);
            patrolPos.push_back(pos);

            const float dx = pos.x - g_SpawnPos.x;
            const float dz = pos.z - g_SpawnPos.z;
            if (dx*dx + dz*dz <= 15.f*15.f) continue;  // スポーン付近を除外

            const float bdx = pos.x - g_BossSpawnPos.x;
            const float bdz = pos.z - g_BossSpawnPos.z;
            if (bdx*bdx + bdz*bdz < 7.f*7.f) continue;  // ボスルームを除外

            candidates.push_back(pos);
        }

    if (!candidates.empty())
    {
        const int count = std::min(
            (int)candidates.size() / g_EnemySpawnRate,
            (int)d.rooms.size() * 2);
        std::shuffle(candidates.begin(), candidates.end(), rng);
        g_EnemySpawnPositions.assign(candidates.begin(), candidates.begin() + count);
    }

    MapPatrolAI_Initialize(patrolPos);
}

//--------------------------------------------------------------------------
// ミニマップ用オブジェクト構築
// ・addGoalMarker = false のときはゴールマーカーを置かない（ボス部屋用）
//--------------------------------------------------------------------------
static void BuildMinimapObjects(const DungeonData& d, bool addGoalMarker)
{
    const int W = d.width, H = d.height;
    constexpr float minimapY = 10.0f;

    g_MinimapObjects.clear();

    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
        {
            if (d.tiles[ty*W + tx] != DGEN_FLOOR) continue;
            const XMFLOAT3 pos = TileToWorld(tx, ty, W, H, minimapY);
            AddMinimapObj(KIND_MINIMAP_FLOOR, pos, Cube_CreateAABB(pos));
        }

    if (addGoalMarker)
    {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const XMFLOAT3 pos = {
                    g_GoalPos.x + dx * CELL_SIZE,
                    minimapY,
                    g_GoalPos.z + dy * CELL_SIZE
                };
                AddMinimapObj(KIND_MINIMAP_GOAL, pos, Cube_CreateAABB(pos));
            }
    }
}

} // namespace

//==============================================================================
// Map_GenerateRandomSeed
//==============================================================================
std::uint32_t Map_GenerateRandomSeed()
{
    std::random_device rd;
    const auto now     = std::chrono::high_resolution_clock::now();
    const auto millis  = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch()).count();
    return static_cast<std::uint32_t>(millis) ^ rd();
}

//==============================================================================
// Map_Initialize
//==============================================================================
void Map_Initialize()
{
    g_FloorTexID   = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_WallTexID    = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_CeilingTexID = Texture_Load(L"Resource/Texture/wall6.jpg");
    g_TreeTexID    = Texture_Load(L"Resource/Texture/tree.png");
    g_GoalTexID    = Texture_Load(L"Resource/Texture/goal.png");
    g_WhiteTexID   = Texture_Load(L"Resource/Texture/white.png");

    Map_GenerateDungeon(Map_GenerateRandomSeed());
}

//==============================================================================
// Map_Finalize
//==============================================================================
void Map_Finalize()
{
    // WallPlaneRenderer にリソースがあれば解放
    // g_WallRenderer.Release();
}

//==============================================================================
// Map_GenerateDungeon
//==============================================================================
void Map_GenerateDungeon(std::uint32_t seed)
{
    //----------------------------------------------------------
    // タイル生成（DungeonGen に委譲）
    //----------------------------------------------------------
    DungeonParams params;
    // DungeonParams はデフォルト値が map.cpp の constexpr と一致している
    // 変えたい場合はここで上書きする
    //   params.width = 79; params.corridorW = 5; など

    DungeonData d = DungeonGen_Generate(seed, params);
    const int W = d.width, H = d.height;

    std::mt19937 rng(seed);  // スポーンのシャッフルに使う

    //----------------------------------------------------------
    // ゴール位置確定（スポーン rooms[0] から最遠の部屋）
    // ・ボスルームと同じ部屋になるのは意図的（ボス部屋にゴールがある）
    //----------------------------------------------------------
    {
        const int spawnCX = d.rooms[0].cx();
        const int spawnCY = d.rooms[0].cy();
        int gx = d.rooms.back().cx();
        int gy = d.rooms.back().cy();
        float bestDsq = -1.f;
        for (const auto& r : d.rooms)
        {
            const float dx = float(r.cx() - spawnCX);
            const float dz = float(r.cy() - spawnCY);
            const float dsq = dx*dx + dz*dz;
            if (dsq > bestDsq) { bestDsq = dsq; gx = r.cx(); gy = r.cy(); }
        }
        ComputeGoal(gx, gy, W, H);
    }

    //----------------------------------------------------------
    // プレイヤースポーン確定
    //----------------------------------------------------------
    g_SpawnPos = TileToWorld(d.rooms[0].cx(), d.rooms[0].cy(), W, H, 1.5f);

    //----------------------------------------------------------
    // コライダー構築
    //----------------------------------------------------------
    g_MapObjects.clear();
    g_MapObjects.reserve(W * H * 2);

    BuildFloorCeiling(d);
    BuildWallGeometry(d);

    AddMapObj(KIND_GOAL, g_GoalPos, g_GoalAabb);

    //----------------------------------------------------------
    // スポーン・パトロール計算
    //----------------------------------------------------------
    ComputeSpawns(d, rng);

    //----------------------------------------------------------
    // ミニマップ
    //----------------------------------------------------------
    BuildMinimapObjects(d, true);
}

//==============================================================================
// Map_GenerateBossRoom
//==============================================================================
void Map_GenerateBossRoom(std::uint32_t /*seed*/)
{
    DungeonData d = DungeonGen_GenerateBossArena(29, 29);
    const int W = d.width, H = d.height;

    //----------------------------------------------------------
    // スポーン / ボス位置（アリーナの手前・奥）
    //----------------------------------------------------------
    constexpr int roomY1 = 2, roomY2 = 26;  // GenerateBossArena と一致
    g_SpawnPos     = TileToWorld(W/2, roomY1 + 2, W, H, 1.5f);
    g_BossSpawnPos = TileToWorld(W/2, roomY2 - 2, W, H, 1.5f);

    //----------------------------------------------------------
    // ゴールを無効化（地下に沈める）
    //----------------------------------------------------------
    g_GoalPos  = { 0.f, -100.f, 0.f };
    g_GoalAabb = { { -0.01f, -101.f, -0.01f }, { 0.01f, -99.f, 0.01f } };

    //----------------------------------------------------------
    // エネミースポーンなし
    //----------------------------------------------------------
    g_EnemySpawnPositions.clear();

    //----------------------------------------------------------
    // コライダー構築
    //----------------------------------------------------------
    g_MapObjects.clear();
    BuildFloorCeiling(d);
    BuildWallGeometry(d);

    //----------------------------------------------------------
    // パトロール
    //----------------------------------------------------------
    {
        std::vector<XMFLOAT3> patrolPos;
        for (int ty = 1; ty < H-1; ++ty)
            for (int tx = 1; tx < W-1; ++tx)
            {
                if (d.tiles[ty*W + tx] != DGEN_FLOOR) continue;
                if (!DungeonGen_IsOpenTile(d.tiles, W, tx, ty)) continue;
                patrolPos.push_back(TileToWorld(tx, ty, W, H, 1.0f));
            }
        MapPatrolAI_Initialize(patrolPos);
    }

    //----------------------------------------------------------
    // ミニマップ（ゴールマーカーなし）
    //----------------------------------------------------------
    BuildMinimapObjects(d, false);
}

//==============================================================================
// Map_RegisterFloors
//==============================================================================
void Map_RegisterFloors()
{
    FloorRegistry::Clear();
    for (const MapObject& o : g_MapObjects)
        if (o.KindId == KIND_FLOOR)
            FloorRegistry::Add(o.Aabb);
}

//==============================================================================
// Map_Draw
//==============================================================================
void Map_Draw()
{
    Map_Light_Reset();

    for (const MapObject& o : g_MapObjects)
    {
        if (o.KindId == KIND_FLOOR)
        {
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_FloorTexID, CELL_SIZE);
        }
        else if (o.KindId == KIND_CEILING)
        {
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTileCeiling(world, g_CeilingTexID, CELL_SIZE);
        }
        else if (o.KindId == KIND_TREE)
        {
            Billboard_Draw(g_TreeTexID, o.Position, { 2.f, 3.f }, { 0.5f, 1.f });
        }
    }

    g_WallRenderer.Draw(g_WallTexID);

    Map_Light_Reset();
}

//==============================================================================
// Map_DrawGoal
// ・全不透明モデルを描いた後に呼ぶ（透過合成順序のため）
//==============================================================================
void Map_DrawGoal()
{
    if (g_GoalTexID < 0) return;

    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    ID3D11BlendState* prevBS = nullptr;
    FLOAT prevFactor[4];
    UINT  prevMask;
    ctx->OMGetBlendState(&prevBS, prevFactor, &prevMask);

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend             = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend            = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp              = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha        = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha       = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha         = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* alphaBS = nullptr;
    Direct3D_GetDevice()->CreateBlendState(&bd, &alphaBS);
    ctx->OMSetBlendState(alphaBS, nullptr, 0xFFFFFFFF);

    Billboard_Draw(g_GoalTexID, g_GoalPos, { 1.f, 1.5f }, { 0.5f, 0.f });

    ctx->OMSetBlendState(prevBS, prevFactor, prevMask);
    SAFE_RELEASE(alphaBS);
    SAFE_RELEASE(prevBS);
}

//==============================================================================
// Map_DrawForMinimap / Map_DrawMinimap
//==============================================================================
void Map_DrawForMinimap()
{
    Shader3d_Begin();
    Light_SetAmbient({ 1.f, 1.f, 1.f });

    for (const MapObject& o : g_MinimapObjects)
    {
        if (o.KindId == KIND_MINIMAP_FLOOR)
        {
            Shader3d_SetColor({ 0.5f, 0.5f, 0.5f, 1.f });
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
        }
        else if (o.KindId == KIND_MINIMAP_GOAL)
        {
            Shader3d_SetColor({ 0.f, 0.f, 0.7f, 1.f });
            const XMMATRIX world = XMMatrixTranslation(o.Position.x, o.Position.y + 1.f, o.Position.z);
            MeshField_DrawTile(world, g_WhiteTexID, CELL_SIZE);
        }
    }

    Shader3d_SetColor({ 1.f, 1.f, 1.f, 1.f });
}

void Map_DrawMinimap()
{
    Map_DrawForMinimap();
}

//==============================================================================
// Map_Light_Reset
//==============================================================================
void Map_Light_Reset()
{
    Shader3d_Begin();
    Light_SetAmbient({ 0.6f, 0.6f, 0.6f });
    Light_SetDirectionalWorld({ 0.f, -1.f, 0.f, 0.f }, { 0.3f, 0.3f, 0.3f, 0.3f });
    Shader3d_SetColor({ 1.f, 1.f, 1.f, 1.f });
}

//==============================================================================
// Map_SetCeilingTexture
//==============================================================================
void Map_SetCeilingTexture(int texId)
{
    g_CeilingTexID = texId;
}

//==============================================================================
// スポーン / ゴール / ボス取得
//==============================================================================
DirectX::XMFLOAT3 Map_GetSpawnPosition()     { return g_SpawnPos; }
DirectX::XMFLOAT3 Map_GetGoalPosition()      { return g_GoalPos; }
DirectX::XMFLOAT3 Map_GetBossSpawnPosition() { return g_BossSpawnPos; }

const std::vector<DirectX::XMFLOAT3>& Map_GetEnemySpawnPositions()
{
    return g_EnemySpawnPositions;
}

//==============================================================================
// ゴール判定
//==============================================================================
bool Map_IsPlayerReachedGoal()
{
    return Collision_IsOverLapAABB(Player_GetAABB(), g_GoalAabb);
}

//==============================================================================
// ゴール到達カウント
//==============================================================================
void Map_AddGoalReachCount()
{
    g_GoalReachCount++;
    g_EnemySpawnRate = std::max(3, g_EnemySpawnRate - 2);
}

int  Map_GetGoalReachCount()  { return g_GoalReachCount; }
void Map_ResetGoalReachCount(){ g_GoalReachCount = 0; }

bool Map_IsClearConditionMet()
{
    return g_GoalReachCount >= GOAL_REQUIRED;
}

//==============================================================================
// MapObject アクセス（衝突用。ミニマップオブジェクトは含まない）
//==============================================================================
int Map_GetObjectsCount()
{
    return static_cast<int>(g_MapObjects.size());
}

const MapObject* Map_GetObject(int index)
{
    return &g_MapObjects[index];
}

//==============================================================================
// テクスチャID取得
//==============================================================================
int Map_GetFloorTexID()   { return g_FloorTexID; }
int Map_GetWallTexID()    { return g_WallTexID; }
int Map_GetCeilingTexID() { return g_CeilingTexID; }
int Map_GetWiteTexID()    { return g_WhiteTexID; }

//==============================================================================
// 壁レイキャスト（スラブ法）
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
    bool  hit      = false;

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
        float tMin = 0.f, tMax = 1.f;

        // X スラブ
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

        // Y スラブ
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

        // Z スラブ
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
            hit      = true;
        }
    }

    if (hit && outHitPos)
    {
        outHitPos->x = start.x + dx * nearestT;
        outHitPos->y = start.y + dy * nearestT;
        outHitPos->z = start.z + dz * nearestT;
    }

    return hit;
}

//==============================================================================
// 視線通過判定
//==============================================================================
bool Map_HasLineOfSight(
    const DirectX::XMFLOAT3& from,
    const DirectX::XMFLOAT3& to,
    bool wallsOnly)
{
    return !Map_RaycastWalls(from, to, nullptr, wallsOnly);
}
