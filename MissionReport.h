/*==============================================================================

   作戦結果レポート [MissionReport.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ゲーム終了時に Game_Manager が集計し、クリア画面・リザルト画面が表示する。
   （作戦名・成否・所要時間・撃破数・与被ダメージ・報酬・評価ランク）

==============================================================================*/
#pragma once

struct MissionReport
{
    bool survival = false;               // サバイバルモードの結果か
    bool success  = false;               // 作戦成功か

    const wchar_t* code       = L"";     // "MISSION 01" / "SURVIVAL"
    const wchar_t* title      = L"";     // 作戦名
    const wchar_t* area       = L"";     // 作戦領域
    const wchar_t* failReason = L"";     // 失敗理由（成功時は空）

    float time        = 0.0f;            // 作戦時間（秒）
    int   kills       = 0;               // 撃破数
    int   damageDealt = 0;
    int   damageTaken = 0;
    int   reward      = 0;               // 成功報酬（失敗時は 0）
    int   score       = 0;               // 最終スコア
    int   phaseReached = 1;              // 到達したフェーズ（1〜）
    int   phaseCount   = 1;
    wchar_t rank      = L'-';            // 評価（S / A / B / C / D / E）
};

// 直近の作戦結果（Game_Manager.cpp が保持する）
const MissionReport& MissionReport_Get();
