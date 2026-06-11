/*==============================================================================

   アセンブル画面 [AssemblyScreen.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/22
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
static constexpr float LP_ITEM_H       = 40.0f;
static constexpr float LP_ITEM_SPACING = 45.0f;
static constexpr float LP_DIV_Y        = 248.0f;
static constexpr float LP_LARM_Y       = 268.0f;
static constexpr float LP_ITEM_START_L = 308.0f;

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
    // 0=R-ARM, 1=L-ARM
    int g_ActivePanel = 0;

    // 各パネルのカーソル位置（WeaponID に対応）
    int g_RightCursor = WEAPON_MACHINEGUN;
    int g_LeftCursor  = WEAPON_SHIELD;

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

// 残クレジット計算
static int CalcRemaining()
{
    return INITIAL_CREDITS
         - k_WeaponDefs[g_RightCursor].cost
         - k_WeaponDefs[g_LeftCursor].cost;
}

// Sprite カラー定数
static constexpr XMFLOAT4 kDim        = { 1.00f, 1.00f, 1.00f, 0.25f };
static constexpr XMFLOAT4 kPanelBg    = { 0.04f, 0.06f, 0.12f, 0.90f };
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
    g_ActivePanel  = 0;
    g_RightCursor  = g_DefaultRight;   // 前回選択を引き継ぐ
    g_LeftCursor   = g_DefaultLeft;
    g_Decided      = false;
    g_Cancelled    = false;
    g_Time         = 0.0;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudio("resource/Sound/ui_tab_switch.wav");

    // テクスチャ
    if (g_WhiteTexID < 0) g_WhiteTexID = Texture_Load(L"resource/Texture/white.png");
    if (g_BgTexID    < 0) g_BgTexID    = Texture_Load(L"resource/Texture/titleBg.png");

    // プレビューモデル（古いものを解放してから再ロード）
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_pPreviewModels[i]) { ModelRelease(g_pPreviewModels[i]); g_pPreviewModels[i] = nullptr; }
        g_pPreviewModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
    }

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

    // パネル切り替え（TAB = R-ARM ↔ L-ARM トグル）
    {
        // TAB / LB / RB：R-ARM ↔ L-ARM トグル
        if (UI_IsTabSwitch())
        { g_ActivePanel = 1 - g_ActivePanel; PlayAudio(g_SeTabSwitch, false); }

        // ENTER / パッドA / 左クリック で決定
        if (UI_IsConfirm())
        {
            if (CalcRemaining() >= 0)
            {
                PlayAudio(g_SeSelect, false);
                g_Decided = true;
                return true;
            }
        }

        // ESC / パッドB でキャンセル（前の画面へ戻る）
        if (UI_IsCancel())
        {
            PlayAudio(g_SeCancel, false);
            g_Cancelled = true;
            return true;
        }
    }

    // カーソル移動（上下）
    const bool up   = UI_IsMoveUp();
    const bool down = UI_IsMoveDown();

    if (g_ActivePanel == 0)
    {
        if (up)   { g_RightCursor = (g_RightCursor + WEAPON_COUNT - 1) % WEAPON_COUNT; PlayAudio(g_SeCursorMove, false); }
        if (down) { g_RightCursor = (g_RightCursor + 1)                % WEAPON_COUNT; PlayAudio(g_SeCursorMove, false); }
    }
    else
    {
        if (up)   { g_LeftCursor = (g_LeftCursor + WEAPON_COUNT - 1) % WEAPON_COUNT; PlayAudio(g_SeCursorMove, false); }
        if (down) { g_LeftCursor = (g_LeftCursor + 1)                % WEAPON_COUNT; PlayAudio(g_SeCursorMove, false); }
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
    // アクティブパネル強調
    //--------------------------------------------------------------------------
    if (g_ActivePanel == 0)
        Sprite_Draw(g_WhiteTexID, LEFT_X, LP_RARM_Y - 4.0f, LEFT_W, 32.0f, kSelBg);
    else
        Sprite_Draw(g_WhiteTexID, LEFT_X, LP_LARM_Y - 4.0f, LEFT_W, 32.0f, kSelBg);

    // 選択アイテム背景
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_ActivePanel == 0 && i == g_RightCursor)
        {
            const float bob = sinf(static_cast<float>(g_Time) * 5.5f) * 2.0f;
            const float ry  = LP_ITEM_START_R + i * LP_ITEM_SPACING;
            Sprite_Draw(g_WhiteTexID, LEFT_X+2, ry+bob, LEFT_W-4, LP_ITEM_H, kSelBg);
        }
        if (g_ActivePanel == 1 && i == g_LeftCursor)
        {
            const float bob = sinf(static_cast<float>(g_Time) * 5.5f) * 2.0f;
            const float ly  = LP_ITEM_START_L + i * LP_ITEM_SPACING;
            Sprite_Draw(g_WhiteTexID, LEFT_X+2, ly+bob, LEFT_W-4, LP_ITEM_H, kSelBg);
        }
    }

    //--------------------------------------------------------------------------
    // (2) ステータスバー
    //--------------------------------------------------------------------------
    const int hoverId = (g_ActivePanel == 0) ? g_RightCursor : g_LeftCursor;
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
            Light_SetAmbient({ 0.5f, 0.5f, 0.5f });

            // 法線パス（エッジ検出用）
            ShaderEdge_BeginNormalPass();
            ShaderEdge_SetWorldMatrix(world);
            ModelDrawWithoutBegin(previewModel, world);
            ShaderEdge_EndNormalPass();

            // トゥーン描画
            setSubVP();
            ModelDrawToon(previewModel, world);

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

        auto makeWeaponWorld = [&](const WeaponDef& def, float sideSign) -> XMMATRIX
        {
            // 位置：player.cpp の barrelOriginPos / shieldOriginPos と同方式
            XMFLOAT3 posF3 = {
                XMVectorGetX(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetX(worldFront) * def.forwardOffset,
                bodyAABB.min.y + def.heightOffset,
                XMVectorGetZ(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetZ(worldFront) * def.forwardOffset
            };
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

        const XMMATRIX rWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_RightCursor], +1.0f);
        const XMMATRIX lWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_LeftCursor],  -1.0f);
        MODEL* rWeaponModel = g_pPreviewModels[g_RightCursor];
        MODEL* lWeaponModel = g_pPreviewModels[g_LeftCursor];

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
        Light_SetAmbient({ 0.5f, 0.5f, 0.5f });

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
        ShaderEdge_EndNormalPass();

        // トゥーン描画
        setPlayerVP();
        ModelDrawToon(g_pPlayerPreviewBody, bodyWorld);
        ModelDrawToon(g_pPlayerPreviewHead, headWorld);
        ModelDrawToon(g_pPlayerPreviewThruster, thrusterWorld);
        if (rWeaponModel) ModelDrawToon(rWeaponModel, rWeaponWorld);
        if (lWeaponModel) ModelDrawToon(lWeaponModel, lWeaponWorld);

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
            fd.Color = (g_ActivePanel == 0)
                ? D2D1::ColorF(0.4f, 0.85f, 1.0f, 1.0f)
                : D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("R-ARM", LEFT_X + 12.0f, LP_RARM_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            const float iy  = LP_ITEM_START_R + i * LP_ITEM_SPACING + 10.0f;
            const bool  sel = (g_ActivePanel == 0 && i == g_RightCursor);
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = sel ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
            fd.Color = sel ? D2D1::ColorF(1,1,1,1) : D2D1::ColorF(0.6f,0.6f,0.6f,1);
            g_pDWBody->SetFont(&fd);
            const std::string lbl = std::string(sel ? "> " : "  ") + k_WeaponDefs[i].name;
            g_pDWBody->DrawString(lbl, LEFT_X + 10.0f, iy, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        // ── 左パネル: L-ARM ───────────────────────────────
        {
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
            fd.Color = (g_ActivePanel == 1)
                ? D2D1::ColorF(0.4f, 0.85f, 1.0f, 1.0f)
                : D2D1::ColorF(0.7f, 0.7f, 0.7f, 1.0f);
            g_pDWBody->SetFont(&fd);
        }
        g_pDWBody->DrawString("L-ARM", LEFT_X + 12.0f, LP_LARM_Y,
            D2D1_DRAW_TEXT_OPTIONS_NONE);

        for (int i = 0; i < WEAPON_COUNT; ++i)
        {
            const float iy  = LP_ITEM_START_L + i * LP_ITEM_SPACING + 10.0f;
            const bool  sel = (g_ActivePanel == 1 && i == g_LeftCursor);
            FontData fd;
            fd.font = Font::Arial; fd.fontSize = 18.0f;
            fd.fontWeight = sel ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
            fd.Color = sel ? D2D1::ColorF(1,1,1,1) : D2D1::ColorF(0.6f,0.6f,0.6f,1);
            g_pDWBody->SetFont(&fd);
            const std::string lbl = std::string(sel ? "> " : "  ") + k_WeaponDefs[i].name;
            g_pDWBody->DrawString(lbl, LEFT_X + 10.0f, iy, D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

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
        snprintf(buf, sizeof(buf), "R-ARM : %s", k_WeaponDefs[g_RightCursor].name);
        g_pDWBody->DrawString(buf, RGT_X + 20.0f, 680.0f, D2D1_DRAW_TEXT_OPTIONS_NONE);
        snprintf(buf, sizeof(buf), "L-ARM : %s", k_WeaponDefs[g_LeftCursor].name);
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
WeaponID AssemblyScreen_GetRightWeapon()     { return static_cast<WeaponID>(g_RightCursor); }
WeaponID AssemblyScreen_GetLeftWeapon()      { return static_cast<WeaponID>(g_LeftCursor);  }
int      AssemblyScreen_GetRemainingCredits(){ return CalcRemaining(); }
bool     AssemblyScreen_WasCancelled()       { return g_Cancelled; }

void AssemblyScreen_SetDefaults(WeaponID right, WeaponID left)
{
    g_DefaultRight = static_cast<int>(right);
    g_DefaultLeft  = static_cast<int>(left);

    // カーソルも即時更新（Initialize() が呼ばれない QuickStart でも正しい値を返せるように）
    g_RightCursor = g_DefaultRight;
    g_LeftCursor  = g_DefaultLeft;
}
