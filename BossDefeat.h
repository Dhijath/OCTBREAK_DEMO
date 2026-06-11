/*==============================================================================

   ボス撃破演出 [BossDefeat.h]
                                                         Author : 51106
                                                         Date   : 2026/06/11
--------------------------------------------------------------------------------
   ・ボス撃破時にカメラがズームインして撃破エフェクトを再生
   ・フェーズ: ZOOM_IN → HOLD → FADE_OUT → DONE
   ・演出中はプレイヤー入力を無効化

==============================================================================*/
#pragma once
#include <DirectXMath.h>

// 演出を開始する（撃破されたボスの座標を渡す）
// onBossVanish : ボスを消すタイミングで呼ばれるコールバック
void BossDefeat_Start(const DirectX::XMFLOAT3& bossPos, void(*onBossVanish)() = nullptr);

// 毎フレーム更新（演出中のみカメラ上書き）
void BossDefeat_Update(double dt);

// 演出中は true
bool BossDefeat_IsPlaying();
