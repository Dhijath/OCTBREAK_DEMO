/*==============================================================================

   作戦結果画面（デブリーフィング） [Result.cpp]
   Author : 51106
   Date   : 2026/04/01（2026/10/03 SF調に一新）

--------------------------------------------------------------------------------
   リザルト（作戦失敗）とクリア（作戦成功）で共通の結果画面。
   Game_Manager が集計した MissionReport を表示する。

   ■レイアウト（1600×900）
     見出し     : y= 70〜250  「MISSION ACCOMPLISHED / MISSION FAILED」と作戦名
     戦果パネル : x=180〜1000, y=280〜760  各項目が順に表示され、数値が数え上がる
     評価パネル : x=1040〜1420, y=280〜760 評価ランク（S〜E）と失敗理由

   入力は Game_Manager 側（決定でミッション選択 / モード選択へ戻る）。
==============================================================================*/
#include "Result.h"
#include "MissionReport.h"
#include "SciFiUI.h"
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace SciFiUI;
using DirectX::XMFLOAT4;

namespace
{
    float g_Time = 0.0f;   // 画面に入ってからの経過秒

    constexpr float SW = 1600.0f;

    constexpr float STAT_X = 180.0f,  STAT_Y = 280.0f, STAT_W = 820.0f, STAT_H = 480.0f;
    constexpr float RANK_X = 1040.0f, RANK_Y = 280.0f, RANK_W = 380.0f, RANK_H = 480.0f;

    constexpr float ROW_FIRST = 0.7f;    // 最初の項目が出る時刻
    constexpr float ROW_GAP   = 0.22f;   // 項目ごとの間隔
    constexpr float COUNT_UP  = 0.6f;    // 数値が数え上がる時間

    constexpr int   STAT_ROWS   = 6;                                        // 戦果の項目数
    constexpr float TOTAL_START = ROW_FIRST + STAT_ROWS * ROW_GAP + 0.2f;   // 合計スコアが出る時刻
    constexpr float RANK_START  = TOTAL_START + 0.5f;                       // 評価ランクが出る時刻

    int g_SeTick = -1;   // 項目が出るたびの電子音
    int g_SeRank = -1;   // 評価ランクが出る瞬間

