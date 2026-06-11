/*==============================================================================
   スコア管理 [Score.h]
   Author : 51106
   Date   : 2026/02/17
==============================================================================*/
#pragma once
#include "WeaponDef.h"

//------------------------------------------------------------------------------
// スコアレコード（ランキング1件分）
//------------------------------------------------------------------------------
static constexpr int SCORE_RECORD_MAX = 10;

struct ScoreRecord
{
    unsigned int score;
    WeaponID     rightWeapon;
    WeaponID     leftWeapon;
};

//------------------------------------------------------------------------------
// 現在スコア
//------------------------------------------------------------------------------
void         Score_Initialize(float x, float y, int digit);
void         Score_Finalize();
void         Score_Draw();
unsigned int Score_GetScore();
void         Score_Addscore(int score);
void         Score_Reset();

//------------------------------------------------------------------------------
// ダメージ統計
//------------------------------------------------------------------------------
void Score_AddDamageDealt(int damage);
void Score_AddDamageTaken(int damage);
int  Score_GetDamageDealt();
int  Score_GetDamageTaken();

//------------------------------------------------------------------------------
// ランキング
//------------------------------------------------------------------------------
void               Score_AddRecord(unsigned int score, WeaponID right, WeaponID left);
int                Score_GetRecordCount();
const ScoreRecord* Score_GetRecords();   // 降順ソート済み配列（SCORE_RECORD_MAX件）
void               Score_ClearRecords();
