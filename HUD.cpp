/*==============================================================================

   HUD [HUD.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
■HUDを管理する
・基本的にUIとの比率は1600*900基準でどうにかする
==============================================================================*/

#include "HUD.h"
#include "Player.h"
#include "WeaponDef.h"
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "model.h"
#include "ModelToon.h"
#include "ShaderToon.h"
#include "shader3d.h"
#include "light.h"
#include "player_camera.h"
#include "SciFiUI.h"
#include "game.h"
#include <algorithm>
#include <d2d1helper.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstdio>

using namespace DirectX;

//==============================================================================
// HUD 用テクスチャID
//==============================================================================

// HPバー本体（白テクスチャを色変更して使用）
static int s_BarTexID = -1;

// HPフレーム装飾用
static int s_FrameTexID = -1;

// ATK / SPEED アイテムアイコン
static int s_TexAtk = -1;
static int s_TexSpeed = -1;

//==============================================================================
// 弾種モード表示（通常弾 / ビーム）
//==============================================================================

// モード切替表示用アイコン
static int s_TexBeam = -1;
static int s_TexNormal = -1;

// モード表示の残り時間（秒）
static double s_ModeTimer = 0.0;

// 現在の表示モード（true: ビーム / false: 通常弾）
static bool s_IsBeamMode = false;

// モードアイコン表示時間
static constexpr double MODE_DISPLAY_DURATION = 2.0;

// モードアイコン描画サイズ
static const float MODE_ICON_SIZE = 200.0f;

//==============================================================================
// HPバー / フレーム レイアウト定義
//==============================================================================

// フレーム全体の横幅（テクスチャ基準の半分）
static const float FRAME_W = 792.0f * 0.5f;

// フレーム内余白（上・左・右）
static const float FRAME_MARGIN_TOP = 40.0f * 0.5f;
static const float FRAME_MARGIN_LEFT = 120.0f * 0.5f;
static const float FRAME_MARGIN_RIGHT = 30.0f * 0.5f;

// HPフレームの描画基準座標（左上）
static const float HP_FRAME_X = 16.0f;
static const float HP_FRAME_Y = 50.0f;

// HPバー実際の描画座標（フレーム余白を考慮）
static const float HP_BAR_X = HP_FRAME_X + FRAME_MARGIN_LEFT;
static const float HP_BAR_Y = HP_FRAME_Y + 22 + FRAME_MARGIN_TOP;

// HPバーの有効描画サイズ
static const float BAR_WIDTH = FRAME_W - FRAME_MARGIN_LEFT - FRAME_MARGIN_RIGHT;
static const float BAR_HEIGHT = 235.0f * 0.5f - 30 - FRAME_MARGIN_TOP * 2.0f;

//==============================================================================
// ステータスアイテム所持数
//==============================================================================

// 表示上の最大アイコン数
static const int ICON_MAX = 64;//スタック制にしたので使わない

// 現在の ATK / SPEED スタック数
static int s_AtkCount = 0;
static int s_SpeedCount = 0;

//==============================================================================
// 照準（クロスヘア）
//==============================================================================

// 弾種ごとの照準テクスチャ
static int s_TexSightBeam = -1;
static int s_TexSightNormal = -1;

// 照準描画サイズ
static const float SIGHT_SIZE = 320.0f;

//==============================================================================
// 左下：ステータスUI（スタック表示）ATK
//==============================================================================

// フレーム / ステータスアイコン / アイテムアイコン
static int s_TexStackATTACKFrame = -1;
static int s_TexStackATTACKStat = -1;
static int s_TexStackATTACKItem = -1;

//==============================================================================
// 左下：ステータスUI（スタック表示）SPEED
//==============================================================================

// フレーム / ステータスアイコン / アイテムアイコン
static int s_TexStackSPEEDFrame = -1;
static int s_TexStackSPEEDStat = -1;
static int s_TexStackSPEEDItem = -1;

//==============================================================================
// 数量表示（×N / 数字）
//==============================================================================

// × 記号テクスチャ
static int s_TexStackX = -1;

// 0〜9 横一列の数字テクスチャ
static int s_TexDigits = -1;

//==============================================================================
// HUD スケーリング基準解像度
//==============================================================================

// この解像度を基準に UI スケールを算出
static constexpr float HUD_BASE_W = 1600.0f;
static constexpr float HUD_BASE_H = 900.0f;

//==============================================================================
// スタックUI レイアウト定義（基準サイズ）
//==============================================================================

// ステータスアイコンサイズ
static constexpr float STACK_STAT_SIZE = 96.0f;

// フレームサイズ
static constexpr float STACK_FRAME_W = 320.0f;
static constexpr float STACK_FRAME_H = 154.0f;

//==============================================================================
// スタックUI 位置・間隔
//==============================================================================

// 横方向オフセット（未使用・調整用）
static constexpr float STACK_GAP_X = -1.0f;

// 画面左 / 下からの基準マージン
static constexpr float STACK_MARGIN_L = 32.0f;
static constexpr float STACK_MARGIN_B = 32.0f;

// ATK 行と SPEED 行の縦間隔
static constexpr float STACK_ROW_GAP_Y = 4.0f;

//==============================================================================
// フレーム内部の配置余白
//==============================================================================

// フレーム左上からの内側余白
static constexpr float STACK_INNER_PAD_X = 50.0f;
static constexpr float STACK_INNER_PAD_Y = 48.0f;

//==============================================================================
// スタックアイテム・数量表示サイズ
//==============================================================================

// アイテムアイコンサイズ

static constexpr float STACK_ITEM_SIZE = 64.0f;

// × 記号サイズ
static constexpr float STACK_X_SIZE_W = 60.0f;
static constexpr float STACK_X_SIZE_H = 70.0f;



//==============================================================================
// スタック内要素の間隔
//==============================================================================

// アイテム同士の横間隔
static constexpr float STACK_ITEM_GAP_X = 10.0f;

// × と数字の間隔
static constexpr float STACK_X_GAP_X = 0.0f;

//==============================================================================
// 数字描画スケール
//==============================================================================

// 数量表示（×N）の数字拡大率
static constexpr float STACK_DIGIT_SCALE = 0.75f;

//==============================================================================
// 新HUD用 DirectWrite インスタンス
// ・HUD_Initialize / HUD_Finalize で生成・破棄
//==============================================================================
static DirectWrite* s_pDW_Large    = nullptr;  // 大フォント（HP数値）
static DirectWrite* s_pDW_Small    = nullptr;  // 小フォント（ラベル・武器名・速度など）
static DirectWrite* s_pDW_GameOver = nullptr;  // GAME OVER テキスト

//==============================================================================
// HP カウントダウンアニメーション
//==============================================================================
static float s_HpDisplayed = -1.0f;  // 表示用HP（実HPに向けて毎フレーム近づく）

//==============================================================================
// 武器ミニプレビュー用
//==============================================================================
static MODEL* s_pWeaponPreviewModels[WEAPON_COUNT] = {};  // 各武器のプレビューモデル
static MODEL* s_pMeleeEdgePreview = nullptr;              // 近接の発光パーツ（Blade に重ねる）

//==============================================================================
// HUDデザイン切り替えフラグ
//==============================================================================
static bool s_UseNewDesign = true;

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
static float HUD_ComputeUIScale(float screenW, float screenH);
static void HUD_DrawAttackStackUI(float scale, float screenW, float screenH);
static void HUD_DrawSpeedStackUI(float scale, float screenW, float screenH);

