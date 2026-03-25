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
#include "text_logo.h"
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
// 描画処理：背景・ロゴ（TextLogo）・メニュー（TextLogo）を描画
// -----------------------------------------------------------------------------
void Title_Draw()
{
    Direct3D_SetDepthEnable(false); // タイトル(2D)中は深度を切る
    Direct3D_SetBlendState(true);

    const int sw = SPRITE_SCREEN_W;
    const int sh = SPRITE_SCREEN_H;

    // メニューレイアウト定数（テクスチャ不要でも白グロー計算に使う）
    static constexpr float MENU_BOX_W = 260.0f; // ボタン枠推定幅（全項目共通）
    static constexpr float MENU_BOX_H = 82.0f;  // ボタン枠推定高さ
    const float baseX = (float)sw * 0.5f;
    const float baseY = (float)sh * 0.55f;
    const float gapY  = 100.0f;

    // ──────────────────────────────────────────
    // 1) スプライト描画（背景 + ボタン枠グロー）
    // ──────────────────────────────────────────
    Sprite_Begin();

    // 背景（画面全体にフィット）
    if (g_TitleBgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_TitleBgTex);
        const float th = (float)Texture_Height(g_TitleBgTex);
        const float sx = (float)sw / std::max(1.0f, tw);
        const float sy = (float)sh / std::max(1.0f, th);
        Sprite_Draw(g_TitleBgTex, 0, 0, tw * sx, th * sy, XMFLOAT4(1, 1, 1, 1));
    }

    // ボタン枠：常時白縁 + 選択中パルスグロー（白テクスチャで描画）
    if (g_WhiteTex >= 0)
    {
        for (int i = 0; i < MENU_COUNT; ++i)
        {
            const bool  sel   = (i == g_Selected);
            const float scale = sel ? 1.1f : 1.0f;
            const float bob   = sel ? std::sin(g_Time * 6.0f) * 5.0f : 0.0f;
            const float bw    = MENU_BOX_W * scale;
            const float bh    = MENU_BOX_H * scale;
            const float bx    = baseX - bw * 0.5f;
            const float by    = baseY + i * gapY - bh * 0.5f + bob;

            // 常時：薄い白縁
            Sprite_Draw(g_WhiteTex, bx - 5, by - 5, bw + 10, bh + 10,
                        XMFLOAT4(1, 1, 1, 0.3f));

            // 選択中：発光パルス
            if (sel)
            {
                const float pulse = std::sin(g_Time * 8.0f) * 0.5f + 0.5f;
                const float a     = 0.25f + 0.25f * pulse;
                Sprite_Draw(g_WhiteTex, bx - 12, by - 12, bw + 24, bh + 24,
                            XMFLOAT4(1, 1, 1, a));
            }
        }
    }

    // ──────────────────────────────────────────
    // 2) TextLogo 描画（D2D でテキストをグラデーション塗り）
    // ──────────────────────────────────────────

    // タイトルロゴ
    {
        LogoStyle s;
        s.fontSize     = 148.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.95f, 0.95f, 1.00f, 1.0f); // 白銀ハイライト
        s.colorBottom  = D2D1::ColorF(0.35f, 0.35f, 0.40f, 1.0f); // クールグレー
        s.outlineColor = D2D1::ColorF(0.06f, 0.06f, 0.08f, 1.0f); // チャコール
        s.outlineWidth = 5.0f;
        TextLogo_Draw(L"Oct Break", (float)sw * 0.5f, (float)sh * 0.20f, s);
    }

    // メニューボタン（START / OPTION / EXIT）
    {
        LogoStyle s;
        s.fontSize     = 68.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(1.0f, 0.92f, 0.70f, 1.0f); // 薄いゴールド
        s.colorBottom  = D2D1::ColorF(0.85f, 0.55f, 0.10f, 1.0f); // ゴールド
        s.outlineColor = D2D1::ColorF(0.05f, 0.02f, 0.00f, 1.0f);
        s.outlineWidth = 2.5f;

        static const wchar_t* labels[MENU_COUNT] = { L"START", L"OPTION", L"EXIT" };
        for (int i = 0; i < MENU_COUNT; ++i)
        {
            const bool  sel   = (i == g_Selected);
            const float sc    = sel ? 1.1f : 1.0f;
            const float bob   = sel ? std::sin(g_Time * 6.0f) * 5.0f : 0.0f;
            const float cy    = baseY + i * gapY + bob;
            TextLogo_Draw(labels[i], baseX, cy, s, sc);
        }
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