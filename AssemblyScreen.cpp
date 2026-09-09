/*==============================================================================

   アセンブル画面 [AssemblyScreen.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

   ■レイアウト（1600×900）
     Left  (x=0,   w=260) : R-ARM / L-ARM 武器リスト
     Center(x=270, w=700) : 選択中パーツ情報・ステータスバー
     Right (x=980, w=620) : ASSEMBLY ヘッダ・クレジット

   ■テキスト描画
     DirectWrite を使用（2 インスタンス：大文字ヘッダ用 / ボディ用）

   ■D3D11 / D2D 共存
     Sprite_Draw (D3D11) でパネル背景・バーを描画後、
     D3D11 RTV をアンバインド → DirectWrite DrawString (D2D) → RTV 再バインド
     という順序で描画する。

==============================================================================*/
#include "AssemblyScreen.h"
#include "WeaponDef.h"
#include "audio.h"
#include "DirectWrite.h"
#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include "model.h"
#include "shader3d.h"
#include "ModelToon.h"
#include "ShaderToon.h"
#include "ShaderEdge.h"
#include "light.h"
#include "UIInput.h"
#include "keyboard.h"
#include "mouse.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <cstdio>
#include <cmath>
#include <string>

using namespace DirectX;

//==============================================================================
// 定数
//==============================================================================
static constexpr int   INITIAL_CREDITS = 200000;
static constexpr float SW = 1600.0f;
static constexpr float SH = 900.0f;

// 左パネル
static constexpr float LEFT_X = 0.0f,    LEFT_W = 260.0f;
// センターパネル
static constexpr float CNT_X  = 270.0f,  CNT_W  = 700.0f;
// 右パネル
static constexpr float RGT_X  = 980.0f,  RGT_W  = 620.0f;

// 左パネル内レイアウト
static constexpr float LP_RARM_Y       = 20.0f;
static constexpr float LP_ITEM_START_R = 60.0f;
static constexpr float LP_DIV_Y        = 248.0f;
static constexpr float LP_LARM_Y       = 268.0f;
static constexpr float LP_ITEM_START_L = 308.0f;

// R-ARM リスト帯（LP_ITEM_START_R 〜 LP_DIV_Y）に WEAPON_COUNT 項目を均等配置する。
// 武器が増減してもレイアウトが崩れないよう、項目間隔・高さを武器数から自動計算する。
// （WEAPON_COUNT=4 のとき従来値 47/41 相当、5 で 37.6/31.6 に自動で詰まる）
static constexpr float LP_ITEM_SPACING = (LP_DIV_Y - LP_ITEM_START_R) / (float)WEAPON_COUNT;
static constexpr float LP_ITEM_H       = LP_ITEM_SPACING - 6.0f;
// 行内の文字を縦センタリングするオフセット（帯の高さと文字高さから算出）。
// 固定値だと武器数が増えて行が詰まったときに帯と文字がズレるため、帯高に追従させる。
static constexpr float LP_ITEM_TEXT_H   = 18.0f;                              // 項目フォントサイズ相当
static constexpr float LP_ITEM_TEXT_OFF = (LP_ITEM_H - LP_ITEM_TEXT_H) * 0.5f; // 帯に対する文字の上端オフセット

// 準備完了（READY）ボタン：左パネル下部、下部ヒントバー（y≒862）の上に配置
static constexpr float LP_READY_X = LEFT_X + 14.0f;
static constexpr float LP_READY_Y = 792.0f;
static constexpr float LP_READY_W = LEFT_W - 28.0f;
static constexpr float LP_READY_H = 54.0f;

// センターパネル内レイアウト
static constexpr float CP_NAME_Y       = 60.0f;
static constexpr float CP_DIV1_Y       = 130.0f;
static constexpr float CP_INFO_LABEL_Y = 380.0f;
static constexpr float CP_BAR_X_LABEL  = CNT_X + 20.0f;
static constexpr float CP_BAR_X_BAR    = CNT_X + 200.0f;
static constexpr float CP_BAR_W        = 380.0f;
static constexpr float CP_BAR_H        = 22.0f;
static constexpr float CP_BAR_ROW1_Y   = 430.0f;
static constexpr float CP_BAR_ROW2_Y   = 500.0f;
static constexpr float CP_BAR_ROW3_Y   = 570.0f;
static constexpr float CP_VAL_X        = CNT_X + 595.0f;

// 右パネル内レイアウト
static constexpr float RP_TITLE_Y      = 35.0f;
static constexpr float RP_DIV_Y        = 80.0f;
static constexpr float RP_CREDIT_Y     = 760.0f;

//==============================================================================
// 内部状態
//==============================================================================
namespace
{
    // アクティブパネル（g_Focus から導出される）
    enum { PANEL_RARM = 0, PANEL_LARM = 1, PANEL_READY = 2, PANEL_COUNT = 3 };
    int g_ActivePanel = PANEL_RARM;

    // グローバル縦カーソル：[R武器 0..N-1][L武器 0..N-1][READY] を通した1本の縦リスト。
    // 上下キーで移動し端でループする。TAB は各パネル先頭へジャンプ。
    //   0 .. N-1        : R-ARM 武器
    //   N .. 2N-1       : L-ARM 武器
    //   2N              : READY ボタン
    int g_Focus = 0;

    // 各アームのカーソル位置（ホバー中の候補。WeaponID に対応。g_Focus と同期）
    int g_RightCursor = WEAPON_MACHINEGUN;
    int g_LeftCursor  = WEAPON_SHIELD;

    // 各アームの確定武器（A で確定。装備されるのはこちら）
    int g_RightSelected = WEAPON_MACHINEGUN;
    int g_LeftSelected  = WEAPON_SHIELD;

    // ショップモード（サバイバルのショップから流用する際に true）
    bool g_ShopMode   = false;
    int  g_ShopBudget = 0;      // 使用可能クレジット

    // 前回選択のデフォルト値（SaveData_Load から上書きされる）
    int g_DefaultRight = WEAPON_MACHINEGUN;
    int g_DefaultLeft  = WEAPON_SHIELD;

    // 確定 / キャンセルフラグ
    bool g_Decided   = false;
    bool g_Cancelled = false;

