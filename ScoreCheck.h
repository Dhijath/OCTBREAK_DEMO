#pragma once

void ScoreCheck_Initialize();
void ScoreCheck_Finalize();
void ScoreCheck_Update(double elapsed_time);
void ScoreCheck_Draw();
bool ScoreCheck_IsEnd();   // ESC/B/Enter で戻る → true
