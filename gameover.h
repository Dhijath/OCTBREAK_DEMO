/*==============================================================================

   ゲームオーバー画面 [gameover.h]
                                                         Author : 51106
                                                         Date   : 2026/02/17
--------------------------------------------------------------------------------
   プレイヤーが死亡したときに表示するゲームオーバー画面のパブリック API。
   GameManager から Initialize / Finalize / Update / Draw を呼ぶことで動作する。

==============================================================================*/
#ifndef GAMEOVER_H
#define GAMEOVER_H

void GameOver_Initialize();
void GameOver_Finalize();
void GameOver_Update(double elapsed_time);
void GameOver_Draw();

#endif // !GAMEOVER_H


