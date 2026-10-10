/*==============================================================================

   ブロックステージ配置（第二作戦区域） [BlockStageLayouts2.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/11
--------------------------------------------------------------------------------

   第二作戦区域（MISSION 07〜13）用のステージ。座標・高さ・色の約束は
   BlockStageBuilder.h を参照。プレイヤーは南（-Z）から開始し、北（+Z）へ進む。

   ■ステージ一覧（見た目のテーマ）
     Spaceport   : ORBITAL SPACEPORT 100 × 120m  紺の夜空と白いグリッド、赤い航空灯。
                                                 発着パッドと格納庫、管制塔。北端の打ち上げ台がゴール
     DataVault   : DATA VAULT         72 ×  84m  天井のある屋内。マゼンタのグリッドと水色の光。
                                                 サーバーラックの迷路と中央のデータコア。北の金庫扉がゴール
     CryoMine    : CRYO MINE          88 ×  88m  白い星雲と氷色のグリッド。氷の岩と採掘機、
                                                 中央に高さ5mの掘削リグ（階段4本）
     CarrierDeck : CARRIER DECK       52 × 140m  夕焼けの星雲と白い甲板線。縦長の飛行甲板と
                                                 東側の艦橋、駐機中の機体。北端の艦首がゴール

==============================================================================*/
#include "BlockStageBuilder.h"
#include <initializer_list>
#include <cmath>
#include <algorithm>

using namespace BlockStageBuilder;
using DirectX::XMFLOAT3;

namespace
{
    float RandRange(std::mt19937& rng, float lo, float hi)
    {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    }

    int RandInt(std::mt19937& rng, int count)   // 0 〜 count-1
    {
        return std::uniform_int_distribution<int>(0, count - 1)(rng);
    }

    Col Scale(const Col& c, float k)
    {
        return { c.x * k, c.y * k, c.z * k };
    }

    // 駐機中の小型機（胴体＋機首＋主翼）。nose は機首の向き（+1 = 北 / -1 = 南）。遮蔽物になる
    void ParkedCraft(float x, float z, float nose, const Col& hull, const Col& neon)
    {
        Solid(x, z, 2.4f, 6.0f, 1.8f, hull);
        SolidMesh(StageMesh::Pyramid, x, z + nose * 3.6f, 2.2f, 1.6f, 1.6f, hull);
        Deco (x, z - nose * 0.6f, 8.0f, 2.0f, 0.25f, hull, 1.0f);   // 主翼
        Glow (x, z - nose * 0.6f, 8.1f, 0.12f, 0.06f, neon, 1.22f, 1.6f);
        Glow (x, z - nose * 3.05f, 1.6f, 0.1f, 0.6f, neon, 0.6f, 2.5f);   // 噴射口
    }
}