static void HUD_DrawNumberLeftScaled(int digitTexId, int value, float x, float y, float scale);
static float HUD_GetDigitW(int digitTexId);
static float HUD_GetDigitH(int digitTexId);

static void HUD_DrawLegacy();
static void HUD_DrawNew();

//==============================================================================
// UI Scale 計算
// 画面解像度に対して HUD を等倍縮放するための共通スケールを算出する
// ・基準解像度（HUD_BASE_W / HUD_BASE_H）を元に
// ・横/縦のうち小さい方を採用することで縦横比を維持
//==============================================================================
static float HUD_ComputeUIScale(float screenW, float screenH)
{
    const float sx = (HUD_BASE_W > 0.0f) ? (screenW / HUD_BASE_W) : 1.0f;
    const float sy = (HUD_BASE_H > 0.0f) ? (screenH / HUD_BASE_H) : 1.0f;

    // UI が画面外にはみ出ないよう、小さいスケールを採用
    return (sx < sy) ? sx : sy;
}

//==============================================================================
// 数字テクスチャ 1 桁分の横幅取得
// digits_0to9.png は「0〜9 が横一列」の前提
//==============================================================================
static float HUD_GetDigitW(int digitTexId)
{
    const float w = (float)Texture_Width(digitTexId);
    return (w > 0.0f) ? (w / 10.0f) : 0.0f;
}

//==============================================================================
// 数字テクスチャの高さ取得
//==============================================================================
static float HUD_GetDigitH(int digitTexId)
{
    const float h = (float)Texture_Height(digitTexId);
    return (h > 0.0f) ? h : 0.0f;
}

//==============================================================================
// 左寄せ数字描画（0-9 横一列テクスチャ対応）
// ・value を 10 進数分解し
// ・左から順に Sprite_Draw で描画
// ・スケール指定対応
//==============================================================================
static void HUD_DrawNumberLeftScaled(
    int digitTexId,
    int value,
    float x,
    float y,
    float scale)
{
    // テクスチャ未ロード時は描画しない
    if (digitTexId < 0) return;

    // マイナス値対策
    if (value < 0) value = 0;

    const float srcW = HUD_GetDigitW(digitTexId);
    const float srcH = HUD_GetDigitH(digitTexId);

    if (srcW <= 0.0f || srcH <= 0.0f) return;

    const float dstW = srcW * scale;
    const float dstH = srcH * scale;

    // 値が 0 の場合は 0 のみ描画
    if (value == 0)
    {
        Sprite_Draw(
            digitTexId,
            x, y,
            dstW, dstH,
            srcW * 0.0f, 0.0f,
            srcW, srcH,
            XMFLOAT4(1, 1, 1, 1));
        return;
    }

    // 数字を 1 桁ずつ分解（最大 16 桁）
    int buf[16] = { 0 };
    int n = 0;

    while (value > 0 && n < 16)
    {
        buf[n++] = value % 10;
        value /= 10;
    }

    // 上位桁から左→右に描画
    for (int i = n - 1; i >= 0; --i)
    {
        const int d = buf[i];

        Sprite_Draw(
            digitTexId,
            x, y,
            dstW, dstH,
            srcW * (float)d, 0.0f,
            srcW, srcH,
            XMFLOAT4(1, 1, 1, 1));

        x += dstW;
    }
}

//==============================================================================
// HUD 初期化
// ・使用する全 HUD テクスチャのロード
// ・スタック UI / 照準 / モード表示用の準備
// 新UIを実装したため、旧UI用のテクスチャも残しているが、将来的には整理する予定
//==============================================================================
void HUD_Initialize()
{
    // HPバー・フレーム
    s_BarTexID = Texture_Load(L"resource/texture/white.png");
    s_FrameTexID = Texture_Load(L"resource/texture/hpframe2.png");

    // アイテムアイコン
    s_TexAtk = Texture_Load(L"resource/texture/item_atk.png");
    s_TexSpeed = Texture_Load(L"resource/texture/item_speed.png");

    // 弾種モード表示
    s_TexBeam = Texture_Load(L"resource/texture/item_beam.png");
    s_TexNormal = Texture_Load(L"resource/texture/item_bullet.png");
    s_ModeTimer = 0.0;

    // 照準（クロスヘア）
    s_TexSightBeam = Texture_Load(L"resource/texture/sight_beam.png");
    s_TexSightNormal = Texture_Load(L"resource/texture/sight_bullet.png");

    // フレーム未ロード時の保険
    if (s_FrameTexID < 0)
        s_FrameTexID = s_BarTexID;

    // スタック数初期化
    s_AtkCount = 0;
    s_SpeedCount = 0;
    s_HpDisplayed = -1.0f;

    // ATK スタック UI
    s_TexStackATTACKFrame = Texture_Load(L"resource/texture/ATTACK_Frame.png");
    s_TexStackATTACKStat = Texture_Load(L"resource/texture/ATTACK_ICON.png");
    s_TexStackATTACKItem = Texture_Load(L"resource/texture/item_atk.png");

    // SPEED スタック UI
    s_TexStackSPEEDFrame = Texture_Load(L"resource/texture/SPEED_Frame.png");
    s_TexStackSPEEDStat = Texture_Load(L"resource/texture/SPEED_ICON.png");
    s_TexStackSPEEDItem = Texture_Load(L"resource/texture/item_speed.png");

    // 数量表示用
    s_TexStackX = Texture_Load(L"resource/texture/ui_x.png");
    s_TexDigits = Texture_Load(L"resource/texture/digits_0to9.png");

    // ── 新HUD用 DirectWrite（2回目以降は再生成しない）────────
    if (!s_pDW_Large)
    {
        static FontData fdLarge;
        fdLarge.font          = Font::DSEG7;
        fdLarge.fontFilePath  = L"resource/fonts/DSEG7Modern-Regular.ttf";
        fdLarge.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdLarge.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdLarge.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdLarge.fontSize      = 52.0f;
        fdLarge.localeName    = L"en-us";
        fdLarge.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdLarge.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        s_pDW_Large = new DirectWrite(&fdLarge);
        s_pDW_Large->Init();
    }
    if (!s_pDW_Small)
    {
        static FontData fdSmall;
        fdSmall.font          = Font::Arial;
        fdSmall.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdSmall.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdSmall.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdSmall.fontSize      = 24.0f;
        fdSmall.localeName    = L"en-us";
        fdSmall.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdSmall.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        s_pDW_Small = new DirectWrite(&fdSmall);
        s_pDW_Small->Init();
        // 小フォントは単一行ラベル専用（ARM名・武器名・ATK等）。
        // 「MULTI MISSILE」のような長い武器名が2行に折り返して
        // 下段の ATK 表示に被るのを防ぐため、折り返しを無効化する。
        s_pDW_Small->SetWordWrapping(false);
    }

    // GAME OVER テキスト用
    if (!s_pDW_GameOver)
    {
        static FontData fdGO;
        fdGO.font          = Font::MeiryoUI;
        fdGO.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdGO.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdGO.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdGO.fontSize      = 108.0f;
        fdGO.localeName    = L"ja-jp";
        fdGO.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdGO.Color         = D2D1::ColorF(1.0f, 0.1f, 0.1f, 1.0f);
        s_pDW_GameOver = new DirectWrite(&fdGO);
        s_pDW_GameOver->Init();
    }

    // 武器ミニプレビュー用モデル（古いものを解放してから再ロード）
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (s_pWeaponPreviewModels[i]) { ModelRelease(s_pWeaponPreviewModels[i]); s_pWeaponPreviewModels[i] = nullptr; }
        s_pWeaponPreviewModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
    }
    // 近接の発光パーツ（BladeEdge）を Blade と同スケールでロード（重ね描画用）
    if (s_pMeleeEdgePreview) { ModelRelease(s_pMeleeEdgePreview); s_pMeleeEdgePreview = nullptr; }
    s_pMeleeEdgePreview = ModelLoad("resource/Models/BladeEdge.fbx", k_WeaponDefs[WEAPON_MELEE].scale);
}

