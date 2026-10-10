/*==============================================================================

   ブロックステージ [BlockStage.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ■このファイルがやること
   ・BlockStageBuilder の API（Begin / Solid / Deco / Glow / Prop ...）を実装し、
     配置物を「描画リスト」と「map.cpp の当たり判定（MapObject）」に登録する
   ・配置が終わったら、空いている地面から巡回点と敵スポーンを自動で決める
   ・ステージ本体 / ゴールビーコン / ミニマップの描画
     （本体とゴールは BlockStageRender、ミニマップは既存の Shader3d で描く）

   ■当たり判定の作り方（既存の判定コードに合わせている）
   ・地面   : 厚さ2mの床1枚
   ・Solid  : 側面＝壁（上端を 2cm 下げる）＋ 上面＝床（外周を 10cm 内側へ）
              → 屋上に立っているとき壁判定に引っかからず、
                壁際の敵が屋上の床を拾って乗り上げることもない
   ・外周   : 飛行上限まで伸びる壁4枚
   ・上限   : 天井1枚（プレイヤーの上昇と弾を止める）

==============================================================================*/
#include "BlockStage.h"
#include "BlockStageBuilder.h"
#include "BlockStageRender.h"
#include "map.h"
#include "cube.h"
#include "meshfield.h"
#include "shader3d.h"
#include "light.h"
#include "direct3d.h"
#include "player_camera.h"
#include "MapPatrolAI.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

using namespace DirectX;
using BlockStageBuilder::Col;
using BlockStageBuilder::Theme;

namespace
{
    //==========================================================================
    // 定数
    //==========================================================================
    constexpr float CAP_HEIGHT     = 30.0f;  // 飛行上限（地面からの高さ）
    constexpr float BOUND_THICK    = 2.0f;   // 外周壁の厚み
    constexpr float WALL_TOP_GAP   = 0.02f;  // 壁判定の上端を下げる量
    constexpr float ROOF_INSET     = 0.10f;  // 屋上の床判定を内側へ寄せる量
    constexpr float ROOF_THICKNESS = 1.0f;   // 屋上の床判定の厚み（高速落下でもすり抜けない厚さ）

    constexpr float PATROL_STEP      = 2.0f; // 巡回点の格子間隔
    constexpr float PATROL_CLEARANCE = 0.9f; // 巡回点と構造物の最低距離
    constexpr float ENEMY_SPACING    = 4.0f; // 敵スポーン同士の最低距離
    constexpr float BOSS_CLEAR       = 8.0f; // ボス出現位置の周囲に敵を置かない半径

    constexpr float GOAL_HALF = 1.3f;        // ゴール判定の半径（XZ）

    // 光のにじみ：発光体を一回り大きくした外殻を2枚重ねる（内側は濃く、外側は薄く）
    constexpr float GLOW_PAD_INNER      = 0.30f;
    constexpr float GLOW_PAD_OUTER      = 0.90f;
    constexpr float GLOW_STRENGTH_INNER = 0.20f;
    constexpr float GLOW_STRENGTH_OUTER = 0.07f;
    constexpr float GLOW_MIN_HEIGHT     = 0.10f;  // これより薄い発光体（路面のライン）にはにじみを付けない

    // ミニマップ用の高さ（既存のマーカーが Y=11 付近にあるので、その下に収める）
    constexpr float MINI_GROUND_Y = 9.0f;
    constexpr float MINI_DECAL_Y  = 9.1f;
    constexpr float MINI_BLOCK_Y  = 9.2f;
    constexpr float MINI_GOAL_Y   = 10.2f;

    //==========================================================================
    // 内部状態
    //==========================================================================
    struct Block
    {
        StageMesh mesh;
        XMFLOAT3  center;
        XMFLOAT3  size;
        XMFLOAT3  rot;       // 初期の向き（ラジアン）
        XMFLOAT3  spin;      // 毎秒の回転量
        Col       color;
        float     emissive;
        bool      halo;      // 光のにじみを付けるか
    };

    // ミニマップに描く矩形
    struct MiniRect
    {
        float cx, cz, sx, sz;
        float top;     // 地面からの高さ（構造物のみ。濃淡に使う）
        Col   color;   // 路面の色（Deco のみ）
        bool  solid;
    };

    // 当たり判定のある構造物の底面（巡回点・敵スポーンを置けない範囲）
    struct Footprint
    {
        float minX, maxX, minZ, maxZ;
        float minY, maxY;   // 高さの範囲（上の階の床板の下は、地面の巡回点を妨げない）
    };

    // 上の階の床（巡回点を床板の上にも置く）
    struct PlatformRect
    {
        float minX, maxX, minZ, maxZ;
        float top;          // 床板の上面（ワールド座標）
    };
    std::vector<PlatformRect> g_Platforms;

    bool  g_Active  = false;
    float g_GroundY = 1.0f;   // 地面の上面Y（既存マップと同じ高さ）
    float g_HalfX   = 0.0f;
    float g_HalfZ   = 0.0f;
    Theme g_Theme{};

