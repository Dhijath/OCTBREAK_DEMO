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
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <vector>
#include <random>
#include <cstdio>

using namespace DirectX;

namespace
{
    static constexpr int   MAX_WAVE        = 10;
    static constexpr double WAVE_START_SEC  = 3.0;   // カウントダウン時間
    static constexpr double WAVE_CLEAR_SEC  = 2.0;   // クリア演出時間
    static constexpr double SHOP_SEC        = 15.0;  // 購入タイム

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
    enum : int { T_NORMAL=0, T_TANK=3, T_SPEED=2, T_SNIPER=3 };
    // ※ EnemyType: Normal=0, Tank=1, Speed=2, Sniper=3

    static std::vector<SpawnEntry> CalcSpawnList(int wave)
    {
        // 基本数：wave * 2 + 3（wave1=5体、wave10=23体）
        const int base = wave * 2 + 3;
        std::vector<SpawnEntry> list;

        if (wave <= 3)
        {
            list.push_back({ 0 /*Normal*/, base });
        }
        else if (wave <= 6)
        {
            list.push_back({ 0 /*Normal*/, base / 2 + 1 });
            list.push_back({ 2 /*Speed*/,  base / 2     });
        }
        else if (wave <= 9)
        {
            list.push_back({ 0 /*Normal*/, base / 3 + 1 });
            list.push_back({ 2 /*Speed*/,  base / 3     });
            list.push_back({ 1 /*Tank*/,   base / 3     });
        }
        else // wave 10
        {
            list.push_back({ 0 /*Normal*/, 6 });
            list.push_back({ 2 /*Speed*/,  6 });
            list.push_back({ 1 /*Tank*/,   5 });
            list.push_back({ 3 /*Sniper*/, 4 });
        }
        return list;
    }

    static void SpawnWaveEnemies(int wave)
    {
        const auto& spawns = Map_GetEnemySpawnPositions();
        if (spawns.empty()) return;

        std::mt19937 rng(static_cast<unsigned>(wave * 12345));
        std::uniform_int_distribution<int> pick(0, (int)spawns.size() - 1);

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
        fd.fontSize      = 52.0f;
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
        snprintf(buf, sizeof(buf), "WAVE %d  START IN  %.0f", g_Wave, g_Timer + 1.0);
        col = D2D1::ColorF(1.0f, 0.9f, 0.3f, 1.0f);
        break;

    case WavePhase::Fighting:
        snprintf(buf, sizeof(buf), "WAVE  %d / %d      ENEMIES: %d",
            g_Wave, MAX_WAVE, Game_GetAliveEnemyCount());
        col = D2D1::ColorF(1, 1, 1, 0.85f);
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
    g_pDW->DrawAt(buf, cx, 60.0f, 700.0f, col, 1.5f);
    g_pDW->EndBatch();
    g_pDW->SetScale(1.0f, 1.0f);
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