    // 3桁区切り
    std::wstring Grouped(int value)
    {
        const bool neg = value < 0;
        std::wstring digits = std::to_wstring(neg ? -value : value);
        std::wstring out;
        for (size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % 3 == 0) out += L',';
            out += digits[i];
        }
        return neg ? L"-" + out : out;
    }

    std::wstring FormatTime(float sec)
    {
        const int total = static_cast<int>(std::max(0.0f, sec));
        const int tenth = static_cast<int>((std::max(0.0f, sec) - total) * 10.0f);
        wchar_t buf[32];
        swprintf_s(buf, L"%02d:%02d.%d", total / 60, total % 60, tenth);
        return buf;
    }

    // 0〜1：時刻 start から duration かけて進む
    float Progress(float start, float duration)
    {
        return std::clamp((g_Time - start) / duration, 0.0f, 1.0f);
    }

    float EaseOut(float t)
    {
        return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    }

    XMFLOAT4 RankColor(wchar_t rank)
    {
        switch (rank)
        {
        case L'S': return { 1.00f, 0.85f, 0.30f, 1.0f };
        case L'A': return kGreen;
        case L'B': return kCyan;
        case L'C': return { 0.70f, 0.80f, 0.90f, 1.0f };
        case L'D': return kAmber;
        default:   return kRed;
        }
    }

    const wchar_t* RankComment(wchar_t rank)
    {
        switch (rank)
        {
        case L'S': return L"完璧な作戦行動だ。";
        case L'A': return L"上出来だ。依頼主も満足している。";
        case L'B': return L"作戦目標は達成した。";
        case L'C': return L"損害が大きい。次は改善しろ。";
        case L'D': return L"辛うじて成功、といったところだ。";
        default:   return L"作戦は失敗した。";
        }
    }

    //==========================================================================
    // 結果画面の描画（成功・失敗共通）
    //==========================================================================
    void DrawDebrief()
    {
        const MissionReport& r = MissionReport_Get();
        const XMFLOAT4 accent  = r.success ? kCyan : kRed;
        const XMFLOAT4 headCol = r.success ? kGreen : kRed;

        BeginSprites();

        //----------------------------------------------------------------------
        // 背景：暗い地＋格子＋走査線＋流れる走査帯
        //----------------------------------------------------------------------
        Fill(0.0f, 0.0f, SW, 900.0f, { 0.010f, 0.018f, 0.032f, 1.0f });
        Grid(0.0f, 0.0f, SW, 900.0f, 40.0f, WithAlpha(accent, 0.045f));
        Scanlines(0.0f, 0.0f, SW, 900.0f, 4.0f, 0.035f);
        const float sweepY = fmodf(g_Time * 140.0f, 1100.0f) - 100.0f;
        Fill(0.0f, sweepY, SW, 60.0f, WithAlpha(accent, 0.035f));
        Fill(0.0f, sweepY + 58.0f, SW, 2.0f, WithAlpha(accent, 0.10f));

        // 画面四隅の枠と上下の帯
        Brackets(20.0f, 20.0f, SW - 40.0f, 860.0f, 40.0f, WithAlpha(accent, 0.55f), 2.0f);
        Fill(40.0f, 56.0f, SW - 80.0f, 1.0f, WithAlpha(accent, 0.30f));
        Ticks(40.0f, 58.0f, SW - 80.0f, 80, 10, WithAlpha(accent, 0.30f));

        //----------------------------------------------------------------------
        // 見出し
        //----------------------------------------------------------------------
        const float headIn = EaseOut(Progress(0.0f, 0.45f));
        const float headW  = 900.0f * headIn;
        Fill(800.0f - headW * 0.5f, 92.0f, headW, 104.0f, WithAlpha(headCol, 0.08f));
        Fill(800.0f - headW * 0.5f, 92.0f, headW, 2.0f, WithAlpha(headCol, 0.85f));
        Fill(800.0f - headW * 0.5f, 194.0f, headW, 2.0f, WithAlpha(headCol, 0.85f));
        if (!r.success)
        {
            HazardTape(0.0f, 262.0f, SW, 5.0f, g_Time * 80.0f, WithAlpha(kRed, 0.6f));
        }

        //----------------------------------------------------------------------
        // 戦果パネル
        //----------------------------------------------------------------------
        Panel(STAT_X, STAT_Y, STAT_W, STAT_H, kPanel, WithAlpha(accent, 0.5f), 18.0f);
        Brackets(STAT_X - 5.0f, STAT_Y - 5.0f, STAT_W + 10.0f, STAT_H + 10.0f, 14.0f, WithAlpha(accent, 0.8f));
        Fill(STAT_X + 1.0f, STAT_Y + 34.0f, STAT_W - 2.0f, 1.0f, WithAlpha(accent, 0.3f));

        struct Row { const wchar_t* label; const wchar_t* sub; float value; int kind; XMFLOAT4 color; };
        // kind : 0 = 整数 / 1 = 時間 / 2 = フェーズ
        const Row rows[] =
        {
            { L"MISSION TIME",  L"作戦時間",     r.time,                              1, kCyan  },
            { L"PHASE",         L"到達段階",     static_cast<float>(r.phaseReached),  2, kCyan  },
            { L"KILLS",         L"撃破数",       static_cast<float>(r.kills),         0, kCyan  },
            { L"DAMAGE DEALT",  L"与ダメージ",   static_cast<float>(r.damageDealt),   0, kAmber },
            { L"DAMAGE TAKEN",  L"被ダメージ",   static_cast<float>(r.damageTaken),   0, kRed   },
            { L"REWARD",        L"成功報酬",     static_cast<float>(r.reward),        0, kGreen },
        };
        constexpr int ROW_COUNT = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
        static_assert(ROW_COUNT == STAT_ROWS, "効果音のタイミング（STAT_ROWS）を合わせること");
        constexpr float ROW_TOP = STAT_Y + 50.0f;
        constexpr float ROW_STEP = 52.0f;

        for (int i = 0; i < ROW_COUNT; ++i)
        {
            const float a = Progress(ROW_FIRST + i * ROW_GAP, 0.2f);
            if (a <= 0.0f) continue;
            const float y = ROW_TOP + i * ROW_STEP;
            Fill(STAT_X + 24.0f, y + 44.0f, (STAT_W - 48.0f) * EaseOut(a), 1.0f, WithAlpha(accent, 0.18f));
            Diamond(STAT_X + 34.0f, y + 22.0f, 4.0f, WithAlpha(rows[i].color, a));
        }

        // 合計スコア
        const float totalStart = TOTAL_START;
        const float totalA = Progress(totalStart, 0.25f);
        const float totalY = ROW_TOP + ROW_COUNT * ROW_STEP + 14.0f;
        if (totalA > 0.0f)
        {
            Fill(STAT_X + 16.0f, totalY, STAT_W - 32.0f, 2.0f, WithAlpha(accent, 0.7f * totalA));
            Fill(STAT_X + 16.0f, totalY + 4.0f, STAT_W - 32.0f, 70.0f, WithAlpha(accent, 0.07f * totalA));
        }

        //----------------------------------------------------------------------
        // 評価パネル
        //----------------------------------------------------------------------
        const float rankStart = RANK_START;
        const float rankA     = Progress(rankStart, 0.15f);
        const XMFLOAT4 rankCol = RankColor(r.rank);

        Panel(RANK_X, RANK_Y, RANK_W, RANK_H, kPanel, WithAlpha(rankCol, 0.5f), 18.0f);
        Brackets(RANK_X - 5.0f, RANK_Y - 5.0f, RANK_W + 10.0f, RANK_H + 10.0f, 14.0f, WithAlpha(rankCol, 0.8f));
        Fill(RANK_X + 1.0f, RANK_Y + 34.0f, RANK_W - 2.0f, 1.0f, WithAlpha(rankCol, 0.3f));

        const float rcx = RANK_X + RANK_W * 0.5f;
        const float rcy = RANK_Y + 190.0f;
        if (rankA > 0.0f)
        {
            // 出現の瞬間に広がる閃光と、回る照準枠
            const float flash = 1.0f - Progress(rankStart, 0.5f);
            Diamond(rcx, rcy, 120.0f + 40.0f * flash, WithAlpha(rankCol, 0.10f * rankA + 0.25f * flash));
            Diamond(rcx, rcy, 104.0f, WithAlpha({ 0.0f, 0.0f, 0.0f, 1.0f }, 0.55f * rankA));
            for (int k = 0; k < 4; ++k)
            {
                const float ang = g_Time * 0.6f + k * DirectX::XM_PIDIV2;
                LineAngle(rcx + cosf(ang) * 140.0f, rcy + sinf(ang) * 140.0f, 34.0f, ang + DirectX::XM_PIDIV2,
                          WithAlpha(rankCol, 0.7f * rankA), 2.0f);
            }
        }
        else
        {
            // 判定中：小さなひし形が明滅する
            for (int k = 0; k < 3; ++k)
                Diamond(rcx - 30.0f + k * 30.0f, rcy, 6.0f,
                        WithAlpha(rankCol, (static_cast<int>(g_Time * 6.0f) % 3 == k) ? 0.9f : 0.25f));
        }

        //----------------------------------------------------------------------
        // 文字
        //----------------------------------------------------------------------
        Text(r.success ? L"SYS://DEBRIEFING  >  RESULT : SUCCESS" : L"SYS://DEBRIEFING  >  RESULT : FAILURE",
             40.0f, 30.0f, 14.0f, ToD2D(accent, 0.85f), UIFont::Mono, UIAlign::Left, true);
        wchar_t buf[128];
        swprintf_s(buf, L"OPERATION TIME %s", FormatTime(r.time).c_str());
        Text(buf, SW - 40.0f, 30.0f, 14.0f, ToD2D(accent, 0.85f), UIFont::Mono, UIAlign::Right, true);

        Text(r.success ? L"MISSION ACCOMPLISHED" : L"MISSION FAILED", 800.0f, 96.0f, 84.0f,
             ToD2D(headCol, headIn), UIFont::Display, UIAlign::Center, true, 2.0f);
        swprintf_s(buf, L"%s  //  %s", r.code, r.title);
        Text(buf, 800.0f, 206.0f, 24.0f, D2D1::ColorF(0.92f, 0.97f, 1.0f, headIn), UIFont::Body, UIAlign::Center, true, 1.0f);
        Text(r.area, 800.0f, 236.0f, 13.0f, ToD2D(accent, 0.7f * headIn), UIFont::Mono, UIAlign::Center);

        Text(L"COMBAT RECORD", STAT_X + 20.0f, STAT_Y + 9.0f, 15.0f, ToD2D(accent), UIFont::Mono, UIAlign::Left, true);
        Text(L"戦果報告", STAT_X + STAT_W - 24.0f, STAT_Y + 8.0f, 15.0f, ToD2D(accent, 0.7f), UIFont::Body, UIAlign::Right);

        for (int i = 0; i < ROW_COUNT; ++i)
        {
            const float start = ROW_FIRST + i * ROW_GAP;
            const float a = Progress(start, 0.2f);
            if (a <= 0.0f) continue;
            const float y = ROW_TOP + i * ROW_STEP;
            const float t = EaseOut(Progress(start, COUNT_UP));

            Text(rows[i].label, STAT_X + 50.0f, y + 8.0f, 18.0f, ToD2D(kCyan, 0.85f * a), UIFont::Mono, UIAlign::Left, true);
            Text(rows[i].sub, STAT_X + 290.0f, y + 10.0f, 15.0f, D2D1::ColorF(0.7f, 0.78f, 0.85f, 0.8f * a), UIFont::Body);

            std::wstring value;
            switch (rows[i].kind)
            {
            case 1:  value = FormatTime(rows[i].value * t); break;
            case 2:
                swprintf_s(buf, L"%d / %d", r.phaseReached, r.phaseCount);
                value = buf;
                break;
            default: value = Grouped(static_cast<int>(rows[i].value * t)); break;
            }
            if (rows[i].kind == 0 && i == ROW_COUNT - 1 && !r.success) value = L"―";
            Text(value, STAT_X + STAT_W - 30.0f, y + 2.0f, 32.0f, ToD2D(rows[i].color, a), UIFont::Display, UIAlign::Right, true);
        }

        if (totalA > 0.0f)
        {
            const float t = EaseOut(Progress(totalStart, COUNT_UP * 1.5f));
            Text(L"TOTAL SCORE", STAT_X + 34.0f, totalY + 24.0f, 22.0f, ToD2D(accent, totalA), UIFont::Mono, UIAlign::Left, true);
            Text(Grouped(static_cast<int>(r.score * t)), STAT_X + STAT_W - 30.0f, totalY + 6.0f, 56.0f,
                 D2D1::ColorF(1.0f, 1.0f, 1.0f, totalA), UIFont::Display, UIAlign::Right, true, 1.5f);
        }

        Text(L"EVALUATION", RANK_X + 20.0f, RANK_Y + 9.0f, 15.0f, ToD2D(rankCol), UIFont::Mono, UIAlign::Left, true);
        Text(L"作戦評価", RANK_X + RANK_W - 24.0f, RANK_Y + 8.0f, 15.0f, ToD2D(rankCol, 0.7f), UIFont::Body, UIAlign::Right);
        if (rankA > 0.0f)
        {
            const wchar_t rankText[2] = { r.rank, L'\0' };
            Text(rankText, rcx, rcy - 105.0f, 190.0f, ToD2D(rankCol, rankA), UIFont::Display, UIAlign::Center, true, 3.0f);
            Text(RankComment(r.rank), rcx, RANK_Y + 340.0f, 18.0f, D2D1::ColorF(0.9f, 0.95f, 1.0f, rankA),
                 UIFont::Body, UIAlign::Center, true);
            if (!r.success && r.failReason && r.failReason[0])
                Text(r.failReason, rcx, RANK_Y + 380.0f, 16.0f, ToD2D(kRed, rankA), UIFont::Body, UIAlign::Center, true);
            else if (r.survival)
                Text(L"ALL WAVES CLEARED", rcx, RANK_Y + 380.0f, 16.0f, ToD2D(kGreen, rankA), UIFont::Mono, UIAlign::Center, true);
        }
        else
        {
            Text(L"ANALYZING...", rcx, rcy + 30.0f, 15.0f, ToD2D(rankCol, 0.7f), UIFont::Mono, UIAlign::Center, true);
        }

        // 決定で戻れることの案内（評価が出たあとに点滅）
        if (rankA >= 1.0f)
        {
            const float blink = 0.55f + 0.45f * sinf(g_Time * 4.0f);
            Text(r.survival ? L">>  RETURN TO MODE SELECT" : L">>  RETURN TO MISSION SELECT",
                 800.0f, 790.0f, 18.0f, ToD2D(accent, blink), UIFont::Mono, UIAlign::Center, true);
        }

        FlushText();
    }
}

