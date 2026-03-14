#pragma once

#ifndef GAME_WINDOW_H
#define GAME_WINDOW_H

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

HWND GameWindow_Create(HINSTANCE hInstance);

// フルスクリーン ↔ ウィンドウ をトグル（ボーダーレス方式）
void GameWindow_ToggleFullscreen(HWND hWnd);

// 現在フルスクリーンかどうか
bool GameWindow_IsFullscreen();

extern bool g_ExitDialogJustClosed;



#endif