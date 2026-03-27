/*==============================================================================

   アセンブル画面 [AssemblyScreen.h]
                                                         Author : 51106
                                                         Date   : 2026/03/22
--------------------------------------------------------------------------------

   AC 風の右腕 / 左腕武器選択画面。
   DirectWrite によるテキスト描画を使用。

   ■使い方（Game_Manager.cpp 側）
     AssemblyScreen_Initialize();
     while (!AssemblyScreen_Update(dt)) AssemblyScreen_Draw();
     AssemblyScreen_Finalize();
     WeaponID r = AssemblyScreen_GetRightWeapon();
     WeaponID l = AssemblyScreen_GetLeftWeapon();

==============================================================================*/
#pragma once
#include "WeaponDef.h"

void     AssemblyScreen_Initialize();
void     AssemblyScreen_Finalize();

// 毎フレーム呼ぶ。true = 確定（ゲーム開始）
bool     AssemblyScreen_Update(double dt);

// 描画
void     AssemblyScreen_Draw();

// 確定した武器 ID を取得（Update が true を返した後に有効）
WeaponID AssemblyScreen_GetRightWeapon();
WeaponID AssemblyScreen_GetLeftWeapon();

// 残クレジット
int      AssemblyScreen_GetRemainingCredits();

// 前回選択を引き継ぐ（SaveData_Load から呼ぶ）
void     AssemblyScreen_SetDefaults(WeaponID right, WeaponID left);
