/*==============================================================================
   チュートリアル（画像スライドショー）[Tutorial.h]
   Author : 51106
   Date   : 2026/04/01
--------------------------------------------------------------------------------
   resource/texture/Tutorial/ 内の 16:9 画像（png / jpg）を読み込み、
   左右で送る（スライドショー）画面。ESC / B で PreGame へ戻る。
==============================================================================*/
#pragma once

void Tutorial_Initialize();
void Tutorial_Finalize();
void Tutorial_Update(double elapsed_time);
void Tutorial_Draw();

// true = 終了（PreGame へ戻る）
bool Tutorial_IsEnd();