    std::vector<Block>     g_Blocks;
    std::vector<MiniRect>  g_MiniRects;
    std::vector<Footprint> g_Footprints;

    XMFLOAT2 g_Spawn = { 0.0f, 0.0f };
    XMFLOAT2 g_Boss  = { 0.0f, 0.0f };
    bool     g_HasBoss = false;
    bool     g_HasGoal = false;
    XMFLOAT3 g_GoalPos = { 0.0f, 0.0f, 0.0f };   // ゴール足場の中心（ワールド座標）
    float    g_EnemyMinDist = 20.0f;

    float g_AnimTime = 0.0f;   // 回転する配置物・ゴールの明滅用

    // 部隊配置の指定
    struct SquadRequest
    {
        float x, z, radius;
        int   count;
        int   type;
        float level;   // 地面からの高さ（上の階の部隊）
    };
    std::vector<SquadRequest> g_Squads;
    std::vector<XMFLOAT2>     g_DropZones;     // 増援の降下地点
    std::vector<XMFLOAT3>     g_Patrol;        // 巡回点（増援の出現位置選びにも使う）
    std::vector<int>          g_SpawnTypes;    // 敵スポーンごとの種別（-1 = 編成の比率で決める）

    constexpr float SQUAD_MIN_SPAWN_DIST = 10.0f;  // 部隊をプレイヤー開始位置から離す最低距離
    constexpr float SQUAD_SPACING        = 1.6f;   // 部隊内の間隔
    constexpr float DROP_MIN_PLAYER_DIST = 25.0f;  // 増援はプレイヤーからこれ以上離れた降下地点から出す
    constexpr float DROP_SCATTER         = 6.0f;   // 降下地点のまわりに散らす半径

    // true の間は map.cpp / 巡回AI に一切触れない（見取り図の取得用）
    bool g_PreviewMode = false;

    // false のときステージ設計の部隊を置かない（同じステージでのボス戦フェーズなど）
    bool g_PlaceSquads = true;

    //==========================================================================
    // 登録ヘルパー
    //==========================================================================
    void AddCollider(int kind, const XMFLOAT3& mn, const XMFLOAT3& mx)
    {
        if (g_PreviewMode) return;

        AABB a{};
        a.min = mn;
        a.max = mx;
        Map_Internal_AddObject(kind, a.GetCenter(), a);
    }

    // 箱の当たり判定（側面＝壁、上面＝床）と、敵を置けない範囲を登録する
    void AddBoxCollider(float cx, float cz, float sx, float sz, float y0, float y1)
    {
        const float hx = sx * 0.5f;
        const float hz = sz * 0.5f;

        AddCollider(Map_Internal_KindWall(),
            { cx - hx, y0, cz - hz }, { cx + hx, y1 - WALL_TOP_GAP, cz + hz });

        // 細い柱には乗れないので床は作らない
        if (hx > ROOF_INSET + 0.1f && hz > ROOF_INSET + 0.1f)
        {
            AddCollider(Map_Internal_KindFloor(),
                { cx - hx + ROOF_INSET, std::max(y0, y1 - ROOF_THICKNESS), cz - hz + ROOF_INSET },
                { cx + hx - ROOF_INSET, y1,                                cz + hz - ROOF_INSET });
        }

        g_Footprints.push_back({ cx - hx, cx + hx, cz - hz, cz + hz, y0, y1 });
    }

    void AddBlock(StageMesh mesh, const XMFLOAT3& center, const XMFLOAT3& size,
                  const Col& color, float emissive, bool halo,
                  const XMFLOAT3& rot = { 0.0f, 0.0f, 0.0f }, const XMFLOAT3& spin = { 0.0f, 0.0f, 0.0f })
    {
        g_Blocks.push_back({ mesh, center, size, rot, spin, color, emissive, halo });
    }

    // 高さ y に立つ点 (x,z) が、その高さの構造物から clearance 以上離れているか
    //（体の高さ 1.6m と重なる構造物だけを見る。上の階の床板の下は通れる）
    bool IsOpenGround(float x, float z, float clearance, float y)
    {
        for (const Footprint& f : g_Footprints)
        {
            if (f.maxY <= y + 0.05f || f.minY >= y + 1.6f) continue;
            if (x > f.minX - clearance && x < f.maxX + clearance &&
                z > f.minZ - clearance && z < f.maxZ + clearance)
                return false;
        }
        return true;
    }

    float DistXZ(float ax, float az, float bx, float bz)
    {
        const float dx = ax - bx;
        const float dz = az - bz;
        return sqrtf(dx * dx + dz * dz);
    }

