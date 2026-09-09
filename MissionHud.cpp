/*==============================================================================

   作戦表示 [MissionHud.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ■レイアウト（1600×900。右上はミニマップ、左上はHUDが使っている）
     目標パネル : x=500〜1080, y=14〜90
     残り時間   : x=1092〜1270, y=14〜90
     増援警告   : 全幅, y=190〜280
     作戦失敗   : 全幅, y=380〜500

==============================================================================*/
#include "MissionHud.h"
#include "SciFiUI.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace SciFiUI;

namespace
{
    float g_Time = 0.0f;

    constexpr float OBJ_X = 500.0f, OBJ_Y = 14.0f, OBJ_W = 580.0f, OBJ_H = 76.0f;
    constexpr float TIME_X = 1092.0f, TIME_W = 178.0f;
    constexpr float WARN_Y = 180.0f, WARN_H = 112.0f;
    constexpr float FAIL_Y = 380.0f, FAIL_H = 136.0f;
    constexpr float SCREEN_W = 1600.0f;

    // 秒 → "MM:SS.d"
    std::wstring FormatTime(float sec)
    {
        if (sec < 0.0f) sec = 0.0f;
        const int total = static_cast<int>(sec);
        const int tenth = static_cast<int>((sec - total) * 10.0f);
        wchar_t buf[32];
        swprintf_s(buf, L"%02d:%02d.%d", total / 60, total % 60, tenth);
        return buf;
    }
}

void MissionHud_Initialize()
{
    g_Time = 0.0f;
}

void MissionHud_Finalize()
{
}

void MissionHud_Update(double elapsed_time)
{
    g_Time += static_cast<float>(elapsed_time);
}