    // SE
    int g_SeCursorMove  = -1;
    int g_SeSelect      = -1;
    int g_SeCancel      = -1;
    int g_SeTabSwitch   = -1;

    double g_Time = 0.0;

    // テクスチャ（白単色・背景用）
    int g_WhiteTexID = -1;
    int g_BgTexID    = -1;

    // DirectWrite インスタンス（2 スタイル）
    DirectWrite* g_pDWLarge  = nullptr;  // 32pt ヘッダ用
    DirectWrite* g_pDWBody   = nullptr;  // 20pt ボディ用

    // プレビューモデル（武器ごとに 1 つ）
    MODEL* g_pPreviewModels[WEAPON_COUNT] = {};
    MODEL* g_pMeleeEdgePreview = nullptr;   // 近接の発光パーツ（Blade に重ねて描画）

    // プレイヤープレビューモデル（ボディ・ヘッド・スラスター）
    MODEL* g_pPlayerPreviewBody     = nullptr;
    MODEL* g_pPlayerPreviewHead     = nullptr;
    MODEL* g_pPlayerPreviewThruster = nullptr;

    // プレビュー回転角（ラジアン）
    float g_PreviewAngle = 0.0f;
}

//==============================================================================
// 内部ヘルパー
//==============================================================================

// 残クレジット計算（確定済みの装備武器で計算する）
static int CalcRemaining()
{
    if (g_ShopMode)
    {
        // ショップ：装備中（初期値）から変更したアームぶんだけ課金する差分方式。
        // 現状維持は無料、武器を替えたアームだけそのコストを支払う。
        int spent = 0;
        if (g_RightSelected != g_DefaultRight) spent += k_WeaponDefs[g_RightSelected].cost;
        if (g_LeftSelected  != g_DefaultLeft)  spent += k_WeaponDefs[g_LeftSelected].cost;
        return g_ShopBudget - spent;
    }

    return INITIAL_CREDITS
         - k_WeaponDefs[g_RightSelected].cost
         - k_WeaponDefs[g_LeftSelected].cost;
}

// グローバル縦カーソル g_Focus から アクティブパネルとホバーカーソルを更新する
static void ApplyFocus()
{
    if (g_Focus < WEAPON_COUNT)
    {
        g_ActivePanel = PANEL_RARM;
        g_RightCursor = g_Focus;
    }
    else if (g_Focus < 2 * WEAPON_COUNT)
    {
        g_ActivePanel = PANEL_LARM;
        g_LeftCursor  = g_Focus - WEAPON_COUNT;
    }
    else
    {
        g_ActivePanel = PANEL_READY;
    }
}

// Sprite カラー定数
static constexpr XMFLOAT4 kDim        = { 1.00f, 1.00f, 1.00f, 0.25f };
static constexpr XMFLOAT4 kPanelBg    = { 0.10f, 0.14f, 0.26f, 0.90f };  // モデルが見やすいよう少し明るめのネイビー
static constexpr XMFLOAT4 kBorder     = { 0.30f, 0.60f, 1.00f, 0.70f };
static constexpr XMFLOAT4 kSelBg      = { 0.15f, 0.35f, 0.80f, 0.80f };
static constexpr XMFLOAT4 kBarEmpty   = { 0.10f, 0.10f, 0.12f, 1.00f };
static constexpr XMFLOAT4 kBarFill    = { 0.20f, 0.72f, 1.00f, 1.00f };
static constexpr XMFLOAT4 kDivider    = { 0.25f, 0.50f, 0.90f, 0.50f };

//==============================================================================
// AssemblyScreen_Initialize
//==============================================================================
void AssemblyScreen_Initialize()
{
    g_RightCursor   = g_DefaultRight;   // 前回選択を引き継ぐ（ホバー初期位置）
    g_LeftCursor    = g_DefaultLeft;
    g_RightSelected = g_DefaultRight;   // 確定武器も前回選択で初期化
    g_LeftSelected  = g_DefaultLeft;
    g_Focus         = g_DefaultRight;   // R-ARM の前回武器にフォーカス
    ApplyFocus();                       // g_ActivePanel / カーソルを同期
    g_ShopMode      = false;            // 既定は通常モード（ショップは Initialize 後に SetShopMode する）
    g_Decided       = false;
    g_Cancelled     = false;
    g_Time          = 0.0;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudioWithVolume("resource/Sound/ui_cursor_move.wav", 0.5f);
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudioWithVolume("resource/Sound/ui_select.wav", 0.5f);
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudioWithVolume("resource/Sound/ui_cancel.wav", 0.5f);
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudioWithVolume("resource/Sound/ui_tab_switch.wav",0.5f);

    // テクスチャ
    if (g_WhiteTexID < 0) g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");
    if (g_BgTexID    < 0) g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");

    // プレビューモデル（古いものを解放してから再ロード）
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_pPreviewModels[i]) { ModelRelease(g_pPreviewModels[i]); g_pPreviewModels[i] = nullptr; }
        g_pPreviewModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
    }
    // 近接の発光パーツ（BladeEdge）を Blade と同スケールでロード
    if (g_pMeleeEdgePreview) { ModelRelease(g_pMeleeEdgePreview); g_pMeleeEdgePreview = nullptr; }
    g_pMeleeEdgePreview = ModelLoad("resource/Models/BladeEdge.fbx", k_WeaponDefs[WEAPON_MELEE].scale);

    // プレイヤープレビューモデル（同様に解放→再ロード）
    if (g_pPlayerPreviewBody)     { ModelRelease(g_pPlayerPreviewBody);     g_pPlayerPreviewBody     = nullptr; }
    if (g_pPlayerPreviewHead)     { ModelRelease(g_pPlayerPreviewHead);     g_pPlayerPreviewHead     = nullptr; }
    if (g_pPlayerPreviewThruster) { ModelRelease(g_pPlayerPreviewThruster); g_pPlayerPreviewThruster = nullptr; }
    g_pPlayerPreviewBody     = ModelLoad("resource/Models/body.fbx",     0.3f);
    g_pPlayerPreviewHead     = ModelLoad("resource/Models/Head.fbx",     0.3f);
    g_pPlayerPreviewThruster = ModelLoad("resource/Models/Thruster.fbx", 0.3f);

    g_PreviewAngle = 0.0f;

    // DirectWrite 大文字ヘッダ（32pt, 白）— 2回目以降は再生成しない
    if (!g_pDWLarge)
    {
        static FontData fdLarge;
        fdLarge.font          = Font::Arial;
        fdLarge.fontSize      = 32.0f;
        fdLarge.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdLarge.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        fdLarge.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        g_pDWLarge = new DirectWrite(&fdLarge);
        g_pDWLarge->Init();
    }

    // DirectWrite ボディ（20pt, 白）— 2回目以降は再生成しない
    if (!g_pDWBody)
    {
        static FontData fdBody;
        fdBody.font          = Font::Arial;
        fdBody.fontSize      = 20.0f;
        fdBody.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        fdBody.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        fdBody.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        g_pDWBody = new DirectWrite(&fdBody);
        g_pDWBody->Init();
    }
}