    //==========================================================================
    // 巡回点と敵スポーンを決める（配置がすべて終わってから呼ぶ）
    //==========================================================================
    // candidates から spacing 以上離れた点を count 個選んで chosen に足す。
    // 足りなければ間隔を無視して埋める。選んだ点は candidates から取り除く
    void ChooseSpread(std::vector<XMFLOAT3>& candidates, int count, float spacing,
                      std::vector<XMFLOAT3>& chosen)
    {
        std::vector<XMFLOAT3> picked;
        std::vector<bool>     used(candidates.size(), false);
        for (int pass = 0; pass < 2; ++pass)
        {
            for (size_t i = 0; i < candidates.size(); ++i)
            {
                if (static_cast<int>(picked.size()) >= count) break;
                if (used[i]) continue;

                const XMFLOAT3& c = candidates[i];
                bool ok = true;
                if (pass == 0)
                {
                    for (const XMFLOAT3& o : chosen)
                        if (DistXZ(c.x, c.z, o.x, o.z) < spacing) { ok = false; break; }
                    for (const XMFLOAT3& o : picked)
                        if (DistXZ(c.x, c.z, o.x, o.z) < spacing) { ok = false; break; }
                }
                if (!ok) continue;

                used[i] = true;
                picked.push_back(c);
            }
        }

        std::vector<XMFLOAT3> rest;
        for (size_t i = 0; i < candidates.size(); ++i)
            if (!used[i]) rest.push_back(candidates[i]);
        candidates.swap(rest);

        chosen.insert(chosen.end(), picked.begin(), picked.end());
    }

    //==========================================================================
    // 巡回点と敵スポーンを決める（配置がすべて終わってから呼ぶ）
    // ・まずステージが指定した部隊を所定の位置に置き（種別も指定どおり）、
    // ・残りの enemyCount 体を空いている場所に散らす（種別は編成の比率で決まる）
    //==========================================================================
    void PlaceEnemies(std::uint32_t seed, int enemyCount)
    {
        // 巡回点：構造物から離れた地面の格子点＋上の階の床板の上の格子点
        g_Patrol.clear();
        for (float z = -g_HalfZ + PATROL_STEP; z <= g_HalfZ - PATROL_STEP; z += PATROL_STEP)
            for (float x = -g_HalfX + PATROL_STEP; x <= g_HalfX - PATROL_STEP; x += PATROL_STEP)
                if (IsOpenGround(x, z, PATROL_CLEARANCE, g_GroundY))
                    g_Patrol.push_back({ x, g_GroundY, z });

        for (const PlatformRect& pf : g_Platforms)
        {
            // 縁から 1m 内側だけ（落ちない位置）
            for (float z = pf.minZ + 1.0f; z <= pf.maxZ - 1.0f; z += PATROL_STEP)
                for (float x = pf.minX + 1.0f; x <= pf.maxX - 1.0f; x += PATROL_STEP)
                    if (IsOpenGround(x, z, PATROL_CLEARANCE, pf.top))
                        g_Patrol.push_back({ x, pf.top, z });
        }

        if (!g_PreviewMode)
            MapPatrolAI_Initialize(g_Patrol);

        std::mt19937 rng(seed);
        std::vector<XMFLOAT3> chosen;
        g_SpawnTypes.clear();

        // 部隊（ステージ設計どおりの位置と種別）。ボス戦などで部隊を置かないフェーズは省く
        for (const SquadRequest& sq : g_Squads)
        {
            if (!g_PlaceSquads) break;

            std::vector<XMFLOAT3> nearPoints;
            for (const XMFLOAT3& p : g_Patrol)
            {
                if (DistXZ(p.x, p.z, sq.x, sq.z) > sq.radius) continue;
                if (std::fabs(p.y - (g_GroundY + sq.level)) > 0.5f) continue;   // 指定の階だけ
                if (DistXZ(p.x, p.z, g_Spawn.x, g_Spawn.y) < SQUAD_MIN_SPAWN_DIST) continue;
                nearPoints.push_back(p);
            }
            std::shuffle(nearPoints.begin(), nearPoints.end(), rng);

            const size_t before = chosen.size();
            ChooseSpread(nearPoints, sq.count, SQUAD_SPACING, chosen);
            for (size_t i = before; i < chosen.size(); ++i)
                g_SpawnTypes.push_back(sq.type);
        }

        // 散らばった哨戒兵：プレイヤー開始位置とボスから離れた巡回点
        std::vector<XMFLOAT3> candidates;
        for (const XMFLOAT3& p : g_Patrol)
        {
            if (DistXZ(p.x, p.z, g_Spawn.x, g_Spawn.y) < g_EnemyMinDist) continue;
            if (g_HasBoss && DistXZ(p.x, p.z, g_Boss.x, g_Boss.y) < BOSS_CLEAR) continue;
            candidates.push_back(p);
        }
        std::shuffle(candidates.begin(), candidates.end(), rng);

        const size_t before = chosen.size();
        ChooseSpread(candidates, enemyCount, ENEMY_SPACING, chosen);
        for (size_t i = before; i < chosen.size(); ++i)
            g_SpawnTypes.push_back(-1);

        if (!g_PreviewMode)
            for (const XMFLOAT3& p : chosen)
                Map_Internal_AddEnemySpawn(p);
    }

