/*==============================================================================

   リザルト（ゲームオーバー）画面 [Result.h]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   ゲームオーバー時の簡易表示モジュール
   - 入力は受けない（Enterでタイトルに戻る処理は Game_Manager 側）
==============================================================================*/
#ifndef RESULT_H
#define RESULT_H


void Result_Initialize();

void Result_Finalize();

void Result_Update(double elapsed_time);

void Result_Draw();

// GAME OVER / GAME CLEAR 共通リザルトパネル（スコア・与ダメ・被ダメ）
void ResultPanel_Draw();

#endif // RESULT_H