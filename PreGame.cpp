/*==============================================================================
   中間メニュー [PreGame.cpp]
   タイトルで START を選んだ後に表示
   選択肢: ゲームモード / チュートリアル / エネミー図鑑
   ESC / Bボタン → タイトルへ戻る
==============================================================================*/
#include "PreGame.h"
#include "UIInput.h"
#include "audio.h"
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include "text_logo.h"
#include "input_hint.h"
#include "SciFiMenu.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
using namespace DirectX;

//------------------------------------------------------------------------------
// 定数
//------------------------------------------------------------------------------
static constexpr int   ITEM_COUNT = 3;
static const wchar_t*  ITEM_LABELS[ITEM_COUNT] = { L"GAME MODE", L"TUTORIAL", L"ENEMY DATABASE" };

//------------------------------------------------------------------------------
// 状態
//------------------------------------------------------------------------------
static int           g_Selected = 0;
static float         g_Time     = 0.0f;
static PreGameResult g_Result   = PreGameResult::None;

// テクスチャ
static int g_BgTex    = -1;
static int g_WhiteTex = -1;

// SE
static int g_SeCursorMove = -1;
static int g_SeSelect     = -1;
static int g_SeCancel     = -1;

//------------------------------------------------------------------------------
void PreGame_Initialize()
{
    g_BgTex    = Texture_Load(L"resource/texture/titleBg.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    g_Selected = 0;
    g_Time     = 0.0f;
    g_Result   = PreGameResult::None;
}

//------------------------------------------------------------------------------
void PreGame_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;
}

//------------------------------------------------------------------------------
void PreGame_Update(double elapsed_time)
{
    g_Time += static_cast<float>(elapsed_time);

    if (UI_IsMoveUp())
    {
        g_Selected = (g_Selected + ITEM_COUNT - 1) % ITEM_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsMoveDown())
    {
        g_Selected = (g_Selected + 1) % ITEM_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }

    if (UI_IsConfirm())
    {
        PlayAudio(g_SeSelect, false);
        if      (g_Selected == 0) g_Result = PreGameResult::QuickStart;
        else if (g_Selected == 1) g_Result = PreGameResult::Tutorial;
        else                      g_Result = PreGameResult::EnemyDex;
    }

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Result = PreGameResult::Back;
    }
}

//------------------------------------------------------------------------------
void PreGame_Draw()
{
    // SF調メニュー画面（背景・ロゴ・メニュー）
    static const SciFiMenuItem items[ITEM_COUNT] =
    {
        { ITEM_LABELS[0], L"モード選択" },
        { ITEM_LABELS[1], L"操作説明" },
        { ITEM_LABELS[2], L"エネミー図鑑" },
    };
    SciFiMenu_Draw(L"SYS://MAIN MENU", items, ITEM_COUNT, g_Selected, g_Time);

    // フッター（InputHint バー）
    static const wchar_t* itemDesc[ITEM_COUNT] = {
        L"ゲームモードを選べます。",
        L"遊び方をスライドショーで確認します",
        L"これまでに確認した敵機と大型兵器の資料を見ます",
    };
    InputHint_Draw(
        "{UP}{DOWN} Move    {ENTER} Select    {ESC} Back",
        "{DPAD_UP}{DPAD_DN} Move    {A} Select    {B} Back",
        itemDesc[g_Selected]);
}

//------------------------------------------------------------------------------
PreGameResult PreGame_GetResult()
{
    PreGameResult r = g_Result;
    g_Result = PreGameResult::None;
    return r;
}