    //==========================================================================
    // 描画ヘルパー
    //==========================================================================
    XMMATRIX BlockWorld(const Block& b, float pad)
    {
        return XMMatrixScaling(b.size.x + pad, b.size.y + pad, b.size.z + pad) *
               XMMatrixRotationRollPitchYaw(b.rot.x + b.spin.x * g_AnimTime,
                                            b.rot.y + b.spin.y * g_AnimTime,
                                            b.rot.z + b.spin.z * g_AnimTime) *
               XMMatrixTranslation(b.center.x, b.center.y, b.center.z);
    }

    StageFrame MakeFrame()
    {
        StageFrame f{};
        f.view      = Player_Camera_GetViewMatrix();
        f.proj      = Player_Camera_GetProjectionMatrix();
        f.camPos    = Player_Camera_GetPosition();
        f.time      = g_AnimTime;
        f.skyTop    = g_Theme.skyTop;
        f.skyBottom = g_Theme.skyBottom;
        f.nebula    = g_Theme.nebula;
        f.fog       = g_Theme.fog;
        f.fogStart  = g_Theme.fogStart;
        f.fogEnd    = g_Theme.fogEnd;
        f.grid      = g_Theme.grid;
        f.gridCell  = g_Theme.gridCell;
        f.ground    = g_Theme.ground;
        return f;
    }

    // 専用描画器が使えないとき（シェーダーのコンパイル失敗）の代替：既存シェーダーで箱として描く
    void DrawFallback()
    {
        const int texId = Map_GetWiteTexID();
        Shader3d_Begin();
        Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
        for (const Block& b : g_Blocks)
        {
            const float k = 1.0f + b.emissive;
            Cube_DrawColor(texId, BlockWorld(b, 0.0f),
                { std::min(b.color.x * 4.0f * k, 1.0f), std::min(b.color.y * 4.0f * k, 1.0f),
                  std::min(b.color.z * 4.0f * k, 1.0f), 1.0f });
        }
    }

    // ミニマップ用の平らな矩形
    void DrawMiniRect(int texId, float cx, float y, float cz, float sx, float sz, const XMFLOAT4& color)
    {
        Shader3d_SetColor(color);
        const XMMATRIX world = XMMatrixScaling(sx, 1.0f, sz) * XMMatrixTranslation(cx, y, cz);
        MeshField_DrawTile(world, texId, 1.0f);
    }
}

//==============================================================================
// 構築API
//==============================================================================
namespace BlockStageBuilder
{
    void Begin(float halfX, float halfZ, float wallH, const Theme& theme)
    {
        g_HalfX = halfX;
        g_HalfZ = halfZ;
        g_Theme = theme;
        g_GroundY = Map_Internal_GetFloorY() + 0.5f;   // 既存マップの床上面と同じ高さ

        const float capY = g_GroundY + CAP_HEIGHT;
        const float outX = halfX + BOUND_THICK;
        const float outZ = halfZ + BOUND_THICK;

        // 地面（見た目はグリッド面として描くので、ここでは当たり判定だけ）
        AddCollider(Map_Internal_KindFloor(),
            { -outX, g_GroundY - 2.0f, -outZ }, { outX, g_GroundY, outZ });

        // 外周壁（南・北・西・東）。見た目は wallH、当たり判定は飛行上限まで
        const float half = BOUND_THICK * 0.5f;
        const struct { float cx, cz, sx, sz; } sides[4] =
        {
            { 0.0f, -(halfZ + half), outX * 2.0f, BOUND_THICK },
            { 0.0f,  (halfZ + half), outX * 2.0f, BOUND_THICK },
            { -(halfX + half), 0.0f, BOUND_THICK, halfZ * 2.0f },
            {  (halfX + half), 0.0f, BOUND_THICK, halfZ * 2.0f },
        };
        for (const auto& s : sides)
        {
            AddCollider(Map_Internal_KindWall(),
                { s.cx - s.sx * 0.5f, g_GroundY - 1.0f, s.cz - s.sz * 0.5f },
                { s.cx + s.sx * 0.5f, capY,             s.cz + s.sz * 0.5f });
            AddBlock(StageMesh::Cube, { s.cx, g_GroundY + wallH * 0.5f, s.cz },
                     { s.sx, wallH, s.sz }, theme.wall, 0.0f, false);
        }

        // 外周壁の上端を一周する発光ライン（作戦領域の境界表示）
        const float lineY = wallH - 0.4f;
        Glow(0.0f, -halfZ, halfX * 2.0f, 0.12f, 0.25f, theme.accent, lineY);
        Glow(0.0f,  halfZ, halfX * 2.0f, 0.12f, 0.25f, theme.accent, lineY);
        Glow(-halfX, 0.0f, 0.12f, halfZ * 2.0f, 0.25f, theme.accent, lineY);
        Glow( halfX, 0.0f, 0.12f, halfZ * 2.0f, 0.25f, theme.accent, lineY);

        // 飛行上限
        AddCollider(Map_Internal_KindCeiling(),
            { -outX, capY, -outZ }, { outX, capY + 1.0f, outZ });
    }

