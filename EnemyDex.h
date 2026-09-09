/*==============================================================================

   エネミー図鑑 [EnemyDex.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   旧スコアボード（ScoreCheck）に代わる画面。全エネミー・大型兵器の
   3Dモデル（パーツのアニメーション付き）、性能、攻撃、解説、撃破数を表示する。

   ■撃破数
     game.cpp がエネミーの消滅時に EnemyDex_RecordKill(種別) を呼ぶ。
     SaveData の [EnemyDex] セクションに保存する（作戦終了時に SaveData_SaveDex）。

   ■操作
     上下 : 項目の選択 / 左右 : モデルの回転 / ESC・B : 戻る

==============================================================================*/
#pragma once

// 撃破数（type は EnemyType の値）
int  EnemyDex_TypeCount();
void EnemyDex_RecordKill(int type);
int  EnemyDex_GetKills(int type);
void EnemyDex_SetKills(int type, int kills);

// 画面
void EnemyDex_Initialize();
void EnemyDex_Finalize();
void EnemyDex_Update(double elapsed_time);
void EnemyDex_Draw();
bool EnemyDex_IsEnd();   // 戻る操作で true
