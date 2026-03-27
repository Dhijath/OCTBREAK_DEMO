/*==============================================================================
   スコア管理 [Score.cpp]
   Author : 51106
   Date   : 2026/02/17
==============================================================================*/
#include "sprite.h"
#include "texture.h"
#include "Score.h"
#include <algorithm>

//------------------------------------------------------------------------------
// 現在スコア
//------------------------------------------------------------------------------
unsigned int g_Score       = 0;
static int   g_Digit       = 1;
static unsigned int g_CounterStop = 1;
static float g_x           = 0.0f;
static float g_y           = 0.0f;
static int   g_ScoreTextureID = -1;
static constexpr int ScoreWidth  = 32;
static constexpr int ScoreHeight = 35;

static void drawNumber(float x, float y, int number);

void Score_Initialize(float x, float y, int digit)
{
    g_Score = 0;
    g_Digit = digit;
    g_x = x;
    g_y = y;

    g_CounterStop = 1;
    for (int i = 0; i < digit; i++) g_CounterStop *= 1000;
    g_CounterStop--;

    g_ScoreTextureID = Texture_Load(L"resource/texture/suji.png");
}

void Score_Finalize() {}

void Score_Draw()
{
    unsigned int temp = g_Score;
    for (int i = 0; i < g_Digit; i++)
    {
        int n = temp % 10;
        float x = g_x + (g_Digit - 1 - i) * ScoreWidth;
        drawNumber(x, g_y, n);
        temp /= 10;
    }
}

unsigned int Score_GetScore()  { return g_Score; }
void Score_Addscore(int score) { g_Score += score; }
void Score_Reset()             { g_Score = 0; }

void drawNumber(float x, float y, int number)
{
    Sprite_Draw(g_ScoreTextureID, x, y,
        ScoreWidth * number, 0, ScoreWidth, ScoreHeight,
        { 1.0f, 0.7f, 0.7f, 1.0f });
}

//------------------------------------------------------------------------------
// ランキング
//------------------------------------------------------------------------------
static ScoreRecord g_Records[SCORE_RECORD_MAX] = {};
static int         g_RecordCount = 0;

void Score_AddRecord(unsigned int score, WeaponID right, WeaponID left)
{
    if (g_RecordCount < SCORE_RECORD_MAX)
    {
        g_Records[g_RecordCount++] = { score, right, left };
    }
    else
    {
        // 最低スコアのエントリを探して置き換え
        int minIdx = 0;
        for (int i = 1; i < SCORE_RECORD_MAX; ++i)
            if (g_Records[i].score < g_Records[minIdx].score)
                minIdx = i;

        if (score > g_Records[minIdx].score)
            g_Records[minIdx] = { score, right, left };
    }

    // スコア降順ソート
    std::sort(g_Records, g_Records + g_RecordCount,
        [](const ScoreRecord& a, const ScoreRecord& b)
        { return a.score > b.score; });
}

int                Score_GetRecordCount() { return g_RecordCount; }
const ScoreRecord* Score_GetRecords()     { return g_Records; }
void               Score_ClearRecords()   { g_RecordCount = 0; }