    void Solid(float cx, float cz, float sx, float sz, float h, const Col& col, float base)
    {
        const float y0 = g_GroundY + base;
        const float y1 = y0 + h;

        AddBoxCollider(cx, cz, sx, sz, y0, y1);
        AddBlock(StageMesh::Cube, { cx, (y0 + y1) * 0.5f, cz }, { sx, h, sz }, col, 0.0f, false);
        g_MiniRects.push_back({ cx, cz, sx, sz, base + h, col, true });
    }

    void SolidMesh(StageMesh mesh, float cx, float cz, float sx, float sz, float h, const Col& col,
                   float base, float collider, float colliderH)
    {
        const float y0 = g_GroundY + base;

        AddBoxCollider(cx, cz, sx * collider, sz * collider, y0, y0 + h * colliderH);
        AddBlock(mesh, { cx, y0 + h * 0.5f, cz }, { sx, h, sz }, col, 0.0f, false);
        g_MiniRects.push_back({ cx, cz, sx * collider, sz * collider, base + h * colliderH, col, true });
    }

    void Deco(float cx, float cz, float sx, float sz, float h, const Col& col, float base)
    {
        AddBlock(StageMesh::Cube, { cx, g_GroundY + base + h * 0.5f, cz }, { sx, h, sz }, col, 0.0f, false);

        // 地面に貼り付いた薄い板（路面）はミニマップにも出す
        if (base < 0.1f && h <= 0.1f)
            g_MiniRects.push_back({ cx, cz, sx, sz, 0.0f, col, false });
    }

    void Glow(float cx, float cz, float sx, float sz, float h, const Col& col, float base, float emissive)
    {
        AddBlock(StageMesh::Cube, { cx, g_GroundY + base + h * 0.5f, cz }, { sx, h, sz },
                 col, emissive, h >= GLOW_MIN_HEIGHT);
    }

    void Prop(StageMesh mesh, const XMFLOAT3& center, const XMFLOAT3& size, const Col& col,
              float emissive, const XMFLOAT3& rot, const XMFLOAT3& spin)
    {
        AddBlock(mesh, { center.x, g_GroundY + center.y, center.z }, size, col, emissive, emissive >= 1.0f, rot, spin);
    }

    void Building(float cx, float cz, float sx, float sz, float h, const Col& body, const Col& neon)
    {
        Solid(cx, cz, sx, sz, h, body);

        // 屋上の縁（上面は屋上よりわずかに下げて重なりを避ける）と、中ほどを一周する帯
        Glow(cx, cz, sx * 1.03f + 0.06f, sz * 1.03f + 0.06f, 0.30f, neon, h - 0.31f, 2.5f);
        Glow(cx, cz, sx * 1.03f + 0.06f, sz * 1.03f + 0.06f, 0.22f, neon, h * 0.6f, 1.4f);
    }

    void Skyline(std::mt19937& rng, int count, const Col& body, const Col& neonA, const Col& neonB)
    {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        for (int i = 0; i < count; ++i)
        {
            // 外周壁の外側をぐるりと囲むように、角度と距離をばらして建てる。
            // edge = その方角で外周壁（＋余白16m）にぶつかるまでの距離
            const float angle = XM_2PI * (static_cast<float>(i) + unit(rng)) / static_cast<float>(count);
            const float dx    = cosf(angle);
            const float dz    = sinf(angle);
            const float edge  = std::min((g_HalfX + 16.0f) / std::max(fabsf(dx), 0.001f),
                                         (g_HalfZ + 16.0f) / std::max(fabsf(dz), 0.001f));
            const float dist  = edge + unit(rng) * 110.0f;
            const float x = dx * dist;
            const float z = dz * dist;
            const float w = 5.0f + unit(rng) * 7.0f;
            const float h = 14.0f + unit(rng) * 46.0f;
            const Col&  neon = (i % 3 == 0) ? neonB : neonA;

            // 遠景なので光のにじみは付けない
            AddBlock(StageMesh::Cube, { x, g_GroundY + h * 0.5f, z }, { w, h, w }, body, 0.0f, false);
            AddBlock(StageMesh::Cube, { x, g_GroundY + h + 0.15f, z }, { w * 1.03f, 0.3f, w * 1.03f }, neon, 2.5f, false);
            AddBlock(StageMesh::Cube, { x, g_GroundY + h * 0.6f, z }, { w * 1.03f, 0.22f, w * 1.03f }, neon, 1.4f, false);
        }
    }

    void Squad(float x, float z, int count, int type, float radius, float level)
    {
        g_Squads.push_back({ x, z, radius, count, type, level });
    }

