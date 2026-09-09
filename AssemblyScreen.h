/*==============================================================================

   アセンブル画面 [AssemblyScreen.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

   右腕 / 左腕の武器選択画面。
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

// 毎フレーム呼ぶ。true = 画面終了（確定 or キャンセル）
bool     AssemblyScreen_Update(double dt);

// true = ESC / パッドB でキャンセルされた（Update が true を返した後に有効）
bool     AssemblyScreen_WasCancelled();

// 描画
void     AssemblyScreen_Draw();

// 確定した武器 ID を取得（Update が true を返した後に有効）
WeaponID AssemblyScreen_GetRightWeapon();
WeaponID AssemblyScreen_GetLeftWeapon();

// 残クレジット
int      AssemblyScreen_GetRemainingCredits();

// 前回選択を引き継ぐ（SaveData_Load から呼ぶ）
void     AssemblyScreen_SetDefaults(WeaponID right, WeaponID left);

//==============================================================================
// ショップモード（サバイバルのショップから同じ画面を流用する）
//
// ■通常モードとの違い
//   ・予算 = INITIAL_CREDITS ではなく budget（WaveManager クレジット）
//   ・コスト計算は「装備中（初期値）から変更したアームぶんだけ」課金する差分方式
//     → 現状維持は無料、武器を替えたアームだけそのコストを支払う
//   ・READY = ゲーム開始ではなく「購入して閉じる」の合図として使う（呼び出し側が処理）
//
// ■引数
//   ・on     : true でショップモード
//   ・budget : 使用可能なクレジット（所持金）
//
// ※ SetDefaults(装備中R, 装備中L) → Initialize() の後に呼ぶこと
//==============================================================================
void     AssemblyScreen_SetShopMode(bool on, int budget);
bool     AssemblyScreen_IsShopMode();
