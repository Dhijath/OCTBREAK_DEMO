/*==============================================================================

   ダンジョンタイル生成 [DungeonGen.cpp]

   ■生成フロー
   1. タイル配列を全部 WALL で初期化
   2. 部屋をランダム配置（重なり・端への食み出しを弾く）
   3. 最遠部屋をボスルームとして 13×13 に拡張
   4. 各部屋を近傍優先ツリーで接続（L字通路）
   5. 少数の追加接続でループを生成
   6. 正規化（壁1層化 → 孤立柱除去 → 再壁付け → 外周固定）

==============================================================================*/

#include "DungeonGen.h"

#include <random>
#include <algorithm>
#include <cassert>
#include <climits>

//==============================================================================
// 内部ヘルパ（このcppのみ）
//==============================================================================
namespace
{

//------------------------------------------------------------------------------
// 水平方向に通路を掘る（corridorW の太さ）
//------------------------------------------------------------------------------
static void CarveH(std::vector<int>& t, int W, int H,
                   int x1, int x2, int cy, int cw)
{
    if (x1 > x2) std::swap(x1, x2);
    const int half = cw / 2;
    for (int x = x1; x <= x2; ++x)
        for (int dy = -half; dy <= half; ++dy)
        {
            const int y = cy + dy;
            if (x > 0 && x < W-1 && y > 0 && y < H-1)
                t[y*W + x] = DGEN_FLOOR;
        }
}

//------------------------------------------------------------------------------
// 垂直方向に通路を掘る（corridorW の太さ）
//------------------------------------------------------------------------------
static void CarveV(std::vector<int>& t, int W, int H,
                   int y1, int y2, int cx, int cw)
{
    if (y1 > y2) std::swap(y1, y2);
    const int half = cw / 2;
    for (int y = y1; y <= y2; ++y)
        for (int dx = -half; dx <= half; ++dx)
        {
            const int x = cx + dx;
            if (x > 0 && x < W-1 && y > 0 && y < H-1)
                t[y*W + x] = DGEN_FLOOR;
        }
}

//------------------------------------------------------------------------------
// 床タイルの4近傍を WALL に昇格させる
// ・EMPTY → WALL のみ（FLOOR は絶対に変えない）
//------------------------------------------------------------------------------
static void MarkWalls(std::vector<int>& out, int W, int H)
{
    for (int y = 1; y < H-1; ++y)
        for (int x = 1; x < W-1; ++x)
        {
            if (out[y*W+x] != DGEN_FLOOR) continue;
            const int n  = (y-1)*W+x;
            const int s  = (y+1)*W+x;
            const int w_ = y*W+(x-1);
            const int e  = y*W+(x+1);
            if (out[n]  != DGEN_FLOOR) out[n]  = DGEN_WALL;
            if (out[s]  != DGEN_FLOOR) out[s]  = DGEN_WALL;
            if (out[w_] != DGEN_FLOOR) out[w_] = DGEN_WALL;
            if (out[e]  != DGEN_FLOOR) out[e]  = DGEN_WALL;
        }
}

//------------------------------------------------------------------------------
// 孤立柱除去
// ・東西 or 南北の両側が床 → 通路内の柱 → 床に戻す
// ・4近傍のうち3つ以上が床 → 完全孤立柱 → 床に戻す
// ・2方向だけ隣接（コーナー壁）は維持
//------------------------------------------------------------------------------
static void RemovePillars(std::vector<int>& out, int W, int H)
{
    for (int y = 1; y < H-1; ++y)
        for (int x = 1; x < W-1; ++x)
        {
            if (out[y*W+x] != DGEN_WALL) continue;

            const bool fn = (out[(y-1)*W+x]  == DGEN_FLOOR);
            const bool fs = (out[(y+1)*W+x]  == DGEN_FLOOR);
            const bool fw = (out[y*W+(x-1)]  == DGEN_FLOOR);
            const bool fe = (out[y*W+(x+1)]  == DGEN_FLOOR);

            const int floorN = (fn?1:0) + (fs?1:0) + (fw?1:0) + (fe?1:0);

            if ((fw && fe) || (fn && fs) || floorN >= 3)
                out[y*W+x] = DGEN_FLOOR;
        }
}

//------------------------------------------------------------------------------
// 外周を必ず WALL に固定
//------------------------------------------------------------------------------
static void FixBorder(std::vector<int>& t, int W, int H)
{
    for (int x = 0; x < W; ++x)
    {
        t[0*(W)+x]    = DGEN_WALL;
        t[(H-1)*W+x]  = DGEN_WALL;
    }
    for (int y = 0; y < H; ++y)
    {
        t[y*W+0]     = DGEN_WALL;
        t[y*W+(W-1)] = DGEN_WALL;
    }
}

//------------------------------------------------------------------------------
// タイル正規化
//
// 1. 床を残し、それ以外を EMPTY にリセット
// 2. 床隣接タイルを WALL に昇格（1層壁）
// 3. 孤立柱を除去（→ 新たな床タイルが生まれる）
// 4. 新しい床の隣接タイルを再度 WALL に昇格
// 5. 外周固定
//
// ※ステップ3 で柱が FLOOR になると、その隣の EMPTY が壁不足になる。
//   ステップ4（再MarkWalls）でそれを補う。2パスが必要な理由がここにある。
//------------------------------------------------------------------------------
static void Normalize(std::vector<int>& tiles, int W, int H)
{
    std::vector<int> out(tiles.size(), DGEN_EMPTY);

    for (int i = 0; i < (int)tiles.size(); ++i)
        if (tiles[i] == DGEN_FLOOR) out[i] = DGEN_FLOOR;

    MarkWalls(out, W, H);
    RemovePillars(out, W, H);
    MarkWalls(out, W, H);
    FixBorder(out, W, H);

    tiles.swap(out);
}

//------------------------------------------------------------------------------
// 部屋重なり判定（margin ぶん余裕を持たせる）
//------------------------------------------------------------------------------
static bool Overlaps(const DungeonRoom& a, const DungeonRoom& b, int margin = 4)
{
    return (a.x - margin < b.x + b.w &&
            a.x + a.w + margin > b.x &&
            a.y - margin < b.y + b.h &&
            a.y + a.h + margin > b.y);
}

//------------------------------------------------------------------------------
// ペアキー生成（重複接続チェック用）
//------------------------------------------------------------------------------
static std::uint32_t MakePairKey(int a, int b)
{
    if (a > b) std::swap(a, b);
    return (static_cast<std::uint32_t>(a) << 16) | static_cast<std::uint32_t>(b);
}

} // namespace

