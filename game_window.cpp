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
#include "ShaderEdge.h"
#include "DirectWrite.h"
#include "text_logo.h"

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
static HWND             g_hWnd              = nullptr; // 生成済みウィンドウハンドル
static bool             g_IsFullscreen      = false;  // 現在フルスクリーンか
static WINDOWPLACEMENT  g_WindowedPlacement = {};     // ウィンドウモード時の位置・サイズ保存
static bool             g_PendingToggle     = false;  // 遅延フルスクリーン切替フラグ

// ウィンドウモード時のスタイル（リサイズ・最大化は無効のまま）
static constexpr DWORD WINDOWED_STYLE =
    WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX);

HWND GameWindow_GetHWND()      { return g_hWnd; }
bool GameWindow_IsFullscreen() { return g_IsFullscreen; }

void GameWindow_RequestFullscreenToggle()
{
    g_PendingToggle = true;   // フラグだけ立てる（実行は次フレーム先頭）
}

void GameWindow_ApplyPendingToggle()
{
    if (!g_PendingToggle) return;
    g_PendingToggle = false;
    GameWindow_ToggleFullscreen(g_hWnd);   // フレーム間（Present 直後）に安全に実行
}

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
        SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOP,
            r.left, r.top,
            r.right  - r.left,
            r.bottom - r.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        // バックバッファをモニター解像度にリサイズ（ネイティブ解像度で描画）
        DirectWrite::PreResize();   // D2D RT を解放（ResizeBuffers の前提条件）
        TextLogo_PreResize();
        Direct3D_ResizeBackBuffer(
            static_cast<unsigned int>(r.right  - r.left),
            static_cast<unsigned int>(r.bottom - r.top));
        ShaderEdge_ResizeBuffers();
        DirectWrite::PostResize();  // D2D RT を新バックバッファで再生成
        TextLogo_PostResize();
        // PostResize 後は D2D が DXGI サーフェスを触るため D3D11 RTV が未バインドになる
        // → 明示的に再バインドして次フレームのフェード等スプライト描画を保護する
        Direct3D_BindMainRenderTarget();

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

        // バックバッファを元のウィンドウ解像度に戻す
        DirectWrite::PreResize();
        TextLogo_PreResize();
        Direct3D_ResizeBackBuffer(1600, 900);
        ShaderEdge_ResizeBuffers();
        DirectWrite::PostResize();
        TextLogo_PostResize();
        Direct3D_BindMainRenderTarget();   // D2D RT 再生成後に RTV を確実に再バインド

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

    g_hWnd = hWnd;   // ゲッター用に保存
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
