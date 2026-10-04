/*==============================================================================
   ウェーブ管理 [WaveManager.cpp]
   Author : 51106
   Date   : 2026/06/12
==============================================================================*/
#include "WaveManager.h"
#include "game.h"
#include "map.h"
#include "DirectWrite.h"
#include "direct3d.h"
#include "sprite.h"
#include "player.h"
#include "EnemyManager.h"
#include "SciFiUI.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <vector>
#include <random>
#include <cstdio>

using namespace DirectX;

namespace
{
    static constexpr int   MAX_WAVE        = 5;
    static constexpr double WAVE_START_SEC  = 3.0;   // カウントダウン時間
    static constexpr double WAVE_CLEAR_SEC  = 2.0;   // クリア演出時間
    static constexpr double SHOP_SEC        = 8.0;   // 購入タイム（ショップを開くとポーズするため短め）

    // 敵の密度倍率（各ウェーブの湧き数・ボスの取り巻きに掛ける）
    static constexpr int   DENSITY_MULT     = 3;

    // 難易度オフセット：スポーン計算上のウェーブを底上げする。
    // 2 なら「1ウェーブ目＝従来の3ウェーブ目相当」の構成・数になる。
    static constexpr int   DIFF_OFFSET      = 2;

    static int       g_Wave    = 0;
    static WavePhase g_Phase   = WavePhase::Idle;
    static double    g_Timer   = 0.0;
    static int       g_Credits = 0;

    static DirectWrite* g_pDW = nullptr;

    //------------------------------------------------------------------
    // ウェーブごとのスポーン設定
    //------------------------------------------------------------------
    struct SpawnEntry { int type; int count; };

    // EnemyType の int 値（EnemyManager.h と一致させる）
    enum : int { T_NORMAL=0, T_TANK=1, T_SPEED=2, T_SNIPER=3 };
    // ※ EnemyType: Normal=0, Tank=1, Speed=2, Sniper=3

    // 5の倍数のウェーブ（5, 10）はボスウェーブ
    static bool IsBossWave(int wave) { return (wave % 5) == 0; }

    //------------------------------------------------------------------
    // 所持クレジット表示（ミニマップの下・右端）
    //------------------------------------------------------------------
    static constexpr float CREDIT_X = 1310.0f;   // 左端（ミニマップと同じ幅にそろえる）
    static constexpr float CREDIT_Y = 330.0f;    // 上端（ミニマップの範囲表示の下）
    static constexpr float CREDIT_W = 270.0f;
    static constexpr float CREDIT_H = 52.0f;

    static void DrawCredits()
    {
        using namespace SciFiUI;

        BeginSprites();
        Panel(CREDIT_X, CREDIT_Y, CREDIT_W, CREDIT_H, kPanel, WithAlpha(kAmber, 0.55f), 10.0f);

        wchar_t buf[32];
        swprintf_s(buf, L"%d c", g_Credits);
        Text(L"CREDIT", CREDIT_X + 14.0f, CREDIT_Y + 8.0f, 13.0f, ToD2D(kAmber, 0.85f), UIFont::Mono, UIAlign::Left, true);
        Text(buf, CREDIT_X + CREDIT_W - 16.0f, CREDIT_Y + 10.0f, 30.0f, ToD2D(kAmber), UIFont::Display, UIAlign::Right, true);
        FlushText();
    }

