/*==============================================================================

   ゲームウィンドウ生成とWndProc [game_window.cpp]
                                                         Author : 51106
                                                         Date   : 2025/12/19
--------------------------------------------------------------------------------

   ・GameWindow_Create でウィンドウ生成
   ・WndProc で Keyboard / Mouse にメッセージを転送
   ・RawInput（WM_INPUT）を取りこぼさないよう WndProc 冒頭で常に転送

   フルスクリーン対応
   ・F11 または Alt+Enter でボーダーレスフルスクリーン ↔ ウィンドウ をトグル
   ・ボーダーレス方式（WS_POPUP + モニターサイズへ SetWindowPos）
     → Alt+Tab / タスクバーが普通に使える、解像度変更なし、切替が速い
   ・WM_SIZE で Direct3D_ResizeBackBuffer を呼び RTV / DSV を再構築
   ・DXGI の排他フルスクリーン（Alt+Enter デフォルト動作）は direct3d.cpp 側で無効化済み

==============================================================================*/

#include "game_window.h"
#include "direct3d.h"
#include <algorithm>
#include <windows.h>

#include "keyboard.h"
#include "mouse.h"
#include "Game_Manager.h"

/* ウィンドウプロシージャ プロトタイプ宣言 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* ウィンドウ情報 */
static constexpr wchar_t WINDOW_CLASS[] = L"GameWindow";
static constexpr wchar_t TITLE[] = L"ウィンドウ表示";

bool g_IsExitDialogOpen = false;
bool g_ExitDialogJustClosed = false;

//==============================================================================
// フルスクリーン管理
//==============================================================================
static bool             g_IsFullscreen      = false;  // 現在フルスクリーンか
static WINDOWPLACEMENT  g_WindowedPlacement = {};     // ウィンドウモード時の位置・サイズ保存

// ウィンドウモード時のスタイル（リサイズ・最大化は無効のまま）
static constexpr DWORD WINDOWED_STYLE =
    WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX);

bool GameWindow_IsFullscreen() { return g_IsFullscreen; }

void GameWindow_ToggleFullscreen(HWND hWnd)
{
    if (!g_IsFullscreen)
    {
        //----------------------------------------------------------------------
        // ウィンドウ → ボーダーレスフルスクリーン
        //----------------------------------------------------------------------

        // 現在のウィンドウ配置を保存（復帰用）
        g_WindowedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hWnd, &g_WindowedPlacement);

        // ウィンドウが乗っているモニターを取得
        HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(MONITORINFO) };
        GetMonitorInfo(hMonitor, &mi);
        const RECT& r = mi.rcMonitor;

        // スタイルを WS_POPUP に変更してモニター全体へ展開
        // バックバッファは 1600×900 のまま維持し DWM にスケーリングさせる
        // → HUD・3D 描画すべてが等倍で引き伸ばされ、相対サイズが変わらない
        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOP,
            r.left, r.top,
            r.right  - r.left,
            r.bottom - r.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        g_IsFullscreen = true;
    }
    else
    {
        //----------------------------------------------------------------------
        // ボーダーレスフルスクリーン → ウィンドウ
        //----------------------------------------------------------------------

        // スタイルを元に戻す
        SetWindowLong(hWnd, GWL_STYLE, WINDOWED_STYLE | WS_VISIBLE);

        // 保存しておいた位置・サイズへ復元
        SetWindowPlacement(hWnd, &g_WindowedPlacement);

        // フレーム変更を反映（位置・サイズは SetWindowPlacement が設定済みなので NOMOVE/NOSIZE）
        SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        // バックバッファはリサイズ不要（1600×900 のまま）

        g_IsFullscreen = false;
    }
}


HWND GameWindow_Create(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = WINDOW_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    RegisterClassExW(&wcex);

    /* メインウィンドウの作成 */
    constexpr int SCREEN_WIDTH  = 1600;
    constexpr int SCREEN_HEIGHT = 900;

    RECT window_rect{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };

    AdjustWindowRect(&window_rect, WINDOWED_STYLE, FALSE);

    const int WINDOW_WIDTH  = window_rect.right  - window_rect.left;
    const int WINDOW_HEIGHT = window_rect.bottom  - window_rect.top;

    int desktop_width  = GetSystemMetrics(SM_CXSCREEN);
    int desktop_height = GetSystemMetrics(SM_CYSCREEN);

    /* ウィンドウの表示位置（中央） */
    const int WINDOW_X = std::max((desktop_width  - WINDOW_WIDTH)  / 2, 0);
    const int WINDOW_Y = std::max((desktop_height - WINDOW_HEIGHT) / 2, 0);

    HWND hWnd = CreateWindowW(
        WINDOW_CLASS,
        TITLE,
        WINDOWED_STYLE,
        WINDOW_X,
        WINDOW_Y,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr);

    return hWnd;
}

/* ウィンドウプロシージャ */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    //==========================================================================
    // 入力モジュールへ「全メッセージ」を常に転送（取りこぼし防止）
    //==========================================================================
    Keyboard_ProcessMessage(message, wParam, lParam);
    Mouse_ProcessMessage(message, wParam, lParam);

    switch (message)
    {
    //--------------------------------------------------------------------------
    // キー入力（通常キー）
    //--------------------------------------------------------------------------
    case WM_KEYDOWN:
        // F11：フルスクリーン トグル
        if (wParam == VK_F11)
        {
            GameWindow_ToggleFullscreen(hWnd);
            return 0;
        }

        if (wParam == VK_ESCAPE)
        {
            // Playing 中は ESC をポーズシステムに委譲する（window 側では何もしない）
            if (GameManager_GetState() != GameState::Playing)
            {
                g_IsExitDialogOpen = true;
                SendMessage(hWnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        break;

    //--------------------------------------------------------------------------
    // システムキー（Alt+〇 が WM_SYSKEYDOWN で来る）
    //--------------------------------------------------------------------------
    case WM_SYSKEYDOWN:
        // Alt+Enter：フルスクリーン トグル
        // lParam の bit29 が立っているとき Alt キーが押されている
        if (wParam == VK_RETURN && (lParam & (1 << 29)))
        {
            GameWindow_ToggleFullscreen(hWnd);
            return 0;
        }
        break;

    //--------------------------------------------------------------------------
    // 閉じるボタン / Alt+F4
    //--------------------------------------------------------------------------
    case WM_CLOSE:
    {
        int r = MessageBoxW(hWnd, L"本当に終了しますか？", L"確認",
            MB_OKCANCEL | MB_DEFBUTTON2);

        if (r == IDOK)
        {
            DestroyWindow(hWnd);
        }
        else
        {
            //  閉じた直後に main 側で時刻補正させる
            g_IsExitDialogOpen = false;
            g_ExitDialogJustClosed = true;
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}
