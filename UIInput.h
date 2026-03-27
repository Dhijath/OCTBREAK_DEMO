/*==============================================================================

   UI 操作の統一入力ラッパー [UIInput.h]
                                                         Author : 51106
                                                         Date   : 2026/03/27
--------------------------------------------------------------------------------
   ■目的
     キー / パッドボタン / マウスの割り当てをここだけに集約する。
     各画面は UI_IsXxx() を呼ぶだけでよく、変更は本ファイルのみで済む。

   ■使い方
     GameManager_Update の先頭で UIInput_Update() を一度だけ呼ぶこと。
     マウストリガーはここで一元管理される。

   ■分類
     IsTrigger 系（立ち上がり）: 通常の画面ナビゲーション
     IsHeld    系（押し続け）  : Pause など手動トリガー管理が必要な画面用

==============================================================================*/
#pragma once
#ifndef UI_INPUT_H
#define UI_INPUT_H

#include "key_logger.h"
#include "pad_logger.h"

//==============================================================================
// 毎フレーム先頭で一度だけ呼ぶ（マウストリガーを更新する）
//==============================================================================
void UIInput_Update();
bool UI_IsMouseLeftTrig();


//==============================================================================
// ── 立ち上がり（IsTrigger）─────────────────────────────────────────────────
//==============================================================================

// カーソル上下左右
inline bool UI_IsMoveUp()
{
    return KeyLogger_IsTrigger(KK_W) || KeyLogger_IsTrigger(KK_UP)
        || PadLogger_IsTrigger(PAD_DPAD_UP);
}
inline bool UI_IsMoveDown()
{
    return KeyLogger_IsTrigger(KK_S) || KeyLogger_IsTrigger(KK_DOWN)
        || PadLogger_IsTrigger(PAD_DPAD_DOWN);
}
inline bool UI_IsMoveLeft()
{
    return KeyLogger_IsTrigger(KK_A) || KeyLogger_IsTrigger(KK_LEFT)
        || PadLogger_IsTrigger(PAD_DPAD_LEFT);
}
inline bool UI_IsMoveRight()
{
    return KeyLogger_IsTrigger(KK_D) || KeyLogger_IsTrigger(KK_RIGHT)
        || PadLogger_IsTrigger(PAD_DPAD_RIGHT);
}

// 決定（ENTER / PAD_A / マウス左クリック）
inline bool UI_IsConfirm()
{
    return KeyLogger_IsTrigger(KK_ENTER) || PadLogger_IsTrigger(PAD_A)
        || UI_IsMouseLeftTrig();
}

// キャンセル / 戻る（ESC / PAD_B）
inline bool UI_IsCancel()
{
    return KeyLogger_IsTrigger(KK_ESCAPE) || PadLogger_IsTrigger(PAD_B);
}

// タブ切り替え（AssemblyScreen：R/L アーム、WeaponSelect：次の武器）
inline bool UI_IsTabSwitch()
{
    return KeyLogger_IsTrigger(KK_TAB)
        || PadLogger_IsTrigger(PAD_LEFT_SHOULDER)
        || PadLogger_IsTrigger(PAD_RIGHT_SHOULDER);
}

//==============================================================================
// ── 押し続け（IsPressed）──────────────────────────────────────────────────
// Pause など自前でトリガー管理している画面はこちらを使う
//==============================================================================

inline bool UI_IsMoveUpHeld()
{
    return KeyLogger_IsPressed(KK_W) || KeyLogger_IsPressed(KK_UP)
        || PadLogger_IsPressed(PAD_DPAD_UP);
}
inline bool UI_IsMoveDownHeld()
{
    return KeyLogger_IsPressed(KK_S) || KeyLogger_IsPressed(KK_DOWN)
        || PadLogger_IsPressed(PAD_DPAD_DOWN);
}
inline bool UI_IsMoveLeftHeld()
{
    return KeyLogger_IsPressed(KK_A) || KeyLogger_IsPressed(KK_LEFT)
        || PadLogger_IsPressed(PAD_DPAD_LEFT);
}
inline bool UI_IsMoveRightHeld()
{
    return KeyLogger_IsPressed(KK_D) || KeyLogger_IsPressed(KK_RIGHT)
        || PadLogger_IsPressed(PAD_DPAD_RIGHT);
}
inline bool UI_IsConfirmHeld()
{
    return KeyLogger_IsPressed(KK_ENTER) || PadLogger_IsPressed(PAD_A);
}
inline bool UI_IsCancelHeld()
{
    return KeyLogger_IsPressed(KK_ESCAPE) || PadLogger_IsPressed(PAD_BACK);
}

#endif // UI_INPUT_H
