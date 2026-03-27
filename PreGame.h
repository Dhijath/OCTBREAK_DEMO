#pragma once

enum class PreGameResult { None, Assembly, ScoreCheck, Back };

void         PreGame_Initialize();
void         PreGame_Finalize();
void         PreGame_Update(double elapsed_time);
void         PreGame_Draw();
PreGameResult PreGame_GetResult();
