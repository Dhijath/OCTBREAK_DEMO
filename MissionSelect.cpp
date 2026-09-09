/*==============================================================================

   ミッション選択画面 [MissionSelect.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ■レイアウト（1600×900）
     Header (y=0〜72)            : SORTIE // MISSION SELECT、稼働時間、回線状態
     List   (x=40,   w=480)      : ミッション一覧（番号・作戦名・難度・完了印）
     Map    (x=550,  w=460)      : 戦術マップ（作戦エリアの見取り図・敵部隊・降下地点）
     Data   (x=1030, w=530)      : 作戦データ（依頼主・領域・目標・脅威分析・報酬）
     Brief  (x=550,  w=1010)     : ブリーフィング（1文字ずつ送る）・装備・出撃ボタン

   ■描画
     図形は SciFiUI のスプライト部品、文字は SciFiUI の文字キュー（最後にまとめて描画）。

==============================================================================*/
#include "MissionSelect.h"
#include "MissionDef.h"
#include "BlockStage.h"
#include "AssemblyScreen.h"
#include "WeaponDef.h"
#include "SciFiUI.h"
#include "UIInput.h"
#include "audio.h"
#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include "input_hint.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DirectX;
using namespace SciFiUI;

//==============================================================================
// 定数（レイアウト）
//==============================================================================
static constexpr float SW = 1600.0f;
static constexpr float SH = 900.0f;

static constexpr float HEAD_H = 72.0f;

static constexpr float LIST_X = 40.0f,   LIST_Y = 104.0f, LIST_W = 480.0f;
static constexpr float ROW_H  = 72.0f,   ROW_STEP = 84.0f;

static constexpr float MAP_X  = 550.0f,  MAP_Y  = 100.0f, MAP_W  = 460.0f, MAP_H  = 420.0f;
static constexpr float DATA_X = 1030.0f, DATA_Y = 100.0f, DATA_W = 530.0f, DATA_H = 420.0f;
static constexpr float BRF_X  = 550.0f,  BRF_Y  = 540.0f, BRF_W  = 1010.0f, BRF_H = 250.0f;

static constexpr float SORTIE_W = 220.0f, SORTIE_H = 58.0f;
static constexpr float SORTIE_X = BRF_X + BRF_W - SORTIE_W - 18.0f;
static constexpr float SORTIE_Y = BRF_Y + BRF_H - SORTIE_H - 16.0f;

static constexpr float BRIEF_CHARS_PER_SEC = 55.0f;   // ブリーフィングの文字送り速度

//==============================================================================
// 内部状態
//==============================================================================
namespace
{
    int                 g_Selected  = 0;
    double              g_Time      = 0.0;
    double              g_BriefTime = 0.0;   // カーソル移動でリセット
    MissionSelectResult g_Result    = MissionSelectResult::None;

    int g_BgTexID = -1;

    int g_SeCursorMove = -1;
    int g_SeSelect     = -1;
    int g_SeCancel     = -1;

    // ステージごとの見取り図（Initialize で作る）
    BlockStagePreview g_Previews[static_cast<int>(BlockStageID::Count)];
}

//==============================================================================
// 内部ヘルパー
//==============================================================================
static const wchar_t* EnemyMixLabel(EnemyMix mix)
{
    switch (mix)
    {
    case EnemyMix::Swarm:  return L"高機動型主体";
    case EnemyMix::Heavy:  return L"重装型主体";
    case EnemyMix::Sniper: return L"狙撃型主体";
    default:               return L"混成部隊";
    }
}

static XMFLOAT4 RankColor(int rank)
{
    if (rank >= 5) return kRed;
    if (rank >= 4) return { 1.0f, 0.45f, 0.2f, 1.0f };
    if (rank >= 3) return kAmber;
    return kCyan;
}

static std::wstring FormatTime(float sec)
{
    const int s = static_cast<int>(sec);
    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", s / 60, s % 60);
    return buf;
}

// 敵戦力の見積もり（部隊＋哨戒兵）
static int EstimateInitialForce(const MissionDef& md)
{
    if (md.legacy) return -1;
    int total = 0;
    for (int p = 0; p < md.phaseCount; ++p)
    {
        const BlockStagePreview& pv = g_Previews[static_cast<int>(md.phases[p].stage)];
        for (const BlockStagePreview::Squad& sq : pv.squads) total += sq.count;
        total += md.phases[p].enemyCount;
    }
    return total;
}