//==============================================================================
// AssemblyScreen_Finalize
//==============================================================================
void AssemblyScreen_Finalize()
{
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeSelect);     g_SeSelect     = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;
    UnloadAudio(g_SeTabSwitch);  g_SeTabSwitch  = -1;

    if (g_pDWLarge) { g_pDWLarge->Release(); delete g_pDWLarge; g_pDWLarge = nullptr; }
    if (g_pDWBody)  { g_pDWBody->Release();  delete g_pDWBody;  g_pDWBody  = nullptr; }

    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        ModelRelease(g_pPreviewModels[i]);
        g_pPreviewModels[i] = nullptr;
    }
    ModelRelease(g_pMeleeEdgePreview);
    g_pMeleeEdgePreview = nullptr;

    ModelRelease(g_pPlayerPreviewBody);     g_pPlayerPreviewBody     = nullptr;
    ModelRelease(g_pPlayerPreviewHead);     g_pPlayerPreviewHead     = nullptr;
    ModelRelease(g_pPlayerPreviewThruster); g_pPlayerPreviewThruster = nullptr;

    if (g_WhiteTexID >= 0) { Texture_Release(g_WhiteTexID); g_WhiteTexID = -1; }
    if (g_BgTexID    >= 0) { Texture_Release(g_BgTexID);    g_BgTexID    = -1; }
}

//==============================================================================
// AssemblyScreen_Update
//==============================================================================
bool AssemblyScreen_Update(double dt)
{
    g_Time += dt;
    g_PreviewAngle += static_cast<float>(dt) * 0.8f;  // プレビュー自動回転

    constexpr int FOCUS_READY = 2 * WEAPON_COUNT;      // READY のフォーカス位置
    constexpr int FOCUS_TOTAL = 2 * WEAPON_COUNT + 1;  // 全フォーカス項目数

    // ── TAB / LB / RB：次のパネルへジャンプ（R-ARM → L-ARM → READY 循環）──
    // 武器パネルへ移る際は先頭ではなく、そのアームの選択中（確定）武器にフォーカスする
    if (UI_IsTabSwitch())
    {
        if      (g_ActivePanel == PANEL_RARM) g_Focus = WEAPON_COUNT + g_LeftSelected; // L-ARM 選択武器
        else if (g_ActivePanel == PANEL_LARM) g_Focus = FOCUS_READY;                   // READY
        else                                  g_Focus = g_RightSelected;               // R-ARM 選択武器
        ApplyFocus();
        PlayAudio(g_SeTabSwitch, false);
    }

    // ── 上下：全項目を通した縦移動（端でループ：一番下→一番上）──
    if (UI_IsMoveDown())
    {
        g_Focus = (g_Focus + 1) % FOCUS_TOTAL;
        ApplyFocus();
        PlayAudio(g_SeCursorMove, false);
    }
    if (UI_IsMoveUp())
    {
        g_Focus = (g_Focus + FOCUS_TOTAL - 1) % FOCUS_TOTAL;
        ApplyFocus();
        PlayAudio(g_SeCursorMove, false);
    }

    // ── ESC / パッドB でキャンセル（前の画面へ戻る）──
    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Cancelled = true;
        return true;
    }

    // ── 決定入力：ENTER / パッドA / 左クリック ──
    //（キーボード A は本来「左移動」なので決定には割り当てない）
    const bool confirm = UI_IsConfirm();
    if (confirm)
    {
        if (g_ActivePanel == PANEL_READY)
        {
            // READY：予算内ならゲーム開始
            if (CalcRemaining() >= 0)
            {
                // 確定したロードアウトを「記憶用」default に反映する。
                // これをしないと再入場時に Initialize が古い default へ戻してしまう
                //（ショップは別途 default を退避/復元するため対象外）。
                if (!g_ShopMode)
                {
                    g_DefaultRight = g_RightSelected;
                    g_DefaultLeft  = g_LeftSelected;
                }
                PlayAudio(g_SeSelect, false);
                g_Decided = true;
                return true;
            }
            PlayAudio(g_SeCancel, false);   // 予算超過中は開始できない
        }
        else
        {
            // R/L-ARM：ホバー中の武器をこのアームに確定する（装備に反映）
            if (g_ActivePanel == PANEL_RARM) g_RightSelected = g_RightCursor;
            else                             g_LeftSelected  = g_LeftCursor;
            PlayAudio(g_SeSelect, false);
        }
    }

    return false;
}