//==============================================================================
// Finalize
//==============================================================================
void HUD_Finalize()
{
    s_BarTexID = -1;
    s_FrameTexID = -1;
    s_TexAtk = -1;
    s_TexSpeed = -1;

    s_AtkCount = 0;
    s_SpeedCount = 0;

    s_TexBeam = -1;
    s_TexNormal = -1;
    s_ModeTimer = 0.0;

    s_TexSightBeam = -1;
    s_TexSightNormal = -1;

    s_TexStackATTACKFrame = -1;
    s_TexStackATTACKStat = -1;
    s_TexStackATTACKItem = -1;

    s_TexStackSPEEDFrame = -1;
    s_TexStackSPEEDStat = -1;
    s_TexStackSPEEDItem = -1;

    s_TexStackX = -1;
    s_TexDigits = -1;

    // 新HUD用 DirectWrite 解放
    if (s_pDW_Large)    { s_pDW_Large->Release();    delete s_pDW_Large;    s_pDW_Large    = nullptr; }
    if (s_pDW_Small)    { s_pDW_Small->Release();    delete s_pDW_Small;    s_pDW_Small    = nullptr; }
    if (s_pDW_GameOver) { s_pDW_GameOver->Release(); delete s_pDW_GameOver; s_pDW_GameOver = nullptr; }

    // 武器ミニプレビュー用モデルの解放
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        ModelRelease(s_pWeaponPreviewModels[i]);
        s_pWeaponPreviewModels[i] = nullptr;
    }
    ModelRelease(s_pMeleeEdgePreview);
    s_pMeleeEdgePreview = nullptr;
}

//==============================================================================
// AddCollectedItem
//==============================================================================
void HUD_AddCollectedItem(ItemType type)
{
    switch (type)
    {
    case ItemType::ATK_UP:
        if (s_AtkCount < ICON_MAX) s_AtkCount++;
        break;
    case ItemType::SPEED_UP:
        if (s_SpeedCount < ICON_MAX) s_SpeedCount++;
        break;
    default:
        break;
    }
}

//==============================================================================
// Draw（デザイン切り替え）
//==============================================================================
void HUD_Draw()
{
    if (s_UseNewDesign)
        HUD_DrawNew();
    else
        HUD_DrawLegacy();
}

