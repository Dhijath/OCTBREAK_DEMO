/*==============================================================================
   ステージ選択画面 [StageSelect.h]
   Author : 51106
   Date   : 2026/06/12
==============================================================================*/
#pragma once

enum class StageSelectResult
{
    None,
    Adventure,
    Survival,
    Back,
};

void             StageSelect_Initialize();
void             StageSelect_Finalize();
void             StageSelect_Update(double elapsed_time);
void             StageSelect_Draw();
StageSelectResult StageSelect_GetResult();
