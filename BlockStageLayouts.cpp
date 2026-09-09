/*==============================================================================

   ブロックステージ配置 [BlockStageLayouts.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   各ステージの配置。座標・高さ・色の約束は BlockStageBuilder.h を参照。
   プレイヤーは南（-Z）から開始し、北（+Z）へ向かって進む。

   ■ステージ一覧（見た目のテーマ）
     City     : NEON FRONTIER   88 × 108m  紫の星雲と水色グリッドのネオン都市。
                                           街区とビル群、北端のヘリパッドがゴール
     Terminal : ORBITAL DOCK    92 ×  76m  琥珀色のグリッドの貨物ドック。
                                           コンテナヤード、倉庫、ガントリークレーン
     Fortress : FORTRESS GATE   88 ×  88m  赤い星雲と橙のランプの要塞。
                                           外壁と4つの門、中央の司令棟とコア
     Trench   : ASTEROID BELT   44 × 124m  青緑の星雲と岩塊の渓谷。
                                           隔壁3枚、北端の搬入口がゴール
     Arena    : OMEGA CORE      56 ×  56m  天井のある八角形のボス戦アリーナ
     Plant    : REACTOR PLANT   80 ×  92m  緑の星雲の二層プラント。地上階の機械群と
                                           高さ5mのデッキ網を階段でつなぐ。北デッキがゴール

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
}

//==============================================================================
// NEON FRONTIER（廃棄市街）
// ・南北3本の大通りと東西4本の通りで区切られた 4×5 の街区
// ・街区ごとに「低層ビル / 塔＋別棟 / 2棟 / 廃墟 / 広場」のいずれかを建てる
// ・中央大通りの北端を塞ぐヘリパッドがゴール（段差を登って到達する）
//==============================================================================
void BlockStageLayout_City(std::mt19937& rng)
{
    const Col cyan    = { 0.15f, 0.80f, 1.00f };
    const Col magenta = { 1.00f, 0.20f, 0.80f };

    Theme theme{};
    theme.skyTop    = { 0.010f, 0.012f, 0.050f };
    theme.skyBottom = { 0.100f, 0.030f, 0.200f };
    theme.nebula    = { 0.90f, 0.25f, 0.80f, 0.50f };
    theme.fog       = { 0.090f, 0.035f, 0.170f };
    theme.fogStart  = 40.0f;
    theme.fogEnd    = 230.0f;
    theme.grid      = { 0.10f, 0.80f, 1.00f };
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.010f, 0.012f, 0.028f };
    theme.ceiling   = false;
    theme.wall      = { 0.040f, 0.050f, 0.100f };
    theme.accent    = cyan;
    theme.minimap   = { 0.45f, 0.75f, 1.00f };

    Begin(44.0f, 54.0f, 10.0f, theme);
    SetSpawn(0.0f, -51.0f);

    const Col body   = { 0.050f, 0.070f, 0.150f };
    const Col body2  = { 0.070f, 0.060f, 0.140f };
    const Col rubble = { 0.060f, 0.060f, 0.090f };
    const Col pave   = { 0.022f, 0.026f, 0.050f };
    const Col lamp   = { 1.00f, 0.85f, 0.60f };

    // 街区の中心。大通り x=-22,0,22／通り z=-30,-8,14,36（いずれも幅8m）
    const float colX[4] = { -33.0f, -11.0f, 11.0f, 33.0f };
    const float rowZ[5] = { -41.0f, -19.0f, 3.0f, 25.0f, 45.0f };
    constexpr float BLOCK_W = 14.0f;

    // 大通りの中央線
    for (float x : { -22.0f, 0.0f, 22.0f })
        Glow(x, 0.0f, 0.2f, 104.0f, 0.02f, cyan, 0.0f, 0.5f);
    for (float z : { -30.0f, -8.0f, 14.0f, 36.0f })
        Glow(0.0f, z, 84.0f, 0.2f, 0.02f, cyan, 0.0f, 0.5f);

    for (int r = 0; r < 5; ++r)
    {
        const float bd = (r == 4) ? 10.0f : 14.0f;   // 最北列だけ奥行きが浅い
        for (int c = 0; c < 4; ++c)
        {
            const float bx = colX[c];
            const float bz = rowZ[r];

            Deco(bx, bz, BLOCK_W, bd, 0.03f, pave);   // 歩道

            // ヘリパッドの両脇は必ず塔にして、ゴールを遠くからでも分かるようにする
            int pattern = RandInt(rng, 5);
            if (r == 4 && (c == 1 || c == 2)) pattern = 1;

            const Col& b    = (RandInt(rng, 2) == 0) ? body : body2;
            const Col& neon = (RandInt(rng, 3) == 0) ? magenta : cyan;

            switch (pattern)
            {
            case 0:   // 低層ビル1棟
                Building(bx, bz, 12.0f, bd - 2.0f, RandRange(rng, 5.0f, 9.0f), b, neon);
                break;

            case 1:   // 塔＋別棟
            {
                const float side = (c < 2) ? -1.0f : 1.0f;   // 塔は外側へ寄せる
                Building(bx + side * 3.0f, bz, 7.0f, 7.0f, RandRange(rng, 11.0f, 17.0f), b, neon);
                Building(bx - side * 3.6f, bz, 4.5f, bd - 3.0f, RandRange(rng, 3.0f, 4.5f), b, neon);
                break;
            }

            case 2:   // 2棟（間は幅2mの路地）
                Building(bx - 3.6f, bz, 5.2f, bd - 2.0f, RandRange(rng, 6.0f, 12.0f), b, neon);
                Building(bx + 3.6f, bz, 5.2f, bd - 2.0f, RandRange(rng, 4.0f, 8.0f), b,
                         (RandInt(rng, 2) == 0) ? magenta : cyan);
                break;

            case 3:   // 廃墟（崩れた壁と基礎。ネオンは切れている）
                Solid(bx - 3.0f, bz - 2.0f, 5.0f, 4.0f, RandRange(rng, 2.0f, 3.2f), rubble);
                Solid(bx + 3.5f, bz + 3.0f, 4.0f, 1.2f, RandRange(rng, 1.2f, 2.0f), rubble);
                Solid(bx + 2.0f, bz - 3.0f, 1.2f, 4.0f, RandRange(rng, 1.5f, 2.6f), rubble);
                Solid(bx - 4.5f, bz + 3.5f, 2.0f, 2.0f, RandRange(rng, 0.9f, 1.4f), rubble);
                break;

            default:  // 広場（台座の上で回る八面体と輪）
                SolidMesh(StageMesh::Hex, bx, bz, 3.4f, 3.4f, 1.6f, body, 0.0f, 0.8f);
                Prop(StageMesh::Octa,  { bx, 3.6f, bz }, { 1.0f, 1.8f, 1.0f }, neon, 2.5f,
                     { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.9f, 0.0f });
                Prop(StageMesh::Torus, { bx, 3.6f, bz }, { 4.4f, 2.4f, 4.4f }, neon, 1.6f,
                     { 0.5f, 0.0f, 0.3f }, { 0.0f, -0.5f, 0.0f });
                Solid(bx - 4.5f, bz, 1.0f, 5.0f, 0.9f, rubble);
                Solid(bx + 4.5f, bz, 1.0f, 5.0f, 0.9f, rubble);
                break;
            }
        }
    }

    // 路上のバリケード（低い遮蔽物。上面に警告ラインが光る）
    const struct { float x, z, sx, sz; } barriers[] =
    {
        {  -2.0f, -36.0f, 3.0f, 0.9f }, {   2.0f, -22.0f, 3.0f, 0.9f },
        { -22.0f, -30.0f, 0.9f, 3.0f }, {  22.0f, -10.0f, 3.0f, 0.9f },
        {   0.0f,  -8.0f, 0.9f, 3.0f }, { -20.0f,  14.0f, 3.0f, 0.9f },
        {   2.0f,   6.0f, 3.0f, 0.9f }, {  -2.0f,  20.0f, 3.0f, 0.9f },
        {  22.0f,  28.0f, 0.9f, 3.0f }, {   0.0f,  32.0f, 3.0f, 0.9f },
    };
    for (const auto& b : barriers)
    {
        Solid(b.x, b.z, b.sx, b.sz, 1.0f, rubble);
        Glow (b.x, b.z, b.sx * 0.9f, b.sz * 0.9f, 0.04f, magenta, 1.0f, 1.2f);
    }

    // 放置車両（荷台＋運転席）
    const struct { float x, z; } wrecks[] = { { -23.0f, -19.0f }, { 21.0f, 5.0f }, { 1.5f, 25.0f } };
    for (const auto& w : wrecks)
    {
        Solid(w.x, w.z,        2.2f, 5.0f, 2.0f, rubble);
        Solid(w.x, w.z - 3.4f, 2.2f, 1.6f, 2.6f, body);
    }

    // 街灯（見た目のみ）。大通りの両側に互い違いに立てる
    const float avenues[3] = { -22.0f, 0.0f, 22.0f };
    for (int a = 0; a < 3; ++a)
    {
        for (int r = 0; r < 4; ++r)
        {
            const float side = ((a + r) % 2 == 0) ? 1.0f : -1.0f;
            const float x = avenues[a] + side * 4.6f;
            Deco(x, rowZ[r], 0.2f, 0.2f, 5.0f, rubble);
            Prop(StageMesh::Sphere, { x, 5.2f, rowZ[r] }, { 0.55f, 0.55f, 0.55f }, lamp, 3.0f);
        }
    }

    // ヘリパッド（ゴール）。手前の段を経由して登る
    Solid(0.0f, 41.5f, 4.0f, 3.0f, 1.2f, body);
    Solid(0.0f, 47.0f, 8.0f, 8.0f, 2.4f, body);
    Glow (0.0f, 47.0f, 8.3f, 8.3f, 0.3f, cyan, 2.09f, 2.5f);
    Prop (StageMesh::Torus, { 0.0f, 2.55f, 47.0f }, { 6.6f, 1.2f, 6.6f }, cyan, 2.0f);
    SetGoal(0.0f, 47.0f, 2.4f);

    //--------------------------------------------------------------------------
    // 敵配置：交差点ごとに防衛線を張り、北へ進むほど硬くなる
    //   第1線 z=-30 : 中央に近接、左右の大通りに狙撃
    //   第2線 z=-8  : 中央に高機動、左右に重装と近接
    //   第3線 z=14  : 中央に狙撃＋重装、左右に高機動（側面から回り込んでくる）
    //   最終線 z=36 : ヘリパッド前に狙撃と重装を集中
    //--------------------------------------------------------------------------
    Squad(  0.0f, -30.0f, 3, NORMAL);
    Squad(-22.0f, -30.0f, 2, SNIPER);
    Squad( 22.0f, -30.0f, 2, SNIPER);

    Squad(  0.0f,  -8.0f, 3, SPEED);
    Squad(-22.0f,  -8.0f, 2, TANK);
    Squad( 22.0f,  -8.0f, 3, GUNNER);

    Squad(  0.0f,  14.0f, 3, SNIPER);
    Squad(  0.0f,  18.0f, 1, TANK);
    Squad(-22.0f,  14.0f, 3, BOMBER);
    Squad( 22.0f,  14.0f, 3, SPEED);
    Squad( 11.0f,   3.0f, 2, PHANTOM);
    Squad( 33.0f,   3.0f, 2, WING,    5.0f);   // 東の大通りの上空を旋回
    Squad(-33.0f,   3.0f, 1, ORBITER, 5.0f);

    Squad(  0.0f,  36.0f, 3, SNIPER);
    Squad(-11.0f,  36.0f, 2, TANK);
    Squad( 11.0f,  36.0f, 1, TURRET, 3.0f);
    Squad(-22.0f,  36.0f, 1, HALO,   4.0f);

    // 増援の降下地点（外周の通り）
    DropZone(-41.0f,   0.0f);
    DropZone( 41.0f,   0.0f);
    DropZone(-41.0f,  30.0f);
    DropZone( 41.0f,  30.0f);
    DropZone(-41.0f, -30.0f);
    DropZone( 41.0f, -30.0f);

    // 遠景：外周壁の向こうに広がる高層ビル群と、空に浮かぶ軌道リング
    Skyline(rng, 64, body, cyan, magenta);
    Prop(StageMesh::Torus, { -40.0f, 75.0f, 170.0f }, { 150.0f, 60.0f, 150.0f }, magenta, 0.9f,
         { 1.1f, 0.0f, 0.35f }, { 0.0f, 0.03f, 0.0f });
}

//==============================================================================
// ORBITAL DOCK（輸送ターミナル）
// ・東西に伸びるコンテナ列が4本。列の途中に切れ目があり、南北にも抜けられる
// ・コンテナは1〜3段積み。上に乗って撃ち下ろせる
// ・北側は倉庫と荷捌き場、東に燃料タンク、ヤードをまたぐガントリークレーンが2基
//==============================================================================
void BlockStageLayout_Terminal(std::mt19937& rng)
{
    const Col amber = { 1.00f, 0.72f, 0.18f };

    Theme theme{};
    theme.skyTop    = { 0.004f, 0.008f, 0.016f };
    theme.skyBottom = { 0.015f, 0.050f, 0.060f };
    theme.nebula    = { 0.25f, 0.90f, 0.65f, 0.28f };
    theme.fog       = { 0.012f, 0.040f, 0.045f };
    theme.fogStart  = 40.0f;
    theme.fogEnd    = 220.0f;
    theme.grid      = amber;
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.012f, 0.013f, 0.016f };
    theme.ceiling   = false;
    theme.wall      = { 0.050f, 0.055f, 0.060f };
    theme.accent    = amber;
    theme.minimap   = { 1.00f, 0.80f, 0.45f };

    Begin(46.0f, 38.0f, 8.0f, theme);
    SetSpawn(0.0f, -34.0f);

    // コンテナの識別色（上面の縁だけが光る。本体はその色を暗くしたもの）
    const Col containerNeon[6] =
    {
        { 1.00f, 0.30f, 0.20f }, { 0.20f, 0.55f, 1.00f }, { 0.25f, 1.00f, 0.50f },
        { 1.00f, 0.80f, 0.20f }, { 0.85f, 0.85f, 0.95f }, { 1.00f, 0.25f, 0.75f },
    };
    const Col steel = { 0.100f, 0.085f, 0.040f };
    const Col dark  = { 0.045f, 0.050f, 0.060f };
    const Col lamp  = { 1.00f, 0.85f, 0.60f };

    constexpr float CONTAINER_L = 6.0f;
    constexpr float CONTAINER_W = 2.5f;
    constexpr float CONTAINER_H = 2.6f;

    // 通路のライン
    for (float z : { -28.0f, -16.0f, -4.0f, 8.0f })
        Glow(0.0f, z, 84.0f, 0.25f, 0.02f, amber, 0.0f, 0.5f);

    // コンテナ列（2個並びを1区画として、9区画 × 4列）
    for (float rowZ : { -22.0f, -10.0f, 2.0f, 14.0f })
    {
        for (int i = 0; i < 9; ++i)
        {
            if (RandInt(rng, 10) < 3) continue;   // 切れ目

            const float x = -36.0f + i * 9.0f;
            for (float offset : { -1.3f, 1.3f })
            {
                if (RandInt(rng, 8) == 0) continue;
                const int levels = 1 + RandInt(rng, 3);
                for (int lv = 0; lv < levels; ++lv)
                {
                    const Col& neon = containerNeon[RandInt(rng, 6)];
                    const float base = lv * CONTAINER_H;
                    Solid(x, rowZ + offset, CONTAINER_L, CONTAINER_W, CONTAINER_H, Scale(neon, 0.09f), base);
                    Glow (x, rowZ + offset, CONTAINER_L + 0.05f, CONTAINER_W + 0.05f, 0.08f, neon,
                          base + CONTAINER_H - 0.09f, 1.0f);
                }
            }
        }
    }

    // ガントリークレーン。脚は飛行上限より高く、桁はその上を渡る（届かない高さ）
    for (float cx : { -18.0f, 18.0f })
    {
        for (float legZ : { -28.0f, 20.0f })
        {
            Solid(cx - 3.5f, legZ, 1.4f, 1.4f, 31.5f, steel);
            Solid(cx + 3.5f, legZ, 1.4f, 1.4f, 31.5f, steel);
            Solid(cx, legZ, 7.0f, 0.6f, 0.6f, steel, 10.0f);   // 脚をつなぐ横梁
            Glow (cx - 3.5f, legZ, 1.5f, 1.5f, 0.2f, amber, 6.0f);
            Glow (cx + 3.5f, legZ, 1.5f, 1.5f, 0.2f, amber, 6.0f);
        }
        Deco(cx, -4.0f, 8.0f, 54.0f, 2.0f, steel, 31.5f);
        for (float z : { -20.0f, -4.0f, 12.0f })
            Prop(StageMesh::Sphere, { cx, 31.0f, z }, { 1.2f, 1.2f, 1.2f }, lamp, 3.0f);
    }

    // 倉庫と荷捌き場
    Building(-22.0f, 32.0f, 34.0f, 10.0f, 7.0f, dark, amber);
    Building( 24.0f, 32.5f, 26.0f,  9.0f, 9.5f, dark, amber);
    for (float x : { -34.0f, -26.0f, -18.0f, -10.0f })
        Glow(x, 27.0f, 4.0f, 0.08f, 3.6f, amber, 0.0f, 0.6f);   // シャッター
    Solid(-22.0f, 25.5f, 34.0f, 3.0f, 1.2f, dark);
    Glow (-22.0f, 24.0f, 34.0f, 0.08f, 0.12f, amber, 1.0f, 1.5f);

    // 駐車中のトレーラー
    for (float x : { -34.0f, -28.0f, 30.0f })
    {
        const Col& neon = containerNeon[RandInt(rng, 6)];
        Solid(x, -31.0f, 2.6f, 7.0f, 2.8f, Scale(neon, 0.09f));
        Glow (x, -31.0f, 2.65f, 7.05f, 0.08f, neon, 2.71f, 1.0f);
        Solid(x, -26.6f, 2.4f, 1.6f, 2.2f, dark);
    }

    // 燃料タンク（東側）。輪が光る
    for (float z : { -14.0f, -4.0f })
    {
        SolidMesh(StageMesh::Cylinder, 41.0f, z, 6.0f, 6.0f, 7.0f, dark);
        Prop(StageMesh::Torus, { 41.0f, 5.0f, z }, { 7.6f, 2.0f, 7.6f }, amber, 1.6f);
    }

    // 照明塔（四隅）
    for (float x : { -42.0f, 42.0f })
        for (float z : { -34.0f, 21.0f })
        {
            Solid(x, z, 0.9f, 0.9f, 16.0f, steel);
            Prop(StageMesh::Sphere, { x, 16.8f, z }, { 1.8f, 1.8f, 1.8f }, lamp, 3.0f);
        }

    // 通路に置かれた資材（小さな遮蔽物）
    const struct { float x, z; } crates[] =
    {
        { -31.0f, -16.0f }, { -9.0f, -16.5f }, { 12.0f, -15.5f },
        { -22.0f,  -4.0f }, { 27.0f,  -4.5f }, {  4.0f,   8.0f }, { -38.0f, 8.5f },
    };
    for (const auto& c : crates)
        Solid(c.x, c.z, 1.4f, 1.4f, 1.2f, Scale(containerNeon[RandInt(rng, 6)], 0.12f));

    //--------------------------------------------------------------------------
    // 敵配置：コンテナ列の間の通路を高機動型が巡回し、通路の奥から狙撃型が撃つ
    //   通路の左右両端に高機動を置き、どこへ逃げても挟まれるようにする
    //--------------------------------------------------------------------------
    Squad(-30.0f, -16.0f, 3, SPEED);
    Squad( 30.0f, -16.0f, 3, SPEED);
    Squad(  0.0f, -16.0f, 2, SNIPER);

    Squad(-20.0f,  -4.0f, 3, BOMBER);
    Squad( 20.0f,  -4.0f, 3, SPEED);
    Squad(  0.0f,  -4.0f, 2, GUNNER);

    Squad(-30.0f,   8.0f, 2, SNIPER);
    Squad( 30.0f,   8.0f, 2, SNIPER);
    Squad(  0.0f,   8.0f, 3, SPEED);

    Squad(-10.0f,  20.0f, 3, GUNNER);
    Squad( 25.0f,  20.0f, 2, TANK);
    Squad(-30.0f,  20.0f, 1, TURRET, 3.0f);
    Squad( 10.0f,   8.0f, 1, GATLING, 5.0f);
    Squad(-36.0f,  -4.0f, 2, WING,    5.0f);

    // ボス戦フェーズ（ARGUS）の出現位置：中央の通路
    SetBoss(0.0f, -4.0f);

    // 増援の降下地点（ヤードの四隅と倉庫前）
    DropZone(-43.0f, -16.0f);
    DropZone( 43.0f,   8.0f);
    DropZone(-43.0f,   8.0f);
    DropZone(  0.0f,  20.0f);
    DropZone( 36.0f, -30.0f);

    // 遠景：沖に停泊する貨物船（外周壁の外・上空）
    const struct { float x, y, z, len; } ships[] =
    {
        { -120.0f, 38.0f,  90.0f, 80.0f }, { 130.0f, 55.0f, 140.0f, 110.0f }, { 20.0f, 70.0f, 200.0f, 90.0f },
    };
    for (const auto& s : ships)
    {
        Prop(StageMesh::Cube, { s.x, s.y, s.z }, { s.len, 12.0f, 20.0f }, dark);
        Prop(StageMesh::Cube, { s.x, s.y - 3.0f, s.z }, { s.len * 1.01f, 0.5f, 20.3f }, amber, 1.6f);
        Prop(StageMesh::Cube, { s.x + s.len * 0.3f, s.y + 9.0f, s.z }, { 12.0f, 6.0f, 12.0f }, dark);
    }
}

//==============================================================================
// FORTRESS GATE（要塞）
// ・一辺52mの外壁（高さ5m）で囲まれ、東西南北に幅8mの門がある
// ・四隅に監視塔、中央に2段構えの司令棟。その上でコアが回っている
// ・南門の外には対装甲障害（低い四角錐の千鳥配置）
//==============================================================================
void BlockStageLayout_Fortress(std::mt19937& rng)
{
    const Col lamp = { 1.00f, 0.40f, 0.10f };

    Theme theme{};
    theme.skyTop    = { 0.030f, 0.008f, 0.008f };
    theme.skyBottom = { 0.080f, 0.020f, 0.015f };
    theme.nebula    = { 1.00f, 0.40f, 0.15f, 0.30f };
    theme.fog       = { 0.100f, 0.025f, 0.015f };
    theme.fogStart  = 35.0f;
    theme.fogEnd    = 200.0f;
    theme.grid      = { 1.00f, 0.38f, 0.10f };
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.020f, 0.012f, 0.010f };
    theme.ceiling   = false;
    theme.wall      = { 0.070f, 0.055f, 0.060f };
    theme.accent    = lamp;
    theme.minimap   = { 1.00f, 0.60f, 0.45f };

    Begin(44.0f, 44.0f, 7.0f, theme);
    SetSpawn(0.0f, -40.0f);
    SetEnemyMinDistance(18.0f);

    const Col metal = { 0.130f, 0.110f, 0.110f };
    const Col wall  = { 0.070f, 0.055f, 0.060f };
    const Col yard  = { 0.030f, 0.020f, 0.018f };

    constexpr float R      = 26.0f;   // 外壁の中心線までの距離
    constexpr float WALL_H = 5.0f;
    constexpr float SEG    = 23.0f;   // 門の片側の壁の長さ
    constexpr float SEG_C  = 15.5f;   // その中心

    // 構内の舗装
    Deco(0.0f, 0.0f, 50.0f, 50.0f, 0.02f, yard);

    // 外壁と門柱（門柱には縦のランプ）
    for (float s : { -1.0f, 1.0f })
    {
        Solid(s * SEG_C, -R, SEG, 2.0f, WALL_H, wall);   // 南
        Solid(s * SEG_C,  R, SEG, 2.0f, WALL_H, wall);   // 北
        Solid(-R, s * SEG_C, 2.0f, SEG, WALL_H, wall);   // 西
        Solid( R, s * SEG_C, 2.0f, SEG, WALL_H, wall);   // 東

        // 外壁の上端のライン
        Glow(s * SEG_C, -R, SEG, 2.06f, 0.15f, lamp, WALL_H - 0.5f, 1.2f);
        Glow(s * SEG_C,  R, SEG, 2.06f, 0.15f, lamp, WALL_H - 0.5f, 1.2f);
        Glow(-R, s * SEG_C, 2.06f, SEG, 0.15f, lamp, WALL_H - 0.5f, 1.2f);
        Glow( R, s * SEG_C, 2.06f, SEG, 0.15f, lamp, WALL_H - 0.5f, 1.2f);

        for (float gz : { -R, R })
        {
            Solid(s * 5.2f, gz, 2.4f, 3.0f, 7.5f, metal);
            Glow (s * 4.0f, gz, 0.2f, 0.5f, 6.0f, lamp, 0.6f, 2.0f);
        }
        for (float gx : { -R, R })
        {
            Solid(gx, s * 5.2f, 3.0f, 2.4f, 7.5f, metal);
            Glow (gx, s * 4.0f, 0.5f, 0.2f, 6.0f, lamp, 0.6f, 2.0f);
        }
    }

    // 監視塔（四隅）。上にすぼまった見張り台が載る
    for (float a : { -1.0f, 1.0f })
        for (float b : { -1.0f, 1.0f })
        {
            Building(a * R, b * R, 6.0f, 6.0f, 9.0f, metal, lamp);
            SolidMesh(StageMesh::Frustum, a * R, b * R, 4.4f, 4.4f, 2.6f, wall, 9.0f, 0.8f);
        }

    // 司令棟（2段）と正面の段。屋上でコアと輪が回っている
    Building(0.0f, 3.0f, 14.0f, 12.0f, 7.0f, metal, lamp);
    Solid(0.0f, 3.0f, 8.0f, 6.0f, 4.0f, wall, 7.0f);
    Glow (0.0f, 3.0f, 8.3f, 6.3f, 0.3f, lamp, 10.69f, 2.5f);
    Solid(0.0f, -4.5f, 6.0f, 3.0f, 1.2f, metal);
    Prop(StageMesh::Octa,  { 0.0f, 15.0f, 3.0f }, { 2.6f, 3.6f, 2.6f }, lamp, 3.0f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.8f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 15.0f, 3.0f }, { 6.5f, 6.5f, 6.5f }, lamp, 1.6f,
         { 0.4f, 0.0f, 0.2f }, { 0.0f, -0.4f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 15.0f, 3.0f }, { 8.5f, 8.5f, 8.5f }, lamp, 1.2f,
         { -0.5f, 0.0f, 0.6f }, { 0.0f, 0.25f, 0.0f });

    // トーチカ（上がすぼまった形。銃眼が南を向く）
    for (float a : { -1.0f, 1.0f })
        for (float z : { -13.0f, 16.0f })
        {
            SolidMesh(StageMesh::Frustum, a * 15.0f, z, 7.5f, 5.5f, 2.4f, metal, 0.0f, 0.85f);
            Glow(a * 15.0f, z - 2.45f, 3.6f, 0.08f, 0.2f, lamp, 1.5f, 2.5f);
        }

    // 構内のバリケード
    const struct { float x, z, sx, sz; } barricades[] =
    {
        { -7.0f, -19.0f, 6.0f, 1.0f }, { 7.0f, -19.0f, 6.0f, 1.0f }, { 0.0f, -14.0f, 5.0f, 1.0f },
        { -16.0f,  2.0f, 1.0f, 6.0f }, { 16.0f,  2.0f, 1.0f, 6.0f }, { 0.0f,  18.0f, 8.0f, 1.0f },
    };
    for (const auto& b : barricades)
    {
        Solid(b.x, b.z, b.sx, b.sz, 1.1f, metal);
        Glow (b.x, b.z, b.sx * 0.9f, b.sz * 0.9f, 0.04f, lamp, 1.1f, 1.2f);
    }

    // 南門前の対装甲障害（中央の通路だけ空ける）
    for (float x = -18.0f; x <= 18.0f; x += 4.0f)
        if (x < -4.0f || x > 4.0f) SolidMesh(StageMesh::Pyramid, x, -32.0f, 1.8f, 1.8f, 1.5f, metal, 0.0f, 0.6f, 0.7f);
    for (float x = -16.0f; x <= 16.0f; x += 4.0f)
        if (x < -4.0f || x > 4.0f) SolidMesh(StageMesh::Pyramid, x, -35.0f, 1.8f, 1.8f, 1.5f, metal, 0.0f, 0.6f, 0.7f);

    // 門へ続く誘導灯
    for (float z = -42.0f; z <= -28.0f; z += 3.5f)
        for (float x : { -3.2f, 3.2f })
            Glow(x, z, 0.25f, 1.6f, 0.03f, lamp, 0.0f, 1.0f);

    // 東西の外側の遮蔽物
    for (float a : { -1.0f, 1.0f })
    {
        Solid(a * 35.0f, -8.0f, 1.0f, 7.0f, 1.1f, metal);
        Solid(a * 35.0f, 10.0f, 1.0f, 7.0f, 1.1f, metal);
    }

    // 北側の燃料集積所（円筒タンクと配管）
    for (float a : { -1.0f, 1.0f })
    {
        SolidMesh(StageMesh::Cylinder, a * 13.0f, 36.0f, 5.0f, 5.0f, 4.2f, metal);
        SolidMesh(StageMesh::Cylinder, a * 20.0f, 36.0f, 5.0f, 5.0f, 4.2f, metal);
        Prop(StageMesh::Torus, { a * 13.0f, 3.2f, 36.0f }, { 6.2f, 1.6f, 6.2f }, lamp, 1.5f);
        Prop(StageMesh::Torus, { a * 20.0f, 3.2f, 36.0f }, { 6.2f, 1.6f, 6.2f }, lamp, 1.5f);
        Deco(a * 16.5f, 36.0f, 3.0f, 0.4f, 0.4f, wall, 2.0f);
    }

    //--------------------------------------------------------------------------
    // 敵配置：南門を重装で塞ぎ、門をくぐると狙撃の十字砲火を受ける
    //   門の外 : 南門前に重装、東西の外側に近接
    //   構内   : バリケードの裏に狙撃、トーチカの間に重装、北側に高機動
    //   北     : 燃料集積所を近接が守る
    //--------------------------------------------------------------------------
    Squad(  0.0f, -29.0f, 2, TANK, 3.0f);
    Squad(-35.0f,   0.0f, 3, NORMAL);
    Squad( 35.0f,   0.0f, 3, NORMAL);

    Squad( -8.0f, -16.5f, 2, SNIPER, 3.0f);
    Squad(  8.0f, -16.5f, 2, SNIPER, 3.0f);
    Squad(-15.0f,  -6.0f, 2, TANK);
    Squad( 15.0f,  -6.0f, 2, ARTILLERY);
    Squad(  0.0f, -10.5f, 3, NORMAL, 3.0f);
    Squad(-15.0f,   9.0f, 3, SPEED);
    Squad( 15.0f,   9.0f, 3, SPEED);
    Squad(-15.0f,  14.0f, 1, WALKER,  5.0f);
    Squad( 15.0f,  20.0f, 1, GATLING, 5.0f);
    Squad(  0.0f,  22.0f, 2, TANK);
    Squad(  0.0f,  22.0f, 2, SNIPER);
    Squad(-20.0f, -20.0f, 1, TURRET, 3.0f);
    Squad( 20.0f, -20.0f, 1, TURRET, 3.0f);

    // ボス戦フェーズ（GOLIATH）の出現位置：南門を抜けた正面
    SetBoss(0.0f, -20.0f);

    Squad(  0.0f,  36.0f, 3, NORMAL);

    // 増援の降下地点（要塞の外の四隅）
    DropZone(-38.0f,  30.0f);
    DropZone( 38.0f,  30.0f);
    DropZone(-38.0f, -22.0f);
    DropZone( 38.0f, -22.0f);
    DropZone(  0.0f,  41.0f);

    // 遠景：外周壁の向こうにそびえる要塞の尖塔
    for (int i = 0; i < 28; ++i)
    {
        const float angle = DirectX::XM_2PI * (i + RandRange(rng, 0.0f, 1.0f)) / 28.0f;
        const float dist  = RandRange(rng, 75.0f, 170.0f);
        const float w     = RandRange(rng, 8.0f, 18.0f);
        const float h     = RandRange(rng, 20.0f, 70.0f);
        const float x = cosf(angle) * dist, z = sinf(angle) * dist;
        Prop(StageMesh::Frustum, { x, h * 0.5f, z }, { w, h, w }, wall);
        Prop(StageMesh::Cube,    { x, h * 0.7f, z }, { w * 0.82f, 0.4f, w * 0.82f }, lamp, 2.0f);
    }
}

//==============================================================================
// ASTEROID BELT（渓谷施設）
// ・左右の崖（高さ8〜15mの段丘）に挟まれた縦長の進攻路。崖には岩塊が張り付く
// ・途中に隔壁が3枚あり、開口部が右→左→右と入れ替わる（ジグザグに進む）
// ・北端の搬入口の前がゴール。上空には小惑星がゆっくり回りながら浮かぶ
//==============================================================================
void BlockStageLayout_Trench(std::mt19937& rng)
{
    const Col teal = { 0.30f, 0.90f, 0.70f };

    Theme theme{};
    theme.skyTop    = { 0.004f, 0.006f, 0.014f };
    theme.skyBottom = { 0.012f, 0.030f, 0.050f };
    theme.nebula    = { 0.20f, 0.75f, 0.90f, 0.60f };
    theme.fog       = { 0.012f, 0.028f, 0.045f };
    theme.fogStart  = 40.0f;
    theme.fogEnd    = 240.0f;
    theme.grid      = teal;
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.012f, 0.016f, 0.018f };
    theme.ceiling   = false;
    theme.wall      = { 0.100f, 0.090f, 0.080f };
    theme.accent    = teal;
    theme.minimap   = { 0.60f, 0.95f, 0.80f };

    Begin(22.0f, 62.0f, 16.0f, theme);
    SetSpawn(0.0f, -58.0f);

    const Col rock[3] = { { 0.34f, 0.29f, 0.26f }, { 0.26f, 0.23f, 0.21f }, { 0.30f, 0.27f, 0.22f } };
    const Col cliff   = { 0.16f, 0.14f, 0.13f };
    const Col metal   = { 0.060f, 0.070f, 0.080f };

    auto randomRot = [&]() -> XMFLOAT3
    {
        return { RandRange(rng, 0.0f, 6.0f), RandRange(rng, 0.0f, 6.0f), RandRange(rng, 0.0f, 6.0f) };
    };

    // 崖：南北10区間 × 左右。張り出しは最大9mまで（隔壁の開口部を必ず確保するため）
    constexpr int   SEGMENTS = 10;
    constexpr float SEG_LEN  = 12.4f;
    for (int i = 0; i < SEGMENTS; ++i)
    {
        const float zc = -62.0f + SEG_LEN * 0.5f + i * SEG_LEN;
        for (float side : { -1.0f, 1.0f })
        {
            const float w = RandRange(rng, 4.0f, 6.5f);
            const float h = RandRange(rng, 8.0f, 15.0f);
            Solid(side * (22.0f - w * 0.5f), zc, w, SEG_LEN, h, cliff);

            // 崖の面と上端に岩塊を張り付ける（見た目のみ）
            Prop(StageMesh::Rock, { side * (22.0f - w), h * 0.45f, zc + RandRange(rng, -3.0f, 3.0f) },
                 { RandRange(rng, 4.0f, 7.0f), h * RandRange(rng, 0.7f, 1.0f), RandRange(rng, 6.0f, 10.0f) },
                 rock[RandInt(rng, 3)], 0.0f, randomRot());
            Prop(StageMesh::Rock, { side * (22.0f - w * 0.4f), h + 1.0f, zc + RandRange(rng, -3.0f, 3.0f) },
                 { RandRange(rng, 5.0f, 9.0f), RandRange(rng, 4.0f, 7.0f), RandRange(rng, 7.0f, 12.0f) },
                 rock[RandInt(rng, 3)], 0.0f, randomRot());

            // 手前の低い段（登って高所を取れる）
            if (RandInt(rng, 2) == 0)
            {
                const float w2 = w + RandRange(rng, 1.5f, 2.5f);
                Solid(side * (22.0f - w2 * 0.5f), zc + RandRange(rng, -2.0f, 2.0f),
                      w2, RandRange(rng, 5.0f, 8.0f), h * RandRange(rng, 0.3f, 0.5f), cliff);
            }
        }
    }

    // 隔壁（開口部：右 → 左 → 右）
    const struct { float z, side; } gates[3] = { { -32.0f, -1.0f }, { -6.0f, 1.0f }, { 20.0f, -1.0f } };
    for (const auto& g : gates)
    {
        Solid(g.side * 6.0f, g.z, 24.0f, 3.0f, 6.0f, metal);
        Glow (g.side * 6.0f, g.z, 24.06f, 3.06f, 0.2f, teal, 5.5f);
        Glow (g.side * 6.0f, g.z, 24.06f, 3.06f, 0.12f, teal, 2.0f, 1.2f);
        // 開口部の縁の柱
        Glow (-g.side * 6.2f, g.z, 0.3f, 3.1f, 5.6f, teal, 0.2f, 3.0f);
    }

    // 見張り台（六角柱。登れば隔壁越しに撃てる）
    SolidMesh(StageMesh::Hex, -10.0f, -20.0f, 6.0f, 6.0f, 3.0f, metal, 0.0f, 0.8f);
    SolidMesh(StageMesh::Hex,  10.0f,   6.0f, 6.0f, 6.0f, 3.0f, metal, 0.0f, 0.8f);
    SolidMesh(StageMesh::Hex,  -9.0f,  32.0f, 6.0f, 6.0f, 3.0f, metal, 0.0f, 0.8f);

    // 岩（遮蔽物）。隔壁の前後と開始位置は空ける
    int placed = 0;
    for (int attempt = 0; attempt < 80 && placed < 14; ++attempt)
    {
        const float x = RandRange(rng, -11.0f, 11.0f);
        const float z = RandRange(rng, -50.0f, 48.0f);

        bool nearGate = false;
        for (const auto& g : gates)
            if (z > g.z - 5.0f && z < g.z + 5.0f) nearGate = true;
        if (nearGate) continue;

        const float s = RandRange(rng, 2.4f, 4.2f);
        SolidMesh(StageMesh::Rock, x, z, s, s * RandRange(rng, 0.8f, 1.2f), s * RandRange(rng, 0.7f, 0.95f),
                  rock[RandInt(rng, 3)], 0.0f, 0.62f, 0.72f);
        ++placed;
    }

    // 路面の誘導灯
    for (float z = -54.0f; z <= 50.0f; z += 8.0f)
        Glow(0.0f, z, 0.3f, 2.0f, 0.03f, teal, 0.0f, 1.0f);

    // 北端の搬入口（ゴール）
    Building(0.0f, 59.0f, 18.0f, 6.0f, 9.0f, metal, teal);
    Glow(0.0f, 56.0f, 5.0f, 0.08f, 4.0f, teal, 0.0f, 1.0f);
    SetGoal(0.0f, 53.0f);

    //--------------------------------------------------------------------------
    // 敵配置：隔壁の開口部ごとに「手前を重装が塞ぎ、奥から狙撃が撃つ」関門を作る
    //   開口部は 右(x≈10) → 左(x≈-10) → 右 と入れ替わる
    //--------------------------------------------------------------------------
    Squad(  9.0f, -38.0f, 2, TANK,   3.0f);   // 第1隔壁の手前
    Squad( -5.0f, -42.0f, 3, NORMAL);
    Squad(  9.0f, -26.0f, 3, SNIPER, 3.0f);   // 第1隔壁の奥
    Squad( -3.0f, -19.0f, 3, SPEED);

    Squad( -9.0f, -12.0f, 2, TANK,   3.0f);   // 第2隔壁の手前
    Squad(  3.0f, -13.0f, 2, SNIPER);
    Squad( -9.0f,   0.0f, 3, SNIPER, 3.0f);   // 第2隔壁の奥
    Squad(  4.0f,   2.0f, 3, NORMAL);
    Squad(  6.0f,  -2.0f, 2, ORBITER, 4.0f);

    Squad(  9.0f,  14.0f, 2, TANK,   3.0f);   // 第3隔壁の手前
    Squad( -3.0f,  11.0f, 3, PHANTOM);
    Squad(  9.0f,  26.0f, 2, SNIPER, 3.0f);   // 第3隔壁の奥
    Squad(  4.0f,  30.0f, 2, ARTILLERY);
    Squad( -4.0f,  36.0f, 1, HALO,    4.0f);
    Squad(  0.0f, -24.0f, 1, WALKER,  4.0f);
    Squad(  0.0f,  40.0f, 3, NORMAL);
    Squad(  0.0f,  47.0f, 2, TANK,   3.0f);   // 搬入口の前

    // 増援の降下地点（南の入口から追撃してくる／北の搬入口から出てくる）
    DropZone(  0.0f, -55.0f);
    DropZone( -8.0f, -47.0f);
    DropZone(  0.0f,  50.0f);
    DropZone(  8.0f,  40.0f);

    // 上空に浮かぶ小惑星（ゆっくり回転する）
    for (int i = 0; i < 46; ++i)
    {
        const float angle = RandRange(rng, 0.0f, DirectX::XM_2PI);
        const float dist  = RandRange(rng, 30.0f, 170.0f);
        const float s     = RandRange(rng, 3.0f, 16.0f);
        Prop(StageMesh::Rock,
             { cosf(angle) * dist, RandRange(rng, 38.0f, 120.0f), sinf(angle) * dist },
             { s, s * RandRange(rng, 0.7f, 1.2f), s * RandRange(rng, 0.7f, 1.2f) },
             rock[0], 0.0f, randomRot(),
             { RandRange(rng, -0.3f, 0.3f), RandRange(rng, -0.3f, 0.3f), RandRange(rng, -0.3f, 0.3f) });
    }
}

//==============================================================================
// OMEGA CORE（制御区画・ボス戦アリーナ）
// ・四隅を段状に落とした八角形の広間。床と天井にグリッドが走る
// ・外周に高さ2mのキャットウォーク、内側に4本の六角柱（遮蔽物）
// ・頭上では巨大な輪とコアが回っている。ボスは北側、プレイヤーは南側から開始
//==============================================================================
void BlockStageLayout_Arena(std::mt19937& /*rng*/)
{
    const Col lamp = { 1.00f, 0.40f, 0.10f };
    const Col cyan = { 0.15f, 0.80f, 1.00f };

    Theme theme{};
    theme.skyTop    = { 0.030f, 0.008f, 0.008f };
    theme.skyBottom = { 0.080f, 0.020f, 0.015f };
    theme.nebula    = { 1.00f, 0.40f, 0.15f, 0.30f };
    theme.fog       = { 0.100f, 0.025f, 0.015f };
    theme.fogStart  = 30.0f;
    theme.fogEnd    = 170.0f;
    theme.grid      = { 1.00f, 0.38f, 0.10f };
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.020f, 0.012f, 0.010f };
    theme.ceiling   = true;
    theme.wall      = { 0.070f, 0.055f, 0.060f };
    theme.accent    = lamp;
    theme.minimap   = { 1.00f, 0.65f, 0.50f };

    Begin(28.0f, 28.0f, 14.0f, theme);
    SetSpawn(0.0f, -22.0f);
    SetBoss(0.0f, 15.0f);
    SetEnemyMinDistance(16.0f);

    const Col metal = { 0.130f, 0.110f, 0.110f };
    const Col wall  = { 0.070f, 0.055f, 0.060f };

    for (float a : { -1.0f, 1.0f })
    {
        for (float b : { -1.0f, 1.0f })
        {
            // 四隅（段状に落として八角形に見せる）。角に縦のランプ
            Solid(a * 24.0f, b * 24.0f, 8.0f, 8.0f, 13.0f, wall);
            Solid(a * 17.5f, b * 26.0f, 5.0f, 4.0f, 13.0f, wall);
            Solid(a * 26.0f, b * 17.5f, 4.0f, 5.0f, 13.0f, wall);
            Glow (a * 20.0f, b * 20.0f, 0.5f, 0.5f, 12.0f, lamp, 0.5f, 3.0f);

            // 六角柱。3本の輪が光る
            SolidMesh(StageMesh::Hex, a * 11.0f, b * 11.0f, 3.6f, 3.6f, 11.0f, metal, 0.0f, 0.8f);
            for (float y : { 2.5f, 5.5f, 8.5f })
                Prop(StageMesh::Torus, { a * 11.0f, y, b * 11.0f }, { 4.6f, 1.4f, 4.6f }, cyan, 1.8f);

            // 動力塔（円筒の台座の上で八面体が回る）
            SolidMesh(StageMesh::Cylinder, a * 18.0f, b * 18.0f, 2.6f, 2.6f, 1.0f, metal);
            Prop(StageMesh::Octa, { a * 18.0f, 3.2f, b * 18.0f }, { 1.3f, 2.4f, 1.3f }, lamp, 3.0f,
                 { 0.0f, 0.0f, 0.0f }, { 0.0f, a * b * 1.2f, 0.0f });
        }

        // キャットウォーク（南北・東西）
        Solid(0.0f, a * 26.5f, 30.0f, 3.0f, 2.0f, metal);
        Glow (0.0f, a * 25.0f, 30.0f, 0.08f, 0.12f, lamp, 1.85f, 2.0f);
        Solid(a * 26.5f, 0.0f, 3.0f, 30.0f, 2.0f, metal);
        Glow (a * 25.0f, 0.0f, 0.08f, 30.0f, 0.12f, lamp, 1.85f, 2.0f);

        // 外周壁の縦ランプ（通路のフレーム風）
        for (float p : { -10.0f, 0.0f, 10.0f })
        {
            Glow(p, a * 27.9f, 0.7f, 0.3f, 9.0f, lamp, 3.0f, 2.5f);
            Glow(a * 27.9f, p, 0.3f, 0.7f, 9.0f, lamp, 3.0f, 2.5f);
        }

        // 低い遮蔽物
        Solid(a * 6.0f, -13.0f, 4.0f, 1.2f, 1.3f, metal);
        Glow (a * 6.0f, -13.0f, 3.6f, 1.0f, 0.04f, lamp, 1.3f, 1.2f);
        Solid(a * 17.0f,  0.0f, 1.2f, 4.0f, 1.3f, metal);
        Glow (a * 17.0f,  0.0f, 1.0f, 3.6f, 0.04f, lamp, 1.3f, 1.2f);

        // 床の発光ライン（内側＝水色、外側＝橙）
        Glow(0.0f, a * 9.0f,  18.0f, 0.15f, 0.03f, cyan, 0.0f, 0.8f);
        Glow(a * 9.0f, 0.0f,  0.15f, 18.0f, 0.03f, cyan, 0.0f, 0.8f);
        Glow(0.0f, a * 18.0f, 30.0f, 0.15f, 0.03f, lamp, 0.0f, 0.8f);
        Glow(a * 18.0f, 0.0f, 0.15f, 30.0f, 0.03f, lamp, 0.0f, 0.8f);
    }

    // 頭上のコア（互い違いに回る輪と八面体）
    Prop(StageMesh::Octa,  { 0.0f, 24.0f, 0.0f }, { 5.0f, 7.0f, 5.0f }, lamp, 3.0f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.6f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 24.0f, 0.0f }, { 16.0f, 10.0f, 16.0f }, lamp, 1.8f,
         { 0.3f, 0.0f, 0.0f }, { 0.0f, 0.35f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 24.0f, 0.0f }, { 26.0f, 12.0f, 26.0f }, cyan, 1.4f,
         { -0.25f, 0.0f, 0.2f }, { 0.0f, -0.2f, 0.0f });

    //--------------------------------------------------------------------------
    // 敵配置：大型兵器の両脇を狙撃が固め、正面を重装が守る
    //--------------------------------------------------------------------------
    Squad(-12.0f,  6.0f, 2, SNIPER, 3.0f);
    Squad( 12.0f,  6.0f, 2, SNIPER, 3.0f);
    Squad(  0.0f,  6.0f, 2, TANK,   3.0f);

    // 増援の降下地点（キャットウォークの下・東西と北）
    DropZone(-21.0f,  0.0f);
    DropZone( 21.0f,  0.0f);
    DropZone(  0.0f, 21.0f);
    DropZone(-12.0f, -4.0f);
    DropZone( 12.0f, -4.0f);
}