//==============================================================================
// 現行HUD
//==============================================================================
static void HUD_DrawLegacy()
{
    if (s_BarTexID < 0) return;

    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);

    Sprite_Begin();

    const float frameTexW = (float)Texture_Width(s_FrameTexID);
    const float frameTexH = (float)Texture_Height(s_FrameTexID);
    const float frameRatio = (frameTexH > 0.0f) ? frameTexW / frameTexH : 1.0f;
    const float frameDrawH = FRAME_W / frameRatio;
    const float ENERGY_FRAME_Y = HP_FRAME_Y + frameDrawH + 20.0f;
    const float ENERGY_BAR_Y = ENERGY_FRAME_Y + 21 + FRAME_MARGIN_TOP;

    // HP フレーム＋バー
    {
        Sprite_Draw(s_BarTexID, HP_FRAME_X, HP_FRAME_Y, FRAME_W, frameDrawH,
            { 0.0f, 0.0f, 0.0f, 0.0f });

        const int   currentHP = Player_GetHP();
        const int   maxHP = Player_GetMaxHP();
        const float hpRatio = (maxHP > 0) ? (float)currentHP / maxHP : 0.0f;
        const float currentBarWidth = BAR_WIDTH * hpRatio;

        XMFLOAT4 hpColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        if (hpRatio < 0.75f) hpColor = { 0.5f, 1.0f, 0.0f, 1.0f };
        if (hpRatio < 0.5f)  hpColor = { 1.0f, 1.0f, 0.0f, 1.0f };
        if (hpRatio < 0.25f) hpColor = { 1.0f, 0.0f, 0.0f, 1.0f };

        if (currentBarWidth > 0.1f)
            Sprite_Draw(s_BarTexID, HP_BAR_X, HP_BAR_Y, currentBarWidth, BAR_HEIGHT, hpColor);

        const float emptyW = BAR_WIDTH - currentBarWidth;
        if (emptyW > 0.1f)
            Sprite_Draw(s_BarTexID, HP_BAR_X + currentBarWidth, HP_BAR_Y,
                emptyW, BAR_HEIGHT, { 0.2f, 0.0f, 0.0f, 0.8f });

        Sprite_Draw(s_FrameTexID, HP_FRAME_X, HP_FRAME_Y, FRAME_W, frameDrawH,
            { 2.0f, 2.0f, 2.0f, 2.0f });
    }

    // エネルギー フレーム＋バー
    {
        Sprite_Draw(s_BarTexID, HP_FRAME_X, ENERGY_FRAME_Y, FRAME_W, frameDrawH,
            { 0.0f, 0.0f, 0.0f, 0.0f });

        const float currentEnergy = Player_GetBeamEnergy();
        const float maxEnergy = Player_GetBeamEnergyMax();
        const float energyRatio = (maxEnergy > 0.0f) ? currentEnergy / maxEnergy : 0.0f;
        const float currentBarWidth = BAR_WIDTH * energyRatio;

        XMFLOAT4 energyColor = { 0.0f, 0.8f, 1.0f, 1.0f };
        if (energyRatio < 0.5f)  energyColor = { 0.0f, 0.5f, 0.8f, 1.0f };
        if (energyRatio < 0.25f) energyColor = { 0.0f, 0.3f, 0.6f, 1.0f };

        if (currentBarWidth > 0.1f)
            Sprite_Draw(s_BarTexID, HP_BAR_X, ENERGY_BAR_Y, currentBarWidth, BAR_HEIGHT, energyColor);

        const float emptyW = BAR_WIDTH - currentBarWidth;
        if (emptyW > 0.1f)
            Sprite_Draw(s_BarTexID, HP_BAR_X + currentBarWidth, ENERGY_BAR_Y,
                emptyW, BAR_HEIGHT, { 0.0f, 0.1f, 0.2f, 0.8f });

        Sprite_Draw(s_FrameTexID, HP_FRAME_X, ENERGY_FRAME_Y, FRAME_W, frameDrawH,
            { 2.0f, 2.0f, 2.0f, 2.0f });
    }

    //--------------------------------------------------------------------------
    // 左下：ステータスUI（スタック表示）
    //--------------------------------------------------------------------------
    {
        const float scale = HUD_ComputeUIScale(SPRITE_SCREEN_W, SPRITE_SCREEN_H);
        HUD_DrawAttackStackUI(scale, SPRITE_SCREEN_W, SPRITE_SCREEN_H);
        HUD_DrawSpeedStackUI(scale, SPRITE_SCREEN_W, SPRITE_SCREEN_H);
    }

    // モード切り替え表示（画面上部中央、常時表示）
    {
        const float iconX = SPRITE_SCREEN_W * 0.5f - MODE_ICON_SIZE * 0.5f;
        const float iconY = 20.0f;

        const int texID = s_IsBeamMode ? s_TexBeam : s_TexNormal;
        if (texID >= 0)
        {
            Sprite_Draw(texID, iconX, iconY, MODE_ICON_SIZE, MODE_ICON_SIZE,
                { 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // サイト（照準）表示（画面中央、モードで切り替え）
    {
        const float sightX = SPRITE_SCREEN_W * 0.5f - SIGHT_SIZE * 0.5f;
        const float sightY = SPRITE_SCREEN_H * 0.5f - SIGHT_SIZE * 0.5f;

        const int sightTex = s_IsBeamMode ? s_TexSightBeam : s_TexSightNormal;

        if (sightTex >= 0)
        {
            Sprite_Draw(
                sightTex,
                sightX,
                sightY,
                SIGHT_SIZE,
                SIGHT_SIZE,
                { 1.0f, 1.0f, 1.0f, 1.0f }
            );
        }
    }

    Direct3D_SetDepthEnable(true);
}

//==============================================================================
// 新HUD ― SF調レイアウト（SciFiUI で描く。テクスチャは武器・強化アイコンのみ）
//
//  ┌──────────────────────────────────────────┐
//  │[左上] APパネル        [上中央] 作戦表示(MissionHud)   [右上] ミニマップ │
//  │                       [上中央] 大型兵器の体力                       │
//  │[左端] ENゲージ（縦）                                              │
//  │                    [中央] 照準（手続き描画）                      │
//  │[左下] 速度                                     [右下] 武器 / 強化 │
//  └──────────────────────────────────────────┘
//==============================================================================
namespace
{
    float s_HudTime         = 0.0f;    // 点滅・回転用の経過時間
    float s_BossHpDisplayed = -1.0f;   // 大型兵器の体力の表示値（減少をゆっくり追う）

    // 縦の区切りゲージ（下から満ちる）
    void VerticalSegments(float x, float y, float w, float h, int segments, float ratio,
                          const XMFLOAT4& on, const XMFLOAT4& off)
    {
        const float gap  = 2.0f;
        const float segH = (h - gap * (segments - 1)) / segments;
        const int   lit  = static_cast<int>(ratio * segments + 0.999f);
        for (int i = 0; i < segments; ++i)
        {
            const float sy = y + h - (i + 1) * segH - i * gap;
            SciFiUI::Fill(x, sy, w, segH, (i < lit) ? on : off);
        }
    }
}

static void HUD_DrawNew()
{
    using namespace SciFiUI;

    const float SW = (float)SPRITE_SCREEN_W;   // 1600
    const float SH = (float)SPRITE_SCREEN_H;   // 900
    const float BOTTOM_MARGIN = 52.0f;         // 下部ヒントバー分のオフセット

    const XMFLOAT4 kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };

    BeginSprites();

    // ================================================================
    // 1. APパネル（左上）
    // ================================================================
    const int   hp          = Player_GetHP();
    const int   hpMax       = Player_GetMaxHP();
    const int   displayedHP = (int)s_HpDisplayed;
    const float hpRatio     = (hpMax > 0) ? std::clamp((float)hp / (float)hpMax, 0.0f, 1.0f) : 0.0f;
    const float hpShown     = (hpMax > 0) ? std::clamp(s_HpDisplayed / (float)hpMax, 0.0f, 1.0f) : 0.0f;
    const bool  hpLow       = hpRatio <= 0.30f;
    const bool  blinkOn     = fmodf(s_HudTime, 0.5f) < 0.28f;
    const XMFLOAT4 apCol    = hpLow ? kRed : kCyan;

    const float AP_X = 16.0f, AP_Y = 14.0f, AP_W = 380.0f, AP_H = 112.0f;
    Panel(AP_X, AP_Y, AP_W, AP_H, kPanel, WithAlpha(apCol, 0.55f));
    Brackets(AP_X - 4.0f, AP_Y - 4.0f, AP_W + 8.0f, AP_H + 8.0f, 10.0f, WithAlpha(apCol, 0.9f));
    Fill(AP_X + 1.0f, AP_Y + 24.0f, AP_W - 2.0f, 1.0f, WithAlpha(apCol, 0.25f));

    // ゲージ：減った分は白く残し、表示値に合わせて縮める
    const float AP_BAR_X = AP_X + 14.0f, AP_BAR_Y = AP_Y + 84.0f, AP_BAR_W = AP_W - 28.0f, AP_BAR_H = 14.0f;
    SegmentBar(AP_BAR_X, AP_BAR_Y, AP_BAR_W, AP_BAR_H, 32, hpShown,
               WithAlpha(kWhite, 0.55f), WithAlpha(apCol, 0.12f));
    SegmentBar(AP_BAR_X, AP_BAR_Y, AP_BAR_W, AP_BAR_H, 32, hpRatio,
               WithAlpha(apCol, (hpLow && !blinkOn) ? 0.45f : 0.95f), { 0.0f, 0.0f, 0.0f, 0.0f });
    Ticks(AP_BAR_X, AP_BAR_Y + AP_BAR_H + 3.0f, AP_BAR_W, 32, 8, WithAlpha(apCol, 0.35f));

    // ================================================================
    // 2. ENゲージ（左端・縦）
    // ================================================================
    const float bEnergy    = Player_GetBeamEnergy();
    const float bEnergyMax = Player_GetBeamEnergyMax();
    const float bRatio     = (bEnergyMax > 0.0f) ? std::clamp(bEnergy / bEnergyMax, 0.0f, 1.0f) : 0.0f;
    const bool  enLow      = bRatio < 0.2f;
    const XMFLOAT4 enCol   = enLow ? kRed : kAmber;

    const float ENE_X = 16.0f, ENE_Y = AP_Y + AP_H + 14.0f, ENE_W = 34.0f, ENE_H = 420.0f;
    Panel(ENE_X, ENE_Y, ENE_W, ENE_H + 44.0f, kPanel, WithAlpha(enCol, 0.5f), 8.0f);
    VerticalSegments(ENE_X + 8.0f, ENE_Y + 10.0f, ENE_W - 16.0f, ENE_H - 10.0f, 40, bRatio,
                     WithAlpha(enCol, (enLow && !blinkOn) ? 0.4f : 0.95f), WithAlpha(enCol, 0.10f));

    // ================================================================
    // 3. 速度（左下）
    // ================================================================
    XMFLOAT3* vel = Player_GetVelocityPtr();
    int speedInt = 0;
    if (vel)
    {
        const float hSpd = sqrtf(vel->x * vel->x + vel->z * vel->z);
        speedInt = (int)(hSpd * 36.0f);  // 単位系に合わせた係数
    }

    const float SPD_W = 230.0f, SPD_H = 60.0f;
    const float SPD_X = 16.0f;
    const float SPD_Y = SH - SPD_H - 14.0f - BOTTOM_MARGIN;
    Panel(SPD_X, SPD_Y, SPD_W, SPD_H, kPanel, WithAlpha(kCyan, 0.5f), 10.0f);
    SegmentBar(SPD_X + 12.0f, SPD_Y + SPD_H - 12.0f, SPD_W - 24.0f, 4.0f, 20,
               std::clamp(speedInt / 900.0f, 0.0f, 1.0f), WithAlpha(kCyan, 0.85f), WithAlpha(kCyan, 0.12f));

    // ================================================================
    // 4. 武器パネル（右下） ― R ARM / L ARM ＋ 3Dミニプレビュー
    // ================================================================
    const float WEP_W      = 300.0f;
    const float WEP_SLOT_H = 124.0f;
    const float WEP_GAP    = 8.0f;
    const float STA_H      = 96.0f;
    const float WEP_X      = SW - WEP_W - 16.0f;
    const float WEP_Y      = SH - 2.0f * (WEP_SLOT_H + WEP_GAP) - STA_H - 14.0f - BOTTOM_MARGIN;

    const float PREV_SZ    = 104.0f;
    const float PREV_PAD_X = 8.0f;
    const float PREV_PAD_Y = (WEP_SLOT_H - PREV_SZ) * 0.5f;

    const int armIdx[2] = { Player_GetRightWeaponIndex(), Player_GetLeftWeaponIndex() };

    for (int arm = 0; arm < 2; ++arm)
    {
        const float slotY = WEP_Y + arm * (WEP_SLOT_H + WEP_GAP);
        Panel(WEP_X, slotY, WEP_W, WEP_SLOT_H, kPanel, WithAlpha(kCyan, 0.5f), 12.0f);
        // プレビューの枠（照準風の四隅）
        Fill(WEP_X + PREV_PAD_X, slotY + PREV_PAD_Y, PREV_SZ, PREV_SZ, WithAlpha(kCyan, 0.05f));
        Brackets(WEP_X + PREV_PAD_X, slotY + PREV_PAD_Y, PREV_SZ, PREV_SZ, 8.0f, WithAlpha(kCyan, 0.6f), 1.0f);
        Fill(WEP_X + PREV_PAD_X + PREV_SZ + 8.0f, slotY + 34.0f, WEP_W - PREV_SZ - 28.0f, 1.0f, WithAlpha(kCyan, 0.25f));
    }

    // 強化アイテムのスタック（武器パネルの下）
    const float STA_Y = WEP_Y + 2.0f * (WEP_SLOT_H + WEP_GAP);
    const float COL_W = WEP_W * 0.5f;
    const float ICON_SZ = 54.0f;
    Panel(WEP_X, STA_Y, WEP_W, STA_H, kPanel, WithAlpha(kAmber, 0.45f), 10.0f);
    Fill(WEP_X + COL_W, STA_Y + 10.0f, 1.0f, STA_H - 20.0f, WithAlpha(kAmber, 0.25f));
    if (s_TexAtk >= 0)
        Sprite_Draw(s_TexAtk, WEP_X + 14.0f, STA_Y + 10.0f, ICON_SZ, ICON_SZ, kWhite);
    if (s_TexSpeed >= 0)
        Sprite_Draw(s_TexSpeed, WEP_X + COL_W + 14.0f, STA_Y + 10.0f, ICON_SZ, ICON_SZ, kWhite);

    // ── 3D ミニプレビュー ──────────────────────────────────────
    // スプライト描画後に depth を復元して 3D 描画し、終わったら depth を戻す
    {
        const float vpScaleX = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
        const float vpScaleY = (float)Direct3D_GetBackBufferHeight() / 900.0f;

        // プレビューカメラ（AssemblyScreen と同じ設定）
        const XMFLOAT3 eyeF3  = { 0.25f, 0.18f, 0.55f };
        const XMVECTOR eyeV   = XMLoadFloat3(&eyeF3);
        const XMVECTOR target = XMVectorZero();
        const XMVECTOR upV    = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        // 角度固定（回転させない）。銃口側をカメラへ→さらに左へ約40°傾けた向き。
        constexpr float PREVIEW_FIXED_DEG = 250.0f;  // 210 + 40
        // 見切れ対策：カメラを引かず、銃（盾・ブレード以外）だけ画面右へ寄せる。
        constexpr float GUN_SHIFT = 0.12f;           // 画面右への移動量（カメラ右方向）
        const XMVECTOR camRight = XMVector3Normalize(
            XMVector3Cross(upV, XMVectorNegate(eyeV)));

        // ゲームシーンが書いた深度値をクリア（モデルが地形に埋まるのを防ぐ）
        Direct3D_ClearDepth();
        Direct3D_SetDepthEnable(true);

        const XMFLOAT3 savedAmbient = Light_GetAmbient();
        Light_SetSpecularWorld(eyeF3, 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
        Light_SetAmbient({ 2.5f, 2.5f, 2.5f });   // プレビュー用に明るく

        for (int arm = 0; arm < 2; ++arm)
        {
            const int wIdx = armIdx[arm];
            if (wIdx < 0 || wIdx >= WEAPON_COUNT) continue;
            MODEL* mdl = s_pWeaponPreviewModels[wIdx];
            if (!mdl) continue;

            const float slotY = WEP_Y + arm * (WEP_SLOT_H + WEP_GAP);
            const float PV_X  = WEP_X + PREV_PAD_X;
            const float PV_Y  = slotY + PREV_PAD_Y;

            const XMMATRIX view = XMMatrixLookAtLH(eyeV, target, upV);
            const XMMATRIX proj = XMMatrixPerspectiveFovLH(
                XMConvertToRadians(45.0f), 1.0f, 0.01f, 100.0f);

            Shader3d_SetViewMatrix(view);
            Shader3d_SetProjectMatrix(proj);
            ShaderToon_SetViewMatrix(view);
            ShaderToon_SetProjectMatrix(proj);

            D3D11_VIEWPORT vp{};
            vp.TopLeftX = PV_X * vpScaleX;
            vp.TopLeftY = PV_Y * vpScaleY;
            vp.Width    = PREV_SZ * vpScaleX;
            vp.Height   = PREV_SZ * vpScaleY;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &vp);

            const bool isShield = (wIdx == WEAPON_SHIELD);
            XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(PREVIEW_FIXED_DEG));
            if (!isShield)
            {
                XMFLOAT3 shift;
                XMStoreFloat3(&shift, camRight * GUN_SHIFT);
                world = world * XMMatrixTranslation(shift.x, shift.y, shift.z);
            }

            ModelDrawToon(mdl, world);
            if (wIdx == WEAPON_MELEE && s_pMeleeEdgePreview)
                ModelDrawToon(s_pMeleeEdgePreview, world);
        }

        // フルビューポートを復元
        D3D11_VIEWPORT fullVP{};
        fullVP.TopLeftX = 0.0f;
        fullVP.TopLeftY = 0.0f;
        fullVP.Width    = (float)Direct3D_GetBackBufferWidth();
        fullVP.Height   = (float)Direct3D_GetBackBufferHeight();
        fullVP.MinDepth = 0.0f;
        fullVP.MaxDepth = 1.0f;
        Direct3D_GetContext()->RSSetViewports(1, &fullVP);

        // プレビューカメラで上書きしたシェーダー行列をゲームカメラに戻す
        // （戻さないと ESC 等で Camera_Update が呼ばれないフレームで
        //   ゲームシーンがHUDカメラで描画されて消えてしまう）
        Player_Camera_ApplyMainViewProj();
        Light_SetAmbient(savedAmbient);
    }

    BeginSprites();   // 3D描画で変わった状態を2D用に戻す

    // ================================================================
    // 5. 照準（中央・手続き描画）
    //    十字の4本線＋中心点＋外周の四隅。ビームモードは琥珀色
    // ================================================================
    const float cx = SW * 0.5f, cy = SH * 0.5f;
    const XMFLOAT4 retCol = s_IsBeamMode ? kAmber : kCyan;
    {
        const float gap = 12.0f, len = 18.0f;
        Fill(cx - gap - len, cy - 1.0f, len, 2.0f, WithAlpha(retCol, 0.9f));
        Fill(cx + gap,       cy - 1.0f, len, 2.0f, WithAlpha(retCol, 0.9f));
        Fill(cx - 1.0f, cy - gap - len, 2.0f, len, WithAlpha(retCol, 0.9f));
        Fill(cx - 1.0f, cy + gap,       2.0f, len * 0.6f, WithAlpha(retCol, 0.9f));
        Diamond(cx, cy, 2.5f, retCol);
        Brackets(cx - 58.0f, cy - 58.0f, 116.0f, 116.0f, 12.0f, WithAlpha(retCol, 0.45f), 1.5f);
        // 外周をゆっくり回る目盛り
        for (int k = 0; k < 4; ++k)
        {
            const float ang = s_HudTime * 0.8f + k * XM_PIDIV2 + XM_PIDIV4;
            LineAngle(cx + cosf(ang) * 74.0f, cy + sinf(ang) * 74.0f, 8.0f, ang, WithAlpha(retCol, 0.5f), 2.0f);
        }
        // 下の目盛り（高度計風）
        Ticks(cx - 60.0f, cy + 84.0f, 120.0f, 12, 6, WithAlpha(retCol, 0.3f));
    }

    // AP低下の警告（画面の縁を赤く点滅）
    if (hpLow && hp > 0)
    {
        const float a = blinkOn ? 0.55f : 0.2f;
        Brackets(5.0f, 5.0f, SW - 10.0f, SH - 10.0f, 80.0f, WithAlpha(kRed, a), 3.0f);   // 画面の縁（パネルに重ならない位置）
        Fill(cx - 110.0f, cy + 110.0f, 220.0f, 26.0f, WithAlpha({ 0.15f, 0.0f, 0.0f, 1.0f }, 0.7f * (blinkOn ? 1.0f : 0.6f)));
        Frame(cx - 110.0f, cy + 110.0f, 220.0f, 26.0f, WithAlpha(kRed, a + 0.3f), 1.0f);
    }

    // ================================================================
    // 6. 大型兵器の体力（上中央。作戦表示パネルの下）
    // ================================================================
    int bossHp = 0, bossMaxHp = 1;
    const wchar_t* bossName = nullptr;
    const bool hasBoss = Game_GetBossStatus(&bossHp, &bossMaxHp, &bossName) && bossMaxHp > 0;
    const float BOSS_X = 400.0f, BOSS_Y = 102.0f, BOSS_W = 800.0f, BOSS_H = 50.0f;
    float bossRatio = 0.0f;
    if (hasBoss)
    {
        bossRatio = std::clamp((float)bossHp / (float)bossMaxHp, 0.0f, 1.0f);
        if (s_BossHpDisplayed < bossRatio) s_BossHpDisplayed = bossRatio;   // 新しいボス・回復
        const float trail = s_BossHpDisplayed;

        Panel(BOSS_X, BOSS_Y, BOSS_W, BOSS_H, kPanel, WithAlpha(kRed, 0.6f), 12.0f);
        Brackets(BOSS_X - 4.0f, BOSS_Y - 4.0f, BOSS_W + 8.0f, BOSS_H + 8.0f, 10.0f, WithAlpha(kRed, 0.9f));
        const float barX = BOSS_X + 12.0f, barY = BOSS_Y + 30.0f, barW = BOSS_W - 24.0f, barH = 10.0f;
        Fill(barX, barY, barW, barH, WithAlpha(kRed, 0.12f));
        Fill(barX, barY, barW * trail, barH, WithAlpha({ 1.0f, 0.85f, 0.7f, 1.0f }, 0.6f));
        Fill(barX, barY, barW * bossRatio, barH, WithAlpha(kRed, 0.95f));
        // 50% の位置（激昂の目安）
        Fill(barX + barW * 0.5f - 1.0f, barY - 3.0f, 2.0f, barH + 6.0f, WithAlpha(kAmber, 0.8f));
        Ticks(barX, barY + barH + 2.0f, barW, 40, 10, WithAlpha(kRed, 0.4f));
    }
    else
    {
        s_BossHpDisplayed = -1.0f;
    }

    // ================================================================
    // 7. 文字
    // ================================================================
    wchar_t buf[64];
    const D2D1_COLOR_F white = D2D1::ColorF(0.94f, 0.98f, 1.0f, 1.0f);

    Text(L"AP", AP_X + 14.0f, AP_Y + 5.0f, 14.0f, ToD2D(apCol), UIFont::Mono, UIAlign::Left, true);
    Text(L"ARMOR POINT", AP_X + 44.0f, AP_Y + 6.0f, 12.0f, ToD2D(apCol, 0.6f), UIFont::Mono);
    Text(hpLow ? L"CRITICAL" : L"STABLE", AP_X + AP_W - 18.0f, AP_Y + 6.0f, 12.0f,
         ToD2D(apCol, hpLow && !blinkOn ? 0.4f : 0.9f), UIFont::Mono, UIAlign::Right, true);
    swprintf_s(buf, L"%d", std::max(0, displayedHP));
    Text(buf, AP_X + 250.0f, AP_Y + 28.0f, 48.0f, hpLow ? ToD2D(kRed) : white, UIFont::Display, UIAlign::Right, true, 1.0f);
    swprintf_s(buf, L"/ %d", hpMax);
    Text(buf, AP_X + 258.0f, AP_Y + 50.0f, 18.0f, ToD2D(apCol, 0.75f), UIFont::Mono, UIAlign::Left, true);

    Text(L"EN", ENE_X + ENE_W * 0.5f, ENE_Y + ENE_H + 6.0f, 14.0f, ToD2D(enCol), UIFont::Mono, UIAlign::Center, true);
    swprintf_s(buf, L"%d", (int)(bRatio * 100.0f + 0.5f));
    Text(buf, ENE_X + ENE_W * 0.5f, ENE_Y + ENE_H + 22.0f, 13.0f, ToD2D(enCol, 0.8f), UIFont::Mono, UIAlign::Center);

    Text(L"SPD", SPD_X + 12.0f, SPD_Y + 6.0f, 13.0f, ToD2D(kCyan, 0.85f), UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"%d", speedInt);
    Text(buf, SPD_X + SPD_W - 62.0f, SPD_Y + 4.0f, 36.0f, white, UIFont::Display, UIAlign::Right, true);
    Text(L"km/h", SPD_X + SPD_W - 56.0f, SPD_Y + 22.0f, 13.0f, ToD2D(kCyan, 0.7f), UIFont::Mono);

    {
        static const wchar_t* ARM_LABEL[2] = { L"R-ARM", L"L-ARM" };
        const float textX = WEP_X + PREV_PAD_X + PREV_SZ + 12.0f;
        for (int arm = 0; arm < 2; ++arm)
        {
            const float slotY = WEP_Y + arm * (WEP_SLOT_H + WEP_GAP);
            const int   wIdx  = armIdx[arm];
            Text(ARM_LABEL[arm], textX, slotY + 10.0f, 14.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
            Text(arm == 0 ? L"R" : L"L", WEP_X + WEP_W - 20.0f, slotY + 8.0f, 16.0f, ToD2D(kCyan, 0.5f),
                 UIFont::Display, UIAlign::Right, true);

            if (wIdx >= 0 && wIdx < WEAPON_COUNT)
            {
                const WeaponDef& def = k_WeaponDefs[wIdx];
                wchar_t name[64] = L"";
                MultiByteToWideChar(CP_UTF8, 0, def.name, -1, name, 64);
                Text(name, textX, slotY + 44.0f, 19.0f, white, UIFont::Body, UIAlign::Left, true);
                swprintf_s(buf, L"ATK %d", def.damage);
                Text(buf, textX, slotY + 86.0f, 15.0f, ToD2D(kAmber), UIFont::Mono, UIAlign::Left, true);
            }
            else
            {
                Text(L"NO WEAPON", textX, slotY + 52.0f, 15.0f, ToD2D(kCyanDim), UIFont::Mono);
            }
        }
    }

    swprintf_s(buf, L"x%d", s_AtkCount);
    Text(buf, WEP_X + 14.0f + ICON_SZ + 8.0f, STA_Y + 14.0f, 30.0f, white, UIFont::Display, UIAlign::Left, true);
    Text(L"ATK UP", WEP_X + COL_W * 0.5f, STA_Y + 70.0f, 13.0f, ToD2D(kAmber), UIFont::Mono, UIAlign::Center, true);
    swprintf_s(buf, L"x%d", s_SpeedCount);
    Text(buf, WEP_X + COL_W + 14.0f + ICON_SZ + 8.0f, STA_Y + 14.0f, 30.0f, white, UIFont::Display, UIAlign::Left, true);
    Text(L"SPD UP", WEP_X + COL_W * 1.5f, STA_Y + 70.0f, 13.0f, ToD2D(kAmber), UIFont::Mono, UIAlign::Center, true);

    if (s_ModeTimer > 0.0)
    {
        const float a = (float)std::min(1.0, s_ModeTimer / 0.4);
        Text(s_IsBeamMode ? L"MODE : BEAM" : L"MODE : NORMAL", cx, cy - 104.0f, 15.0f, ToD2D(retCol, a),
             UIFont::Mono, UIAlign::Center, true);
    }

    if (hpLow && hp > 0)
        Text(L"WARNING  AP LOW", cx, cy + 113.0f, 15.0f, ToD2D(kRed, blinkOn ? 1.0f : 0.6f), UIFont::Mono, UIAlign::Center, true);

    if (hasBoss)
    {
        Text(L"TARGET //", BOSS_X + 14.0f, BOSS_Y + 7.0f, 13.0f, ToD2D(kRed, 0.8f), UIFont::Mono, UIAlign::Left, true);
        if (bossName)
            Text(bossName, BOSS_X + 100.0f, BOSS_Y + 2.0f, 24.0f, D2D1::ColorF(1.0f, 0.9f, 0.88f, 1.0f),
                 UIFont::Display, UIAlign::Left, true);
        if (bossRatio <= 0.5f)
            Text(L"ENRAGED", BOSS_X + BOSS_W * 0.5f, BOSS_Y + 7.0f, 13.0f, ToD2D(kAmber, blinkOn ? 1.0f : 0.5f),
                 UIFont::Mono, UIAlign::Center, true);
        swprintf_s(buf, L"%5.1f%%", bossRatio * 100.0f);
        Text(buf, BOSS_X + BOSS_W - 14.0f, BOSS_Y + 6.0f, 15.0f, ToD2D(kRed), UIFont::Mono, UIAlign::Right, true);
    }

    FlushText();
    Direct3D_SetDepthEnable(true);
}

//------------------------------------------------------------------------------
// ATK表示
//------------------------------------------------------------------------------
static void HUD_DrawAttackStackUI(float scale, float screenW, float screenH)
{
    if (s_TexStackATTACKFrame < 0 || s_TexStackATTACKStat < 0 || s_TexStackATTACKItem < 0) return;

    const float statSize = STACK_STAT_SIZE * scale;
    const float frameW = STACK_FRAME_W * scale;
    const float frameH = STACK_FRAME_H * scale;

    const float baseX = STACK_MARGIN_L * scale;
    const float baseY = screenH - (STACK_MARGIN_B * scale) - frameH;

    const float statX = baseX;
    const float statY = baseY + (frameH - statSize) * 0.5f;

    const float frameX = statX + statSize + (STACK_GAP_X * scale);
    const float frameY = baseY;

    Sprite_Draw(s_TexStackATTACKStat, statX, statY, statSize, statSize, XMFLOAT4(1, 1, 1, 1));
    Sprite_Draw(s_TexStackATTACKFrame, frameX, frameY, frameW, frameH, XMFLOAT4(1, 1, 1, 1));

    const float itemX = frameX + (STACK_INNER_PAD_X * scale);
    const float itemY = frameY + (STACK_INNER_PAD_Y * scale);
    const float itemSize = STACK_ITEM_SIZE * scale;

    Sprite_Draw(s_TexStackATTACKItem, itemX, itemY, itemSize, itemSize, XMFLOAT4(1.5f, 1.5f, 1.5f, 1.5f));

    if (s_TexStackX >= 0)
    {
        const float xW = STACK_X_SIZE_W * scale;
        const float xH = STACK_X_SIZE_H * scale;
        const float xX = itemX + itemSize + (STACK_ITEM_GAP_X * scale);
        const float xY = itemY + (itemSize - xH) * 0.5f;

        Sprite_Draw(s_TexStackX, xX, xY, xW, xH, XMFLOAT4(1, 1, 1, 1));

        if (s_TexDigits >= 0)
        {
            const float digitScale = STACK_DIGIT_SCALE * scale;
            const float digitH = HUD_GetDigitH(s_TexDigits) * digitScale;
            const float numX = xX + xW + (STACK_X_GAP_X * scale);
            const float numY = itemY + (itemSize - digitH) * 0.5f;

            HUD_DrawNumberLeftScaled(s_TexDigits, s_AtkCount, numX, numY, digitScale);
        }
    }
    else
    {
        if (s_TexDigits >= 0)
        {
            const float digitScale = STACK_DIGIT_SCALE * scale;
            const float digitH = HUD_GetDigitH(s_TexDigits) * digitScale;
            const float numX = itemX + itemSize + (STACK_ITEM_GAP_X * scale);
            const float numY = itemY + (itemSize - digitH) * 0.5f;

            HUD_DrawNumberLeftScaled(s_TexDigits, s_AtkCount, numX, numY, digitScale);
        }
    }
}

//------------------------------------------------------------------------------
// SPEED表示
//------------------------------------------------------------------------------
static void HUD_DrawSpeedStackUI(float scale, float screenW, float screenH)
{
    if (s_TexStackSPEEDFrame < 0 || s_TexStackSPEEDStat < 0 || s_TexStackSPEEDItem < 0) return;

    const float statSize = STACK_STAT_SIZE * scale;
    const float frameW = STACK_FRAME_W * scale;
    const float frameH = STACK_FRAME_H * scale;

    const float baseX = STACK_MARGIN_L * scale;
    const float baseY = screenH - (STACK_MARGIN_B * scale) - frameH - (frameH + STACK_ROW_GAP_Y * scale);

    const float statX = baseX;
    const float statY = baseY + (frameH - statSize) * 0.5f;

    const float frameX = statX + statSize + (STACK_GAP_X * scale);
    const float frameY = baseY;

    Sprite_Draw(s_TexStackSPEEDStat, statX, statY, statSize, statSize, XMFLOAT4(1, 1, 1, 1));
    Sprite_Draw(s_TexStackSPEEDFrame, frameX, frameY, frameW, frameH, XMFLOAT4(1, 1, 1, 1));

    const float itemX = frameX + (STACK_INNER_PAD_X * scale);
    const float itemY = frameY + (STACK_INNER_PAD_Y * scale);
    const float itemSize = STACK_ITEM_SIZE * scale;

    Sprite_Draw(s_TexStackSPEEDItem, itemX, itemY, itemSize, itemSize, XMFLOAT4(1.5f, 1.5f, 1.5f, 1.5f));

    if (s_TexStackX >= 0)
    {
        const float xW = STACK_X_SIZE_W * scale;
        const float xH = STACK_X_SIZE_H * scale;
        const float xX = itemX + itemSize + (STACK_ITEM_GAP_X * scale);
        const float xY = itemY + (itemSize - xH) * 0.5f;

        Sprite_Draw(s_TexStackX, xX, xY, xW, xH, XMFLOAT4(1, 1, 1, 1));

        if (s_TexDigits >= 0)
        {
            const float digitScale = STACK_DIGIT_SCALE * scale;
            const float digitH = HUD_GetDigitH(s_TexDigits) * digitScale;
            const float numX = xX + xW + (STACK_X_GAP_X * scale);
            const float numY = itemY + (itemSize - digitH) * 0.5f;

            HUD_DrawNumberLeftScaled(s_TexDigits, s_SpeedCount, numX, numY, digitScale);
        }
    }
    else
    {
        if (s_TexDigits >= 0)
        {
            const float digitScale = STACK_DIGIT_SCALE * scale;
            const float digitH = HUD_GetDigitH(s_TexDigits) * digitScale;
            const float numX = itemX + itemSize + (STACK_ITEM_GAP_X * scale);
            const float numY = itemY + (itemSize - digitH) * 0.5f;

            HUD_DrawNumberLeftScaled(s_TexDigits, s_SpeedCount, numX, numY, digitScale);
        }
    }
}

//==============================================================================
// モード切り替え通知
//
// ■役割
// ・武器モード切り替え時に呼び出し、表示タイマーをリセットする
//
// ■引数
// ・isBeam : trueでビームモード、falseで通常弾モード
//==============================================================================
void HUD_NotifyModeChange(bool isBeam)
{
    s_IsBeamMode = isBeam;
    s_ModeTimer = MODE_DISPLAY_DURATION;
}

int HUD_GetSightTexture()
{
    return s_IsBeamMode ? s_TexSightBeam : s_TexSightNormal;
}

void HUD_SetUseNewDesign(bool useNew) { s_UseNewDesign = useNew; }
bool HUD_GetUseNewDesign()            { return s_UseNewDesign; }

//==============================================================================
// HUD更新
//
// ■役割
// ・モード表示タイマーを減算する
//
// ■引数
// ・elapsed_time : 経過時間（秒）
//==============================================================================
void HUD_Update(double elapsed_time)
{
    s_HudTime += static_cast<float>(elapsed_time);

    // 大型兵器の体力の表示値：実際の値まで毎秒 25% の速さで減らす
    {
        int bossHp = 0, bossMaxHp = 0;
        if (s_BossHpDisplayed >= 0.0f && Game_GetBossStatus(&bossHp, &bossMaxHp, nullptr) && bossMaxHp > 0)
        {
            const float ratio = std::clamp((float)bossHp / (float)bossMaxHp, 0.0f, 1.0f);
            s_BossHpDisplayed = std::max(ratio, s_BossHpDisplayed - 0.25f * (float)elapsed_time);
        }
    }
    if (s_ModeTimer > 0.0)
    {
        s_ModeTimer -= elapsed_time;
        if (s_ModeTimer < 0.0) s_ModeTimer = 0.0;
    }

    // 武器ミニプレビューは角度固定（回転更新なし）

    // HP カウントダウンアニメーション
    // HP が減ったときだけ数字をゆっくりカウントダウン、回復は即時反映
    {
        const float realHP = (float)Player_GetHP();
        if (s_HpDisplayed < 0.0f)
        {
            s_HpDisplayed = realHP;  // 初回: 即時同期
        }
        else if (s_HpDisplayed > realHP)
        {
            // ダメージ: 800 HP/秒でカウントダウン
            s_HpDisplayed -= 800.0f * (float)elapsed_time;
            if (s_HpDisplayed < realHP) s_HpDisplayed = realHP;
        }
        else
        {
            s_HpDisplayed = realHP;  // 回復: 即時
        }
    }
}

//==============================================================================
// GAME OVER オーバーレイ描画
//
// ■役割
// ・死亡演出中に「GAME OVER」テキストと暗幕を重ねる
// ■引数
// ・alpha : 0.0=完全透明, 1.0=完全不透明（フェードインに使う）
//==============================================================================
void HUD_DrawGameOver(float alpha)
{
    using namespace SciFiUI;
    if (alpha <= 0.0f) return;
    if (alpha > 1.0f)  alpha = 1.0f;

    const float SW = (float)SPRITE_SCREEN_W;
    const float SH = (float)SPRITE_SCREEN_H;
    const float bandY = SH * 0.5f - 80.0f, bandH = 160.0f;

    BeginSprites();

    // 暗幕と走査線（信号が途切れたモニター風）
    Fill(0.0f, 0.0f, SW, SH, { 0.0f, 0.0f, 0.0f, 0.55f * alpha });
    Scanlines(0.0f, 0.0f, SW, SH, 3.0f, 0.08f * alpha);

    // 画面を横切るノイズの筋（時間でずれる）
    for (int i = 0; i < 7; ++i)
    {
        const float y = fmodf(s_HudTime * (90.0f + i * 37.0f) + i * 131.0f, SH);
        const float w = 120.0f + fmodf(i * 211.0f + s_HudTime * 300.0f, 520.0f);
        const float x = fmodf(i * 397.0f + s_HudTime * 700.0f, SW + w) - w;
        Fill(x, y, w, 2.0f, WithAlpha(kRed, 0.25f * alpha));
    }

    // 中央の帯
    Fill(0.0f, bandY, SW, bandH, WithAlpha({ 0.12f, 0.0f, 0.0f, 1.0f }, 0.75f * alpha));
    HazardTape(0.0f, bandY, SW, 6.0f, s_HudTime * 120.0f, WithAlpha(kRed, 0.8f * alpha));
    HazardTape(0.0f, bandY + bandH - 6.0f, SW, 6.0f, -s_HudTime * 120.0f, WithAlpha(kRed, 0.8f * alpha));
    Brackets(SW * 0.5f - 420.0f, bandY + 18.0f, 840.0f, bandH - 36.0f, 18.0f, WithAlpha(kRed, alpha));

    // 文字：わずかに横へぶれる残像を重ねる
    const float jitter = (fmodf(s_HudTime, 0.9f) < 0.08f) ? 6.0f : 0.0f;
    Text(L"SIGNAL LOST", SW * 0.5f + jitter + 3.0f, bandY + 22.0f, 80.0f, ToD2D({ 0.3f, 0.9f, 1.0f, 1.0f }, 0.25f * alpha),
         UIFont::Display, UIAlign::Center, true);
    Text(L"SIGNAL LOST", SW * 0.5f - jitter, bandY + 22.0f, 80.0f, ToD2D(kRed, alpha),
         UIFont::Display, UIAlign::Center, true, 2.0f);
    Text(L"機体大破 ― AP 0  //  CONNECTION TERMINATED", SW * 0.5f, bandY + 114.0f, 17.0f,
         D2D1::ColorF(1.0f, 0.82f, 0.78f, alpha), UIFont::Body, UIAlign::Center, true);
    FlushText();

    Direct3D_SetDepthEnable(true);
}