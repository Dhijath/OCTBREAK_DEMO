/*==============================================================================
   ステージ選択画面 [StageSelect.cpp]
   Author : 51106
   Date   : 2026/06/12
==============================================================================*/
#include "StageSelect.h"
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

static constexpr int  ITEM_COUNT = 2;
static const wchar_t* ITEM_LABELS[ITEM_COUNT] = { L"ADVENTURE", L"SURVIVAL" };

static int              g_Selected = 0;
static float            g_Time     = 0.0f;
static StageSelectResult g_Result  = StageSelectResult::None;

static int g_BgTex    = -1;
static int g_WhiteTex = -1;

static int g_SeCursorMove = -1;
static int g_SeSelect     = -1;
static int g_SeCancel     = -1;

void StageSelect_Initialize()
{
    g_BgTex    = Texture_Load(L"resource/texture/titleBg.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    g_Selected = 0;
    g_Time     = 0.0f;
    g_Result   = StageSelectResult::None;
}

void StageSelect_Finalize()
{
    // SE はここでは解放しない。
    // この関数は決定/キャンセル SE を鳴らした直後（同フレーム）に呼ばれるため、
    // ここで UnloadAudio すると DestroyVoice() で再生が即停止し、SE が聞こえなくなる。
    // （サバイバル決定時に効果音が鳴らなかった原因）
    // SE ハンドルは Initialize 側が「< 0 のときだけロード」する多重ロード防止付きなので、
    // 保持したまま次回入場時に再利用する（Title / PreGame など他メニューと同じ扱い）。
}

void StageSelect_Update(double elapsed_time)
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
        g_Result = (g_Selected == 0)
                   ? StageSelectResult::Adventure
                   : StageSelectResult::Survival;
    }

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Result = StageSelectResult::Back;
    }
}

void StageSelect_Draw()
{
    // SF調メニュー画面（背景・ロゴ・メニュー）
    static const SciFiMenuItem items[ITEM_COUNT] =
    {
        { ITEM_LABELS[0], L"作戦行動" },
        { ITEM_LABELS[1], L"防衛戦" },
    };
    SciFiMenu_Draw(L"SYS://MODE SELECT", items, ITEM_COUNT, g_Selected, g_Time);

    // フッター
    static const wchar_t* itemDesc[ITEM_COUNT] = {
        L"ダンジョンを探索し、最奥のボス討伐を目指します",
        L"屋外アリーナで5ウェーブ間、生き残りを目指します",
    };
    InputHint_Draw(
        "{UP}{DOWN} Move    {ENTER} Select    {ESC} Back",
        "{DPAD_UP}{DPAD_DN} Move    {A} Select    {B} Back",
        itemDesc[g_Selected]);
}

StageSelectResult StageSelect_GetResult()
{
    StageSelectResult r = g_Result;
    g_Result = StageSelectResult::None;
    return r;
}
