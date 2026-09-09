/*==============================================================================

   作戦結果画面（デブリーフィング） [Result.h]
   Author : 51106
   Date   : 2026/04/01

--------------------------------------------------------------------------------
   作戦成功・失敗で共通の結果画面（表示内容は MissionReport_Get() による）
   - 入力は受けない（決定で選択画面に戻る処理は Game_Manager 側）
==============================================================================*/
#ifndef RESULT_H
#define RESULT_H

void Result_Initialize();

void Result_Finalize();

// 経過時間（項目の順次表示・数値のカウントアップに使う）
void Result_Update(double elapsed_time);

void Result_Draw();

#endif // RESULT_H