void MissionHud_Draw(const MissionHudState& s)
{
    const DirectX::XMFLOAT4 accent = s.bossPhase ? kRed : kCyan;

    BeginSprites();

    //--------------------------------------------------------------------------
    // 目標パネル
    //--------------------------------------------------------------------------
    Panel(OBJ_X, OBJ_Y, OBJ_W, OBJ_H, kPanel, WithAlpha(accent, 0.55f));
    Brackets(OBJ_X - 4.0f, OBJ_Y - 4.0f, OBJ_W + 8.0f, OBJ_H + 8.0f, 10.0f, WithAlpha(accent, 0.9f));
    Fill(OBJ_X + 1.0f, OBJ_Y + 24.0f, OBJ_W - 2.0f, 1.0f, WithAlpha(accent, 0.25f));
    // 左端の縦バー（ゆっくり明滅）
    Fill(OBJ_X + 8.0f, OBJ_Y + 32.0f, 3.0f, 34.0f, WithAlpha(accent, 0.6f + 0.4f * sinf(g_Time * 3.0f)));

    // フェーズの進行ゲージ
    SegmentBar(OBJ_X + OBJ_W - 120.0f, OBJ_Y + 9.0f, 100.0f, 6.0f, s.phaseCount,
               static_cast<float>(s.phase) / static_cast<float>(s.phaseCount),
               WithAlpha(accent, 0.9f), WithAlpha(accent, 0.18f));

    //--------------------------------------------------------------------------
    // 残り時間
    //--------------------------------------------------------------------------
    const bool hasTime = (s.timeLeft >= 0.0f);
    const bool hurry   = hasTime && s.timeLeft < 30.0f;
    const float blink  = (hurry && fmodf(g_Time, 0.6f) < 0.3f) ? 0.45f : 1.0f;
    const DirectX::XMFLOAT4 timeCol = hurry ? kRed : kAmber;
    if (hasTime)
    {
        Panel(TIME_X, OBJ_Y, TIME_W, OBJ_H, kPanel, WithAlpha(timeCol, 0.55f), 10.0f);
        Fill(TIME_X + 1.0f, OBJ_Y + 24.0f, TIME_W - 2.0f, 1.0f, WithAlpha(timeCol, 0.25f));
    }

    //--------------------------------------------------------------------------
    // 増援警告
    //--------------------------------------------------------------------------
    float warnAlpha = 0.0f;
    if (s.warning > 0.0f && !s.failed)
    {
        // 出だしで広がり、最後の 0.5 秒で消える。全体を点滅させる
        const float fade = std::fmin(1.0f, s.warning / 0.5f);
        warnAlpha = fade * ((fmodf(g_Time, 0.5f) < 0.32f) ? 1.0f : 0.55f);

        // 地は暗く（赤い文字が沈まないように）、上下に流れる警告帯
        Fill(0.0f, WARN_Y, SCREEN_W, WARN_H, WithAlpha({ 0.10f, 0.0f, 0.0f, 1.0f }, 0.72f * fade));
        HazardTape(0.0f, WARN_Y,                 SCREEN_W, 8.0f, g_Time * 120.0f, WithAlpha(kRed, 0.85f * warnAlpha));
        HazardTape(0.0f, WARN_Y + WARN_H - 8.0f, SCREEN_W, 8.0f, -g_Time * 120.0f, WithAlpha(kRed, 0.85f * warnAlpha));
        Brackets(440.0f, WARN_Y + 14.0f, 720.0f, WARN_H - 28.0f, 18.0f, WithAlpha(kRed, warnAlpha));
    }

    //--------------------------------------------------------------------------
    // 作戦失敗
    //--------------------------------------------------------------------------
    if (s.failed)
    {
        Fill(0.0f, FAIL_Y, SCREEN_W, FAIL_H, WithAlpha({ 0.0f, 0.0f, 0.0f, 1.0f }, 0.7f));
        Fill(0.0f, FAIL_Y, SCREEN_W, 2.0f, kRed);
        Fill(0.0f, FAIL_Y + FAIL_H - 2.0f, SCREEN_W, 2.0f, kRed);
        Scanlines(0.0f, FAIL_Y, SCREEN_W, FAIL_H, 4.0f, 0.06f);
    }

    //--------------------------------------------------------------------------
    // 文字
    //--------------------------------------------------------------------------
    wchar_t buf[64];
    swprintf_s(buf, L"OBJECTIVE  //  PHASE %d/%d", s.phase, s.phaseCount);
    Text(buf, OBJ_X + 14.0f, OBJ_Y + 6.0f, 13.0f, ToD2D(accent, 0.85f), UIFont::Mono, UIAlign::Left, true);

    if (s.objective)
        Text(s.objective, OBJ_X + 20.0f, OBJ_Y + 34.0f, 20.0f, D2D1::ColorF(0.92f, 0.97f, 1.0f, 1.0f),
             UIFont::Body, UIAlign::Left, true, 1.0f);
    if (s.detail)
        Text(s.detail, OBJ_X + OBJ_W - 16.0f, OBJ_Y + 38.0f, 17.0f, ToD2D(accent), UIFont::Mono, UIAlign::Right, true);

    if (hasTime)
    {
        Text(L"TIME LIMIT", TIME_X + 12.0f, OBJ_Y + 6.0f, 13.0f, ToD2D(timeCol, 0.85f), UIFont::Mono, UIAlign::Left, true);
        Text(FormatTime(s.timeLeft), TIME_X + TIME_W * 0.5f, OBJ_Y + 26.0f, 38.0f, ToD2D(timeCol, blink),
             UIFont::Display, UIAlign::Center, true);
    }

    if (warnAlpha > 0.0f)
    {
        Text(L"WARNING", 800.0f, WARN_Y + 16.0f, 52.0f, D2D1::ColorF(1.0f, 0.42f, 0.36f, warnAlpha),
             UIFont::Display, UIAlign::Center, true, 2.0f);
        swprintf_s(buf, L"敵増援 %d 機 降下を確認 ― ENEMY REINFORCEMENTS INBOUND", s.reinforceCount);
        Text(buf, 800.0f, WARN_Y + 72.0f, 16.0f, D2D1::ColorF(1.0f, 0.88f, 0.84f, std::fmin(1.0f, warnAlpha + 0.2f)),
             UIFont::Body, UIAlign::Center, true, 1.0f);
    }

    if (s.failed)
    {
        Text(L"MISSION FAILED", 800.0f, FAIL_Y + 18.0f, 64.0f, ToD2D(kRed), UIFont::Display, UIAlign::Center, true, 2.0f);
        Text(L"TIME OVER ― 作戦時間を超過しました", 800.0f, FAIL_Y + 98.0f, 18.0f,
             D2D1::ColorF(1.0f, 0.8f, 0.75f, 1.0f), UIFont::Body, UIAlign::Center, true);
    }

    FlushText();
}