//==============================================================================
// 戦術マップ
//==============================================================================
static void DrawTacticalMap(const MissionDef& md)
{
    const float t  = static_cast<float>(g_Time);
    const float ax = MAP_X + 16.0f, ay = MAP_Y + 40.0f;   // 地図の描画範囲
    const float aw = MAP_W - 32.0f, ah = MAP_H - 76.0f;

    Fill(ax, ay, aw, ah, { 0.0f, 0.02f, 0.04f, 0.85f });
    Grid(ax, ay, aw, ah, 26.0f, { 0.3f, 0.8f, 1.0f, 0.06f });

    if (md.legacy)
    {
        // 自動生成ダンジョンは見取り図なし：ノイズ状のブロックを流す
        for (int i = 0; i < 60; ++i)
        {
            const float seed = static_cast<float>(i) * 12.9898f;
            const float fx = fmodf(fabsf(sinf(seed) * 43758.5f), 1.0f);
            const float fy = fmodf(fabsf(sinf(seed * 1.7f + floorf(t * 4.0f)) * 24634.6f), 1.0f);
            Fill(ax + fx * (aw - 20.0f), ay + fy * (ah - 10.0f), 20.0f, 6.0f, { 0.3f, 0.8f, 1.0f, 0.12f });
        }
        Text(L"NO SURVEY DATA", ax + aw * 0.5f, ay + ah * 0.5f - 30.0f, 30.0f, ToD2D(kCyan, 0.8f),
             UIFont::Display, UIAlign::Center, true);
        Text(L"構造が毎回変わる区画のため、事前の見取り図はありません", ax + aw * 0.5f, ay + ah * 0.5f + 12.0f, 14.0f,
             ToD2D(kCyan, 0.6f), UIFont::Body, UIAlign::Center);
        return;
    }

    const BlockStagePreview& pv = g_Previews[static_cast<int>(md.phases[0].stage)];
    const XMFLOAT4 accent = { pv.accent.x, pv.accent.y, pv.accent.z, 1.0f };

    // ワールド (x,z) → 地図上の座標。北（+Z）が上
    const float scale = std::min(aw / (pv.halfX * 2.0f), ah / (pv.halfZ * 2.0f)) * 0.94f;
    const float cx = ax + aw * 0.5f, cy = ay + ah * 0.5f;
    auto toMap = [&](float x, float z) { return XMFLOAT2{ cx + x * scale, cy - z * scale }; };

    // 作戦エリアの枠
    const XMFLOAT2 tl = toMap(-pv.halfX, pv.halfZ);
    Frame(tl.x, tl.y, pv.halfX * 2.0f * scale, pv.halfZ * 2.0f * scale, WithAlpha(accent, 0.7f), 1.0f);

    // 構造物（高いものほど明るい）
    for (const BlockStagePreview::Rect& r : pv.structures)
    {
        const float h = std::min(r.top / 14.0f, 1.0f);
        const XMFLOAT2 p = toMap(r.cx - r.sx * 0.5f, r.cz + r.sz * 0.5f);
        Fill(p.x, p.y, r.sx * scale, r.sz * scale, { 0.25f + 0.45f * h, 0.55f + 0.35f * h, 0.75f + 0.25f * h, 0.20f + 0.45f * h });
    }

    // 増援の降下地点
    for (const XMFLOAT2& dz : pv.dropZones)
    {
        const XMFLOAT2 p = toMap(dz.x, dz.y);
        Frame(p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f, WithAlpha(kAmber, 0.8f), 1.0f);
        Fill(p.x - 1.0f, p.y - 1.0f, 2.0f, 2.0f, kAmber);
    }

    // 敵部隊（数に応じた大きさのひし形）
    const float pulse = 0.5f + 0.5f * sinf(t * 4.0f);
    for (const BlockStagePreview::Squad& sq : pv.squads)
    {
        const XMFLOAT2 p = toMap(sq.x, sq.z);
        const float r = 3.0f + sq.count * 1.2f;
        Diamond(p.x, p.y, r + 2.0f + pulse * 2.0f, WithAlpha(kRed, 0.18f));
        Diamond(p.x, p.y, r, WithAlpha(kRed, 0.85f));
    }

    // 開始位置・ゴール・大型兵器
    const XMFLOAT2 sp = toMap(pv.spawn.x, pv.spawn.y);
    Diamond(sp.x, sp.y, 6.0f, kGreen);
    if (pv.hasGoal)
    {
        const XMFLOAT2 g = toMap(pv.goal.x, pv.goal.y);
        const float s = 12.0f + pulse * 4.0f;
        Brackets(g.x - s, g.y - s, s * 2.0f, s * 2.0f, 5.0f, kCyan, 2.0f);
        Fill(g.x - 3.0f, g.y - 3.0f, 6.0f, 6.0f, kCyan);
    }
    if (pv.hasBoss)
    {
        const XMFLOAT2 b = toMap(pv.boss.x, pv.boss.y);
        Diamond(b.x, b.y, 14.0f + pulse * 3.0f, WithAlpha(kAmber, 0.25f));
        Diamond(b.x, b.y, 9.0f, kAmber);
    }

    // 走査線（左右に往復するスキャン）
    const float scan = (sinf(t * 0.9f) * 0.5f + 0.5f) * aw;
    Fill(ax + scan, ay, 2.0f, ah, WithAlpha(kCyan, 0.45f));
    Fill(ax + std::max(0.0f, scan - 40.0f), ay, std::min(40.0f, scan), ah, WithAlpha(kCyan, 0.05f));

    // 凡例
    const float ly = MAP_Y + MAP_H - 28.0f;
    Diamond(ax + 8.0f,   ly + 7.0f, 5.0f, kGreen);
    Diamond(ax + 92.0f,  ly + 7.0f, 5.0f, kRed);
    Frame  (ax + 186.0f, ly + 1.0f, 12.0f, 12.0f, kAmber, 1.0f);
    Fill   (ax + 300.0f, ly + 4.0f, 6.0f, 6.0f, pv.hasBoss ? kAmber : kCyan);
    Text(L"START",    ax + 18.0f,  ly, 12.0f, ToD2D(kGreen), UIFont::Mono);
    Text(L"HOSTILE",  ax + 102.0f, ly, 12.0f, ToD2D(kRed),   UIFont::Mono);
    Text(L"DROP ZONE", ax + 204.0f, ly, 12.0f, ToD2D(kAmber), UIFont::Mono);
    Text(pv.hasBoss ? L"TARGET" : L"GOAL", ax + 312.0f, ly, 12.0f, ToD2D(pv.hasBoss ? kAmber : kCyan), UIFont::Mono);

    if (md.phaseCount > 1)
        Text(L"PHASE 1 / 2", ax + aw - 6.0f, ay + 6.0f, 13.0f, ToD2D(kAmber), UIFont::Mono, UIAlign::Right, true);
}