    void Platform(float cx, float cz, float sx, float sz, float top, const Col& col)
    {
        // 床板（厚さ 0.5m）：Solid と同じく側面＝壁、上面＝床。
        // 壁は高さを持つので、下の階の敵・プレイヤーは床板の下を通れる
        constexpr float THICK = 0.5f;
        Solid(cx, cz, sx, sz, THICK, col, top - THICK);
        g_Platforms.push_back({ cx - sx * 0.5f, cx + sx * 0.5f, cz - sz * 0.5f, cz + sz * 0.5f, g_GroundY + top });

        // 縁の発光ライン（床板の側面）
        Glow(cx, cz, sx + 0.06f, sz + 0.06f, 0.08f, g_Theme.accent, top - 0.3f, 1.2f);
    }

    void Stairs(float x0, float z0, int dir, float width, float run, float rise, const Col& col, float base)
    {
        // 1段 0.25m 以下になるよう段数を決める（プレイヤーは段に乗り上げ、敵は 0.4m まで登れる）
        const int   steps = std::max(1, static_cast<int>(std::ceil(rise / 0.25f)));
        const float tread = run / steps;
        const float dx = (dir == 1) ? 1.0f : (dir == 3) ? -1.0f : 0.0f;
        const float dz = (dir == 0) ? 1.0f : (dir == 2) ? -1.0f : 0.0f;

        for (int i = 0; i < steps; ++i)
        {
            const float along = tread * (i + 0.5f);
            const float cx = x0 + dx * along;
            const float cz = z0 + dz * along;
            const float sx = (dx != 0.0f) ? tread : width;
            const float sz = (dz != 0.0f) ? tread : width;
            const float y0 = g_GroundY + base;
            const float y1 = y0 + rise * static_cast<float>(i + 1) / steps;

            // 踏み面：段の上面の床（厚さ 0.3m）。足元から段差以内なのでそのまま上れる
            AddCollider(Map_Internal_KindFloor(), { cx - sx * 0.5f, std::max(y0, y1 - 0.3f), cz - sz * 0.5f }, { cx + sx * 0.5f, y1, cz + sz * 0.5f });

            // 段の下の中身：壁。上端を「1つ下の段の踏み面」より少し低くしておくと、
            // 1つ下の段に立って進むときには当たらず、横や下からは通り抜けられない
            const float wallTop = y0 + rise * static_cast<float>(i) / steps - 0.05f;
            if (wallTop > y0 + 0.05f)
                AddCollider(Map_Internal_KindWall(), { cx - sx * 0.5f, y0, cz - sz * 0.5f }, { cx + sx * 0.5f, wallTop, cz + sz * 0.5f });
            AddBlock(StageMesh::Cube, { cx, (y0 + y1) * 0.5f, cz }, { sx, y1 - y0, sz }, col, 0.0f, false);
            g_Footprints.push_back({ cx - sx * 0.5f, cx + sx * 0.5f, cz - sz * 0.5f, cz + sz * 0.5f, y0, y1 });
            g_MiniRects.push_back({ cx, cz, sx, sz, base + (y1 - y0), col, true });
        }
    }

    void DropZone(float x, float z)
    {
        g_DropZones.push_back({ x, z });
    }

    void SetSpawn(float x, float z)
    {
        g_Spawn = { x, z };
        if (!g_PreviewMode)
            Map_Internal_SetSpawnPos({ x, g_GroundY + 0.5f, z });
    }

    void SetGoal(float x, float z, float base)
    {
        g_HasGoal = true;
        g_GoalPos = { x, g_GroundY + base, z };

        // 足場に乗ったら到達。高さ方向は足元から 3m まで
        AABB goal{};
        goal.min = { x - GOAL_HALF, g_GoalPos.y - 0.2f, z - GOAL_HALF };
        goal.max = { x + GOAL_HALF, g_GoalPos.y + 3.0f, z + GOAL_HALF };
        if (!g_PreviewMode)
            Map_Internal_SetGoal(g_GoalPos, goal);

        // 足場の発光プレート
        Glow(x, z, GOAL_HALF * 2.0f, GOAL_HALF * 2.0f, 0.05f, g_Theme.accent, base, 1.5f);
    }

    void SetBoss(float x, float z)
    {
        g_HasBoss = true;
        g_Boss = { x, z };
        if (!g_PreviewMode)
            Map_Internal_SetBossSpawnPos({ x, g_GroundY + 0.5f, z });
    }

    void SetEnemyMinDistance(float meters)
    {
        g_EnemyMinDist = meters;
    }
}