//==============================================================================
// REACTOR PLANT（二層プラント）
// ・地上階：反応炉・コンベア・溶鉱炉が並ぶ機械フロア。デッキの下は屋根付きの通路
// ・上階　：高さ5mのデッキ網（西デッキ・東デッキ・北デッキ、中央のハブと連絡橋）
//           縁には手すり（遮蔽物）。手すりの切れ目に階段がつながる
// ・階段　：南西・南東（開始地点の左右）、中央（ハブ）、北の左右の5本
// ・ゴールは北デッキの中央の制御室。上の階を通らないと到達できない
//==============================================================================
void BlockStageLayout_Plant(std::mt19937& rng)
{
    const Col lime   = { 0.55f, 1.00f, 0.20f };
    const Col hazard = { 1.00f, 0.55f, 0.08f };

    Theme theme{};
    theme.skyTop    = { 0.006f, 0.012f, 0.010f };
    theme.skyBottom = { 0.030f, 0.060f, 0.035f };
    theme.nebula    = { 0.40f, 0.90f, 0.30f, 0.35f };
    theme.fog       = { 0.025f, 0.050f, 0.030f };
    theme.fogStart  = 35.0f;
    theme.fogEnd    = 210.0f;
    theme.grid      = lime;
    theme.gridCell  = 4.0f;
    theme.ground    = { 0.012f, 0.016f, 0.012f };
    theme.ceiling   = false;
    theme.wall      = { 0.060f, 0.070f, 0.060f };
    theme.accent    = lime;
    theme.minimap   = { 0.70f, 1.00f, 0.55f };

    Begin(40.0f, 46.0f, 12.0f, theme);
    SetSpawn(0.0f, -42.0f);

    const Col metal = { 0.070f, 0.080f, 0.070f };
    const Col deck  = { 0.090f, 0.100f, 0.085f };
    const Col dark  = { 0.040f, 0.045f, 0.040f };
    const Col rail  = { 0.120f, 0.130f, 0.110f };

    constexpr float TOP = 5.0f;   // 上の階の床の高さ

    // 手すり（上の階の縁。高さ0.9m＝しゃがみ撃ちの遮蔽物になる）。軸に沿った線分で指定
    auto Rail = [&](float x0, float z0, float x1, float z1)
    {
        const float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
        const float sx = std::max(std::fabs(x1 - x0), 0.3f);
        const float sz = std::max(std::fabs(z1 - z0), 0.3f);
        Solid(cx, cz, sx, sz, 0.9f, rail, TOP);
        Glow (cx, cz, sx + 0.04f, sz + 0.04f, 0.06f, hazard, TOP + 0.86f, 1.4f);
    };

    // 支柱（デッキを支える。地上階では遮蔽物になる）
    auto Pillar = [&](float x, float z)
    {
        Solid(x, z, 0.7f, 0.7f, TOP - 0.5f, dark);
        Glow (x, z, 0.74f, 0.74f, 0.25f, hazard, 2.2f, 1.0f);
    };

    //--------------------------------------------------------------------------
    // 上の階：デッキ網
    //   西デッキ x -34〜-22 / z -30〜29、東デッキはその鏡像
    //   北デッキ x -34〜34  / z  29〜39
    //   中央ハブ x  -5〜5   / z  -1〜9、連絡橋 x -22〜22 / z 2〜6
    //--------------------------------------------------------------------------
    for (float s : { -1.0f, 1.0f })
    {
        Platform(s * 28.0f, -0.5f, 12.0f, 59.0f, TOP, deck);

        // 手すり：外側は全長、内側は連絡橋の接続部を空ける、南端は階段の降り口を空ける
        Rail(s * 34.0f, -30.0f, s * 34.0f, 29.0f);
        Rail(s * 22.0f, -30.0f, s * 22.0f,  2.0f);
        Rail(s * 22.0f,   6.0f, s * 22.0f, 29.0f);
        Rail(s * 34.0f, -30.0f, s * 29.5f, -30.0f);
        Rail(s * 26.5f, -30.0f, s * 22.0f, -30.0f);

        // 支柱
        for (float z : { -25.0f, -12.0f, 0.0f, 13.0f, 25.0f })
        {
            Pillar(s * 33.3f, z);
            Pillar(s * 22.7f, z);
        }

        // デッキ裏の照明（下の通路を照らす）
        Glow(s * 28.0f, -0.5f, 0.3f, 56.0f, 0.06f, lime, TOP - 0.62f, 1.6f);

        // 南の階段（開始地点の左右から西デッキ／東デッキへ）
        Stairs(s * 28.0f, -44.0f, 0, 3.0f, 14.0f, TOP, metal);
        Glow  (s * 28.0f, -37.0f, 3.1f, 14.0f, 0.04f, hazard, 0.0f, 0.6f);
    }

    Platform(0.0f, 34.0f, 68.0f, 10.0f, TOP, deck);
    Rail(-34.0f, 39.0f,  34.0f, 39.0f);
    Rail(-34.0f, 29.0f, -34.0f, 39.0f);
    Rail( 34.0f, 29.0f,  34.0f, 39.0f);
    Rail(-22.0f, 29.0f, -15.5f, 29.0f);
    Rail(-12.5f, 29.0f,  12.5f, 29.0f);
    Rail( 15.5f, 29.0f,  22.0f, 29.0f);
    for (float x : { -26.0f, -6.0f, 6.0f, 26.0f })
    {
        Pillar(x, 29.7f);
        Pillar(x, 38.3f);
    }
    Glow(0.0f, 34.0f, 64.0f, 0.3f, 0.06f, lime, TOP - 0.62f, 1.6f);

    Platform(0.0f, 4.0f, 10.0f, 10.0f, TOP, deck);
    Platform(0.0f, 4.0f, 44.0f,  4.0f, TOP, deck);
    Rail(-5.0f, -1.0f, -1.5f, -1.0f);
    Rail( 1.5f, -1.0f,  5.0f, -1.0f);
    Rail(-5.0f,  9.0f,  5.0f,  9.0f);
    for (float s : { -1.0f, 1.0f })
    {
        Rail(s * 5.0f, -1.0f, s * 5.0f, 2.0f);
        Rail(s * 5.0f,  6.0f, s * 5.0f, 9.0f);
        Rail(s * 22.0f, 2.0f, s * 5.0f, 2.0f);
        Rail(s * 22.0f, 6.0f, s * 5.0f, 6.0f);
        Pillar(s * 11.0f, 4.0f);
        Pillar(s * 4.0f, -0.4f);
        Pillar(s * 4.0f,  8.4f);
    }

    // ハブの上で回るリアクターコア（目印）
    Prop(StageMesh::Octa,  { 0.0f, 9.5f, 4.0f }, { 1.6f, 2.6f, 1.6f }, lime, 3.0f,
         { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    Prop(StageMesh::Torus, { 0.0f, 9.5f, 4.0f }, { 5.0f, 2.0f, 5.0f }, lime, 1.8f,
         { 0.4f, 0.0f, 0.2f }, { 0.0f, -0.6f, 0.0f });

    // 中央の階段（地上からハブへ）と、北の左右の階段（地上から北デッキへ）
    Stairs(0.0f, -15.0f, 0, 3.0f, 14.0f, TOP, metal);
    Stairs(-14.0f, 15.0f, 0, 3.0f, 14.0f, TOP, metal);
    Stairs( 14.0f, 15.0f, 0, 3.0f, 14.0f, TOP, metal);
    Glow(0.0f, -8.0f, 3.1f, 14.0f, 0.04f, hazard, 0.0f, 0.6f);

    //--------------------------------------------------------------------------
    // 地上階：機械群
    //--------------------------------------------------------------------------
    // 反応炉（円筒。光る輪が3本）
    for (float s : { -1.0f, 1.0f })
    {
        SolidMesh(StageMesh::Cylinder, s * 12.0f, -8.0f, 5.0f, 5.0f, 7.5f, metal, 0.0f, 0.8f);
        for (float y : { 1.5f, 3.8f, 6.1f })
            Prop(StageMesh::Torus, { s * 12.0f, y, -8.0f }, { 5.8f, 1.2f, 5.8f }, lime, 1.8f);
        Prop(StageMesh::Sphere, { s * 12.0f, 8.0f, -8.0f }, { 2.2f, 2.2f, 2.2f }, lime, 2.5f);

        // コンベア（低い遮蔽物。上面に警告ライン）
        Solid(s * 9.0f, -26.0f, 12.0f, 1.2f, 1.1f, dark);
        Glow (s * 9.0f, -26.0f, 11.6f, 0.3f, 0.04f, hazard, 1.1f, 1.2f);

        // 連絡橋の下の遮蔽物
        Solid(s * 9.0f, -2.0f, 4.0f, 1.0f, 1.2f, metal);
        Glow (s * 9.0f, -2.0f, 3.6f, 0.8f, 0.04f, hazard, 1.2f, 1.2f);

        // 外周の貯蔵タンク（外周壁沿い）
        for (float z : { -20.0f, 0.0f, 20.0f })
        {
            SolidMesh(StageMesh::Cylinder, s * 37.2f, z, 3.0f, 3.0f, 8.0f, metal, 0.0f, 0.8f);
            Glow(s * 37.2f, z, 3.1f, 3.1f, 0.2f, lime, 6.5f, 1.4f);
        }
    }

    // 溶鉱炉（北の地上。北の階段のあいだ）
    Solid(0.0f, 22.0f, 8.0f, 8.0f, 4.0f, metal);
    Glow (0.0f, 22.0f, 8.2f, 8.2f, 0.25f, hazard, 3.6f, 2.5f);
    Glow (0.0f, 17.95f, 3.0f, 0.1f, 2.0f, hazard, 0.0f, 3.0f);   // 炉口

    // 資材コンテナ（遮蔽物）
    const struct { float x, z, sx, sz, h; } crates[] =
    {
        {  -8.0f, -36.0f, 2.2f, 2.2f, 1.4f }, {   7.0f, -34.0f, 3.0f, 2.0f, 1.6f },
        { -18.0f, -18.0f, 2.0f, 2.0f, 1.4f }, {  18.0f, -16.0f, 2.0f, 3.0f, 1.8f },
        { -16.0f,  14.0f, 2.2f, 2.2f, 1.4f }, {  19.0f,  12.0f, 2.2f, 2.2f, 1.6f },
        { -28.0f,  10.0f, 2.0f, 4.0f, 1.4f }, {  28.0f, -14.0f, 2.0f, 4.0f, 1.4f },
    };
    for (const auto& c : crates)
    {
        Solid(c.x, c.z, c.sx, c.sz, c.h, dark);
        Glow (c.x, c.z, c.sx * 1.02f, c.sz * 1.02f, 0.06f, lime, c.h * 0.55f, 0.9f);
    }

    // 地上の誘導ライン
    Glow(0.0f, -30.0f, 50.0f, 0.15f, 0.03f, hazard, 0.0f, 0.7f);
    Glow(0.0f,  12.0f, 36.0f, 0.15f, 0.03f, hazard, 0.0f, 0.7f);

    // 制御室（北デッキ中央。ゴール）
    Solid(0.0f, 37.6f, 8.0f, 2.0f, 3.0f, metal, TOP);
    Glow (0.0f, 36.55f, 6.0f, 0.08f, 1.2f, lime, TOP + 1.0f, 2.0f);
    SetGoal(0.0f, 33.0f, TOP);

    // ボス戦フェーズ（SPECTRE）の出現位置：中央ハブの北の地上
    SetBoss(0.0f, 13.0f);

    //--------------------------------------------------------------------------
    // 敵配置：地上は近接・機動、デッキ上は狙撃・砲撃が撃ち下ろす
    //--------------------------------------------------------------------------
    Squad(  0.0f, -22.0f, 3, NORMAL);
    Squad(-14.0f, -16.0f, 2, TANK);
    Squad( 14.0f, -16.0f, 3, GUNNER);
    Squad(  0.0f,  10.0f, 3, SPEED);
    Squad(-10.0f,  20.0f, 2, BOMBER);
    Squad( 10.0f,  20.0f, 2, PHANTOM);
    Squad(-28.0f,   0.0f, 2, NORMAL, 5.0f);     // 西デッキの下の通路
    Squad( 28.0f,   6.0f, 1, WALKER, 5.0f);     // 東デッキの下の通路
    Squad(-14.0f,   6.0f, 1, GATLING, 4.0f);

    Squad(-28.0f, -12.0f, 2, SNIPER,    4.0f, TOP);
    Squad( 28.0f, -12.0f, 2, SNIPER,    4.0f, TOP);
    Squad(  0.0f,   4.0f, 2, ARTILLERY, 3.0f, TOP);
    Squad(-28.0f,  16.0f, 2, GUNNER,    4.0f, TOP);
    Squad( 28.0f,  16.0f, 1, TURRET,    3.0f, TOP);
    Squad(-20.0f,  34.0f, 1, TURRET,    3.0f, TOP);
    Squad( 12.0f,  34.0f, 2, TANK,      5.0f, TOP);
    Squad( 28.0f,   0.0f, 2, WING,      4.0f, TOP);   // 東デッキの上を旋回
    Squad(  0.0f,   4.0f, 1, HALO,      3.0f, TOP);   // 中央ハブ

    // 増援の降下地点（地上）
    DropZone(-17.0f,   0.0f);
    DropZone( 17.0f,   0.0f);
    DropZone(-10.0f,  27.0f);
    DropZone( 10.0f,  27.0f);
    DropZone(-16.0f, -36.0f);
    DropZone( 16.0f, -36.0f);

    // 遠景：冷却塔と排気の光
    for (int i = 0; i < 18; ++i)
    {
        const float angle = DirectX::XM_2PI * (i + RandRange(rng, 0.0f, 1.0f)) / 18.0f;
        const float dist  = RandRange(rng, 80.0f, 170.0f);
        const float w     = RandRange(rng, 14.0f, 26.0f);
        const float h     = RandRange(rng, 24.0f, 55.0f);
        const float x = cosf(angle) * dist, z = sinf(angle) * dist;
        Prop(StageMesh::Frustum, { x, h * 0.5f, z }, { w, h, w }, theme.wall);
        Prop(StageMesh::Torus,   { x, h + 0.5f, z }, { w * 0.75f, 1.0f, w * 0.75f }, lime, 1.5f);
    }
}