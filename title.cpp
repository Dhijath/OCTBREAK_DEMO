/*==============================================================================

   タイトル画面 [Title.cpp]
   Author : 51106
   Date   : 2026/04/01

--------------------------------------------------------------------------------
   背景＋タイトルロゴ＋メニュー（START / OPTION / EXIT）
   - W / S または 十字キー上下 で選択
   - Enter または Aボタン で決定
   - 描画は SF調メニュー画面（SciFiMenu）に任せる
==============================================================================*/

#include "Title.h"
#include "texture.h"
#include "sprite.h"
#include "UIInput.h"
#include "direct3d.h"
#include "audio.h"
#include "text_logo.h"
#include "SciFiMenu.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

// -----------------------------------------------------------------------------
// グローバルリソース
// -----------------------------------------------------------------------------
static int g_TitleBgTex = -1;               // 背景テクスチャ
static int g_WhiteTex = -1;                 // 白テクスチャ（縁や発光に使用）

// -----------------------------------------------------------------------------
// メニュー関連
// -----------------------------------------------------------------------------
static constexpr int MENU_COUNT = 3;        // メニュー項目数
static int   g_Selected = 0;                // 現在選択中のインデックス
static float g_Time = 0.0f;                 // 経過時間（アニメ用）
static TitleResult g_Result = TitleResult::None; // 選択結果
static bool g_OneShotStart = false;         // 旧API互換フラグ

// SE
static int g_SeCursorMove = -1;
static int g_SeSelect     = -1;

// -----------------------------------------------------------------------------
// 初期化
// -----------------------------------------------------------------------------
void Title_Initialize()
{
    // 背景・ボタン周りのグロー用テクスチャを読み込み
    g_TitleBgTex = Texture_Load(L"resource/texture/titleBg.png");
    // 白テクスチャ（1x1）… 縁や発光に使う
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    

    g_Selected = 0;
    g_Time = 0.0f;
    g_Result = TitleResult::None;
    g_OneShotStart = false;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");
}

// -----------------------------------------------------------------------------
// 終了処理
// -----------------------------------------------------------------------------
void Title_Finalize()
{
    // 今回は個別解放不要（Texture_Finalizeでまとめて解放）
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;
}

// -----------------------------------------------------------------------------
// 更新処理：キー入力＋パッド入力受付と選択処理
// -----------------------------------------------------------------------------
void Title_Update(double elapsed_time)
{
    g_Time += static_cast<float>(elapsed_time);

    

    if (UI_IsMoveUp())
    {
        g_Selected = (g_Selected + MENU_COUNT - 1) % MENU_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsMoveDown())
    {
        g_Selected = (g_Selected + 1) % MENU_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsCancel())
    {
        g_Selected = MENU_COUNT - 1; // EXITにカーソル移動
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsConfirm())
    {
        PlayAudio(g_SeSelect, false);
        if (g_Selected == 0) {
            g_Result = TitleResult::Start;
            g_OneShotStart = true; // 旧API互換
        }
        else if (g_Selected == 1) {
            g_Result = TitleResult::Option;
        }
        else {
            g_Result = TitleResult::Exit;
        }
    }
}

// -----------------------------------------------------------------------------
// 描画処理：背景・ロゴ（TextLogo）・メニュー（TextLogo）を描画
// -----------------------------------------------------------------------------
void Title_Draw()
{
    // SF調メニュー画面（背景・ロゴ・メニュー）。入力ヒントバーは Game_Manager が描く
    static const SciFiMenuItem items[MENU_COUNT] =
    {
        { L"START",  L"出撃準備" },
        { L"OPTION", L"システム設定" },
        { L"EXIT",   L"終了" },
    };
    SciFiMenu_Draw(L"SYS://TITLE", items, MENU_COUNT, g_Selected, g_Time);
}

// -----------------------------------------------------------------------------
// 選択結果の取得
// -----------------------------------------------------------------------------
TitleResult Title_GetResult()
{
    TitleResult r = g_Result;
    g_Result = TitleResult::None; // ワンショット消費
    return r;
}

// -----------------------------------------------------------------------------
// 旧API互換：Startが選ばれた時だけtrue
// -----------------------------------------------------------------------------
bool Title_IsEnd()
{
    if (g_OneShotStart) {
        g_OneShotStart = false;
        return true;
    }
    return false;
}

int Title_GetSelected() { return g_Selected; }