//==============================================================================
// ORBITAL SPACEPORT（宇宙港）
// ・南から北へ伸びる誘導路（中央）の両側に、発着パッド・格納庫・燃料タンク
// ・東に管制塔（頂上で輪が回る）、西に格納庫の列。誘導路には赤い航空灯
// ・北端に高さ2mの打ち上げ台。南の階段を上った先のシャトル前がゴール
//==============================================================================
void BlockStageLayout_Spaceport(std::mt19937& rng)
{
    const Col white = { 0.80f, 0.90f, 1.00f };
    const Col red   = { 1.00f, 0.22f, 0.18f };

    Theme theme{};
    theme.skyTop    = { 0.004f, 0.006f, 0.020f };
    theme.skyBottom = { 0.020f, 0.035f, 0.090f };
    theme.nebula    = { 0.30f, 0.50f, 1.00f, 0.40f };
    theme.fog       = { 0.015f, 0.025f, 0.060f };
    theme.fogStart  = 45.0f;
    theme.fogEnd    = 240.0f;
    theme.grid      = { 0.35f, 0.50f, 0.90f };
    theme.gridCell  = 5.0f;
    theme.ground    = { 0.010f, 0.012f, 0.022f };
    theme.ceiling   = false;
    theme.wall      = { 0.040f, 0.050f, 0.080f };
    theme.accent    = red;
    theme.minimap   = { 0.75f, 0.85f, 1.00f };

    Begin(50.0f, 60.0f, 9.0f, theme);
    SetSpawn(0.0f, -56.0f);

    const Col hull  = { 0.090f, 0.100f, 0.130f };
    const Col dark  = { 0.040f, 0.045f, 0.060f };
    const Col pad   = { 0.030f, 0.034f, 0.050f };
    const Col lamp  = { 1.00f, 0.90f, 0.70f };

    // 誘導路（中央）：両脇の白線と、等間隔の赤い航空灯
    for (float x : { -6.0f, 6.0f })
        Glow(x, -6.0f, 0.2f, 100.0f, 0.02f, white, 0.0f, 0.7f);
    for (float z = -50.0f; z <= 38.0f; z += 8.0f)
        for (float x : { -6.6f, 6.6f })
            Prop(StageMesh::Sphere, { x, 0.25f, z }, { 0.35f, 0.35f, 0.35f }, red, 4.0f);

    // 発着パッド（低い六角形の台。縁の輪が光る）と駐機中の機体
    const struct { float x, z; bool craft; } pads[] =
    {
        { -26.0f, -32.0f, true }, { 26.0f, -14.0f, true }, { -24.0f, 18.0f, false }, { 28.0f, 30.0f, true },
    };
    for (const auto& p : pads)
    {
        // パッドは床に描いた標示（当たり判定なし。上でも部隊が動ける）
        Prop(StageMesh::Hex, { p.x, 0.03f, p.z }, { 16.0f, 0.06f, 16.0f }, pad);
        Prop(StageMesh::Torus, { p.x, 0.08f, p.z }, { 15.0f, 0.15f, 15.0f }, red, 1.4f);
        Glow(p.x, p.z, 6.0f, 0.3f, 0.02f, white, 0.06f, 0.8f);   // 着陸マーク（H の横棒）
        for (float s : { -1.0f, 1.0f })
            Glow(p.x + s * 3.0f, p.z, 0.3f, 6.0f, 0.02f, white, 0.06f, 0.8f);
        if (p.craft) ParkedCraft(p.x, p.z, (p.x < 0.0f) ? 1.0f : -1.0f, hull, red);
    }
    // 空いたパッドには資材（遮蔽物）
    for (const auto& c : { XMFLOAT3{ -27.0f, 0.0f, 15.0f }, XMFLOAT3{ -21.0f, 0.0f, 21.0f }, XMFLOAT3{ -25.0f, 0.0f, 23.0f } })
    {
        Solid(c.x, c.z, 2.0f, 2.0f, 1.6f, dark);
        Glow (c.x, c.z, 2.05f, 2.05f, 0.06f, white, 0.9f, 0.9f);
    }

    // 西の格納庫の列（扉が光る）
    for (float z : { -40.0f, -10.0f, 40.0f })
    {
        Building(-42.0f, z, 12.0f, 16.0f, 9.0f, dark, white);
        Glow(-35.95f, z, 0.08f, 9.0f, 5.0f, red, 0.0f, 0.8f);
    }

    // 東の管制塔（台座の上に細い塔。頂上で輪が回る）
    SolidMesh(StageMesh::Cylinder, 40.0f, 6.0f, 10.0f, 10.0f, 4.0f, hull);
    Solid(40.0f, 6.0f, 4.0f, 4.0f, 30.0f, hull);
    Glow (40.0f, 6.0f, 4.1f, 4.1f, 0.3f, white, 12.0f, 1.6f);
    Glow (40.0f, 6.0f, 4.1f, 4.1f, 0.3f, white, 22.0f, 1.6f);
    Prop(StageMesh::Torus, { 40.0f, 31.0f, 6.0f }, { 9.0f, 1.2f, 9.0f }, red, 2.0f,
         { 0.2f, 0.0f, 0.0f }, { 0.0f, 0.6f, 0.0f });
    Prop(StageMesh::Sphere, { 40.0f, 31.0f, 6.0f }, { 2.0f, 2.0f, 2.0f }, lamp, 3.0f);

    // 東の燃料タンク（南東）
    for (float z : { -44.0f, -34.0f })
    {
        SolidMesh(StageMesh::Cylinder, 42.0f, z, 6.0f, 6.0f, 6.0f, hull, 0.0f, 0.8f);
        Prop(StageMesh::Torus, { 42.0f, 3.5f, z }, { 7.4f, 1.6f, 7.4f }, white, 1.4f);
    }

    // 誘導路わきの牽引車・資材（小さな遮蔽物）
    const struct { float x, z, sx, sz; } crates[] =
    {
        { -10.0f, -44.0f, 2.0f, 3.0f }, {  10.0f, -36.0f, 2.0f, 2.0f }, { -11.0f, -20.0f, 3.0f, 2.0f },
        {  11.0f,  -4.0f, 2.0f, 3.0f }, { -10.0f,   6.0f, 2.0f, 2.0f }, {  12.0f,  18.0f, 3.0f, 2.0f },
        { -12.0f,  30.0f, 2.0f, 3.0f }, {  18.0f,  -2.0f, 2.0f, 2.0f }, { -18.0f, -12.0f, 2.0f, 2.0f },
    };
    for (const auto& c : crates)
    {
        Solid(c.x, c.z, c.sx, c.sz, 1.4f, dark);
        Glow (c.x, c.z, c.sx * 1.02f, c.sz * 1.02f, 0.05f, red, 0.9f, 0.9f);
    }

    // 照明塔
    for (float x : { -47.0f, 47.0f })
        for (float z : { -20.0f, 22.0f })
        {
            Solid(x, z, 0.8f, 0.8f, 14.0f, hull);
            Prop(StageMesh::Sphere, { x, 14.8f, z }, { 1.6f, 1.6f, 1.6f }, lamp, 3.0f);
        }

    //--------------------------------------------------------------------------
    // 北端の打ち上げ台（高さ2m）。南の階段から上り、シャトルの前がゴール
    //--------------------------------------------------------------------------
    constexpr float DECK = 2.0f;
    Platform(0.0f, 51.0f, 30.0f, 14.0f, DECK, hull);
    Glow(0.0f, 44.05f, 30.0f, 0.1f, 0.1f, red, DECK - 0.1f, 2.0f);
    Stairs(0.0f, 38.0f, 0, 6.0f, 6.0f, DECK, hull);
    for (float s : { -1.0f, 1.0f })
    {
        // 台の上の遮蔽物（手すり）
        Solid(s * 9.0f, 46.0f, 4.0f, 0.4f, 1.0f, dark, DECK);
        Glow (s * 9.0f, 46.0f, 4.0f, 0.44f, 0.06f, red, DECK + 0.96f, 1.4f);
    }
    // シャトル（台の北側。発射架台に立てかけた姿）
    Solid(0.0f, 56.5f, 4.0f, 3.0f, 12.0f, Scale(white, 0.16f), DECK);
    SolidMesh(StageMesh::Pyramid, 0.0f, 56.5f, 4.0f, 3.0f, 4.0f, Scale(white, 0.16f), DECK + 12.0f);
    Glow(0.0f, 54.95f, 3.0f, 0.1f, 0.4f, red, DECK + 6.0f, 2.5f);
    for (float s : { -1.0f, 1.0f })
    {
        Solid(s * 4.5f, 57.0f, 1.0f, 1.0f, 18.0f, hull, DECK);   // 架台の塔
        Glow (s * 4.5f, 57.0f, 1.05f, 1.05f, 0.3f, white, DECK + 9.0f, 1.6f);
    }
    SetGoal(0.0f, 51.0f, DECK);

    //--------------------------------------------------------------------------
    // 敵配置：誘導路の要所に部隊。パッドと格納庫の陰から挟み込む
    //--------------------------------------------------------------------------
    Squad(  0.0f, -38.0f, 3, NORMAL);
    Squad(-24.0f, -30.0f, 2, GUNNER);
    Squad( 22.0f, -38.0f, 2, SNIPER, 3.0f);
    Squad( 24.0f, -12.0f, 3, SPEED);
    Squad(  0.0f, -14.0f, 2, TANK);
    Squad(-30.0f, -10.0f, 1, TURRET, 3.0f);
    Squad(-22.0f,  16.0f, 3, BOMBER);
    Squad(  0.0f,  12.0f, 2, GATLING, 5.0f);
    Squad( 28.0f,  28.0f, 2, SNIPER, 4.0f);
    Squad( 22.0f,  10.0f, 2, WING, 5.0f);
    Squad( -8.0f,  32.0f, 2, WALKER, 4.0f);
    Squad(  0.0f,  51.0f, 2, GUNNER, 4.0f, DECK);   // 打ち上げ台の上
    Squad( 12.0f,  40.0f, 1, ORBITER, 3.0f);

    SetBoss(0.0f, 20.0f);

    // 増援の降下地点（外周と誘導路の北）
    DropZone(-44.0f,  10.0f);
    DropZone( 44.0f, -24.0f);
    DropZone( 30.0f,  44.0f);
    DropZone(-30.0f,  44.0f);
    DropZone(-36.0f, -52.0f);
    DropZone( 16.0f, -52.0f);

    // 遠景：軌道エレベーター（北の空へ伸びる光の柱）と、上空を行き交う輸送船
    Prop(StageMesh::Cylinder, { 30.0f, 200.0f, 260.0f }, { 5.0f, 420.0f, 5.0f }, white, 1.2f);
    for (float y : { 60.0f, 120.0f, 180.0f, 240.0f })
        Prop(StageMesh::Torus, { 30.0f, y, 260.0f }, { 16.0f, 2.0f, 16.0f }, red, 1.8f,
             { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.3f, 0.0f });
    for (int i = 0; i < 7; ++i)
    {
        const float x = RandRange(rng, -160.0f, 160.0f);
        const float y = RandRange(rng, 50.0f, 110.0f);
        const float z = RandRange(rng, 120.0f, 240.0f);
        const float l = RandRange(rng, 20.0f, 40.0f);
        Prop(StageMesh::Cube, { x, y, z }, { l, 5.0f, l * 0.35f }, hull);
        Prop(StageMesh::Cube, { x, y - 2.6f, z }, { l * 1.02f, 0.3f, l * 0.36f }, white, 1.5f);
        Prop(StageMesh::Sphere, { x + l * 0.52f, y, z }, { 1.4f, 1.4f, 1.4f }, red, 4.0f);
    }
}

