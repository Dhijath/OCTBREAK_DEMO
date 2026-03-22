/*==============================================================================

   タイトル画面 [Title.cpp]
   Author : 51106
   Date   : 2026/02/08

--------------------------------------------------------------------------------
   背景＋タイトルロゴ＋メニュー（START / OPTION / EXIT）
   - W / S または 十字キー上下 で選択
   - Enter または Aボタン で決定
   - 全ボタンに薄い白い縁（常時）
   - 選択中ボタンは拡大＋上下ユラユラ＋発光パルスで強調
==============================================================================*/

#include "Title.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "pad_logger.h"
#include "direct3d.h"
#include "audio.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

// -----------------------------------------------------------------------------
// グローバルリソース
// -----------------------------------------------------------------------------
static int g_TitleBgTex = -1;               // 背景テクスチャ
static int g_TitleLogoTex = -1;             // タイトルロゴ
static int g_MenuTex[3] = { -1,-1,-1 };     // 各メニューボタン画像
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
    // 背景・ロゴ・ボタン画像を読み込み
    g_TitleBgTex = Texture_Load(L"resource/texture/titleBg.png");
    g_TitleLogoTex = Texture_Load(L"resource/texture/title_logo.png");

    g_MenuTex[0] = Texture_Load(L"resource/texture/btn_start.png");//btn_start
    g_MenuTex[1] = Texture_Load(L"resource/texture/btn_option.png");
    g_MenuTex[2] = Texture_Load(L"resource/texture/btn_exit.png");

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

    

    // 上キー（W または 十字キー上）で前の項目へ
    if (KeyLogger_IsTrigger(KK_W) || PadLogger_IsTrigger(PAD_DPAD_UP))
    {
        g_Selected = (g_Selected + MENU_COUNT - 1) % MENU_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }

    // 下キー（S または 十字キー下）で次の項目へ
    if (KeyLogger_IsTrigger(KK_S) || PadLogger_IsTrigger(PAD_DPAD_DOWN))
    {
        g_Selected = (g_Selected + 1) % MENU_COUNT;
        PlayAudio(g_SeCursorMove, false);
    }

    // Enter または Aボタンで決定
    if (KeyLogger_IsTrigger(KK_ENTER) || PadLogger_IsTrigger(PAD_A))
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
// 描画処理：背景・ロゴ・メニューを描画
// -----------------------------------------------------------------------------
void Title_Draw()
{
    Direct3D_SetDepthEnable(false); // タイトル(2D)中は深度を切る

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // ------------------------------
    // 背景（画面全体にフィット）
    // ------------------------------
    if (g_TitleBgTex >= 0) {
        const float tw = (float)Texture_Width(g_TitleBgTex);
        const float th = (float)Texture_Height(g_TitleBgTex);
        const float sx = (float)sw / std::max(1.0f, tw);
        const float sy = (float)sh / std::max(1.0f, th);
        Sprite_Draw(g_TitleBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
        Sprite_Begin();
    }


    // ------------------------------
    // ロゴ（画像サイズ通りに中央上部に表示）
    // ------------------------------
    if (g_TitleLogoTex >= 0) {
        const float lw = (float)Texture_Width(g_TitleLogoTex);   // ロゴ画像の幅
        const float lh = (float)Texture_Height(g_TitleLogoTex);  // ロゴ画像の高さ
        const float lx = ((float)sw - lw) * 0.5f;                // 中央揃えX座標
        const float ly = (float)sh * 0.12f;                      // 上部に配置
        Sprite_Draw(g_TitleLogoTex, lx, ly, lw, lh, XMFLOAT4(1, 1, 1, 1));
        Sprite_Begin();
    }

    // ------------------------------
    // メニュー（全項目を画像サイズ通りに表示）
    // ------------------------------
    const float baseX = (float)sw * 0.5f;
    const float baseY = (float)sh * 0.55f; // ロゴと被らない位置
    const float gapY = 100.0f;            // 項目間隔

    for (int i = 0; i < MENU_COUNT; ++i)
    {
        if (g_MenuTex[i] < 0) continue;

        const bool sel = (i == g_Selected);

        // 画像の実サイズを取得
        const float tw = (float)Texture_Width(g_MenuTex[i]);
        const float th = (float)Texture_Height(g_MenuTex[i]);

        // 選択時は拡大＋上下ユラユラ
        const float scale = sel ? 1.1f : 1.0f;
        const float bob = sel ? std::sin(g_Time * 6.0f) * 5.0f : 0.0f;

        const float bw = tw * scale;
        const float bh = th * scale;
        const float bx = baseX - bw * 0.5f;
        const float by = baseY + i * gapY - bh * 0.5f + bob;

        // --- 常時：薄い白縁 ---
        if (g_WhiteTex >= 0) {
            Sprite_Draw(g_WhiteTex, bx - 5, by - 5, bw + 10, bh + 10, XMFLOAT4(1, 1, 1, 0.3f));
        }

        // --- 選択中：発光パルス ---
        if (sel && g_WhiteTex >= 0) {
            float pulse = (std::sin(g_Time * 8.0f) * 0.5f + 0.5f);
            float a = 0.25f + 0.25f * pulse;
            Sprite_Draw(g_WhiteTex, bx - 12, by - 12, bw + 24, bh + 24, XMFLOAT4(1, 1, 1, a));
        }

        // --- 本体（ボタン画像）---
        Sprite_Draw(g_MenuTex[i], bx, by, bw, bh, XMFLOAT4(1, 1, 1, 1));
        Sprite_Begin();
    }
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