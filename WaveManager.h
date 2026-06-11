/*==============================================================================
   ウェーブ管理 [WaveManager.h]
   Author : 51106
   Date   : 2026/06/12
--------------------------------------------------------------------------------
   サバイバルモードのウェーブ進行を管理する。

   ■フェーズ
     Idle        : 未開始
     WaveStart   : ウェーブ開始カウントダウン（3秒）
     Fighting    : 戦闘中（敵が全滅で次へ）
     WaveCleared : クリア演出（2秒）
     Shopping    : ウェーブ間休憩・購入タイム（15秒）
     Victory     : 全10ウェーブ突破
     Defeat      : プレイヤー死亡（GameManager側で検知）

==============================================================================*/
#pragma once

enum class WavePhase
{
    Idle,
    WaveStart,
    Fighting,
    WaveCleared,
    Shopping,
    Victory,
};

void      WaveManager_Initialize();
void      WaveManager_Finalize();
void      WaveManager_Update(double elapsed_time);
void      WaveManager_Draw();          // HUD上に情報を表示

void      WaveManager_StartSurvival(); // サバイバル開始

int       WaveManager_GetCurrentWave();
WavePhase WaveManager_GetPhase();
float     WaveManager_GetShopTimeRemaining(); // 残り購入時間（秒）

bool      WaveManager_IsVictory();

// 所持金
int       WaveManager_GetCredits();
void      WaveManager_AddCredits(int amount);
bool      WaveManager_SpendCredits(int amount); // 足りなければ false