//==============================================================================
// DATA VAULT（電子金庫）
// ・天井のある屋内。南半分と北半分にサーバーラックの列（切れ目が通路になる迷路）
// ・中央の吹き抜けに、回るデータコア（八面体）と輪。まわりに低い端末台
// ・北端の金庫扉の前がゴール。東西の壁沿いは冷却ダクトの通路
//==============================================================================
void BlockStageLayout_DataVault(std::mt19937& rng)
{
    const Col magenta = { 1.00f, 0.20f, 0.70f };
    const Col cyan    = { 0.20f, 0.85f, 1.00f };

    Theme theme{};
    theme.skyTop    = { 0.020f, 0.004f, 0.020f };
    theme.skyBottom = { 0.050f, 0.010f, 0.050f };
    theme.nebula    = { 0.90f, 0.20f, 0.80f, 0.25f };
    theme.fog       = { 0.040f, 0.010f, 0.045f };
    theme.fogStart  = 25.0f;
    theme.fogEnd    = 140.0f;
    theme.grid      = magenta;
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.012f, 0.008f, 0.016f };
    theme.ceiling   = true;
    theme.wall      = { 0.050f, 0.040f, 0.065f };
    theme.accent    = cyan;
    theme.minimap   = { 1.00f, 0.60f, 0.95f };

    Begin(36.0f, 42.0f, 12.0f, theme);
    SetSpawn(0.0f, -39.0f);
    SetEnemyMinDistance(14.0f);

    const Col rack  = { 0.045f, 0.040f, 0.070f };
    const Col metal = { 0.080f, 0.070f, 0.100f };
    const Col dark  = { 0.030f, 0.028f, 0.045f };

    // サーバーラック1本（高さ3.2m。前面に縦の光の帯、上面の縁が光る）
    auto Rack = [&](float cx, float cz, float len)
    {
        Solid(cx, cz, len, 1.4f, 3.2f, rack);
        Glow (cx, cz, len + 0.04f, 1.44f, 0.06f, magenta, 3.15f, 1.3f);
        const int lights = std::max(1, static_cast<int>(len / 1.5f));
        for (int i = 0; i < lights; ++i)
        {
            const float x  = cx - len * 0.5f + (i + 0.5f) * len / lights;
            const Col&  c  = (RandInt(rng, 3) == 0) ? magenta : cyan;
            const float h  = RandRange(rng, 0.8f, 2.6f);
            for (float side : { -1.0f, 1.0f })
                Glow(x, cz + side * 0.71f, 0.12f, 0.04f, h, c, 0.3f, 1.8f);
        }
    };

    // ラックの列：南半分（z -32〜-12）と北半分（z 12〜32）。各列は左右3本ずつで、ところどころ抜ける
    for (float rowZ : { -32.0f, -24.0f, -16.0f, 16.0f, 24.0f, 32.0f })
    {
        for (float x : { -26.0f, -15.0f, -5.5f, 5.5f, 15.0f, 26.0f })
        {
            if (RandInt(rng, 6) == 0) continue;               // 切れ目
            if (rowZ == 32.0f && std::fabs(x) < 8.0f) continue; // 金庫扉の前は空ける
            Rack(x, rowZ, (std::fabs(x) < 8.0f) ? 7.0f : 8.0f);
        }
    }

    // 中央の吹き抜け：床の円と、回るデータコア
    Prop(StageMesh::Torus, { 0.0f, 0.05f, 0.0f }, { 18.0f, 0.1f, 18.0f }, cyan, 0.9f);
    Prop(StageMesh::Octa,  { 0.0f, 6.0f, 0.0f }, { 3.0f, 4.6f, 3.0f }, cyan, 3.0f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.9f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 6.0f, 0.0f }, { 8.0f, 1.0f, 8.0f }, magenta, 1.8f,
         { 0.5f, 0.0f, 0.2f }, { 0.0f, -0.5f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 6.0f, 0.0f }, { 11.0f, 1.0f, 11.0f }, cyan, 1.4f,
         { -0.3f, 0.0f, 0.4f }, { 0.0f, 0.35f, 0.0f });
    SolidMesh(StageMesh::Cylinder, 0.0f, 0.0f, 3.0f, 3.0f, 1.2f, metal);   // コアの台座（遮蔽物）

    // 吹き抜けを囲む端末台（低い遮蔽物）
    for (int i = 0; i < 6; ++i)
    {
        const float a = DirectX::XM_2PI * (i + 0.5f) / 6.0f;
        const float x = cosf(a) * 9.0f, z = sinf(a) * 9.0f;
        Solid(x, z, 2.4f, 2.4f, 1.1f, metal);
        Glow (x, z, 2.0f, 2.0f, 0.04f, cyan, 1.1f, 1.4f);
    }

    // 東西の冷却ダクト（壁沿いの太い管。通路を区切る）
    for (float s : { -1.0f, 1.0f })
    {
        for (float z : { -28.0f, -8.0f, 8.0f, 28.0f })
        {
            SolidMesh(StageMesh::Cylinder, s * 33.0f, z, 3.0f, 3.0f, 9.0f, dark, 0.0f, 0.8f);
            Glow(s * 33.0f, z, 3.1f, 3.1f, 0.2f, magenta, 5.0f, 1.2f);
        }
        // 吹き抜けの東西にある配電盤（壁際の遮蔽物）
        Solid(s * 22.0f, 0.0f, 1.2f, 6.0f, 2.0f, metal);
        Glow (s * 22.0f, 0.0f, 1.24f, 5.6f, 0.06f, cyan, 1.95f, 1.4f);
    }

    // 天井の梁（見た目だけ）と、浮かんで回るデータキューブ
    for (float z = -36.0f; z <= 36.0f; z += 12.0f)
        Deco(0.0f, z, 72.0f, 0.6f, 0.4f, metal, 11.4f);
    for (int i = 0; i < 14; ++i)
    {
        const float x = RandRange(rng, -30.0f, 30.0f);
        const float z = RandRange(rng, -36.0f, 36.0f);
        const float y = RandRange(rng, 6.5f, 10.5f);
        const float s = RandRange(rng, 0.4f, 0.9f);
        Prop(StageMesh::Cube, { x, y, z }, { s, s, s }, (i % 2) ? cyan : magenta, 2.5f,
             { 0.6f, 0.0f, 0.3f }, { 0.4f, 0.9f, 0.0f });
    }

    // 北端の金庫扉（ゴール）
    Solid(0.0f, 41.0f, 12.0f, 2.0f, 9.0f, metal);
    Glow (0.0f, 39.95f, 9.0f, 0.1f, 6.0f, cyan, 0.5f, 1.6f);
    Prop(StageMesh::Torus, { 0.0f, 4.0f, 39.8f }, { 6.0f, 6.0f, 1.0f }, magenta, 2.0f,
         { DirectX::XM_PIDIV2, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
    SetGoal(0.0f, 37.0f);

    //--------------------------------------------------------------------------
    // 敵配置：ラックの通路を近接型が走り、交差点を固定砲台が押さえる。吹き抜けに光輪型
    //--------------------------------------------------------------------------
    Squad(  0.0f, -28.0f, 3, BOMBER, 4.0f);
    Squad(-20.0f, -20.0f, 2, PHANTOM, 4.0f);
    Squad( 20.0f, -20.0f, 3, GUNNER, 4.0f);
    Squad(-10.0f, -12.0f, 1, TURRET, 2.0f);
    Squad( 10.0f, -12.0f, 1, TURRET, 2.0f);
    Squad(  0.0f,   0.0f, 1, HALO, 3.0f);
    Squad(-14.0f,   4.0f, 2, ORBITER, 4.0f);
    Squad( 14.0f,  -4.0f, 2, TANK, 4.0f);
    Squad(-20.0f,  20.0f, 3, SPEED, 4.0f);
    Squad( 20.0f,  20.0f, 2, PHANTOM, 4.0f);
    Squad(  0.0f,  28.0f, 2, GATLING, 4.0f);
    Squad(-28.0f,  -2.0f, 2, SNIPER, 3.0f);
    Squad( 28.0f,   2.0f, 2, SNIPER, 3.0f);

    SetBoss(0.0f, 10.0f);

    // 増援の降下地点（東西の通路と、南北の端）
    DropZone(-29.0f, -18.0f);
    DropZone( 29.0f, -18.0f);
    DropZone(-29.0f,  18.0f);
    DropZone( 29.0f,  18.0f);
    DropZone(-12.0f,  36.0f);
    DropZone( 12.0f,  36.0f);
}

//==============================================================================
// CRYO MINE（氷結採掘場）
// ・中央に高さ5mの掘削リグ（20×20m のデッキ）。四方から階段で上がれ、上で掘削塔が回る
// ・まわりに氷の岩塊と光る結晶、鉱石のコンベア、停止中の採掘機
// ・外周は崖（大きな岩）で囲まれる。ボス戦では北側の広場に大型兵器が降りる
//==============================================================================
void BlockStageLayout_CryoMine(std::mt19937& rng)
{
    const Col ice  = { 0.55f, 0.90f, 1.00f };
    const Col warn = { 1.00f, 0.60f, 0.15f };

    Theme theme{};
    theme.skyTop    = { 0.010f, 0.016f, 0.030f };
    theme.skyBottom = { 0.060f, 0.090f, 0.120f };
    theme.nebula    = { 0.70f, 0.90f, 1.00f, 0.35f };
    theme.fog       = { 0.050f, 0.075f, 0.100f };
    theme.fogStart  = 35.0f;
    theme.fogEnd    = 200.0f;
    theme.grid      = ice;
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.030f, 0.038f, 0.050f };
    theme.ceiling   = false;
    theme.wall      = { 0.080f, 0.100f, 0.120f };
    theme.accent    = ice;
    theme.minimap   = { 0.70f, 0.95f, 1.00f };

    Begin(44.0f, 44.0f, 10.0f, theme);
    SetSpawn(0.0f, -35.0f);

    const Col rock  = { 0.150f, 0.230f, 0.320f };   // 氷をかぶった岩（青白い）
    const Col metal = { 0.070f, 0.080f, 0.095f };
    const Col dark  = { 0.035f, 0.040f, 0.050f };

    //--------------------------------------------------------------------------
    // 中央の掘削リグ（デッキ高さ5m）。四方の階段と手すり
    //--------------------------------------------------------------------------
    constexpr float TOP = 5.0f;
    Platform(0.0f, 0.0f, 20.0f, 20.0f, TOP, metal);
    for (float s : { -1.0f, 1.0f })   // デッキの縁の発光（側面の帯）
    {
        Glow(0.0f, s * 10.05f, 20.2f, 0.1f, 0.12f, warn, TOP - 0.3f, 1.4f);
        Glow(s * 10.05f, 0.0f, 0.1f, 20.2f, 0.12f, warn, TOP - 0.3f, 1.4f);
    }

    // 手すり（階段の降り口を空ける）。軸に沿った線分で指定
    auto Rail = [&](float x0, float z0, float x1, float z1)
    {
        const float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
        const float sx = std::max(std::fabs(x1 - x0), 0.3f);
        const float sz = std::max(std::fabs(z1 - z0), 0.3f);
        Solid(cx, cz, sx, sz, 0.9f, dark, TOP);
        Glow (cx, cz, sx + 0.04f, sz + 0.04f, 0.06f, warn, TOP + 0.86f, 1.4f);
    };
    for (float s : { -1.0f, 1.0f })
    {
        Rail(-10.0f, s * 10.0f, -2.0f, s * 10.0f);
        Rail(  2.0f, s * 10.0f, 10.0f, s * 10.0f);
        Rail(s * 10.0f, -10.0f, s * 10.0f, -2.0f);
        Rail(s * 10.0f,   2.0f, s * 10.0f, 10.0f);
        // 脚
        for (float t : { -1.0f, 1.0f })
        {
            Solid(s * 9.0f, t * 9.0f, 1.0f, 1.0f, TOP - 0.5f, dark);
            Glow (s * 9.0f, t * 9.0f, 1.04f, 1.04f, 0.2f, ice, 2.0f, 1.0f);
        }
    }
    // 四方の階段（南・北・東・西からデッキの縁へ）
    Stairs(  0.0f, -24.0f, 0, 4.0f, 14.0f, TOP, metal);
    Stairs(  0.0f,  24.0f, 2, 4.0f, 14.0f, TOP, metal);
    Stairs( 24.0f,   0.0f, 3, 4.0f, 14.0f, TOP, metal);
    Stairs(-24.0f,   0.0f, 1, 4.0f, 14.0f, TOP, metal);

    // 掘削塔（デッキの中央。回る輪と、光る掘削ヘッド）
    SolidMesh(StageMesh::Cylinder, 0.0f, 0.0f, 3.0f, 3.0f, 10.0f, metal, TOP, 0.8f);
    for (float y : { TOP + 2.0f, TOP + 5.0f, TOP + 8.0f })
        Prop(StageMesh::Torus, { 0.0f, y, 0.0f }, { 4.4f, 1.0f, 4.4f }, ice, 1.8f,
             { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.8f, 0.0f });
    Prop(StageMesh::Octa, { 0.0f, TOP + 11.5f, 0.0f }, { 2.2f, 3.0f, 2.2f }, warn, 2.5f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.5f, 0.0f });
    // デッキ上の資材箱（遮蔽物）
    for (float s : { -1.0f, 1.0f })
    {
        Solid(s * 6.0f, 6.0f, 2.0f, 2.0f, 1.2f, dark, TOP);
        Solid(s * 6.0f, -6.0f, 2.0f, 2.0f, 1.2f, dark, TOP);
    }

    //--------------------------------------------------------------------------
    // 地上：氷の岩塊・結晶・コンベア・採掘機
    //--------------------------------------------------------------------------
    // 岩塊（遮蔽物。四分円ごとに散らす。階段とリグのまわりは避ける）
    for (int i = 0; i < 26; ++i)
    {
        const float x = RandRange(rng, -38.0f, 38.0f);
        const float z = RandRange(rng, -32.0f, 38.0f);
        if (std::fabs(x) < 16.0f && std::fabs(z) < 16.0f) continue;   // リグ
        if (std::fabs(x) < 4.0f || std::fabs(z) < 4.0f)  continue;   // 階段へ向かう道
        const float s = RandRange(rng, 2.0f, 4.5f);
        const float h = RandRange(rng, 1.4f, 4.0f);
        SolidMesh(StageMesh::Rock, x, z, s, s * RandRange(rng, 0.8f, 1.2f), h, rock, 0.0f, 0.7f);
    }

    // 光る氷の結晶（見た目だけ。2〜3本の束で岩の間に生える）
    for (int i = 0; i < 20; ++i)
    {
        const float x = RandRange(rng, -39.0f, 39.0f);
        const float z = RandRange(rng, -39.0f, 39.0f);
        if (std::fabs(x) < 12.0f && std::fabs(z) < 12.0f) continue;
        const int n = 2 + RandInt(rng, 2);
        for (int k = 0; k < n; ++k)
        {
            const float h = RandRange(rng, 1.4f, 3.6f);
            Prop(StageMesh::Octa, { x + RandRange(rng, -0.8f, 0.8f), h * 0.45f, z + RandRange(rng, -0.8f, 0.8f) },
                 { h * 0.32f, h, h * 0.32f }, ice, 2.6f,
                 { RandRange(rng, -0.35f, 0.35f), RandRange(rng, 0.0f, 3.0f), RandRange(rng, -0.35f, 0.35f) });
        }
    }
    // 床に張った氷（薄い板。見た目だけ）
    for (int i = 0; i < 10; ++i)
    {
        const float x = RandRange(rng, -36.0f, 36.0f);
        const float z = RandRange(rng, -36.0f, 36.0f);
        if (std::fabs(x) < 12.0f && std::fabs(z) < 12.0f) continue;
        Prop(StageMesh::Hex, { x, 0.02f, z }, { RandRange(rng, 4.0f, 9.0f), 0.04f, RandRange(rng, 4.0f, 9.0f) },
             { 0.30f, 0.55f, 0.70f }, 0.5f, { 0.0f, RandRange(rng, 0.0f, 3.0f), 0.0f });
    }

    // 鉱石のコンベア（東西に伸びる低い遮蔽物）
    for (float s : { -1.0f, 1.0f })
    {
        Solid(s * 28.0f, -18.0f, 14.0f, 1.4f, 1.1f, dark);
        Glow (s * 28.0f, -18.0f, 13.6f, 0.3f, 0.04f, warn, 1.1f, 1.2f);
        Solid(s * 28.0f, 18.0f, 14.0f, 1.4f, 1.1f, dark);
        Glow (s * 28.0f, 18.0f, 13.6f, 0.3f, 0.04f, warn, 1.1f, 1.2f);
    }

    // 停止中の採掘機（車体＋腕。大きな遮蔽物）
    const struct { float x, z, dir; } diggers[] = { { -26.0f, -30.0f, 1.0f }, { 28.0f, 30.0f, -1.0f }, { 30.0f, -32.0f, 1.0f } };
    for (const auto& d : diggers)
    {
        Solid(d.x, d.z, 6.0f, 4.0f, 2.6f, metal);
        Solid(d.x, d.z + d.dir * 0.5f, 3.5f, 2.6f, 1.6f, dark, 2.6f);
        Glow (d.x, d.z - d.dir * 2.0f, 4.0f, 0.1f, 0.3f, warn, 3.0f, 2.0f);
        Deco (d.x + 3.6f, d.z + d.dir * 2.0f, 1.0f, 5.0f, 0.8f, metal, 1.6f);   // 腕
    }

    // 外周の崖（壁沿いの大きな岩。見た目と当たり判定）
    for (int i = 0; i < 16; ++i)
    {
        const float t = -40.0f + i * 5.4f;
        for (float s : { -1.0f, 1.0f })
        {
            SolidMesh(StageMesh::Rock, s * 42.0f, t, 6.0f, 6.0f, RandRange(rng, 6.0f, 11.0f), rock, 0.0f, 0.6f);
            if (i % 2 == 0)
                SolidMesh(StageMesh::Rock, t, s * 42.0f, 6.0f, 6.0f, RandRange(rng, 6.0f, 11.0f), rock, 0.0f, 0.6f);
        }
    }

    // 照明（リグの四隅の上）
    for (float x : { -12.0f, 12.0f })
        for (float z : { -12.0f, 12.0f })
        {
            Solid(x, z, 0.6f, 0.6f, 12.0f, metal);
            Prop(StageMesh::Sphere, { x, 12.6f, z }, { 1.4f, 1.4f, 1.4f }, ice, 3.0f);
        }

    //--------------------------------------------------------------------------
    // 敵配置：リグの上から狙撃と砲撃、地上は脚付き・重装・機動
    //--------------------------------------------------------------------------
    Squad(-20.0f, -26.0f, 3, NORMAL);
    Squad( 20.0f, -24.0f, 2, WALKER, 5.0f);
    Squad(-28.0f,   0.0f, 2, TANK, 5.0f);
    Squad( 28.0f,   0.0f, 3, GUNNER, 5.0f);
    Squad(-22.0f,  26.0f, 3, SPEED, 5.0f);
    Squad( 22.0f,  24.0f, 2, WALKER, 5.0f);
    Squad(  0.0f,  32.0f, 2, ARTILLERY, 4.0f);
    Squad(-30.0f, -32.0f, 1, TURRET, 3.0f);
    Squad( 32.0f,  10.0f, 1, ORBITER, 4.0f);

    Squad( -5.0f,  -5.0f, 2, SNIPER,  3.0f, TOP);   // リグの上
    Squad(  5.0f,   5.0f, 2, GATLING, 3.0f, TOP);
    Squad(  6.0f,  -6.0f, 1, HALO,    2.0f, TOP);

    // ボス戦フェーズ（BASTION）の出現位置：リグの北の広場
    SetBoss(0.0f, 28.0f);

    // 増援の降下地点（四隅の空き地と東西）
    DropZone(-34.0f, -36.0f);
    DropZone( 34.0f, -36.0f);
    DropZone(-34.0f,  36.0f);
    DropZone( 34.0f,  36.0f);
    DropZone(-34.0f,   8.0f);
    DropZone( 34.0f,  -8.0f);

    // 遠景：雪をかぶった山並みと、採掘場を見下ろす輸送塔
    for (int i = 0; i < 16; ++i)
    {
        const float angle = DirectX::XM_2PI * (i + RandRange(rng, 0.0f, 1.0f)) / 16.0f;
        const float dist  = RandRange(rng, 110.0f, 190.0f);
        const float w     = RandRange(rng, 40.0f, 70.0f);
        const float h     = RandRange(rng, 30.0f, 70.0f);
        Prop(StageMesh::Pyramid, { cosf(angle) * dist, h * 0.5f, sinf(angle) * dist }, { w, h, w }, { 0.04f, 0.06f, 0.09f });
        Prop(StageMesh::Pyramid, { cosf(angle) * dist, h * 0.82f, sinf(angle) * dist }, { w * 0.36f, h * 0.36f, w * 0.36f },
             { 0.70f, 0.85f, 1.00f }, 1.2f);
    }
}

