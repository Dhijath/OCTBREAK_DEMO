/*==============================================================================

   UI 入力管理 [UIInput.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
   マウスのトリガー検出など、前フレーム状態が必要な入力をここで管理する。
   GameManager_Update の先頭で UIInput_Update() を一度だけ呼ぶこと。

   ■メニュー画面のマウス
     メニュー系の画面にいる間、GameManager が毎フレーム UIInput_UpdateMenuMouse() を呼ぶ。
     カーソルを表示（絶対座標モード）し、ホイール量を集計する。
     ゲーム中はカメラ側（UpdateMouseCamera）が相対モードへ戻す。

==============================================================================*/
#include "UIInput.h"
#include "game_window.h"
#include "mouse.h"
#include <windows.h>
#include <cmath>

namespace
{
    bool s_PrevLeft   = false;
    bool s_TrigLeft   = false;
    bool s_HeldLeft   = false;
    bool s_PrevRight  = false;
    bool s_TrigRight  = false;

    // カーソル位置（スプライト座標 1600×900）
    bool  s_PosValid  = false;
    float s_MouseX    = 0.0f;
    float s_MouseY    = 0.0f;
    float s_PrevX     = 0.0f;
    float s_PrevY     = 0.0f;
    bool  s_Moved     = false;

    // ホイール（メニュー中だけ集計する）
    int  s_PrevWheel     = 0;
    int  s_WheelNotches  = 0;    // このフレームの回転量（奥 = +）
    int  s_WheelRemain   = 0;    // 1ノッチ未満の端数（高精度ホイール用）
    bool s_MenuMouseLast = false;   // 前フレームに UIInput_UpdateMenuMouse を呼んだか
    bool s_MenuMouseNow  = false;

    // カーソル位置を取得する。非アクティブ・クライアント外なら false
    bool ReadCursor(float* x, float* y)
    {
        const HWND hwnd = GameWindow_GetHWND();
        if (GetForegroundWindow() != hwnd) return false;

        POINT pt{};
        if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt)) return false;

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return false;
        if (pt.x < 0 || pt.y < 0 || pt.x >= w || pt.y >= h) return false;

        // スプライトは仮想解像度 1600×900 をクライアント全体へ引き伸ばして描いている
        *x = static_cast<float>(pt.x) * 1600.0f / static_cast<float>(w);
        *y = static_cast<float>(pt.y) * 900.0f  / static_cast<float>(h);
        return true;
    }
}

//------------------------------------------------------------------------------
// 毎フレーム先頭で一度だけ呼ぶ（GameManager_Update の先頭で呼ぶ）
//------------------------------------------------------------------------------
void UIInput_Update()
{
    // Mouse_GetState はマウスカメラ側（UpdateMouseCamera）が読む専用。
    // ここで読むとデルタが消費されてカメラが動かなくなるため、
    // ボタン状態は Windows API から直接取得する。
    // 別ウィンドウがアクティブな間はクリックを拾わない。
    const bool inFocus  = (GetForegroundWindow() == GameWindow_GetHWND());
    const bool leftNow  = inFocus && (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rightNow = inFocus && (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
    s_TrigLeft  = leftNow && !s_PrevLeft;
    s_HeldLeft  = leftNow;
    s_PrevLeft  = leftNow;
    s_TrigRight = rightNow && !s_PrevRight;
    s_PrevRight = rightNow;

    // カーソル位置と移動
    s_PrevX = s_MouseX;
    s_PrevY = s_MouseY;
    const bool wasValid = s_PosValid;
    s_PosValid = ReadCursor(&s_MouseX, &s_MouseY);
    s_Moved = s_PosValid && wasValid && (s_MouseX != s_PrevX || s_MouseY != s_PrevY);

    // ホイールはメニュー中のフレームだけ有効（前フレームの UpdateMenuMouse の結果を使う）
    s_MenuMouseLast = s_MenuMouseNow;
    s_MenuMouseNow  = false;
    s_WheelNotches  = 0;
}

//------------------------------------------------------------------------------
// メニュー画面にいる間、毎フレーム呼ぶ（UIInput_Update の後）
//   カーソルを表示（絶対座標モードへ切り替え）し、ホイール量を集計する
//------------------------------------------------------------------------------
void UIInput_UpdateMenuMouse()
{
    // ※ Mouse_GetState は相対モード中に呼ぶとカメラ用のデルタを消費する。
    //   この関数はゲーム更新（カメラ）を回さないフレームにだけ呼ばれる。
    Mouse_State ms{};
    Mouse_GetState(&ms);

    if (ms.positionMode != MOUSE_POSITION_MODE_ABSOLUTE)
    {
        Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);
        Mouse_ProcessMessage(WM_MOUSEMOVE, 0, 0);   // モード切替を即確定させる
        Mouse_SetVisible(true);

        // 切替時は前回の絶対座標（初回は左上）へ飛ぶので、画面中央へ置き直す
        const HWND hwnd = GameWindow_GetHWND();
        RECT rc{};
        GetClientRect(hwnd, &rc);
        POINT pt{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        ClientToScreen(hwnd, &pt);
        SetCursorPos(pt.x, pt.y);
        s_PosValid = false;   // 置き直した移動を「動かした」と扱わない
        s_Moved    = false;
    }

    // ホイール：メニューに入った最初のフレームは基準値を取るだけ
    //（ゲーム中に回した分をメニューで拾わないため）
    if (s_MenuMouseLast)
    {
        s_WheelRemain += ms.scrollWheelValue - s_PrevWheel;
        s_WheelNotches = s_WheelRemain / WHEEL_DELTA;
        s_WheelRemain -= s_WheelNotches * WHEEL_DELTA;
    }
    else
    {
        s_WheelRemain = 0;
    }
    s_PrevWheel    = ms.scrollWheelValue;
    s_MenuMouseNow = true;
}

//------------------------------------------------------------------------------
// ボタン
//------------------------------------------------------------------------------
bool UI_IsMouseLeftTrig()  { return s_TrigLeft; }
bool UI_IsMouseLeftHeld()  { return s_HeldLeft; }
bool UI_IsMouseRightTrig() { return s_TrigRight; }
int  UI_GetMouseWheel()    { return s_WheelNotches; }

//------------------------------------------------------------------------------
// カーソル
//------------------------------------------------------------------------------
bool UI_GetMousePos(float* x, float* y)
{
    if (!s_PosValid) return false;
    *x = s_MouseX;
    *y = s_MouseY;
    return true;
}

bool UI_IsMouseMoved()
{
    return s_Moved;
}

float UI_GetMouseDeltaX()
{
    return s_Moved ? (s_MouseX - s_PrevX) : 0.0f;
}

bool UI_IsMouseIn(float x, float y, float w, float h)
{
    return s_PosValid
        && s_MouseX >= x && s_MouseX < x + w
        && s_MouseY >= y && s_MouseY < y + h;
}

bool UI_IsClickIn(float x, float y, float w, float h)
{
    return s_TrigLeft && UI_IsMouseIn(x, y, w, h);
}