    static std::vector<SpawnEntry> CalcSpawnList(int wave)
    {
        // 難易度オフセットを掛けた実効ウェーブで構成・数を決める
        const int ew = wave + DIFF_OFFSET;

        // 基本数：(実効wave * 2 + 3) × 密度倍率
        const int base = (ew * 2 + 3) * DENSITY_MULT;
        std::vector<SpawnEntry> list;

        // 割合指定で追加（0体になる種は入れない）
        auto add = [&](int type, float frac)
        {
            const int c = static_cast<int>(base * frac);
            if (c > 0) list.push_back({ type, c });
        };

        // 実効ウェーブが進むほど強い敵種（Speed→Sniper→Tank）を段階的に混ぜる
        //   Normal=0 / Tank=1 / Speed=2 / Sniper=3
        if (ew <= 1)
        {
            list.push_back({ 0 /*Normal*/, base });                 // 導入：通常のみ
        }
        else if (ew == 2)
        {
            add(0, 0.60f); add(2, 0.40f);                           // ＋Speed（機動戦）
        }
        else if (ew == 3)
        {
            add(0, 0.45f); add(2, 0.35f); add(3, 0.20f);            // ＋Sniper（遠距離の圧）
        }
        else // 実効 wave 4 以降：全種フルミックス
        {
            add(0, 0.35f); add(2, 0.30f); add(1, 0.20f); add(3, 0.15f);
        }
        return list;
    }

    // EnemyType::Boss の int 値（EnemyManager.h: Normal=0,Tank=1,Speed=2,Sniper=3,Boss=4）
    static constexpr int T_BOSS = 4;

    static void SpawnWaveEnemies(int wave)
    {
        const auto& spawns = Map_GetEnemySpawnPositions();
        if (spawns.empty()) return;

        std::mt19937 rng(static_cast<unsigned>(wave * 12345));
        std::uniform_int_distribution<int> pick(0, (int)spawns.size() - 1);

        if (IsBossWave(wave))
        {
            // ボスウェーブ：ボス2体＋取り巻き（Tank/Speed/Sniper の混成）
            constexpr int BOSS_COUNT = 2;
            for (int b = 0; b < BOSS_COUNT; ++b)
                Game_SpawnEnemy(spawns[pick(rng)], T_BOSS);

            // ボス2体ぶん重いので取り巻きは控えめに
            const int adds = (2 + wave / 5) * DENSITY_MULT;
            for (int i = 0; i < adds; ++i)
            {
                const int t = (i % 3 == 0) ? 1 /*Tank*/
                            : (i % 3 == 1) ? 2 /*Speed*/
                                           : 3 /*Sniper*/;
                Game_SpawnEnemy(spawns[pick(rng)], t);
            }
            return;
        }

        const auto list = CalcSpawnList(wave);
        for (const auto& e : list)
            for (int i = 0; i < e.count; ++i)
                Game_SpawnEnemy(spawns[pick(rng)], e.type);
    }
}

//==============================================================================
// 初期化 / 終了
//==============================================================================
void WaveManager_Initialize()
{
    g_Wave    = 0;
    g_Phase   = WavePhase::Idle;
    g_Timer   = 0.0;
    g_Credits = 0;

    if (!g_pDW)
    {
        static FontData fd;
        fd.font          = Font::Arial;
        fd.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fd.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fd.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize      = 34.0f;
        fd.localeName    = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color         = D2D1::ColorF(1, 1, 1, 1);
        g_pDW = new DirectWrite(&fd);
        g_pDW->Init();
    }
}

void WaveManager_Finalize()
{
    if (g_pDW) { g_pDW->Release(); delete g_pDW; g_pDW = nullptr; }
}

//==============================================================================
// サバイバル開始
//==============================================================================
void WaveManager_StartSurvival()
{
    g_Wave  = 1;
    g_Phase = WavePhase::WaveStart;
    g_Timer = WAVE_START_SEC;
}

//==============================================================================
// 更新
//==============================================================================
void WaveManager_Update(double dt)
{
    if (g_Phase == WavePhase::Idle || g_Phase == WavePhase::Victory) return;

    g_Timer -= dt;

    switch (g_Phase)
    {
    case WavePhase::WaveStart:
        if (g_Timer <= 0.0)
        {
            SpawnWaveEnemies(g_Wave);
            g_Phase = WavePhase::Fighting;
        }
        break;

    case WavePhase::Fighting:
        if (Game_GetAliveEnemyCount() == 0)
        {
            g_Phase = WavePhase::WaveCleared;
            g_Timer = WAVE_CLEAR_SEC;
        }
        break;

    case WavePhase::WaveCleared:
        if (g_Timer <= 0.0)
        {
            if (g_Wave >= MAX_WAVE)
            {
                g_Phase = WavePhase::Victory;
            }
            else
            {
                g_Phase = WavePhase::Shopping;
                g_Timer = SHOP_SEC;
            }
        }
        break;

    case WavePhase::Shopping:
        if (g_Timer <= 0.0)
        {
            g_Wave++;
            g_Phase = WavePhase::WaveStart;
            g_Timer = WAVE_START_SEC;
        }
        break;

    default: break;
    }
}

