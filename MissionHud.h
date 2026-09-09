/*==============================================================================

   作戦表示 [MissionHud.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   プレイ中に画面上部へ出す作戦情報（ブロックステージのミッション用）。
     ・目標パネル   : フェーズ、作戦目標、距離／残り敵数
     ・残り時間     : 制限時間のあるフェーズのみ。30秒を切ると赤く点滅
     ・増援警告     : 増援が降下した直後に画面中央付近へ帯を出す
     ・作戦失敗     : 時間切れのとき

==============================================================================*/
#pragma once

struct MissionHudState
{
    const wchar_t* objective = nullptr;   // 作戦目標（1行）
    const wchar_t* detail    = nullptr;   // 補足（距離・残り敵数など）
    int   phase      = 1;                 // 現在のフェーズ（1〜）
    int   phaseCount = 1;
    float timeLeft   = -1.0f;             // 残り時間（秒。負 = 制限なし）
    float warning    = 0.0f;              // 増援警告の残り表示時間（秒。0 = 非表示）
    int   reinforceCount = 0;             // 直近の増援の数（警告に表示）
    bool  bossPhase  = false;             // 大型兵器戦（目標を赤で強調）
    bool  failed     = false;             // 時間切れで作戦失敗
};

void MissionHud_Initialize();
void MissionHud_Finalize();

void MissionHud_Update(double elapsed_time);   // 点滅・流れる帯などのアニメーション
void MissionHud_Draw(const MissionHudState& state);
