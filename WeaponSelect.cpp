/*==============================================================================

   武器選択画面 [WeaponSelect.cpp]
   Author : 51106
   Date   : 2026/03/09

--------------------------------------------------------------------------------
   DebugText は 1 インスタンスのみ使用（複数生成による static リソース競合回避）。
   SetText を複数回呼んでテキスト色を分けながら 1 Draw で描画。

   ■行レイアウト（lineSpacing=21, offsetY=65 の場合）
     line  0  y= 65  : "WEAPON SELECT"
     line  6  y=191  : タブ名（NORMAL / SHOTGUN / MISSILE）
     line 12  y=317  : 武器名ヘッダ（ステータスパネル内）
     line 15  y=380  : Damage
     line 18  y=443  : FireRate
     line 21  y=506  : Explosion
     line 27  y=632  : フッター
==============================================================================*/

#include "WeaponSelect.h"
#include "direct3d.h"
#include "audio.h"
#include "texture.h"
#include "sprite.h"
#include "debug_text.h"
#include "key_logger.h"
#include "keyboard.h"
#include "pad_logger.h"
#include "mouse.h"
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>

using namespace DirectX;

//------------------------------------------------------------------------------
// 武器パラメータ定義
//------------------------------------------------------------------------------
struct WeaponData
{
    const char* name;
    int         damage;
    float       fireInterval;   // 秒
    float       explosionR;     // 爆発半径 m（0=なし）
    float       dmgBar;         // バー割合 0〜1
    float       rateBar;
    float       expBar;
};

static constexpr WeaponData k_Weapons[3] =
{
    //  name       dmg    interval   expR   dmgBar  rateBar  expBar
    { "NORMAL",    45,    0.09f,    0.0f,  0.15f,  1.00f,   0.00f },
    { "SHOTGUN",   405,   0.70f,    0.0f,  0.70f,  0.20f,   0.00f },
    { "MISSILE",   400,  1.00f,    7.5f,  0.60f,  0.10f,   1.00f },
};

//------------------------------------------------------------------------------
// 内部状態
//------------------------------------------------------------------------------
namespace
{
    int                g_Selected = 0;
    WeaponSelectResult g_Result   = WeaponSelectResult::None;

    int g_SeCursorMove = -1;
    int g_SeSelect     = -1;
    double             g_Time     = 0.0;

    int g_BgTexID    = -1;
    int g_WhiteTexID = -1;

    // DebugText は 1 個のみ（static D3D リソース競合を回避）
    // offsetX=0, offsetY=65, lineSpacing=21, charSpacing=21
    hal::DebugText* g_pText = nullptr;
}

//------------------------------------------------------------------------------
// レイアウト定数
//------------------------------------------------------------------------------
// タブ（3*220+2*50=760, startX=(1600-760)/2=420）
static constexpr float TAB_W       = 220.0f;
static constexpr float TAB_H       = 58.0f;
static constexpr float TAB_GAP     = 50.0f;
static constexpr float TAB_START_X = 420.0f;
static constexpr float TAB_Y       = 185.0f;

// ステータスパネル
static constexpr float PNL_X = 300.0f;
static constexpr float PNL_Y = 280.0f;
static constexpr float PNL_W = 1000.0f;
static constexpr float PNL_H = 300.0f;

// パネル内バー
// ・lineSpacing=21, offsetY=65 のとき:
//   line15 y=380 (Damage), line18 y=443 (FireRate), line21 y=506 (Explosion)
static constexpr float BAR_X     = 800.0f;
static constexpr float BAR_Y0    = 380.0f;   // line15 y
static constexpr float BAR_W_MAX = 460.0f;
static constexpr float BAR_H     = 22.0f;
static constexpr float BAR_GAP_Y = 63.0f;    // 3 lines * 21px

//------------------------------------------------------------------------------
// WeaponSelect_Initialize
//------------------------------------------------------------------------------
void WeaponSelect_Initialize()
{
    g_Selected = 0;
    g_Result   = WeaponSelectResult::None;
    g_Time     = 0.0;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");

    // 既存リソースを解放
    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }
    delete g_pText; g_pText = nullptr;

    // テクスチャロード
    g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");
    g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");

    auto*      dev = Direct3D_GetDevice();
    auto*      ctx = Direct3D_GetContext();
    const UINT sw  = SPRITE_SCREEN_W;
    const UINT sh  = SPRITE_SCREEN_H;

    // DebugText 1 個（main.cpp の dt と同等のパラメータ）
    // offsetX=0, offsetY=65 を原点として '\n' で行を進める
    g_pText = new hal::DebugText(
        dev, ctx,
        L"Resource/Texture/consolab_ascii_512.png",
        sw, sh,
        0.0f, 65.0f,    // テキスト描画原点
        0, 0,
        21.0f, 21.0f    // lineSpacing=21, charSpacing=21
    );
}

