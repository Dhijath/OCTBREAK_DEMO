/*==============================================================================

   クリア画面 [Clear.cpp]
   Author : 51106
   Date   : 2026/04/01（2026/10/03 結果画面を Result と共通化）

--------------------------------------------------------------------------------
   作戦成功時の結果画面。表示は Result.cpp のデブリーフィング画面と共通で、
   MissionReport の成否によって「MISSION ACCOMPLISHED」の表示になる。
   入力は Game_Manager 側。
==============================================================================*/
#include "Clear.h"
#include "Result.h"

void Clear_Initialize()
{
    Result_Initialize();
}

void Clear_Finalize()
{
}

void Clear_Update(double elapsed_time)
{
    Result_Update(elapsed_time);
}

void Clear_Draw()
{
    Result_Draw();
}