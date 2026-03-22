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
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include <DirectXMath.h>

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

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
static float HUD_ComputeUIScale(float screenW, float screenH);
static void HUD_DrawAttackStackUI(float scale, float screenW, float screenH);
static void HUD_DrawSpeedStackUI(float scale, float screenW, float screenH);

static void HUD_DrawNumberLeftScaled(int digitTexId, int value, float x, float y, float scale);
static float HUD_GetDigitW(int digitTexId);
static float HUD_GetDigitH(int digitTexId);

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
// Draw
//==============================================================================
void HUD_Draw()
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
}