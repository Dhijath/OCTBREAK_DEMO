/*==============================================================================

   ポーズメニュー [Pause.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/13
--------------------------------------------------------------------------------
   ・半透明ダークオーバーレイ + タイトル流用ボタン画像 2 枚
   ・btn_start.png → RESUME
   ・btn_exit.png  → TITLE
   ・選択時は拡大＋発光パルス（タイトルと同じ演出）
   ・ESC → 即 Resume

==============================================================================*/
#include "Pause.h"
#include "sprite.h"
#include "audio.h"
#include "texture.h"
#include "direct3d.h"
#include "key_logger.h"
#include "pad_logger.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

namespace
{
    static constexpr int ITEM_COUNT = 2; // RESUME / TITLE

    // テクスチャ ID
    static int g_TexWhite  = -1;
    static int g_TexBtn[ITEM_COUNT] = { -1, -1 };

    // 選択カーソル
    static int  g_Cursor = 0;

    // 経過時間（選択アニメ用）
    static float g_Time = 0.0f;

    // 入力の立ち上がり管理（ポーズ中の誤操作防止）
    static bool g_PrevUp    = false;
    static bool g_PrevDown  = false;
    static bool g_PrevEnter = false;
    static bool g_PrevEsc   = false;

    // SE
    static int g_SeCursorMove = -1;
    static int g_SeSelect     = -1;
}

//==============================================================================
// 初期化
//==============================================================================
void Pause_Initialize()
{
    g_Cursor    = 0;
    g_Time      = 0.0f;
    g_PrevUp    = false;
    g_PrevDown  = false;
    g_PrevEnter = false;
    g_PrevEsc   = false;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");

    if (g_TexWhite < 0)
        g_TexWhite  = Texture_Load(L"resource/texture/white.png");
    if (g_TexBtn[0] < 0)
        g_TexBtn[0] = Texture_Load(L"resource/texture/btn_start.png");  // RESUME
    if (g_TexBtn[1] < 0)
        g_TexBtn[1] = Texture_Load(L"resource/texture/btn_exit.png");   // TITLE
}

//==============================================================================
// 更新（カーソル操作 + 決定）
// 戻り値で呼び出し側が遷移を制御する
//==============================================================================
PauseResult Pause_Update()
{
    g_Time += 1.0f / 60.0f; // 60fps 想定でアニメ時間を加算

    //----------------------------------------------------------
    // カーソル移動（立ち上がり検出）
    //----------------------------------------------------------
    const bool nowUp   = KeyLogger_IsPressed(KK_W) || KeyLogger_IsPressed(KK_UP)
                       || PadLogger_IsPressed(PAD_DPAD_UP);
    const bool nowDown = KeyLogger_IsPressed(KK_S) || KeyLogger_IsPressed(KK_DOWN)
                       || PadLogger_IsPressed(PAD_DPAD_DOWN);

    if (nowUp   && !g_PrevUp)   { g_Cursor = (g_Cursor - 1 + ITEM_COUNT) % ITEM_COUNT; PlayAudio(g_SeCursorMove, false); }
    if (nowDown && !g_PrevDown) { g_Cursor = (g_Cursor + 1) % ITEM_COUNT;              PlayAudio(g_SeCursorMove, false); }

    g_PrevUp   = nowUp;
    g_PrevDown = nowDown;

    //----------------------------------------------------------
    // ESC / PAD_BACK → 即リジューム
    //----------------------------------------------------------
    const bool nowEsc = KeyLogger_IsPressed(KK_ESCAPE)
                      || PadLogger_IsPressed(PAD_BACK);
    if (nowEsc && !g_PrevEsc)
    {
        g_PrevEsc = nowEsc;
        return PauseResult::Resume;
    }
    g_PrevEsc = nowEsc;

    //----------------------------------------------------------
    // Enter / PAD_A → 決定
    //----------------------------------------------------------
    const bool nowEnter  = KeyLogger_IsPressed(KK_ENTER)
                         || PadLogger_IsPressed(PAD_A);
    const bool triggered = nowEnter && !g_PrevEnter;
    g_PrevEnter = nowEnter;

    if (triggered)
    {
        PlayAudio(g_SeSelect, false);
        switch (g_Cursor)
        {
        case 0: return PauseResult::Resume;
        case 1: return PauseResult::GoTitle;
        }
    }

    return PauseResult::None;
}

//==============================================================================
// 描画（オーバーレイ + ボタン）
//==============================================================================
void Pause_Draw()
{
    if (g_TexWhite < 0) return;

    const float W = static_cast<float>(SPRITE_SCREEN_W);
    const float H = static_cast<float>(SPRITE_SCREEN_H);

    Sprite_Begin(); // 2D パイプライン

    //----------------------------------------------------------
    // 半透明ダークオーバーレイ
    //----------------------------------------------------------
    Sprite_Draw(g_TexWhite, 0.0f, 0.0f, W, H, XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.55f });
    Sprite_Begin();

    //----------------------------------------------------------
    // ボタン描画（タイトル画面と同スタイル）
    //----------------------------------------------------------
    const float baseX = W * 0.5f;
    const float baseY = H * 0.40f;  // 画面中央よりやや上
    const float gapY  = 90.0f;

    for (int i = 0; i < ITEM_COUNT; ++i)
    {
        if (g_TexBtn[i] < 0) continue;

        const bool sel = (i == g_Cursor);

        // ボタン画像のサイズ取得
        const float tw = static_cast<float>(Texture_Width(g_TexBtn[i]));
        const float th = static_cast<float>(Texture_Height(g_TexBtn[i]));

        // 選択時は拡大 + 上下ゆらゆら
        const float scale = sel ? 1.12f : 1.0f;
        const float bob   = sel ? std::sinf(g_Time * 6.0f) * 5.0f : 0.0f;

        const float bw = tw * scale;
        const float bh = th * scale;
        const float bx = baseX - bw * 0.5f;
        const float by = baseY + static_cast<float>(i) * gapY - bh * 0.5f + bob;

        // 常時：薄い白縁
        if (g_TexWhite >= 0)
        {
            Sprite_Draw(g_TexWhite, bx - 5.0f, by - 5.0f,
                bw + 10.0f, bh + 10.0f, XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f });
        }

        // 選択中：発光パルス
        if (sel && g_TexWhite >= 0)
        {
            const float pulse = (std::sinf(g_Time * 8.0f) * 0.5f + 0.5f);
            const float a     = 0.25f + 0.25f * pulse;
            Sprite_Draw(g_TexWhite, bx - 12.0f, by - 12.0f,
                bw + 24.0f, bh + 24.0f, XMFLOAT4{ 1.0f, 1.0f, 0.3f, a }); // 黄色発光
        }

        // ボタン本体
        Sprite_Draw(g_TexBtn[i], bx, by, bw, bh, XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
        Sprite_Begin();
    }
}
