/*==============================================================================

   HUD [HUD.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/18
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
static const int ICON_MAX = 32;//スタック制にしたので使わない

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
static float  s_WeaponPreviewAngle = 0.0f;                // 回転角（ラジアン）

//==============================================================================
// HUDデザイン切り替えフラグ
//==============================================================================
static bool s_UseNewDesign = false;

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

    // ── 新HUD用 DirectWrite ──────────────────────────────
    {
        static FontData fdLarge;
        fdLarge.font          = Font::DSEG7;
        fdLarge.fontFilePath  = L"resource/fonts/DSEG7Modern-Regular.ttf";
        fdLarge.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdLarge.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdLarge.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdLarge.fontSize      = 36.0f;
        fdLarge.localeName    = L"en-us";
        fdLarge.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdLarge.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        s_pDW_Large = new DirectWrite(&fdLarge);
        s_pDW_Large->Init();

        static FontData fdSmall;
        fdSmall.font          = Font::Arial;
        fdSmall.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fdSmall.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fdSmall.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fdSmall.fontSize      = 18.0f;
        fdSmall.localeName    = L"en-us";
        fdSmall.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fdSmall.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        s_pDW_Small = new DirectWrite(&fdSmall);
        s_pDW_Small->Init();
    }

    // GAME OVER テキスト用（Agency FB 大フォント）
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

    // 武器ミニプレビュー用モデルをロード
    for (int i = 0; i < WEAPON_COUNT; ++i)
        s_pWeaponPreviewModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
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
// 新HUD ― ACシリーズ風レイアウト
//
//  ┌─────────────────────────────┐
//  │[左上] APパネル(HP数値+バー)      [右] 武器パネル │
//  │[左端] エネルギーバー（縦）                     │
//  │                [中央] 照準                   │
//  │[左下] 速度                                  │
//  └─────────────────────────────┘
//==============================================================================
static void HUD_DrawNew()
{
    if (s_BarTexID < 0) return;

    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();

    const float SW = (float)SPRITE_SCREEN_W;   // 1600
    const float SH = (float)SPRITE_SCREEN_H;   // 900
    const float BRD = 2.0f;                    // 枠線幅

    // ── 共通カラー定義 ─────────────────────────────────
    // 購入・アセンブリ画面風のネイビーブルー基調
    const XMFLOAT4 colAmber    = { 1.0f, 0.70f, 0.00f, 1.0f };
    const XMFLOAT4 colAmberDim = { 1.0f, 0.70f, 0.00f, 0.55f };
    const XMFLOAT4 colDark     = { 0.04f, 0.07f, 0.20f, 0.88f };   // ← ネイビー
    const XMFLOAT4 colHPGreen  = { 0.2f, 0.85f, 0.25f, 1.0f };
    const XMFLOAT4 colHPRed    = { 1.0f, 0.18f, 0.08f, 1.0f };
    const XMFLOAT4 colEnergy   = { 1.0f, 0.60f, 0.00f, 1.0f };
    const XMFLOAT4 colWepSel   = { 0.05f, 0.55f, 0.15f, 0.88f };
    const XMFLOAT4 colWepNorm  = { 0.05f, 0.09f, 0.24f, 0.88f };   // ← ネイビー
    const XMFLOAT4 colWhite    = { 1.0f, 1.0f,  1.0f,  1.0f };
    const XMFLOAT4 colBarBG    = { 0.08f, 0.13f, 0.32f, 0.95f };   // ← 少し明るいネイビー

    // ── ローカルヘルパー: 4辺に枠線を引く ──────────────────
    auto DrawBorder = [&](float x, float y, float w, float h, const XMFLOAT4& col)
    {
        Sprite_Draw(s_BarTexID, x,         y,             w,   BRD, col); // 上
        Sprite_Draw(s_BarTexID, x,         y + h - BRD,   w,   BRD, col); // 下
        Sprite_Draw(s_BarTexID, x,         y,              BRD, h,   col); // 左
        Sprite_Draw(s_BarTexID, x + w - BRD, y,            BRD, h,   col); // 右
    };

    // ================================================================
    // 1. APパネル（左上）― HP数値 + ゲージ
    // ================================================================
    const int   hp          = Player_GetHP();
    const int   hpMax       = Player_GetMaxHP();
    const int   displayedHP = (int)s_HpDisplayed;          // アニメーション用表示値
    const float hpRatio     = (hpMax > 0) ? (float)hp / (float)hpMax : 0.0f;

    const float AP_X  = 16.0f;
    const float AP_Y  = 14.0f;
    const float AP_W  = 300.0f;
    const float AP_H  = 96.0f;   // 「AP」ラベル + 分数 + バーが収まる高さ

    const float AP_BAR_X  = AP_X + 8.0f;
    const float AP_BAR_Y  = AP_Y + 78.0f;  // 分数テキスト下に余白を取って配置
    const float AP_BAR_W  = AP_W - 16.0f;
    const float AP_BAR_H  = 12.0f;

    // 背景
    Sprite_Draw(s_BarTexID, AP_X, AP_Y, AP_W, AP_H, colDark);

    // AP 数値 → DirectWrite セクションで描画

    // ゲージ背景 + 充填
    Sprite_Draw(s_BarTexID, AP_BAR_X, AP_BAR_Y, AP_BAR_W, AP_BAR_H, colBarBG);
    const float hpFillW = AP_BAR_W * hpRatio;
    if (hpFillW > 0.0f)
    {
        const XMFLOAT4 barCol = (hpRatio > 0.30f) ? colHPGreen : colHPRed;
        Sprite_Draw(s_BarTexID, AP_BAR_X, AP_BAR_Y, hpFillW, AP_BAR_H, barCol);
    }

    // 枠線
    DrawBorder(AP_X, AP_Y, AP_W, AP_H, colAmber);

    // ================================================================
    // 2. ビームエネルギーバー（左端・縦）
    // ================================================================
    const float bEnergy    = Player_GetBeamEnergy();
    const float bEnergyMax = Player_GetBeamEnergyMax();
    const float bRatio     = (bEnergyMax > 0.0f) ? (bEnergy / bEnergyMax) : 0.0f;

    const float ENE_X = 16.0f;
    const float ENE_Y = AP_Y + AP_H + 2.0f;   // APパネル直下に詰める
    const float ENE_W = 18.0f;
    const float ENE_H = 180.0f;

    Sprite_Draw(s_BarTexID, ENE_X, ENE_Y, ENE_W, ENE_H, colBarBG);
    {
        const float fillH = ENE_H * bRatio;
        if (fillH > 0.0f)
            Sprite_Draw(s_BarTexID,
                ENE_X, ENE_Y + ENE_H - fillH,
                ENE_W, fillH,
                colEnergy);
    }
    DrawBorder(ENE_X, ENE_Y, ENE_W, ENE_H, colAmber);

    // ================================================================
    // 3. 速度表示（左下）
    // ================================================================
    XMFLOAT3* vel = Player_GetVelocityPtr();
    int speedInt = 0;
    if (vel)
    {
        const float hSpd = sqrtf(vel->x * vel->x + vel->z * vel->z);
        speedInt = (int)(hSpd * 36.0f);  // 単位系に合わせた係数
    }

    const float SPD_W = 160.0f;
    const float SPD_H = 42.0f;
    const float SPD_X = 16.0f;
    const float SPD_Y = SH - SPD_H - 14.0f;

    Sprite_Draw(s_BarTexID, SPD_X, SPD_Y, SPD_W, SPD_H, colDark);
    DrawBorder(SPD_X, SPD_Y, SPD_W, SPD_H, colAmber);
    // 速度数値 → DirectWrite セクションで描画

    // ================================================================
    // 4. 武器パネル（右側） ― R ARM / L ARM 各1スロット＋3Dミニプレビュー
    // ================================================================
    const float WEP_W      = 240.0f;
    const float WEP_SLOT_H = 110.0f;
    const float WEP_GAP    = 6.0f;
    const float WEP_X      = SW - WEP_W - 10.0f;
    // 下端から固定（ミニマップが右上を占有するため右下に配置）
    // スタックパネル(104px) + ギャップ(8px) + 余白(16px) を含めて収まるよう計算
    const float WEP_Y      = SH - 2.0f * (WEP_SLOT_H + WEP_GAP) - 104.0f - 8.0f - 16.0f;

    // プレビュー領域（スロット内、左側に正方形配置）
    const float PREV_SZ    = 90.0f;    // プレビューの一辺
    const float PREV_PAD_X = 4.0f;
    const float PREV_PAD_Y = (WEP_SLOT_H - PREV_SZ) * 0.5f;

    const int rArmIdx = Player_GetNormalWeaponIndex();
    const int lArmIdx = Player_GetLeftWeaponIndex();
    const int armIdx[2] = { rArmIdx, lArmIdx };

    // ── スロット背景＋枠（スプライト）─────────────────────────
    for (int arm = 0; arm < 2; ++arm)
    {
        const float slotY = WEP_Y + arm * (WEP_SLOT_H + WEP_GAP);
        Sprite_Draw(s_BarTexID, WEP_X, slotY, WEP_W, WEP_SLOT_H, colWepNorm);
        DrawBorder(WEP_X, slotY, WEP_W, WEP_SLOT_H, colAmber);
    }

    // ── ATK / SPEED スタック表示（武器パネル下） ────────────
    {
        const float STA_Y   = WEP_Y + 2.0f * (WEP_SLOT_H + WEP_GAP) + 8.0f;
        const float STA_H   = 104.0f;
        const float COL_W   = WEP_W * 0.5f;   // 1アイテムあたりの列幅
        const float ICON_SZ = 56.0f;

        // 常時描画（0個でも表示）
        {
            Sprite_Draw(s_BarTexID, WEP_X, STA_Y, WEP_W, STA_H, colDark);
            DrawBorder(WEP_X, STA_Y, WEP_W, STA_H, colAmberDim);

            // ATK列（アイコンは常時表示、カウントは0でも出す）
            if (s_TexAtk >= 0)
            {
                const float cx = WEP_X + COL_W * 0.5f;
                Sprite_Draw(s_TexAtk,
                    cx - ICON_SZ * 0.5f, STA_Y + 8.0f,
                    ICON_SZ, ICON_SZ, colWhite);
            }
            // SPEED列
            if (s_TexSpeed >= 0)
            {
                const float cx = WEP_X + COL_W + COL_W * 0.5f;
                Sprite_Draw(s_TexSpeed,
                    cx - ICON_SZ * 0.5f, STA_Y + 8.0f,
                    ICON_SZ, ICON_SZ, colWhite);
            }
        }
    }

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

        // ゆっくり Y 軸回転
        const XMMATRIX world = XMMatrixRotationY(s_WeaponPreviewAngle);

        // ゲームシーンが書いた深度値をクリア（モデルが地形に埋まるのを防ぐ）
        Direct3D_ClearDepth();
        Direct3D_SetDepthEnable(true);

        // ── プレビュー用ライト設定（他に影響しないよう事前に保存）──────────
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
                XMConvertToRadians(45.0f), PREV_SZ / PREV_SZ, 0.01f, 100.0f);

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

            ModelDrawToon(mdl, world);
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

        // ── ライトをプレビュー前の状態に完全に戻す ───────────────────────
        Light_SetAmbient(savedAmbient);

        Direct3D_SetDepthEnable(false);
        Direct3D_SetBlendState(true);
    }

    // ================================================================
    // 5. 照準（中央）
    // ================================================================
    {
        const float sightX = SW * 0.5f - SIGHT_SIZE * 0.5f;
        const float sightY = SH * 0.5f - SIGHT_SIZE * 0.5f;
        const int sightTex = s_IsBeamMode ? s_TexSightBeam : s_TexSightNormal;
        if (sightTex >= 0)
            Sprite_Draw(sightTex, sightX, sightY, SIGHT_SIZE, SIGHT_SIZE, colWhite);
    }

    // ================================================================
    // 6. テキスト描画（DirectWrite）
    //    スプライト描画完了後に D2D バッチで一括描画
    // ================================================================
    {
        // 仮想 1600x900 座標系 → 実ピクセル座標系へのスケール
        const float sx = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
        const float sy = (float)Direct3D_GetBackBufferHeight() / 900.0f;

        const D2D1_COLOR_F d2Amber = D2D1::ColorF(1.0f, 0.70f, 0.00f, 1.0f);
        const D2D1_COLOR_F d2White = D2D1::ColorF(1.0f, 1.0f,  1.0f,  1.0f);
        const D2D1_COLOR_F d2Dim   = D2D1::ColorF(0.5f, 0.5f,  0.5f,  1.0f);

        // ── 大フォント: HP数値 ──────────────────────────────
        if (s_pDW_Large)
        {
            s_pDW_Large->SetScale(sx, sy);
            s_pDW_Large->BeginBatch();

            char buf[16];
            snprintf(buf, sizeof(buf), "%d/%d", displayedHP, hpMax);
            // APパネル中央に大きく表示
            s_pDW_Large->DrawAt(buf,
                AP_X + AP_W * 0.5f, AP_Y + 50.0f,  // パネル中央寄り、スケール等倍
                AP_W * 0.5f - 8.0f,
                d2White);

            s_pDW_Large->EndBatch();
        }

        // ── 小フォント: ラベル・武器名・速度・スタックカウント ────
        if (s_pDW_Small)
        {
            s_pDW_Small->SetScale(sx, sy);
            s_pDW_Small->BeginBatch();

            // "AP" ラベル（HPパネル左上）
            s_pDW_Small->DrawAt("AP",
                AP_X + 22.0f, AP_Y + 18.0f,   // 枠上辺から8px確保
                18.0f, d2Amber);

            // "ENE" ラベル（エネルギーバー下）
            s_pDW_Small->DrawAt("ENE",
                ENE_X + ENE_W * 0.5f, ENE_Y + ENE_H + 13.0f,
                22.0f, d2Amber);

            // 速度値
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d km/h", speedInt);
                s_pDW_Small->DrawAt(buf,
                    SPD_X + SPD_W * 0.5f, SPD_Y + SPD_H * 0.5f,
                    SPD_W * 0.5f - 4.0f, d2White);
            }

            // 武器スロット（R-ARM / L-ARM）ラベル＋武器名＋ATK
            {
                // テキスト領域: プレビュー右端〜スロット右端
                const float textAreaX  = WEP_X + PREV_PAD_X + PREV_SZ + 6.0f;
                const float textAreaW  = WEP_X + WEP_W - 4.0f - textAreaX;
                const float textCX     = textAreaX + textAreaW * 0.5f;

                static const char* ARM_LABEL[2] = { "R-ARM", "L-ARM" };

                for (int arm = 0; arm < 2; ++arm)
                {
                    const float slotY = WEP_Y + arm * (WEP_SLOT_H + WEP_GAP);
                    const int   wIdx  = armIdx[arm];

                    // ARM ラベル
                    s_pDW_Small->DrawAt(ARM_LABEL[arm],
                        textCX, slotY + 22.0f,  // 枠上辺から10px確保
                        textAreaW * 0.5f, d2Amber);

                    if (wIdx >= 0 && wIdx < WEAPON_COUNT)
                    {
                        const WeaponDef& def = k_WeaponDefs[wIdx];

                        // 武器名
                        s_pDW_Small->DrawAt(def.name,
                            textCX, slotY + 50.0f,
                            textAreaW * 0.5f, d2White);

                        // ATK 値
                        char buf[16];
                        snprintf(buf, sizeof(buf), "ATK %d", def.damage);
                        s_pDW_Small->DrawAt(buf,
                            textCX, slotY + 82.0f,
                            textAreaW * 0.5f, d2Amber);
                    }
                    else
                    {
                        s_pDW_Small->DrawAt("---",
                            textCX, slotY + WEP_SLOT_H * 0.5f,
                            textAreaW * 0.5f, d2Dim);
                    }
                }
            }

            // 常時描画・2桁対応（幅を20.0fに拡張）
            {
                const float STA_Y = WEP_Y + 2.0f * (WEP_SLOT_H + WEP_GAP) + 8.0f;
                const float STA_H = 104.0f;
                const float COL_W = WEP_W * 0.5f;
                const float ICON_SZ = 56.0f;
                const float NAME_CY = STA_Y + 8.0f + ICON_SZ + 6.0f + 10.0f;

                // ATK列（常時）
                {
                    const float cx = WEP_X + COL_W * 0.5f;
                    char buf[8];
                    snprintf(buf, sizeof(buf), "x%d", s_AtkCount);
                    s_pDW_Small->DrawAt(buf,
                        cx + ICON_SZ * 0.5f + 16.0f,  // 2桁分の横幅を確保
                        STA_Y + 18.0f,
                        20.0f, d2White);               // 14.0f → 20.0f
                    s_pDW_Small->DrawAt("ATK UP",
                        cx, NAME_CY,
                        COL_W * 0.5f - 4.0f, d2Amber);
                }
                // SPEED列（常時）
                {
                    const float cx = WEP_X + COL_W + COL_W * 0.5f;
                    char buf[8];
                    snprintf(buf, sizeof(buf), "x%d", s_SpeedCount);
                    s_pDW_Small->DrawAt(buf,
                        cx + ICON_SZ * 0.5f + 16.0f,
                        STA_Y + 18.0f,
                        20.0f, d2White);
                    s_pDW_Small->DrawAt("SPD UP",
                        cx, NAME_CY,
                        COL_W * 0.5f - 4.0f, d2Amber);
                }
            }

            s_pDW_Small->EndBatch();
        }
    }

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
    if (s_ModeTimer > 0.0)
    {
        s_ModeTimer -= elapsed_time;
        if (s_ModeTimer < 0.0) s_ModeTimer = 0.0;
    }

    // 武器ミニプレビュー回転角を更新
    s_WeaponPreviewAngle += static_cast<float>(elapsed_time * 1.2);

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
    if (!s_pDW_GameOver || s_BarTexID < 0) return;
    if (alpha <= 0.0f) return;
    if (alpha > 1.0f)  alpha = 1.0f;

    const float SW = (float)SPRITE_SCREEN_W;
    const float SH = (float)SPRITE_SCREEN_H;
    const float sx = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
    const float sy = (float)Direct3D_GetBackBufferHeight() / 900.0f;

    // ── 半透明暗幕 ────────────────────────────────────────
    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();
    Sprite_Draw(s_BarTexID, 0.0f, 0.0f, SW, SH,
        { 0.0f, 0.0f, 0.0f, 0.55f * alpha });

    // ── "GAME OVER" テキスト（赤・中央）────────────────────
    s_pDW_GameOver->SetScale(sx, sy);
    s_pDW_GameOver->BeginBatch();

    // 縁取り付きで視認性確保
    const D2D1_COLOR_F col = D2D1::ColorF(1.0f, 0.10f, 0.10f, alpha);
    s_pDW_GameOver->DrawAt(
        std::wstring(L"損傷甚大"),
        SW * 0.5f, SH * 0.5f,
        SW * 0.5f - 20.0f,
        col,
        /*outlinePx=*/3.0f);

    s_pDW_GameOver->EndBatch();

    Direct3D_SetDepthEnable(true);
}