//==============================================================================
// DungeonGen_IsOpenTile
//==============================================================================
bool DungeonGen_IsOpenTile(const std::vector<int>& tiles, int w, int tx, int ty)
{
    const bool n  = (tiles[(ty-1)*w + tx]     == DGEN_FLOOR);
    const bool s  = (tiles[(ty+1)*w + tx]     == DGEN_FLOOR);
    const bool ww = (tiles[ty*w   + (tx-1)]   == DGEN_FLOOR);
    const bool e  = (tiles[ty*w   + (tx+1)]   == DGEN_FLOOR);
    const bool nw = (tiles[(ty-1)*w + (tx-1)] == DGEN_FLOOR);
    const bool ne = (tiles[(ty-1)*w + (tx+1)] == DGEN_FLOOR);
    const bool sw = (tiles[(ty+1)*w + (tx-1)] == DGEN_FLOOR);
    const bool se = (tiles[(ty+1)*w + (tx+1)] == DGEN_FLOOR);
    return n && s && ww && e && nw && ne && sw && se;
}

//==============================================================================
// DungeonGen_Generate
//==============================================================================
DungeonData DungeonGen_Generate(std::uint32_t seed, const DungeonParams& p)
{
    DungeonData d;
    d.width  = p.width;
    d.height = p.height;
    d.bossRoomIdx = 0;

    const int W = p.width;
    const int H = p.height;

    d.tiles.assign(W * H, DGEN_WALL);
    auto& tiles = d.tiles;
    auto& rooms = d.rooms;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> rwDist(p.roomMin, p.roomMax);
    std::uniform_int_distribution<int> rhDist(p.roomMin, p.roomMax);
    std::uniform_int_distribution<int> rxDist(2, W - p.roomMax - 3);
    std::uniform_int_distribution<int> ryDist(2, H - p.roomMax - 3);

    //--------------------------------------------------------------------------
    // 部屋配置
    //--------------------------------------------------------------------------
    rooms.reserve(64);
    for (int i = 0; i < p.roomTries; ++i)
    {
        DungeonRoom r{};
        r.w = rwDist(rng) | 1;
        r.h = rhDist(rng) | 1;
        r.x = rxDist(rng);
        r.y = ryDist(rng);

        if (r.x + r.w >= W-2 || r.y + r.h >= H-2) continue;

        bool ok = true;
        for (const auto& e : rooms)
            if (Overlaps(r, e)) { ok = false; break; }
        if (!ok) continue;

        rooms.push_back(r);
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                tiles[y*W + x] = DGEN_FLOOR;
    }

    // 1部屋も置けなかった場合の保険
    if (rooms.empty())
    {
        DungeonRoom r{};
        r.x = W/2 - p.roomMin/2;
        r.y = H/2 - p.roomMin/2;
        r.w = p.roomMin | 1;
        r.h = p.roomMin | 1;
        rooms.push_back(r);
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                tiles[y*W + x] = DGEN_FLOOR;
    }

    //--------------------------------------------------------------------------
    // ボスルーム確定・拡張（rooms[0] から最遠の部屋を選ぶ）
    //--------------------------------------------------------------------------
    if ((int)rooms.size() > 1)
    {
        int bestD2 = 0;
        for (int i = 1; i < (int)rooms.size(); ++i)
        {
            const int dx = rooms[i].cx() - rooms[0].cx();
            const int dy = rooms[i].cy() - rooms[0].cy();
            const int d2 = dx*dx + dy*dy;
            if (d2 > bestD2) { bestD2 = d2; d.bossRoomIdx = i; }
        }

        const int bx = rooms[d.bossRoomIdx].cx();
        const int by = rooms[d.bossRoomIdx].cy();

        for (int ry = by - p.bossHalf; ry <= by + p.bossHalf; ++ry)
            for (int rx = bx - p.bossHalf; rx <= bx + p.bossHalf; ++rx)
                if (rx > 0 && rx < W-1 && ry > 0 && ry < H-1)
                    tiles[ry*W + rx] = DGEN_FLOOR;

        rooms[d.bossRoomIdx] = {
            bx - p.bossHalf, by - p.bossHalf,
            p.bossHalf*2 + 1, p.bossHalf*2 + 1
        };
    }

    //--------------------------------------------------------------------------
    // 通路接続（L字）
    //--------------------------------------------------------------------------
    auto ConnectL = [&](int x1, int y1, int x2, int y2)
    {
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0)
        {
            CarveH(tiles, W, H, x1, x2, y1, p.corridorW);
            CarveV(tiles, W, H, y1, y2, x2, p.corridorW);
        }
        else
        {
            CarveV(tiles, W, H, y1, y2, x1, p.corridorW);
            CarveH(tiles, W, H, x1, x2, y2, p.corridorW);
        }
    };

    // 近傍優先ツリー接続
    for (int i = 1; i < (int)rooms.size(); ++i)
    {
        int best = 0, bestD2 = INT_MAX;
        for (int j = 0; j < i; ++j)
        {
            const int dx = rooms[i].cx() - rooms[j].cx();
            const int dy = rooms[i].cy() - rooms[j].cy();
            const int d2 = dx*dx + dy*dy;
            if (d2 < bestD2) { bestD2 = d2; best = j; }
        }
        ConnectL(rooms[best].cx(), rooms[best].cy(),
                 rooms[i].cx(),    rooms[i].cy());
    }

    // 追加接続（ループ生成）
    const int extra = std::max(1, (int)rooms.size() / 4);
    std::uniform_int_distribution<int> rPick(0, (int)rooms.size() - 1);
    std::vector<std::uint32_t> used;

    for (int k = 0; k < extra; ++k)
    {
        const int a = rPick(rng);
        int bestB = -1, bestD2 = INT_MAX;

        for (int b = 0; b < (int)rooms.size(); ++b)
        {
            if (b == a) continue;
            const auto key = MakePairKey(a, b);
            if (std::find(used.begin(), used.end(), key) != used.end()) continue;

            const int dx = rooms[b].cx() - rooms[a].cx();
            const int dy = rooms[b].cy() - rooms[a].cy();
            const int d2 = dx*dx + dy*dy;
            if (d2 < bestD2) { bestD2 = d2; bestB = b; }
        }

        if (bestB < 0) continue;
        used.push_back(MakePairKey(a, bestB));
        ConnectL(rooms[a].cx(), rooms[a].cy(),
                 rooms[bestB].cx(), rooms[bestB].cy());
    }

    //--------------------------------------------------------------------------
    // 正規化（壁1層化 + 柱除去 + 外周固定）
    //--------------------------------------------------------------------------
    Normalize(tiles, W, H);

    return d;
}

//==============================================================================
// DungeonGen_GenerateBossArena
//==============================================================================
DungeonData DungeonGen_GenerateBossArena(int arenaW, int arenaH)
{
    DungeonData d;
    d.width  = arenaW;
    d.height = arenaH;
    d.bossRoomIdx = 0;
    d.tiles.assign(arenaW * arenaH, DGEN_WALL);

    const int x1 = 2, x2 = arenaW - 3;
    const int y1 = 2, y2 = arenaH - 3;

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            d.tiles[y*arenaW + x] = DGEN_FLOOR;

    Normalize(d.tiles, arenaW, arenaH);

    DungeonRoom r{ x1, y1, x2-x1+1, y2-y1+1 };
    d.rooms.push_back(r);

    return d;
}