//==============================================================================
// MissionSelect_Initialize
//==============================================================================
void MissionSelect_Initialize()
{
    g_Selected  = Mission_GetCurrent();   // 前回出撃したミッションにカーソルを合わせる
    g_Time      = 0.0;
    g_BriefTime = 0.0;
    g_Result    = MissionSelectResult::None;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudioWithVolume("resource/Sound/ui_cursor_move.wav", 0.5f);
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudioWithVolume("resource/Sound/ui_select.wav", 0.5f);
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudioWithVolume("resource/Sound/ui_cancel.wav", 0.5f);

    if (g_BgTexID < 0) g_BgTexID = Texture_Load(L"resource/Texture/titleBg.png");

    // 戦術マップ用の見取り図（出撃時にステージは作り直されるので、ここで上書きしてよい）
    for (int i = 0; i < static_cast<int>(BlockStageID::Count); ++i)
        BlockStage_GetPreview(static_cast<BlockStageID>(i), &g_Previews[i]);
}

//==============================================================================
// MissionSelect_Finalize
// アプリ終了時（GameManager_Finalize）に呼ぶ。
// 出撃/キャンセル直後には呼ばないこと（決定 SE が途中で止まるため。StageSelect と同じ理由）。
//==============================================================================
void MissionSelect_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;

    // テクスチャは他画面と同一パス（＝同一ID）を共有しているためここでは解放しない
    g_BgTexID = -1;
}