//==============================================================================
// CARRIER DECK（空母甲板）
// ・南北140mの飛行甲板。中央に発艦用のカタパルト線、斜めに着艦帯
// ・東側に艦橋（高さ6mの管制デッキ付き。階段で上れる）、西側に駐機中の機体の列
// ・北端の艦首（一段高い甲板）の先端がゴール。ボス戦は甲板の中央で行う
//==============================================================================
void BlockStageLayout_CarrierDeck(std::mt19937& rng)
{
    const Col white  = { 0.85f, 0.90f, 1.00f };
    const Col orange = { 1.00f, 0.38f, 0.15f };
    const Col yellow = { 1.00f, 0.80f, 0.20f };

    Theme theme{};
    theme.skyTop    = { 0.015f, 0.008f, 0.025f };
    theme.skyBottom = { 0.120f, 0.040f, 0.050f };
    theme.nebula    = { 1.00f, 0.40f, 0.30f, 0.40f };
    theme.fog       = { 0.090f, 0.035f, 0.045f };
    theme.fogStart  = 40.0f;
    theme.fogEnd    = 230.0f;
    theme.grid      = { 0.55f, 0.45f, 0.55f };
    theme.gridCell  = 6.0f;
    theme.ground    = { 0.020f, 0.022f, 0.028f };
    theme.ceiling   = false;
    theme.wall      = { 0.060f, 0.060f, 0.070f };
    theme.accent    = orange;
    theme.minimap   = { 0.85f, 0.85f, 1.00f };

    Begin(26.0f, 70.0f, 3.0f, theme);
    SetSpawn(-6.0f, -66.0f);

    const Col hull  = { 0.075f, 0.080f, 0.095f };
    const Col dark  = { 0.040f, 0.042f, 0.050f };
    const Col craft = { 0.100f, 0.105f, 0.120f };

    // 甲板の標示：発艦用のカタパルト線（2本）と、斜めの着艦帯（短い線を並べる）
    for (float x : { -10.0f, 2.0f })
    {
        Glow(x, -30.0f, 0.3f, 70.0f, 0.02f, yellow, 0.0f, 1.0f);
        Solid(x, -66.0f, 3.0f, 1.2f, 0.9f, dark);   // 発艦の衝立（遮蔽物）
        Glow (x, -66.0f, 3.0f, 0.1f, 0.06f, orange, 0.86f, 1.6f);
    }
    for (int i = 0; i < 14; ++i)
    {
        const float z = -20.0f + i * 5.0f;
        const float x = -12.0f + i * 1.3f;
        Glow(x, z, 3.0f, 0.2f, 0.02f, white, 0.0f, 0.8f);
    }
    // 甲板の縁の照明
    for (float z = -66.0f; z <= 62.0f; z += 6.0f)
        for (float s : { -1.0f, 1.0f })
            Prop(StageMesh::Sphere, { s * 25.4f, 0.3f, z }, { 0.4f, 0.4f, 0.4f }, (s < 0.0f) ? orange : white, 3.5f);

    //--------------------------------------------------------------------------
    // 艦橋（東側）。地上は大きな壁、北側に高さ6mの管制デッキと階段
    //--------------------------------------------------------------------------
    constexpr float BRIDGE_DECK = 6.0f;
    Building(19.0f, 4.0f, 10.0f, 26.0f, 15.0f, hull, orange);
    Solid(19.0f, 4.0f, 6.0f, 14.0f, 6.0f, hull, 15.0f);   // 上部構造
    Glow (16.0f, 4.0f, 0.1f, 12.0f, 1.0f, white, 17.5f, 2.0f);   // 艦橋の窓
    SolidMesh(StageMesh::Cylinder, 19.0f, 4.0f, 1.0f, 1.0f, 8.0f, hull, 21.0f, 0.8f);   // マスト
    Prop(StageMesh::Torus, { 19.0f, 27.0f, 4.0f }, { 5.0f, 0.6f, 5.0f }, orange, 2.0f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    Platform(19.0f, 22.0f, 10.0f, 10.0f, BRIDGE_DECK, hull);
    // 手すり：西の縁と、北の縁（階段の降り口 x 17〜21 を空ける）
    const struct { float cx, cz, sx, sz; } bridgeRails[] =
    {
        { 14.2f, 22.0f, 0.4f, 10.0f }, { 15.5f, 26.8f, 3.0f, 0.4f }, { 22.5f, 26.8f, 3.0f, 0.4f },
    };
    for (const auto& r : bridgeRails)
    {
        Solid(r.cx, r.cz, r.sx, r.sz, 1.0f, dark, BRIDGE_DECK);
        Glow (r.cx, r.cz, r.sx + 0.04f, r.sz + 0.04f, 0.06f, orange, BRIDGE_DECK + 0.96f, 1.4f);
    }
    Stairs(19.0f, 41.0f, 2, 4.0f, 14.0f, BRIDGE_DECK, hull);   // 北から南へ上る（デッキの北の縁へ）

    // 甲板用エレベーター（四角い光の枠）
    for (const auto& e : { XMFLOAT3{ -18.0f, 0.0f, -40.0f }, XMFLOAT3{ -18.0f, 0.0f, 30.0f } })
    {
        Deco(e.x, e.z, 12.0f, 12.0f, 0.02f, dark);
        Glow(e.x, e.z - 6.0f, 12.0f, 0.2f, 0.03f, yellow, 0.0f, 1.2f);
        Glow(e.x, e.z + 6.0f, 12.0f, 0.2f, 0.03f, yellow, 0.0f, 1.2f);
    }

    // 駐機中の機体（西側の列。南北どちらかへ機首）
    for (float z : { -50.0f, -28.0f, -6.0f, 16.0f, 44.0f })
        ParkedCraft(-18.0f, z, (RandInt(rng, 2) == 0) ? 1.0f : -1.0f, craft, orange);
    ParkedCraft(6.0f, -20.0f, 1.0f, craft, white);
    ParkedCraft(-4.0f, 26.0f, -1.0f, craft, white);

    // 牽引車・弾薬カート（小さな遮蔽物）
    const struct { float x, z; } carts[] =
    {
        { 8.0f, -52.0f }, { -6.0f, -38.0f }, { 10.0f, -32.0f }, { 4.0f, -6.0f }, { -8.0f, 6.0f },
        { 8.0f, 14.0f }, { 6.0f, 36.0f }, { -10.0f, 40.0f }, { 4.0f, 52.0f },
    };
    for (const auto& c : carts)
    {
        Solid(c.x, c.z, 1.8f, 2.6f, 1.2f, dark);
        Glow (c.x, c.z, 1.84f, 2.64f, 0.05f, yellow, 0.8f, 1.0f);
    }

    //--------------------------------------------------------------------------
    // 北端の艦首（一段高い甲板。左右の斜路から上る）。先端がゴール
    //--------------------------------------------------------------------------
    constexpr float BOW = 1.5f;
    Platform(0.0f, 64.0f, 40.0f, 12.0f, BOW, hull);
    Glow(0.0f, 58.05f, 40.0f, 0.1f, 0.1f, orange, BOW - 0.1f, 2.0f);
    for (float x : { -14.0f, 14.0f })
        Stairs(x, 52.0f, 0, 6.0f, 6.0f, BOW, hull);
    Solid(0.0f, 59.0f, 14.0f, 0.5f, 1.0f, dark, BOW);   // 艦首の衝立（中央は遮蔽物）
    Glow (0.0f, 59.0f, 14.0f, 0.54f, 0.06f, orange, BOW + 0.96f, 1.4f);
    Prop(StageMesh::Pyramid, { 0.0f, BOW + 0.5f, 69.0f }, { 3.0f, 1.0f, 3.0f }, orange, 2.5f);
    SetGoal(0.0f, 65.0f, BOW);

    //--------------------------------------------------------------------------
    // 敵配置：甲板の上を翼型が飛び交い、機体の陰から突撃型。艦橋の上から狙撃
    //--------------------------------------------------------------------------
    Squad(  0.0f, -48.0f, 3, SPEED, 5.0f);
    Squad(-14.0f, -34.0f, 2, GUNNER, 4.0f);
    Squad(  8.0f, -26.0f, 2, WING, 5.0f);
    Squad(  0.0f, -10.0f, 2, TANK, 4.0f);
    Squad(-12.0f,   4.0f, 3, BOMBER, 4.0f);
    Squad(  6.0f,  20.0f, 2, WING, 5.0f);
    Squad(-14.0f,  24.0f, 2, GATLING, 4.0f);
    Squad(  4.0f,  40.0f, 3, GUNNER, 5.0f);
    Squad(-10.0f,  48.0f, 1, TURRET, 3.0f);
    Squad( 10.0f,  48.0f, 2, WALKER, 4.0f);
    Squad( 19.0f,  22.0f, 2, SNIPER, 3.0f, BRIDGE_DECK);   // 艦橋の管制デッキ
    Squad(  0.0f,  64.0f, 2, ORBITER, 4.0f, BOW);          // 艦首

    // ボス戦フェーズ（NEST）の出現位置：甲板の中央
    SetBoss(-4.0f, 10.0f);

    // 増援の降下地点（甲板の両舷）
    DropZone(-22.0f, -54.0f);
    DropZone(  8.0f, -58.0f);
    DropZone(-22.0f,   0.0f);
    DropZone(  6.0f,  30.0f);
    DropZone(-22.0f,  46.0f);
    DropZone( 10.0f,  54.0f);

    // 遠景：随伴艦（左右の海の上）と、夕焼けの空を横切る編隊
    for (int i = 0; i < 6; ++i)
    {
        const float side = (i % 2) ? 1.0f : -1.0f;
        const float x = side * RandRange(rng, 90.0f, 160.0f);
        const float z = RandRange(rng, -120.0f, 160.0f);
        const float l = RandRange(rng, 50.0f, 90.0f);
        Prop(StageMesh::Cube, { x, -2.0f, z }, { l * 0.18f, 10.0f, l }, hull);
        Prop(StageMesh::Cube, { x, 6.0f, z + l * 0.1f }, { l * 0.1f, 10.0f, l * 0.16f }, hull);
        Prop(StageMesh::Sphere, { x, 12.0f, z + l * 0.1f }, { 1.6f, 1.6f, 1.6f }, orange, 4.0f);
    }
    for (int i = 0; i < 5; ++i)
        Prop(StageMesh::Pyramid, { -60.0f + i * 12.0f, 70.0f + i * 3.0f, 180.0f - i * 8.0f }, { 4.0f, 1.2f, 6.0f },
             white, 1.5f, { DirectX::XM_PIDIV2, 0.0f, 0.0f });
}