//------------------------------------------------------------------------------
// WeaponSelect_Finalize
//------------------------------------------------------------------------------
void WeaponSelect_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;

    delete g_pText; g_pText = nullptr;
    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }
}

//------------------------------------------------------------------------------
// WeaponSelect_Update
//------------------------------------------------------------------------------
void WeaponSelect_Update(double elapsed_time)
{
    g_Time += elapsed_time;

    // 右クリックトリガー検出（前フレームと比較）
    Mouse_State ms{};
    Mouse_GetState(&ms);
    static bool s_PrevMouseRight = false;
    const bool mouseRightTrig = ms.rightButton && !s_PrevMouseRight;
    s_PrevMouseRight = ms.rightButton;

    // ── アーム（武器）移動：TAB / 左右キー / パッド ───────────────────
    if (KeyLogger_IsTrigger(KK_TAB)          ||
        KeyLogger_IsTrigger(KK_RIGHT)         ||
        KeyLogger_IsTrigger(KK_D)             ||
        PadLogger_IsTrigger(PAD_DPAD_RIGHT))
    {
        g_Selected = (g_Selected + 1) % 3;
        PlayAudio(g_SeCursorMove, false);
    }

    if (KeyLogger_IsTrigger(KK_LEFT) ||
        KeyLogger_IsTrigger(KK_A)    ||
        PadLogger_IsTrigger(PAD_DPAD_LEFT))
    {
        g_Selected = (g_Selected + 2) % 3;
        PlayAudio(g_SeCursorMove, false);
    }

    // ── 決定：右クリック / ENTER / パッドA ───────────────────────────
    if (mouseRightTrig || KeyLogger_IsTrigger(KK_ENTER) || PadLogger_IsTrigger(PAD_A))
    {
        PlayAudio(g_SeSelect, false);
        g_Result = WeaponSelectResult::Decided;
    }
}

