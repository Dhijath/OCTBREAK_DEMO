/*==============================================================================

   ミッション選択画面 [MissionSelect.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   アドベンチャーモードでアセンブリ確定後に表示する出撃ミッション選択画面。
   左にミッション一覧、右に依頼主・作戦領域・報酬・ブリーフィングを表示する。

   ■使い方（Game_Manager.cpp 側）
     MissionSelect_Initialize();
     MissionSelect_Update(dt);  MissionSelect_Draw();
     MissionSelectResult r = MissionSelect_GetResult();
       Sortie : 出撃（Mission_GetCurrent() が確定済み）
       Back   : アセンブリへ戻る

==============================================================================*/
#pragma once

enum class MissionSelectResult
{
    None,
    Sortie,
    Back,
};

void                MissionSelect_Initialize();
void                MissionSelect_Finalize();
void                MissionSelect_Update(double elapsed_time);
void                MissionSelect_Draw();
MissionSelectResult MissionSelect_GetResult();