//------------------------------------------------------------------------------
// 公開関数
//------------------------------------------------------------------------------
void Result_Initialize()
{
    g_Time = 0.0f;
    if (g_SeTick < 0) g_SeTick = LoadAudioWithVolume("resource/Sound/ui_tick.wav", 0.6f);
    if (g_SeRank < 0) g_SeRank = LoadAudioWithVolume("resource/Sound/ui_rank.wav", 0.9f);
}

void Result_Finalize()
{
    UnloadAudio(g_SeTick); g_SeTick = -1;
    UnloadAudio(g_SeRank); g_SeRank = -1;
}

void Result_Update(double elapsed_time)
{
    const float prev = g_Time;
    g_Time += static_cast<float>(elapsed_time);

    // 描画の演出（項目・合計・評価が順に出る）に合わせて効果音を鳴らす
    auto reached = [&](float at) { return prev < at && g_Time >= at; };
    for (int i = 0; i < STAT_ROWS; ++i)
        if (reached(ROW_FIRST + i * ROW_GAP)) PlayAudio(g_SeTick, false);
    if (reached(TOTAL_START)) PlayAudio(g_SeTick, false);
    if (reached(RANK_START))  PlayAudio(g_SeRank, false);
}

void Result_Draw()
{
    DrawDebrief();
}