//------------------------------------------------------------------------------
// WeaponSelect_Draw
//  ※ Sprite_Begin() は main.cpp 側で GameManager_Draw() 前に呼ばれている
//  ※ DebugText::Draw() 後に Sprite_Begin() を再呼び出し → Fade_Draw() を保護
//------------------------------------------------------------------------------
void WeaponSelect_Draw()
{
    if (g_WhiteTexID < 0) return;

    static const XMFLOAT4 WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const XMFLOAT4 DIM = { 1.0f, 1.0f, 1.0f, 0.25f };
    static const XMFLOAT4 GRAY = { 0.6f, 0.6f, 0.6f, 1.0f };
    static const XMFLOAT4 SEL_TAB = { 0.2f, 0.45f, 1.0f, 0.85f };
    static const XMFLOAT4 UNS_TAB = { 0.05f,0.05f, 0.05f,0.55f };
    static const XMFLOAT4 PANEL_BG = { 0.0f, 0.0f,  0.0f, 0.65f };
    static const XMFLOAT4 BORDER = { 1.0f, 1.0f,  1.0f, 0.45f };
    static const XMFLOAT4 BAR_FILL = { 0.2f, 0.72f, 1.0f, 1.0f };
    static const XMFLOAT4 BAR_MPTY = { 0.1f, 0.1f,  0.1f, 1.0f };

    // ── 背景 ─────────────────────────────────────────────────────────────
    if (g_BgTexID >= 0)
        Sprite_Draw(g_BgTexID, 0.0f, 0.0f, 1600.0f, 900.0f, WHITE);

    // ── タブ（3つ）──────────────────────────────────────────────────────
    for (int i = 0; i < 3; ++i)
    {
        const float tx = TAB_START_X + i * (TAB_W + TAB_GAP);
        const bool  sel = (i == g_Selected);
        const float bob = sel
            ? sinf(static_cast<float>(g_Time) * 5.5f) * 3.0f
            : 0.0f;

        // 枠
        Sprite_Draw(g_WhiteTexID,
            tx - 3.0f, TAB_Y - 3.0f + bob,
            TAB_W + 6.0f, TAB_H + 6.0f,
            sel ? WHITE : DIM);
        // 本体
        Sprite_Draw(g_WhiteTexID, tx, TAB_Y + bob, TAB_W, TAB_H,
            sel ? SEL_TAB : UNS_TAB);
        // 下線（選択中）
        if (sel)
            Sprite_Draw(g_WhiteTexID,
                tx, TAB_Y + TAB_H + bob + 3.0f,
                TAB_W, 4.0f, WHITE);
    }

    // ── ステータスパネル ─────────────────────────────────────────────────
    Sprite_Draw(g_WhiteTexID,
        PNL_X - 2.0f, PNL_Y - 2.0f, PNL_W + 4.0f, PNL_H + 4.0f, BORDER);
    Sprite_Draw(g_WhiteTexID, PNL_X, PNL_Y, PNL_W, PNL_H, PANEL_BG);

    // ── バー 3 行（テキストと同じ Y 位置に配置）───────────────────────
    const WeaponData& wd = k_Weapons[g_Selected];
    const float bars[3] = { wd.dmgBar, wd.rateBar, wd.expBar };
    for (int r = 0; r < 3; ++r)
    {
        const float by = BAR_Y0 + r * BAR_GAP_Y;
        Sprite_Draw(g_WhiteTexID, BAR_X, by, BAR_W_MAX, BAR_H, BAR_MPTY);
        const float filled = bars[r] * BAR_W_MAX;
        if (filled >= 1.0f)
            Sprite_Draw(g_WhiteTexID, BAR_X, by, filled, BAR_H, BAR_FILL);
    }

    // ── DebugText（1インスタンスで全テキストを一括描画）─────────────────
    //
    //  行構成（SetText の '\n' で行を進める）
    //   line  0 (y= 65) : タイトル
    //   line  1〜 5     : 空行（タブ領域まで移動）
    //   line  6 (y=191) : タブ名（選択中=白、未選択=灰）
    //   line  7〜11     : 空行（パネルまで移動）
    //   line 12 (y=317) : 武器名ヘッダ
    //   line 13〜14     : 空行
    //   line 15 (y=380) : Damage
    //   line 16〜17     : 空行
    //   line 18 (y=443) : FireRate
    //   line 19〜20     : 空行
    //   line 21 (y=506) : Explosion
    //   line 22〜26     : 空行
    //   line 27 (y=632) : フッター
    //
    if (g_pText)
    {
        g_pText->Clear();

        // ── タイトル（line0, y=65）
        //    "WEAPON SELECT" を中央付近に配置（31 スペース ≈ x=651）
        g_pText->SetText("                               WEAPON SELECT\n", WHITE);

        // ── 空行 ×5 → line6 へ
        g_pText->SetText("\n\n\n\n\n", WHITE);

        // ── タブ名（line6, y=191）
        //    charSpacing=21: 22スペース≈x=462（tab0中心530から -68px）
        g_pText->SetText("                      ", WHITE);                               // 22sp
        g_pText->SetText(k_Weapons[0].name, g_Selected == 0 ? WHITE : GRAY);            // NORMAL
        g_pText->SetText("      ", WHITE);                                               // 6sp
        g_pText->SetText(k_Weapons[1].name, g_Selected == 1 ? WHITE : GRAY);            // SHOTGUN
        g_pText->SetText("      ", WHITE);                                               // 6sp
        g_pText->SetText(k_Weapons[2].name, g_Selected == 2 ? WHITE : GRAY);            // MISSILE

        // ── 空行 ×6 → line12 へ
        g_pText->SetText("\n\n\n\n\n\n", WHITE);

        // ── 武器名ヘッダ（line12, y=317）
        char line[64];
        snprintf(line, sizeof(line), "                  [ %s ]", wd.name);
        g_pText->SetText(line, WHITE);

        // ── 空行 ×3 → line15 へ（header の '\n' 込み）
        g_pText->SetText("\n\n\n", WHITE);

        // ── Damage（line15, y=380）
        snprintf(line, sizeof(line), "                Damage      %d", wd.damage);
        g_pText->SetText(line, WHITE);

        // ── 空行 ×3 → line18 へ
        g_pText->SetText("\n\n\n", WHITE);

        // ── FireRate（line18, y=443）
        snprintf(line, sizeof(line), "                FireRate    %.2fs", wd.fireInterval);
        g_pText->SetText(line, WHITE);

        // ── 空行 ×3 → line21 へ
        g_pText->SetText("\n\n\n", WHITE);

        // ── Explosion（line21, y=506）
        if (wd.explosionR > 0.0f)
            snprintf(line, sizeof(line), "                Explosion   %.1fm", wd.explosionR);
        else
            snprintf(line, sizeof(line), "                Explosion   ---");
        g_pText->SetText(line, WHITE);

        // ── 空行 ×6 → line27 へ
        g_pText->SetText("\n\n\n\n\n\n", WHITE);

        // ── フッター（line27, y=632）
        g_pText->SetText(
            "        < TAB / LEFT / RIGHT >   |   RIGHT CLICK / ENTER : SELECT", GRAY);

        g_pText->Draw();

        // DebugText::Draw() が D3D パイプライン状態を変更するため
        // 後続の Fade_Draw()（Sprite_Draw 使用）向けに Sprite を再設定
        Sprite_Begin();
    }
}

//------------------------------------------------------------------------------
// WeaponSelect_GetResult / WeaponSelect_GetSelectedIndex
//------------------------------------------------------------------------------
WeaponSelectResult WeaponSelect_GetResult()        { return g_Result;   }
int                WeaponSelect_GetSelectedIndex() { return g_Selected; }