//==============================================================================
// 構築
//==============================================================================
// 配置データを作る（Build と GetPreview の共通部分）
static void BuildInternal(BlockStageID id, std::uint32_t seed, int enemyCount)
{
    g_Blocks.clear();
    g_MiniRects.clear();
    g_Footprints.clear();
    g_Platforms.clear();
    g_Squads.clear();
    g_DropZones.clear();
    g_HasBoss = false;
    g_HasGoal = false;
    g_EnemyMinDist = 20.0f;
    g_AnimTime = 0.0f;

    if (!g_PreviewMode)
    {
        Map_Internal_ClearObjects();
        Map_Internal_ClearEnemySpawns();
        Map_Internal_SetGoalInvalid();
    }

    // 地形のばらつきはステージごとに固定（毎回同じ地形になる）
    std::mt19937 layoutRng(1000u + static_cast<std::uint32_t>(id) * 7919u);

    switch (id)
    {
    case BlockStageID::City:     BlockStageLayout_City(layoutRng);     break;
    case BlockStageID::Terminal: BlockStageLayout_Terminal(layoutRng); break;
    case BlockStageID::Fortress: BlockStageLayout_Fortress(layoutRng); break;
    case BlockStageID::Trench:   BlockStageLayout_Trench(layoutRng);   break;
    case BlockStageID::Plant:    BlockStageLayout_Plant(layoutRng);    break;
    case BlockStageID::Spaceport:   BlockStageLayout_Spaceport(layoutRng);   break;
    case BlockStageID::DataVault:   BlockStageLayout_DataVault(layoutRng);   break;
    case BlockStageID::CryoMine:    BlockStageLayout_CryoMine(layoutRng);    break;
    case BlockStageID::CarrierDeck: BlockStageLayout_CarrierDeck(layoutRng); break;
    default:                     BlockStageLayout_Arena(layoutRng);    break;
    }

    PlaceEnemies(seed, enemyCount);
}

void BlockStage_Build(BlockStageID id, std::uint32_t seed, int enemyCount, bool placeSquads)
{
    g_PreviewMode = false;
    g_PlaceSquads = placeSquads;
    BuildInternal(id, seed, enemyCount);
    g_PlaceSquads = true;
    g_Active = true;
}

void BlockStage_GetPreview(BlockStageID id, BlockStagePreview* out)
{
    if (!out) return;

    g_PreviewMode = true;
    BuildInternal(id, 0u, 0);
    g_PreviewMode = false;
    g_Active = false;   // 配置データを上書きしたので、出撃時に作り直すまで無効

    *out = BlockStagePreview{};
    out->halfX = g_HalfX;
    out->halfZ = g_HalfZ;
    for (const MiniRect& r : g_MiniRects)
        if (r.solid) out->structures.push_back({ r.cx, r.cz, r.sx, r.sz, r.top });
    for (const SquadRequest& sq : g_Squads)
        out->squads.push_back({ sq.x, sq.z, sq.count, sq.type });
    out->dropZones = g_DropZones;
    out->spawn   = g_Spawn;
    out->goal    = { g_GoalPos.x, g_GoalPos.z };
    out->boss    = g_Boss;
    out->hasGoal = g_HasGoal;
    out->hasBoss = g_HasBoss;
    out->accent  = g_Theme.accent;
}

int BlockStage_GetEnemyType(int index)
{
    if (index < 0 || index >= static_cast<int>(g_SpawnTypes.size())) return -1;
    return g_SpawnTypes[index];
}

bool BlockStage_PickReinforcePoint(const XMFLOAT3& playerPos, std::uint32_t random, int slot, XMFLOAT3* outPos)
{
    if (!outPos || g_DropZones.empty()) return false;

    // プレイヤーから十分離れた降下地点のうち、近い2か所のどちらかから出す
    // （遠すぎると敵がプレイヤーを見つけられず、増援が圧力にならないため）。
    // 条件を満たす地点がなければ最も遠い地点
    std::vector<std::pair<float, int>> farZones;
    int   farthest = 0;
    float farthestDist = -1.0f;
    for (int i = 0; i < static_cast<int>(g_DropZones.size()); ++i)
    {
        const float d = DistXZ(g_DropZones[i].x, g_DropZones[i].y, playerPos.x, playerPos.z);
        if (d >= DROP_MIN_PLAYER_DIST) farZones.push_back({ d, i });
        if (d > farthestDist) { farthestDist = d; farthest = i; }
    }
    std::sort(farZones.begin(), farZones.end());

    XMFLOAT2 zone = g_DropZones[farthest];
    if (!farZones.empty())
    {
        const size_t choices = std::min<size_t>(2, farZones.size());
        zone = g_DropZones[farZones[random % choices].second];
    }

    // 降下地点のまわりの空き地（巡回点）に散らす
    std::vector<const XMFLOAT3*> nearPoints;
    for (const XMFLOAT3& p : g_Patrol)
        if (DistXZ(p.x, p.z, zone.x, zone.y) <= DROP_SCATTER) nearPoints.push_back(&p);

    if (nearPoints.empty())
        *outPos = { zone.x, g_GroundY, zone.y };
    else
        *outPos = *nearPoints[(random / 7u + static_cast<std::uint32_t>(slot) * 13u) % nearPoints.size()];
    return true;
}

bool BlockStage_IsActive()
{
    return g_Active;
}

void BlockStage_Deactivate()
{
    g_Active = false;
}

void BlockStage_Finalize()
{
    g_Active = false;
    StageRender::Finalize();
}