//==============================================================================
// HUD描画
//==============================================================================
void WaveManager_Draw()
{
    if (!g_pDW) return;
    if (g_Phase == WavePhase::Idle) return;

    const float scaleX = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
    const float scaleY = (float)Direct3D_GetBackBufferHeight() / 900.0f;
    const float cx = 800.0f;

    char buf[64];
    D2D1_COLOR_F col = D2D1::ColorF(1, 1, 1, 1);

    switch (g_Phase)
    {
    case WavePhase::WaveStart:
        if (IsBossWave(g_Wave))
        {
            snprintf(buf, sizeof(buf), "BOSS WAVE %d  START IN  %.0f", g_Wave, g_Timer + 1.0);
            col = D2D1::ColorF(1.0f, 0.3f, 0.3f, 1.0f);   // ボスは赤系で強調
        }
        else
        {
            snprintf(buf, sizeof(buf), "WAVE %d  START IN  %.0f", g_Wave, g_Timer + 1.0);
            col = D2D1::ColorF(1.0f, 0.9f, 0.3f, 1.0f);
        }
        break;

    case WavePhase::Fighting:
        if (IsBossWave(g_Wave))
        {
            snprintf(buf, sizeof(buf), "BOSS WAVE  %d / %d      ENEMIES: %d",
                g_Wave, MAX_WAVE, Game_GetAliveEnemyCount());
            col = D2D1::ColorF(1.0f, 0.4f, 0.4f, 0.9f);
        }
        else
        {
            snprintf(buf, sizeof(buf), "WAVE  %d / %d      ENEMIES: %d",
                g_Wave, MAX_WAVE, Game_GetAliveEnemyCount());
            col = D2D1::ColorF(1, 1, 1, 0.85f);
        }
        break;

    case WavePhase::WaveCleared:
        snprintf(buf, sizeof(buf), "WAVE %d  CLEARED!", g_Wave);
        col = D2D1::ColorF(0.3f, 1.0f, 0.5f, 1.0f);
        break;

    case WavePhase::Shopping:
        snprintf(buf, sizeof(buf), "SHOP TIME  %.0f s", g_Timer);
        col = D2D1::ColorF(0.4f, 0.8f, 1.0f, 1.0f);
        break;

    case WavePhase::Victory:
        snprintf(buf, sizeof(buf), "ALL WAVES CLEARED!");
        col = D2D1::ColorF(1.0f, 0.9f, 0.2f, 1.0f);
        break;

    default: return;
    }

    g_pDW->SetScale(scaleX, scaleY);
    g_pDW->BeginBatch();
    g_pDW->DrawAt(buf, cx, 56.0f, 480.0f, col, 1.5f);
    g_pDW->EndBatch();
    g_pDW->SetScale(1.0f, 1.0f);

    DrawCredits();
}

//==============================================================================
// ゲッター
//==============================================================================
int       WaveManager_GetCurrentWave()        { return g_Wave; }
WavePhase WaveManager_GetPhase()              { return g_Phase; }
float     WaveManager_GetShopTimeRemaining()  { return (float)g_Timer; }
bool      WaveManager_IsVictory()             { return g_Phase == WavePhase::Victory; }

int  WaveManager_GetCredits()                 { return g_Credits; }
void WaveManager_AddCredits(int amount)       { if (amount > 0) g_Credits += amount; }
bool WaveManager_SpendCredits(int amount)
{
    if (g_Credits < amount) return false;
    g_Credits -= amount;
    return true;
}
