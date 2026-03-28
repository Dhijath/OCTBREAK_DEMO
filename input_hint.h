/*==============================================================================

   入力デバイス検出 & チュートリアルバー [input_hint.h]
   Author : 51106
   Date   : 2026/03/25

--------------------------------------------------------------------------------
   ■概要
   ・最後に使われた入力デバイス（KB/マウス or ゲームパッド）を毎フレーム検出
   ・画面下部にヒントバーを描画する
     → KBモード時は kbmText、パッドモード時は padText を表示

   ■使い方
     // main.cpp の Update ループ内
     InputHint_Update();

     // 各 Draw 関数の末尾（Fade_Draw の直前）
     // {TAG} プレースホルダーがボタンアイコン画像に置き換わる
     InputHint_Draw(
         "{TAB} R/L ARM    {W}{S} Weapon    {MOUSE_L} Decide",  // KB/Mouse
         "{LB}{RB} R/L ARM    {DPAD_UP}{DPAD_DN} Weapon    {A} Decide"  // Gamepad
     );

   ■使用可能タグ
     KB/Mouse : {TAB} {ENTER} {ESC}
                {W} {S} {K_A} {K_D}
                {UP} {DOWN} {LEFT} {RIGHT}
                {MOUSE_L} {MOUSE_R}
     Gamepad  : {LB} {RB} {LT} {RT}
                {A} {B} {START} {L_STICK}
                {DPAD_UP} {DPAD_DN} {DPAD_LR}

==============================================================================*/

#pragma once

enum class InputDevice
{
    KBMouse,   // キーボード + マウス
    Gamepad,   // ゲームパッド
};

// 初期化 / 終了（main.cpp で一度だけ呼ぶ）
void InputHint_Initialize();
void InputHint_Finalize();

// 毎フレーム入力デバイスを検出（KeyLogger_Update / PadLogger_Update の直後に呼ぶ）
void InputHint_Update();

// 現在アクティブなデバイスを返す
InputDevice InputHint_GetActiveDevice();

// ヒントバーを画面下部に描画する
// kbmText : KB/マウス向け説明文
// padText : ゲームパッド向け説明文
// desc    : ヒントバーの上行に表示する項目説明（nullptr で非表示）
void InputHint_Draw(const char* kbmText, const char* padText, const wchar_t* desc = nullptr);
