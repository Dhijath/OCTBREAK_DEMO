/*==============================================================================

   ポーズメニュー [Pause.h]
                                                         Author : 51106
                                                         Date   : 2026/03/13
--------------------------------------------------------------------------------
   ・ESC / PAD_START でポーズ ON/OFF（呼び出し側がトグル管理）
   ・W/S または D-pad 上下でカーソル移動
   ・Enter / PAD_A で決定
   ・項目: 0=RESUME  1=TITLE

==============================================================================*/
#pragma once
#ifndef PAUSE_H
#define PAUSE_H

// Pause_Update の戻り値
enum class PauseResult
{
    None,     // 何もなし
    Resume,   // ゲーム再開
    GoTitle,  // タイトルへ戻る
};

void        Pause_Initialize();
void        Pause_Open();       // ポーズを開く時に呼ぶ（入力の初期化）
PauseResult Pause_Update();
void        Pause_Draw();

#endif // PAUSE_H