//==============================================================================
// MissionSelect_Update
//==============================================================================
void MissionSelect_Update(double elapsed_time)
{
    g_Time      += elapsed_time;
    g_BriefTime += elapsed_time;

    if (UI_IsMoveUp())
    {
        g_Selected  = (g_Selected + MISSION_COUNT - 1) % MISSION_COUNT;
        g_BriefTime = 0.0;
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsMoveDown())
    {
        g_Selected  = (g_Selected + 1) % MISSION_COUNT;
        g_BriefTime = 0.0;
        PlayAudio(g_SeCursorMove, false);
    }

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Result = MissionSelectResult::Back;
        return;
    }

    if (UI_IsConfirm())
    {
        Mission_SetCurrent(g_Selected);
        PlayAudio(g_SeSelect, false);
        g_Result = MissionSelectResult::Sortie;
    }
}

//==============================================================================
// MissionSelect_Draw
//==============================================================================
void MissionSelect_Draw()
{
    const MissionDef& md = k_MissionDefs[g_Selected];
    const float t     = static_cast<float>(g_Time);
    const float pulse = 0.5f + 0.5f * sinf(t * 4.0f);
    const XMFLOAT4 rankCol = RankColor(md.rank);

    BeginSprites();

    //--------------------------------------------------------------------------
    // 背景：暗い地＋格子＋上から下へ流れる走査帯
    //--------------------------------------------------------------------------
    Fill(0.0f, 0.0f, SW, SH, { 0.01f, 0.02f, 0.04f, 1.0f });
    if (g_BgTexID >= 0) Sprite_Draw(g_BgTexID, 0.0f, 0.0f, SW, SH, { 0.5f, 0.75f, 1.0f, 0.08f });
    Grid(0.0f, 0.0f, SW, SH, 40.0f, { 0.3f, 0.7f, 1.0f, 0.035f });
    {
        const float sweepY = fmodf(t * 80.0f, SH + 60.0f) - 30.0f;
        Fill(0.0f, sweepY - 40.0f, SW, 40.0f, { 0.4f, 0.9f, 1.0f, 0.03f });
        Fill(0.0f, sweepY, SW, 1.0f, { 0.4f, 0.9f, 1.0f, 0.15f });
    }

    //--------------------------------------------------------------------------
    // ヘッダ
    //--------------------------------------------------------------------------
    Fill(0.0f, 0.0f, SW, HEAD_H, { 0.0f, 0.03f, 0.06f, 0.9f });
    Fill(0.0f, HEAD_H, SW, 1.0f, kCyanDim);
    Ticks(0.0f, HEAD_H + 1.0f, SW, 81, 5, WithAlpha(kCyan, 0.35f));
    Fill(36.0f, 18.0f, 4.0f, 36.0f, kCyan);
    // 回線状態の点滅ランプ
    Fill(SW - 146.0f, 42.0f, 8.0f, 8.0f, WithAlpha(kGreen, (fmodf(t, 1.2f) < 0.8f) ? 1.0f : 0.25f));

    //--------------------------------------------------------------------------
    // ミッション一覧
    //--------------------------------------------------------------------------
    int clearedCount = 0;
    for (int i = 0; i < MISSION_COUNT; ++i)
    {
        const MissionDef& m = k_MissionDefs[i];
        const float y   = LIST_Y + i * ROW_STEP;
        const bool  sel = (i == g_Selected);
        if (Mission_IsCleared(i)) ++clearedCount;

        if (sel)
        {
            Panel(LIST_X, y, LIST_W, ROW_H, kPanelHi, kCyan);
            const float e = 3.0f + pulse * 3.0f;
            Brackets(LIST_X - e, y - e, LIST_W + e * 2.0f, ROW_H + e * 2.0f, 12.0f, kCyan, 2.0f);
            Fill(LIST_X + 1.0f, y + 1.0f, 4.0f, ROW_H - 2.0f, kCyan);
        }
        else
        {
            Panel(LIST_X, y, LIST_W, ROW_H, kPanel, WithAlpha(kCyanDim, 0.45f));
        }

        // 番号の箱
        Fill (LIST_X + 14.0f, y + 12.0f, 48.0f, 48.0f, WithAlpha(kCyan, sel ? 0.22f : 0.06f));
        Frame(LIST_X + 14.0f, y + 12.0f, 48.0f, 48.0f, WithAlpha(kCyan, sel ? 0.9f : 0.35f), 1.0f);

        // 難度（ひし形5つ）
        for (int r = 0; r < 5; ++r)
            Diamond(LIST_X + LIST_W - 96.0f + r * 16.0f, y + 52.0f, 4.5f,
                    (r < m.rank) ? WithAlpha(RankColor(m.rank), sel ? 1.0f : 0.6f) : WithAlpha(kCyan, 0.12f));

        // 完了印
        if (Mission_IsCleared(i))
        {
            Fill (LIST_X + LIST_W - 104.0f, y + 10.0f, 88.0f, 18.0f, WithAlpha(kAmber, 0.18f));
            Frame(LIST_X + LIST_W - 104.0f, y + 10.0f, 88.0f, 18.0f, WithAlpha(kAmber, 0.8f), 1.0f);
        }
    }

    //--------------------------------------------------------------------------
    // 戦術マップ／作戦データ／ブリーフィングのパネル
    //--------------------------------------------------------------------------
    Panel(MAP_X, MAP_Y, MAP_W, MAP_H);
    Brackets(MAP_X - 4.0f, MAP_Y - 4.0f, MAP_W + 8.0f, MAP_H + 8.0f, 14.0f, WithAlpha(kCyan, 0.8f));
    DrawTacticalMap(md);

    Panel(DATA_X, DATA_Y, DATA_W, DATA_H);
    Fill(DATA_X + 1.0f, DATA_Y + 118.0f, DATA_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.25f));
    Fill(DATA_X + 1.0f, DATA_Y + 268.0f, DATA_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.25f));
    // 脅威度ゲージ
    SegmentBar(DATA_X + 150.0f, DATA_Y + 286.0f, 200.0f, 10.0f, 10, md.rank / 5.0f,
               rankCol, WithAlpha(kCyan, 0.1f));

    Panel(BRF_X, BRF_Y, BRF_W, BRF_H);
    Scanlines(BRF_X + 1.0f, BRF_Y + 1.0f, BRF_W - 2.0f, BRF_H - 2.0f, 4.0f, 0.025f);
    Fill(BRF_X + 1.0f, BRF_Y + 32.0f, BRF_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.25f));

    // 出撃ボタン
    {
        const XMFLOAT4 edge = { 0.35f, 1.0f, 0.6f, 1.0f };
        Panel(SORTIE_X, SORTIE_Y, SORTIE_W, SORTIE_H, { 0.05f, 0.35f, 0.18f, 0.45f + 0.35f * pulse }, edge, 12.0f);
        Brackets(SORTIE_X - 5.0f, SORTIE_Y - 5.0f, SORTIE_W + 10.0f, SORTIE_H + 10.0f, 10.0f, WithAlpha(edge, 0.6f + 0.4f * pulse));
    }

    //--------------------------------------------------------------------------
    // 文字
    //--------------------------------------------------------------------------
    wchar_t buf[128];
    const D2D1_COLOR_F white  = D2D1::ColorF(0.92f, 0.97f, 1.0f, 1.0f);
    const D2D1_COLOR_F label  = ToD2D(kCyan, 0.65f);
    const D2D1_COLOR_F cyan   = ToD2D(kCyan);

    // ヘッダ
    Text(L"SORTIE // MISSION SELECT", 52.0f, 12.0f, 40.0f, cyan, UIFont::Display, UIAlign::Left, true);
    Text(L"ARMORED OPERATIONS NETWORK  ―  出撃する作戦を選択せよ", 470.0f, 30.0f, 14.0f, label, UIFont::Body);
    {
        const int sec = static_cast<int>(g_Time);
        swprintf_s(buf, L"T+%02d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);
        Text(buf, SW - 40.0f, 14.0f, 16.0f, cyan, UIFont::Mono, UIAlign::Right, true);
        Text(L"LINK  STABLE", SW - 40.0f, 38.0f, 13.0f, ToD2D(kGreen, 0.9f), UIFont::Mono, UIAlign::Right);
    }

    // 一覧
    Text(L"MISSION LIST", LIST_X, LIST_Y - 26.0f, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
    for (int i = 0; i < MISSION_COUNT; ++i)
    {
        const MissionDef& m = k_MissionDefs[i];
        const float y   = LIST_Y + i * ROW_STEP;
        const bool  sel = (i == g_Selected);

        swprintf_s(buf, m.legacy ? L"--" : L"%02d", i + 1);
        Text(buf, LIST_X + 38.0f, y + 14.0f, 30.0f, ToD2D(kCyan, sel ? 1.0f : 0.55f), UIFont::Display, UIAlign::Center, true);
        Text(m.code, LIST_X + 76.0f, y + 10.0f, 12.0f, ToD2D(kCyan, sel ? 0.9f : 0.5f), UIFont::Mono);
        Text(m.title, LIST_X + 76.0f, y + 30.0f, 21.0f,
             sel ? white : D2D1::ColorF(0.7f, 0.78f, 0.85f, 1.0f), UIFont::Body, UIAlign::Left, sel);
        if (Mission_IsCleared(i))
            Text(L"COMPLETE", LIST_X + LIST_W - 60.0f, y + 11.0f, 12.0f, ToD2D(kAmber), UIFont::Mono, UIAlign::Center, true);
    }
    swprintf_s(buf, L"COMPLETED  %d / %d", clearedCount, static_cast<int>(MISSION_COUNT));
    Text(buf, LIST_X, LIST_Y + MISSION_COUNT * ROW_STEP + 4.0f, 15.0f,
         (clearedCount >= MISSION_COUNT) ? ToD2D(kGreen) : label, UIFont::Mono, UIAlign::Left, true);

    // 戦術マップ
    Text(L"TACTICAL MAP", MAP_X + 16.0f, MAP_Y + 10.0f, 15.0f, cyan, UIFont::Mono, UIAlign::Left, true);
    Text(md.area, MAP_X + MAP_W - 20.0f, MAP_Y + 12.0f, 12.0f, label, UIFont::Body, UIAlign::Right);

    // 作戦データ
    Text(md.code, DATA_X + 20.0f, DATA_Y + 12.0f, 14.0f, cyan, UIFont::Mono, UIAlign::Left, true);
    Text(md.title, DATA_X + 20.0f, DATA_Y + 34.0f, 32.0f, white, UIFont::Body, UIAlign::Left, true, 1.0f);
    Text(md.objective, DATA_X + 20.0f, DATA_Y + 84.0f, 15.0f, ToD2D(kAmber), UIFont::Body, UIAlign::Left, true);

    const float rowY0 = DATA_Y + 132.0f;
    constexpr float ROW = 26.0f;
    Text(L"CLIENT", DATA_X + 20.0f, rowY0,           12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(L"AREA",   DATA_X + 20.0f, rowY0 + ROW,     12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(L"REWARD", DATA_X + 20.0f, rowY0 + ROW * 2, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(L"PHASES", DATA_X + 20.0f, rowY0 + ROW * 3, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(md.client, DATA_X + 130.0f, rowY0 - 3.0f,           16.0f, white, UIFont::Body);
    Text(md.area,   DATA_X + 130.0f, rowY0 + ROW - 3.0f,     16.0f, white, UIFont::Body);
    swprintf_s(buf, L"%d c", md.reward);
    Text(buf, DATA_X + 130.0f, rowY0 + ROW * 2 - 4.0f, 20.0f, ToD2D(kAmber), UIFont::Display, UIAlign::Left, true);
    swprintf_s(buf, L"%d", Mission_GetStageCount(md));
    Text(buf, DATA_X + 130.0f, rowY0 + ROW * 3 - 4.0f, 20.0f, white, UIFont::Display, UIAlign::Left, true);

    // 脅威分析
    const float thY = DATA_Y + 280.0f;
    Text(L"THREAT", DATA_X + 20.0f, thY, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"RANK %d", md.rank);
    Text(buf, DATA_X + 368.0f, thY - 3.0f, 18.0f, ToD2D(rankCol), UIFont::Display, UIAlign::Left, true);

    const int force = EstimateInitialForce(md);
    const int reinf = Mission_GetReinforceCount(md);
    if (force >= 0) swprintf_s(buf, L"%d 機  ＋増援 %d 機", force, reinf);
    else            swprintf_s(buf, L"不明（階層ごとに増加）");
    Text(L"FORCE",   DATA_X + 20.0f,  thY + 30.0f, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(buf,        DATA_X + 130.0f, thY + 27.0f, 16.0f, white, UIFont::Body);
    Text(L"FORMATION", DATA_X + 20.0f, thY + 56.0f, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(EnemyMixLabel(Mission_GetMainEnemyMix(md)), DATA_X + 130.0f, thY + 53.0f, 16.0f, white, UIFont::Body);

    swprintf_s(buf, L"ARMOR ×%.2f", md.enemyHpScale);
    Text(buf, DATA_X + 300.0f, thY + 30.0f, 14.0f, ToD2D(md.enemyHpScale > 1.0f ? kAmber : kCyan), UIFont::Mono, UIAlign::Left, true);
    const float limit = Mission_GetTimeLimit(md);
    swprintf_s(buf, L"TIME  %s", (limit > 0.0f) ? FormatTime(limit).c_str() : L"--:--");
    Text(buf, DATA_X + 300.0f, thY + 56.0f, 14.0f, ToD2D(limit > 0.0f ? kAmber : kCyan), UIFont::Mono, UIAlign::Left, true);

    const bool hasBoss = Mission_HasBoss(md);
    Text(hasBoss ? L"!! LARGE-SCALE WEAPON DETECTED" : L"NO LARGE-SCALE SIGNATURE",
         DATA_X + 20.0f, thY + 90.0f, 14.0f,
         hasBoss ? ToD2D(kRed, (fmodf(t, 0.8f) < 0.55f) ? 1.0f : 0.4f) : label,
         UIFont::Mono, UIAlign::Left, true);

    // ブリーフィング（1文字ずつ送る。末尾にカーソル）
    Text(L"BRIEFING", BRF_X + 16.0f, BRF_Y + 9.0f, 15.0f, cyan, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"ENCRYPTED CHANNEL  //  %s", md.client);
    Text(buf, BRF_X + BRF_W - 20.0f, BRF_Y + 11.0f, 12.0f, label, UIFont::Body, UIAlign::Right);
    {
        const std::wstring brief(md.briefing);
        const size_t shown = std::min(brief.size(), static_cast<size_t>(g_BriefTime * BRIEF_CHARS_PER_SEC));
        std::wstring text = brief.substr(0, shown);
        if (fmodf(t, 0.8f) < 0.45f) text += L"_";
        Text(text, BRF_X + 24.0f, BRF_Y + 48.0f, 17.0f, D2D1::ColorF(0.82f, 0.9f, 0.95f, 1.0f), UIFont::Body);
    }

    // 装備
    swprintf_s(buf, L"R-ARM  %hs", k_WeaponDefs[AssemblyScreen_GetRightWeapon()].name);
    Text(buf, BRF_X + 24.0f, SORTIE_Y + 6.0f, 14.0f, cyan, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"L-ARM  %hs", k_WeaponDefs[AssemblyScreen_GetLeftWeapon()].name);
    Text(buf, BRF_X + 24.0f, SORTIE_Y + 28.0f, 14.0f, cyan, UIFont::Mono, UIAlign::Left, true);

    // 出撃ボタン
    Text(L"SORTIE", SORTIE_X + SORTIE_W * 0.5f, SORTIE_Y + 8.0f, 34.0f,
         D2D1::ColorF(0.85f, 1.0f, 0.9f, 1.0f), UIFont::Display, UIAlign::Center, true);

    FlushText();

    InputHint_Draw(
        "{UP}{DOWN} Mission    {ENTER} Sortie    {ESC} Back",
        "{DPAD_UP}{DPAD_DN} Mission    {A} Sortie    {B} Back",
        L"作戦を選択して出撃します（戻るとアセンブリをやり直せます）");
}

MissionSelectResult MissionSelect_GetResult()
{
    MissionSelectResult r = g_Result;
    g_Result = MissionSelectResult::None;
    return r;
}