//==============================================================================
// AssemblyScreen_Draw
// 描画順：
//   (1) 背景・パネル（Sprite / D3D11）
//   (2) ステータスバー（Sprite / D3D11）
//   (3) D3D11 RTV アンバインド + Flush
//   (4) DirectWrite テキスト（D2D）
//   (5) D3D11 RTV 再バインド → Sprite_Begin（Fade 用）
//==============================================================================
void AssemblyScreen_Draw()
{
    if (g_WhiteTexID < 0) return;

    //--------------------------------------------------------------------------
    // (1) 背景
    //--------------------------------------------------------------------------
    if (g_BgTexID >= 0)
        Sprite_Draw(g_BgTexID, 0.0f, 0.0f, SW, SH, kDim);

    // 左パネル背景
    Sprite_Draw(g_WhiteTexID, LEFT_X, 0.0f, LEFT_W, SH, kPanelBg);
    Sprite_Draw(g_WhiteTexID, LEFT_X,          0.0f, 2.0f, SH, kBorder);
    Sprite_Draw(g_WhiteTexID, LEFT_X+LEFT_W-2, 0.0f, 2.0f, SH, kBorder);

    // センターパネル背景
    Sprite_Draw(g_WhiteTexID, CNT_X, 0.0f, CNT_W, SH, kPanelBg);
    Sprite_Draw(g_WhiteTexID, CNT_X,         0.0f, 2.0f, SH, kBorder);
    Sprite_Draw(g_WhiteTexID, CNT_X+CNT_W-2, 0.0f, 2.0f, SH, kBorder);

    // 右パネル背景
    Sprite_Draw(g_WhiteTexID, RGT_X, 0.0f, RGT_W, SH, kPanelBg);
    Sprite_Draw(g_WhiteTexID, RGT_X+RGT_W-2, 0.0f, 2.0f, SH, kBorder);

    // 分割線
    Sprite_Draw(g_WhiteTexID, LEFT_X, LP_DIV_Y, LEFT_W, 2.0f, kDivider);
    Sprite_Draw(g_WhiteTexID, CNT_X+10, CP_DIV1_Y, CNT_W-20, 1.0f, kDivider);
    Sprite_Draw(g_WhiteTexID, CNT_X+10, CP_INFO_LABEL_Y+28.0f, CNT_W-20, 1.0f, kDivider);
    Sprite_Draw(g_WhiteTexID, RGT_X+10, RP_DIV_Y, RGT_W-20, 1.0f, kBorder);
    Sprite_Draw(g_WhiteTexID, RGT_X+10, RP_CREDIT_Y-20.0f, RGT_W-20, 1.0f, kDivider);

    //--------------------------------------------------------------------------
    // アクティブパネル強調（R/L-ARM ラベルバー。READY 時はどちらも光らせない）
    //--------------------------------------------------------------------------
    if (g_ActivePanel == PANEL_RARM)
        Sprite_Draw(g_WhiteTexID, LEFT_X, LP_RARM_Y - 4.0f, LEFT_W, 32.0f, kSelBg);
    else if (g_ActivePanel == PANEL_LARM)
        Sprite_Draw(g_WhiteTexID, LEFT_X, LP_LARM_Y - 4.0f, LEFT_W, 32.0f, kSelBg);

    // 確定武器の背景（常時・淡色）＋ ホバーカーソル背景（アクティブパネルのみ・明色）
    {
        const XMFLOAT4 kSelDim = { 0.12f, 0.28f, 0.55f, 0.45f };  // 確定表示（淡）
        const float bob = sinf(static_cast<float>(g_Time) * 5.5f) * 2.0f;

        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            const float ry = LP_ITEM_START_R + i * LP_ITEM_SPACING;
            const float ly = LP_ITEM_START_L + i * LP_ITEM_SPACING;

            // 確定済み武器（淡色・常時）
            if (i == g_RightSelected)
                Sprite_Draw(g_WhiteTexID, LEFT_X+2, ry, LEFT_W-4, LP_ITEM_H, kSelDim);
            if (i == g_LeftSelected)
                Sprite_Draw(g_WhiteTexID, LEFT_X+2, ly, LEFT_W-4, LP_ITEM_H, kSelDim);

            // ホバーカーソル（アクティブパネルのみ・明色・上下に揺れる）
            if (g_ActivePanel == PANEL_RARM && i == g_RightCursor)
                Sprite_Draw(g_WhiteTexID, LEFT_X+2, ry+bob, LEFT_W-4, LP_ITEM_H, kSelBg);
            if (g_ActivePanel == PANEL_LARM && i == g_LeftCursor)
                Sprite_Draw(g_WhiteTexID, LEFT_X+2, ly+bob, LEFT_W-4, LP_ITEM_H, kSelBg);
        }
    }

    //--------------------------------------------------------------------------
    // 準備完了（READY）ボタン ─ 予算内なら緑パルス、超過なら赤で無効表現
    //--------------------------------------------------------------------------
    {
        const bool  ready   = (CalcRemaining() >= 0);
        const bool  focused = (g_ActivePanel == PANEL_READY);
        const float pulse   = 0.5f + 0.5f * sinf(static_cast<float>(g_Time) * 4.0f);

        // フォーカス時は明るく（パルス強め）、非フォーカス時は控えめ
        const float fillA = focused ? (0.55f + 0.40f * pulse) : 0.35f;
        const XMFLOAT4 fill = ready
            ? XMFLOAT4{ 0.10f, 0.55f, 0.25f, fillA }
            : XMFLOAT4{ 0.40f, 0.10f, 0.10f, focused ? 0.80f : 0.55f };
        const XMFLOAT4 edge = ready
            ? XMFLOAT4{ 0.35f, 1.00f, 0.55f, focused ? 1.00f : 0.60f }
            : XMFLOAT4{ 1.00f, 0.35f, 0.35f, focused ? 0.95f : 0.60f };

        // フォーカス時は枠を太く（2px→3px）
        const float bw = focused ? 3.0f : 2.0f;

        // 本体
        Sprite_Draw(g_WhiteTexID, LP_READY_X, LP_READY_Y, LP_READY_W, LP_READY_H, fill);
        // 枠（上下左右）
        Sprite_Draw(g_WhiteTexID, LP_READY_X, LP_READY_Y,                 LP_READY_W, bw,         edge);
        Sprite_Draw(g_WhiteTexID, LP_READY_X, LP_READY_Y + LP_READY_H-bw, LP_READY_W, bw,         edge);
        Sprite_Draw(g_WhiteTexID, LP_READY_X, LP_READY_Y,                 bw,         LP_READY_H, edge);
        Sprite_Draw(g_WhiteTexID, LP_READY_X + LP_READY_W-bw, LP_READY_Y, bw,         LP_READY_H, edge);
    }

    //--------------------------------------------------------------------------
    // (2) ステータスバー
    //--------------------------------------------------------------------------
    // 中央の詳細プレビューはホバー中の候補武器を表示する
    const int hoverId = (g_ActivePanel == PANEL_LARM) ? g_LeftCursor : g_RightCursor;
    const WeaponDef& wd = k_WeaponDefs[hoverId];

    const float bars[3] = { wd.dmgBar, wd.rateBar, wd.expBar };
    const float barYs[3] = { CP_BAR_ROW1_Y, CP_BAR_ROW2_Y, CP_BAR_ROW3_Y };
    for (int r = 0; r < 3; ++r)
    {
        Sprite_Draw(g_WhiteTexID, CP_BAR_X_BAR, barYs[r], CP_BAR_W, CP_BAR_H, kBarEmpty);
        const float filled = bars[r] * CP_BAR_W;
        if (filled >= 1.0f)
            Sprite_Draw(g_WhiteTexID, CP_BAR_X_BAR, barYs[r], filled, CP_BAR_H, kBarFill);
    }

    //--------------------------------------------------------------------------
    // (3) センターパネル – 武器モデルプレビュー（プレイヤーと同じ描画）
    //--------------------------------------------------------------------------
    {
        MODEL* previewModel = g_pPreviewModels[hoverId];
        if (previewModel)
        {
            using namespace DirectX;

            // プレビュー領域（センターパネル内、仕切り線〜PARTS INFO の間）
            constexpr float PV_X = CNT_X + 100.0f;
            constexpr float PV_Y = CP_DIV1_Y + 5.0f;
            constexpr float PV_W = CNT_W - 200.0f;
            constexpr float PV_H = CP_INFO_LABEL_Y - CP_DIV1_Y - 10.0f;

            // プレビューカメラ
            const XMFLOAT3 eyeF3  = { 0.25f, 0.18f, 0.55f };
            const XMVECTOR eyeV   = XMLoadFloat3(&eyeF3);
            const XMVECTOR target = XMVectorZero();
            const XMVECTOR upV    = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            const XMMATRIX view   = XMMatrixLookAtLH(eyeV, target, upV);
            const XMMATRIX proj   = XMMatrixPerspectiveFovLH(
                XMConvertToRadians(45.0f), PV_W / PV_H, 0.01f, 100.0f);

            // 全シェーダーに View / Proj を設定（Player_Camera_Update と同じパターン）
            Shader3d_SetViewMatrix(view);
            Shader3d_SetProjectMatrix(proj);
            ShaderToon_SetViewMatrix(view);
            ShaderToon_SetProjectMatrix(proj);
            ShaderEdge_SetViewMatrix(view);
            ShaderEdge_SetProjectMatrix(proj);

            // ゆっくり Y 軸回転
            const XMMATRIX world = XMMatrixRotationY(g_PreviewAngle);

            // サブビューポートのラムダ（仮想座標→実ピクセル座標にスケール）
            const float vpScaleX = (float)Direct3D_GetBackBufferWidth()  / SW;
            const float vpScaleY = (float)Direct3D_GetBackBufferHeight() / SH;
            auto setSubVP = [&]()
            {
                D3D11_VIEWPORT vp{};
                vp.TopLeftX = PV_X * vpScaleX; vp.TopLeftY = PV_Y * vpScaleY;
                vp.Width    = PV_W * vpScaleX; vp.Height   = PV_H * vpScaleY;
                vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
                Direct3D_GetContext()->RSSetViewports(1, &vp);
            };

            // ── ゲームループと同じ順序で描画 ────────────────
            // game.cpp が Player_Draw() の前に SetDepthEnable(true) するのと同様に
            // スプライト（2D）後の depth 無効状態から 3D 描画用に切り替える
            Direct3D_SetDepthEnable(true);
            setSubVP();

            // ライティング
            Light_SetSpecularWorld(eyeF3, 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
            Light_SetAmbient({ 0.65f, 0.65f, 0.65f });   // プレビューを少し明るく（モデル視認性UP）

            // 法線パス（エッジ検出用）
            const bool hoverMelee = (hoverId == WEAPON_MELEE && g_pMeleeEdgePreview);

            ShaderEdge_BeginNormalPass();
            ShaderEdge_SetWorldMatrix(world);
            ModelDrawWithoutBegin(previewModel, world);
            // ※発光エッジはアウトラインに含めない（ゲーム中と同じ）
            ShaderEdge_EndNormalPass();

            // トゥーン描画
            setSubVP();
            ModelDrawToon(previewModel, world);
            // 発光エッジ：この描画だけアンビエントを上げて光って見せる（ゲーム中と同じ）
            if (hoverMelee)
            {
                const XMFLOAT3 prevAmb = Light_GetAmbient();
                Light_SetAmbient({ 3.0f, 3.0f, 3.0f });
                ModelDrawToon(g_pMeleeEdgePreview, world);
                Light_SetAmbient(prevAmb);
            }

            // エッジ合成
            // DrawEdge は UV 0→1 をフル画面にマップするので、sub-viewport を解除してから呼ぶ
            D3D11_VIEWPORT fullVP{};
            fullVP.TopLeftX = 0.0f; fullVP.TopLeftY = 0.0f;
            fullVP.Width    = static_cast<float>(Direct3D_GetBackBufferWidth());
            fullVP.Height   = static_cast<float>(Direct3D_GetBackBufferHeight());
            fullVP.MinDepth = 0.0f; fullVP.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &fullVP);
            ShaderEdge_DrawEdge();
            Direct3D_SetDepthEnable(false);
        }
    }

    //--------------------------------------------------------------------------
    // (3b) 右パネル – プレイヤーモデルプレビュー
    //--------------------------------------------------------------------------
    if (g_pPlayerPreviewBody && g_pPlayerPreviewHead && g_pPlayerPreviewThruster)
    {
        using namespace DirectX;

        // プレビュー領域（右パネル内、仕切り線〜クレジット上部の間）
        constexpr float PP_X = RGT_X + 10.0f;
        constexpr float PP_Y = RP_DIV_Y + 5.0f;
        constexpr float PP_W = RGT_W - 20.0f;
        constexpr float PP_H = RP_CREDIT_Y - RP_DIV_Y - 30.0f;

        // プレビューカメラ（プレイヤー全体が収まるよう少し引いた位置）
        const XMFLOAT3 ppEyeF3 = { 0.0f, 0.55f, 1.7f };
        const XMVECTOR ppEye   = XMLoadFloat3(&ppEyeF3);
        const XMVECTOR ppTarget = XMVectorSet(0.0f, 0.1f, 0.0f, 1.0f);
        const XMVECTOR ppUp    = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX ppView  = XMMatrixLookAtLH(ppEye, ppTarget, ppUp);
        const XMMATRIX ppProj  = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(40.0f), PP_W / PP_H, 0.01f, 100.0f);

        Shader3d_SetViewMatrix(ppView);
        Shader3d_SetProjectMatrix(ppProj);
        ShaderToon_SetViewMatrix(ppView);
        ShaderToon_SetProjectMatrix(ppProj);
        ShaderEdge_SetViewMatrix(ppView);
        ShaderEdge_SetProjectMatrix(ppProj);

        // ボディ回転（player.cpp の rotY * rotYawFix と同じ）
        // プレビュー初期角度 +90° → bodyRot = RotY(90°+90°) = RotY(180°)
        // → ボディ FBX 前面(-Z) がカメラ方向(+Z) を向く
        const XMMATRIX bodyRot = XMMatrixRotationY(g_PreviewAngle + XMConvertToRadians(90.0f)) *
                                 XMMatrixRotationY(XMConvertToRadians(90.0f));

        // プレビューでのプレイヤー正面方向（player.cpp の g_PlayerFront に相当）
        // bodyRot 初期角90° → previewFront = (sin(0), 0, cos(0)) = (0,0,1) = カメラ方向
        const XMVECTOR worldFront = XMVectorSet(
            sinf(g_PreviewAngle), 0.0f, cosf(g_PreviewAngle), 0.0f);

        // AABB 計算（ボディは原点基準）
        const AABB bodyAABB     = ModelGetAABB(g_pPlayerPreviewBody,     {0,0,0});
        const AABB headAABB     = ModelGetAABB(g_pPlayerPreviewHead,     {0,0,0});
        const AABB thrusterAABB = ModelGetAABB(g_pPlayerPreviewThruster, {0,0,0});

        // 各パーツのワールド行列（player.cpp の GetHead/ThrusterWorldMatrix と同じ方式）
        // player.cpp: THRUSTER_FORWARD_OFFSET = -0.05f（プレイヤー前方からの後退量）
        constexpr float THRUSTER_FORWARD_OFFSET = +0.05f;
        const XMMATRIX bodyWorld = bodyRot;
        const XMMATRIX headWorld = bodyRot *
            XMMatrixTranslation(0.0f, bodyAABB.max.y - headAABB.min.y, 0.0f);
        const XMMATRIX thrusterWorld = XMMatrixRotationY(XMConvertToRadians(180.0f)) * bodyRot *
            XMMatrixTranslation(
                XMVectorGetX(worldFront) * THRUSTER_FORWARD_OFFSET,
                bodyAABB.min.y - thrusterAABB.max.y - 0.01f,
                XMVectorGetZ(worldFront) * THRUSTER_FORWARD_OFFSET);

        // 武器ワールド行列ビルダー（player.cpp の GetBarrel/ShieldWorldMatrix を簡略化）
        const XMVECTOR ppUp2      = XMVectorSet(0, 1, 0, 0);
        const XMVECTOR worldRight = XMVector3Normalize(
            XMVector3Cross(ppUp2, worldFront));

        auto makeWeaponWorld = [&](const WeaponDef& def, float sideSign, bool isMelee = false) -> XMMATRIX
        {
            // 位置：player.cpp の barrelOriginPos / shieldOriginPos と同方式
            XMFLOAT3 posF3 = {
                XMVectorGetX(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetX(worldFront) * def.forwardOffset,
                bodyAABB.min.y + def.heightOffset,
                XMVectorGetZ(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetZ(worldFront) * def.forwardOffset
            };
            // 近接は共通定数（WeaponDef.h）で胴体側面中心＋前へ。ゲーム中と一致させる。
            if (isMelee)
            {
                posF3 = {
                    XMVectorGetX(worldRight) * sideSign * MELEE_REST_SIDE
                        + XMVectorGetX(worldFront) * MELEE_REST_FWD,
                    bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_REST_UP_R,
                    XMVectorGetZ(worldRight) * sideSign * MELEE_REST_SIDE
                        + XMVectorGetZ(worldFront) * MELEE_REST_FWD
                };
            }
            // 向き：aimDir = playerFront 固定（カメラ追従なし）
            XMVECTOR aimZ = XMVectorNegate(worldFront);
            XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(ppUp2, aimZ));
            XMVECTOR aimY = XMVector3Cross(aimZ, aimX);
            XMFLOAT3 ax, ay, az;
            XMStoreFloat3(&ax, aimX); XMStoreFloat3(&ay, aimY); XMStoreFloat3(&az, aimZ);
            XMMATRIX aimRot(
                ax.x, ax.y, ax.z, 0,
                ay.x, ay.y, ay.z, 0,
                az.x, az.y, az.z, 0,
                0, 0, 0, 1);
            XMMATRIX localRot =
                XMMatrixRotationZ(XMConvertToRadians(def.flipDeg + def.leanDeg * sideSign)) *
                XMMatrixRotationX(XMConvertToRadians(def.tiltDeg));
            return localRot * aimRot * XMMatrixTranslation(posF3.x, posF3.y, posF3.z);
        };

        // プレイヤー人形には確定済み（装備）の武器を表示する
        const XMMATRIX rWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_RightSelected], +1.0f, g_RightSelected == WEAPON_MELEE);
        const XMMATRIX lWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_LeftSelected],  -1.0f, g_LeftSelected  == WEAPON_MELEE);
        MODEL* rWeaponModel = g_pPreviewModels[g_RightSelected];
        MODEL* lWeaponModel = g_pPreviewModels[g_LeftSelected];

        const float ppScaleX = (float)Direct3D_GetBackBufferWidth()  / SW;
        const float ppScaleY = (float)Direct3D_GetBackBufferHeight() / SH;
        auto setPlayerVP = [&]()
        {
            D3D11_VIEWPORT vp{};
            vp.TopLeftX = PP_X * ppScaleX; vp.TopLeftY = PP_Y * ppScaleY;
            vp.Width    = PP_W * ppScaleX; vp.Height   = PP_H * ppScaleY;
            vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &vp);
        };

        Direct3D_SetDepthEnable(true);
        Light_SetSpecularWorld(ppEyeF3, 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
        Light_SetAmbient({ 0.65f, 0.65f, 0.65f });   // プレビューを少し明るく（モデル視認性UP）

        // 法線パス
        setPlayerVP();
        ShaderEdge_BeginNormalPass();
        ShaderEdge_SetWorldMatrix(bodyWorld);
        ModelDrawWithoutBegin(g_pPlayerPreviewBody, bodyWorld);
        ShaderEdge_SetWorldMatrix(headWorld);
        ModelDrawWithoutBegin(g_pPlayerPreviewHead, headWorld);
        ShaderEdge_SetWorldMatrix(thrusterWorld);
        ModelDrawWithoutBegin(g_pPlayerPreviewThruster, thrusterWorld);
        if (rWeaponModel) { ShaderEdge_SetWorldMatrix(rWeaponWorld); ModelDrawWithoutBegin(rWeaponModel, rWeaponWorld); }
        if (lWeaponModel) { ShaderEdge_SetWorldMatrix(lWeaponWorld); ModelDrawWithoutBegin(lWeaponModel, lWeaponWorld); }
        // ※発光エッジ(BladeEdge)はアウトラインに含めない（後段でアンビアップ描画）
        ShaderEdge_EndNormalPass();

        // トゥーン描画
        setPlayerVP();
        ModelDrawToon(g_pPlayerPreviewBody, bodyWorld);
        ModelDrawToon(g_pPlayerPreviewHead, headWorld);
        ModelDrawToon(g_pPlayerPreviewThruster, thrusterWorld);
        if (rWeaponModel) ModelDrawToon(rWeaponModel, rWeaponWorld);
        if (lWeaponModel) ModelDrawToon(lWeaponModel, lWeaponWorld);
        // 近接装備時は発光パーツをアンビアップで重ねる（ゲーム中と同じ光り方）
        if ((g_RightSelected == WEAPON_MELEE || g_LeftSelected == WEAPON_MELEE) && g_pMeleeEdgePreview)
        {
            const XMFLOAT3 prevAmb = Light_GetAmbient();
            Light_SetAmbient({ 3.0f, 3.0f, 3.0f });
            if (g_RightSelected == WEAPON_MELEE) ModelDrawToon(g_pMeleeEdgePreview, rWeaponWorld);
            if (g_LeftSelected  == WEAPON_MELEE) ModelDrawToon(g_pMeleeEdgePreview, lWeaponWorld);
            Light_SetAmbient(prevAmb);
        }

        // エッジ合成（フルVP復元してから DrawEdge）
        {
            D3D11_VIEWPORT fullVP{};
            fullVP.TopLeftX = 0.0f; fullVP.TopLeftY = 0.0f;
            fullVP.Width    = static_cast<float>(Direct3D_GetBackBufferWidth());
            fullVP.Height   = static_cast<float>(Direct3D_GetBackBufferHeight());
            fullVP.MinDepth = 0.0f; fullVP.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &fullVP);
        }
        ShaderEdge_DrawEdge();

        Direct3D_SetDepthEnable(false);
    }

    //--------------------------------------------------------------------------
    // (4) D3D11 RTV アンバインド（D2D との共存のため）
    //--------------------------------------------------------------------------
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        ctx->Flush();
    }

    //--------------------------------------------------------------------------
    // (5) DirectWrite テキスト描画
    //--------------------------------------------------------------------------
    if (g_pDWLarge && g_pDWBody)
    {
        // 仮想座標系（1600×900）→実ピクセル座標系へスケール
        const float dwScaleX = (float)Direct3D_GetBackBufferWidth()  / SW;
        const float dwScaleY = (float)Direct3D_GetBackBufferHeight() / SH;
        g_pDWLarge->SetScale(dwScaleX, dwScaleY);
        g_pDWBody->SetScale(dwScaleX, dwScaleY);

        char buf[128];

        // ── 左パネル: R-ARM ───────────────────────────────
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = (g_ActivePanel == PANEL_RARM)
                ? D2D1::ColorF(0.4f, 0.85f, 1.0f, 1.0f)
                : D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("R-ARM", LEFT_X + 12.0f, LP_RARM_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            const float iy   = LP_ITEM_START_R + i * LP_ITEM_SPACING + LP_ITEM_TEXT_OFF;
            const bool  cur  = (g_ActivePanel == PANEL_RARM && i == g_RightCursor); // ホバー中
            const bool  eqp  = (i == g_RightSelected);                              // 確定・装備中
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = (cur || eqp) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
            fd.Color = cur ? D2D1::ColorF(1,1,1,1)
                     : eqp ? D2D1::ColorF(0.55f, 1.0f, 0.65f, 1.0f)   // 確定＝緑
                           : D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
            g_pDWBody->SetFont(&fd);
            const char* mark = cur ? "> " : (eqp ? "* " : "  ");
            const std::string lbl = std::string(mark) + k_WeaponDefs[i].name;
            g_pDWBody->DrawString(lbl, LEFT_X + 10.0f, iy, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // ── 左パネル: L-ARM ───────────────────────────────
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = (g_ActivePanel == PANEL_LARM)
                ? D2D1::ColorF(0.4f, 0.85f, 1.0f, 1.0f)
                : D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("L-ARM", LEFT_X + 12.0f, LP_LARM_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            const float iy   = LP_ITEM_START_L + i * LP_ITEM_SPACING + LP_ITEM_TEXT_OFF;
            const bool  cur  = (g_ActivePanel == PANEL_LARM && i == g_LeftCursor); // ホバー中
            const bool  eqp  = (i == g_LeftSelected);                             // 確定・装備中
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = (cur || eqp) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
            fd.Color = cur ? D2D1::ColorF(1,1,1,1)
                     : eqp ? D2D1::ColorF(0.55f, 1.0f, 0.65f, 1.0f)   // 確定＝緑
                           : D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
            g_pDWBody->SetFont(&fd);
            const char* mark = cur ? "> " : (eqp ? "* " : "  ");
            const std::string lbl = std::string(mark) + k_WeaponDefs[i].name;
            g_pDWBody->DrawString(lbl, LEFT_X + 10.0f, iy, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // ── 準備完了（READY）ボタンのラベル ───────────────────
        {
            const bool ready = (CalcRemaining() >= 0);
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 26.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = ready ? D2D1::ColorF(0.90f, 1.0f, 0.92f, 1.0f)
                             : D2D1::ColorF(1.0f, 0.70f, 0.70f, 1.0f);
            g_pDWLarge->SetFont(&fd);
        }
        g_pDWLarge->DrawString("READY",
            LP_READY_X + 80.0f, LP_READY_Y + 12.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);

        // ── センターパネル ─────────────────────────────────
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 30.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = D2D1::ColorF(0.4f, 0.85f, 1.0f, 1.0f);
            g_pDWLarge->SetFont(&fd);
        }
        g_pDWLarge->DrawString(wd.name, CNT_X + 20.0f, CP_NAME_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 16.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("PARTS INFO", CNT_X + 20.0f, CP_INFO_LABEL_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.Color = D2D1::ColorF(0.85f, 0.85f, 0.85f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("Damage",    CP_BAR_X_LABEL, CP_BAR_ROW1_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);
        g_pDWBody->DrawString("FireRate",  CP_BAR_X_LABEL, CP_BAR_ROW2_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);
        g_pDWBody->DrawString("Explosion", CP_BAR_X_LABEL, CP_BAR_ROW3_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);

        snprintf(buf, sizeof(buf), "%d", wd.damage);
        g_pDWBody->DrawString(buf, CP_VAL_X, CP_BAR_ROW1_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);

        if (wd.fireInterval > 0.0f) snprintf(buf, sizeof(buf), "%.2fs", wd.fireInterval);
        else                        snprintf(buf, sizeof(buf), "---");
        g_pDWBody->DrawString(buf, CP_VAL_X, CP_BAR_ROW2_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);

        if (wd.explosionR > 0.0f) snprintf(buf, sizeof(buf), "%.1fm", wd.explosionR);
        else                      snprintf(buf, sizeof(buf), "---");
        g_pDWBody->DrawString(buf, CP_VAL_X, CP_BAR_ROW3_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);

        // 説明文
        {
            FontData fd2{};
            fd2.font = Font::Meiryo; fd2.fontSize = 17.0f;
            fd2.Color = D2D1::ColorF(0.65f, 0.65f, 0.65f, 1.0f);
            g_pDWBody->SetFont(&fd2);
        }
        g_pDWBody->DrawString(std::wstring(wd.description),
            CP_BAR_X_LABEL, CP_BAR_ROW3_Y + 55.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);

        // 値段（コスト）― 説明文の下に配置
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = D2D1::ColorF(1.0f, 0.75f, 0.2f, 1.0f);  // アンバー
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("COST",  CP_BAR_X_LABEL, CP_BAR_ROW3_Y + 120.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);
        snprintf(buf, sizeof(buf), "%dc", wd.cost);
        g_pDWBody->DrawString(buf,     CP_VAL_X,       CP_BAR_ROW3_Y + 120.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);

        // ── 右パネル ──────────────────────────────────────
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 36.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            g_pDWLarge->SetFont(&fd);
        }
        g_pDWLarge->DrawString("ASSEMBLY", RGT_X + 20.0f, RP_TITLE_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.Color = D2D1::ColorF(0.35f, 0.35f, 0.35f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }

        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 20.0f;
            fd.Color = D2D1::ColorF(0.7f, 0.9f, 1.0f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        snprintf(buf, sizeof(buf), "R-ARM : %s", k_WeaponDefs[g_RightSelected].name);
        g_pDWBody->DrawString(buf, RGT_X + 20.0f, 680.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);
        snprintf(buf, sizeof(buf), "L-ARM : %s", k_WeaponDefs[g_LeftSelected].name);
        g_pDWBody->DrawString(buf, RGT_X + 20.0f, 710.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);

        const int  remaining  = CalcRemaining();
        const bool overBudget = (remaining < 0);
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 24.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = overBudget
                ? D2D1::ColorF(1.0f, 0.2f, 0.2f, 1.0f)
                : D2D1::ColorF(0.3f, 1.0f, 0.5f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        snprintf(buf, sizeof(buf), "CREDIT  %dc", remaining);
        g_pDWBody->DrawString(buf, RGT_X + 20.0f, RP_CREDIT_Y, D2D1_DRAW_TEXT_OPTIONS_NONE);

        if (overBudget)
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 16.0f;
            fd.Color = D2D1::ColorF(1.0f, 0.3f, 0.3f, 1.0f);
            g_pDWBody->SetFont(&fd);
            g_pDWBody->DrawString("BUDGET EXCEEDED",
                RGT_X + 20.0f, RP_CREDIT_Y + 30.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // スケールをリセット
        g_pDWLarge->SetScale(1.0f, 1.0f);
        g_pDWBody->SetScale(1.0f, 1.0f);
    }

    //--------------------------------------------------------------------------
    // (6) D3D11 RTV 再バインド（Fade_Draw 等の後続処理用）
    //--------------------------------------------------------------------------
    Direct3D_BindMainRenderTarget();
    Sprite_Begin();
}

//==============================================================================
// Getter
//==============================================================================
WeaponID AssemblyScreen_GetRightWeapon()     { return static_cast<WeaponID>(g_RightSelected); }
WeaponID AssemblyScreen_GetLeftWeapon()      { return static_cast<WeaponID>(g_LeftSelected);  }
int      AssemblyScreen_GetRemainingCredits(){ return CalcRemaining(); }
bool     AssemblyScreen_WasCancelled()       { return g_Cancelled; }

void AssemblyScreen_SetShopMode(bool on, int budget)
{
    g_ShopMode   = on;
    g_ShopBudget = budget;
}

bool AssemblyScreen_IsShopMode() { return g_ShopMode; }

void AssemblyScreen_SetDefaults(WeaponID right, WeaponID left)
{
    g_DefaultRight = static_cast<int>(right);
    g_DefaultLeft  = static_cast<int>(left);

    // カーソル・確定武器も即時更新
    //（Initialize() が呼ばれない QuickStart でも正しい値を返せるように）
    g_RightCursor   = g_DefaultRight;
    g_LeftCursor    = g_DefaultLeft;
    g_RightSelected = g_DefaultRight;
    g_LeftSelected  = g_DefaultLeft;
}