//==============================================================================
// 描画：ステージ本体
// ・空 → 地面グリッド（→ 天井グリッド）→ 配置物 → 光のにじみ の順
// ・直前に描かれた既存のスカイボックスは、空で上書きする
//==============================================================================
void BlockStage_Draw()
{
    g_AnimTime += 1.0f / 60.0f;

    if (StageRender::Begin(MakeFrame()))
    {
        StageRender::DrawSky();
        StageRender::DrawGrid(g_GroundY, true);
        if (g_Theme.ceiling)
            StageRender::DrawGrid(g_GroundY + CAP_HEIGHT + 1.0f, false);

        for (const Block& b : g_Blocks)
            StageRender::DrawMesh(b.mesh, BlockWorld(b, 0.0f), b.color, b.emissive);

        StageRender::BeginGlow();
        for (const Block& b : g_Blocks)
        {
            if (!b.halo) continue;
            StageRender::DrawGlow(b.mesh, BlockWorld(b, GLOW_PAD_INNER), b.color, GLOW_STRENGTH_INNER);
            StageRender::DrawGlow(b.mesh, BlockWorld(b, GLOW_PAD_OUTER), b.color, GLOW_STRENGTH_OUTER);
        }
        StageRender::EndGlow();

        StageRender::End();
    }
    else
    {
        DrawFallback();
    }

    // 以降のモデル描画のためにシェーダーとライトを既定値へ戻す（Map_Draw の末尾と同じ）
    Map_Light_Reset();
}

//==============================================================================
// 描画：ゴールビーコン（光柱＋回転する八面体のマーカー）
//==============================================================================
void BlockStage_DrawGoal()
{
    if (!g_HasGoal) return;
    if (!StageRender::Begin(MakeFrame())) return;

    const float pulse  = 0.5f + 0.5f * sinf(g_AnimTime * 3.0f);
    const Col&  c      = g_Theme.accent;
    const float height = 18.0f;

    // マーカー（足場の上で回転・上下する）
    const float bob = sinf(g_AnimTime * 2.0f) * 0.25f;
    StageRender::DrawMesh(StageMesh::Octa,
        XMMatrixScaling(0.9f, 1.3f, 0.9f) * XMMatrixRotationY(g_AnimTime * 1.5f) *
        XMMatrixTranslation(g_GoalPos.x, g_GoalPos.y + 2.4f + bob, g_GoalPos.z),
        c, 2.5f);

    // 光柱（芯は細く濃く、外側は太く薄く）
    const XMMATRIX at = XMMatrixTranslation(g_GoalPos.x, g_GoalPos.y + height * 0.5f, g_GoalPos.z);
    StageRender::BeginGlow();
    StageRender::DrawGlow(StageMesh::Cylinder, XMMatrixScaling(0.4f, height, 0.4f) * at, c, 0.9f);
    StageRender::DrawGlow(StageMesh::Cylinder, XMMatrixScaling(2.0f, height, 2.0f) * at, c, 0.20f + 0.15f * pulse);
    StageRender::EndGlow();

    StageRender::End();
    Map_Light_Reset();
}

//==============================================================================
// 描画：ミニマップ
// ・地面 → 路面 → 構造物（高いものほど明るく・上に重ねる）→ ゴール の順
//==============================================================================
void BlockStage_DrawMinimap()
{
    const int texId = Map_GetWiteTexID();

    Shader3d_Begin();
    Light_SetAmbient({ 1.0f, 1.0f, 1.0f });

    // 地面
    DrawMiniRect(texId, 0.0f, MINI_GROUND_Y, 0.0f, g_HalfX * 2.0f, g_HalfZ * 2.0f,
                 { 0.07f, 0.09f, 0.13f, 1.0f });

    for (const MiniRect& r : g_MiniRects)
    {
        if (r.solid)
        {
            // 高さ 0〜14m を 0.30〜0.75 の明るさに割り当てる
            const float t     = std::min(r.top / 14.0f, 1.0f);
            const float shade = 0.30f + 0.45f * t;
            const float y     = MINI_BLOCK_Y + std::min(r.top, CAP_HEIGHT) * 0.02f;
            DrawMiniRect(texId, r.cx, y, r.cz, r.sx, r.sz,
                { g_Theme.minimap.x * shade, g_Theme.minimap.y * shade, g_Theme.minimap.z * shade, 1.0f });
        }
        else
        {
            DrawMiniRect(texId, r.cx, MINI_DECAL_Y, r.cz, r.sx, r.sz, { 0.12f, 0.15f, 0.20f, 1.0f });
        }
    }

    // ゴール：明滅する四角
    if (g_HasGoal)
    {
        const float pulse = 0.5f + 0.5f * sinf(g_AnimTime * 3.0f);
        const float size  = 3.5f + 1.5f * pulse;
        const Col&  c     = g_Theme.accent;
        DrawMiniRect(texId, g_GoalPos.x, MINI_GOAL_Y, g_GoalPos.z, size, size,
                     { c.x * 0.75f, c.y * 0.75f, c.z * 0.75f, 1.0f });
    }

    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
}
