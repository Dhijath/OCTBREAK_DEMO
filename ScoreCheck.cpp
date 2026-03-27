/*==============================================================================
   スコア確認画面 [ScoreCheck.cpp]
   左パネル : ランキングリスト（最大10件・カーソル操作）
   右パネル : 選択レコードのプレイヤーモデル（武装付き）＋腕部名称
==============================================================================*/
#include "ScoreCheck.h"
#include "Score.h"
#include "WeaponDef.h"
#include "UIInput.h"
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "audio.h"
#include "text_logo.h"
#include "input_hint.h"
#include "model.h"
#include "shader3d.h"
#include "ModelToon.h"
#include "ShaderToon.h"
#include "ShaderEdge.h"
#include "light.h"
#include <d2d1helper.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cmath>
using namespace DirectX;

//------------------------------------------------------------------------------
// レイアウト定数（仮想 1600×900）
//------------------------------------------------------------------------------
static constexpr float LP_X   =  80.0f;
static constexpr float LP_W   = 540.0f;
static constexpr float LP_Y   = 160.0f;
static constexpr float ROW_H  =  62.0f;

static constexpr float RP_X   = 680.0f;
static constexpr float RP_Y   = 160.0f;
static constexpr float RP_W   = 860.0f;
static constexpr float RP_H   = 680.0f;
static constexpr float RP_CX  = RP_X + RP_W * 0.5f;

// 左パネルの高さは右パネルに合わせる（下端を揃える）
static constexpr float LP_H   = RP_H;

// モデルビューポート（右パネル内）
static constexpr float PP_X   = RP_X + 10.0f;
static constexpr float PP_Y   = RP_Y + 95.0f;
static constexpr float PP_W   = RP_W - 20.0f;
static constexpr float PP_H   = 430.0f;

// 腕部ラベル（モデル下）
static constexpr float ARM_Y0 = PP_Y + PP_H + 30.0f;  // 右腕
static constexpr float ARM_Y1 = PP_Y + PP_H + 80.0f;  // 左腕

//------------------------------------------------------------------------------
// 状態
//------------------------------------------------------------------------------
static bool  g_IsEnd        = false;
static int   g_Cursor       = 0;
static float g_PreviewAngle = 0.0f;

static int g_BgTex       = -1;
static int g_WhiteTex    = -1;
static int g_SeCursorMove = -1;

// プレイヤーモデル
static MODEL* g_pBody      = nullptr;
static MODEL* g_pHead      = nullptr;
static MODEL* g_pThruster  = nullptr;

// 武器モデル（全種ロード）
static MODEL* g_pWeaponModels[WEAPON_COUNT] = {};

// DirectWrite
static DirectWrite* g_pDWRank  = nullptr;
static DirectWrite* g_pDWScore = nullptr;
static DirectWrite* g_pDWLabel = nullptr;
static FontData g_fdRank, g_fdScore, g_fdLabel;

//------------------------------------------------------------------------------
// 初期化
//------------------------------------------------------------------------------
void ScoreCheck_Initialize()
{
    g_BgTex    = Texture_Load(L"resource/texture/titleBg.png");
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    if (g_SeCursorMove < 0)
        g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");

    // ── DirectWrite ─────────────────────────────────────────
    if (!g_pDWRank)
    {
        g_fdRank.font          = Font::AgencyFB;
        g_fdRank.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        g_fdRank.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        g_fdRank.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        g_fdRank.fontSize      = 22.0f;
        g_fdRank.localeName    = L"en-us";
        g_fdRank.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        g_fdRank.Color         = D2D1::ColorF(0.55f, 0.55f, 0.55f, 1.0f);
        g_pDWRank = new DirectWrite(&g_fdRank);
        g_pDWRank->Init();
    }
    if (!g_pDWScore)
    {
        g_fdScore.font          = Font::AgencyFB;
        g_fdScore.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        g_fdScore.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        g_fdScore.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        g_fdScore.fontSize      = 34.0f;
        g_fdScore.localeName    = L"en-us";
        g_fdScore.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        g_fdScore.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDWScore = new DirectWrite(&g_fdScore);
        g_pDWScore->Init();
    }
    if (!g_pDWLabel)
    {
        g_fdLabel.font          = Font::Arial;
        g_fdLabel.fontWeight    = DWRITE_FONT_WEIGHT_NORMAL;
        g_fdLabel.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        g_fdLabel.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        g_fdLabel.fontSize      = 20.0f;
        g_fdLabel.localeName    = L"en-us";
        g_fdLabel.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        g_fdLabel.Color         = D2D1::ColorF(0.7f, 0.9f, 1.0f, 1.0f);
        g_pDWLabel = new DirectWrite(&g_fdLabel);
        g_pDWLabel->Init();
    }

    // ── モデルロード ──────────────────────────────────────────
    if (!g_pBody)     g_pBody     = ModelLoad("resource/Models/body.fbx",     0.3f);
    if (!g_pHead)     g_pHead     = ModelLoad("resource/Models/Head.fbx",     0.3f);
    if (!g_pThruster) g_pThruster = ModelLoad("resource/Models/Thruster.fbx", 0.3f);

    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (!g_pWeaponModels[i])
            g_pWeaponModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
    }

    const int cnt = Score_GetRecordCount();
    g_Cursor       = (cnt == 0) ? 0 : std::min(g_Cursor, cnt - 1);
    g_IsEnd        = false;
    g_PreviewAngle = 0.0f;
}

//------------------------------------------------------------------------------
// 終了
//------------------------------------------------------------------------------
void ScoreCheck_Finalize()
{
    if (g_pDWRank)  { g_pDWRank->Release();  delete g_pDWRank;  g_pDWRank  = nullptr; }
    if (g_pDWScore) { g_pDWScore->Release(); delete g_pDWScore; g_pDWScore = nullptr; }
    if (g_pDWLabel) { g_pDWLabel->Release(); delete g_pDWLabel; g_pDWLabel = nullptr; }
    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;

    ModelRelease(g_pBody);     g_pBody     = nullptr;
    ModelRelease(g_pHead);     g_pHead     = nullptr;
    ModelRelease(g_pThruster); g_pThruster = nullptr;
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        ModelRelease(g_pWeaponModels[i]);
        g_pWeaponModels[i] = nullptr;
    }
}

//------------------------------------------------------------------------------
// 更新
//------------------------------------------------------------------------------
void ScoreCheck_Update(double dt)
{
    g_PreviewAngle += static_cast<float>(dt) * 0.8f;

    const int cnt = Score_GetRecordCount();
    if (cnt > 0)
    {
        if (UI_IsMoveUp())
        {
            g_Cursor = (g_Cursor + cnt - 1) % cnt;
            PlayAudio(g_SeCursorMove, false);
        }
        if (UI_IsMoveDown())
        {
            g_Cursor = (g_Cursor + 1) % cnt;
            PlayAudio(g_SeCursorMove, false);
        }
    }
    if (UI_IsCancel() || UI_IsConfirm())
        g_IsEnd = true;
}

//------------------------------------------------------------------------------
// 描画
//------------------------------------------------------------------------------
void ScoreCheck_Draw()
{
    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);

    const int   sw     = SPRITE_SCREEN_W;
    const int   sh     = SPRITE_SCREEN_H;
    const float scaleX = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
    const float scaleY = (float)Direct3D_GetBackBufferHeight() / 900.0f;

    const int          cnt  = Score_GetRecordCount();
    const ScoreRecord* recs = Score_GetRecords();

    // =========================================================
    // パス1: スプライト（背景・枠・ハイライト）
    // =========================================================
    Sprite_Begin();

    if (g_BgTex >= 0)
    {
        const float tw = (float)Texture_Width(g_BgTex);
        const float th = (float)Texture_Height(g_BgTex);
        Sprite_Draw(g_BgTex, 0, 0,
            tw * (sw / std::max(1.0f, tw)),
            th * (sh / std::max(1.0f, th)),
            XMFLOAT4(1, 1, 1, 1));
    }

    if (g_WhiteTex >= 0)
    {
        // 左パネル（高さを右パネルと同じ LP_H=RP_H に揃える）
        Sprite_Draw(g_WhiteTex, LP_X-8, LP_Y-8, LP_W+16, LP_H+16, XMFLOAT4(0,0,0.05f,0.55f));
        Sprite_Draw(g_WhiteTex, LP_X-8, LP_Y-8, LP_W+16, 2,        XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, LP_X-8, LP_Y-8, 2, LP_H+16,        XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, LP_X-8, LP_Y+LP_H+8, LP_W+16, 2,  XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, LP_X+LP_W+8, LP_Y-8, 2, LP_H+16,  XMFLOAT4(0.3f,0.6f,1.0f,0.7f));

        // カーソルハイライト
        if (cnt > 0)
        {
            const float hy = LP_Y + g_Cursor * ROW_H;
            Sprite_Draw(g_WhiteTex, LP_X-8, hy, LP_W+16, ROW_H, XMFLOAT4(0.2f,0.5f,1.0f,0.25f));
            Sprite_Draw(g_WhiteTex, LP_X-8, hy, 4, ROW_H,        XMFLOAT4(0.3f,0.7f,1.0f,0.9f));
        }

        // 右パネル背景・枠
        Sprite_Draw(g_WhiteTex, RP_X-8, RP_Y-8, RP_W+16, RP_H+16, XMFLOAT4(0,0.02f,0.08f,0.60f));
        Sprite_Draw(g_WhiteTex, RP_X-8, RP_Y-8, RP_W+16, 2,        XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, RP_X-8, RP_Y-8, 2, RP_H+16,        XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, RP_X-8, RP_Y+RP_H+8, RP_W+16, 2,  XMFLOAT4(0.3f,0.6f,1.0f,0.7f));
        Sprite_Draw(g_WhiteTex, RP_X+RP_W+8, RP_Y-8, 2, RP_H+16,  XMFLOAT4(0.3f,0.6f,1.0f,0.7f));

        // スコア下の区切り線
        Sprite_Draw(g_WhiteTex, RP_X+10, RP_Y+90, RP_W-20, 1, XMFLOAT4(0.3f,0.6f,1.0f,0.4f));

        // 腕部ラベル上の区切り線
        if (cnt > 0)
            Sprite_Draw(g_WhiteTex, RP_X+10, ARM_Y0-12, RP_W-20, 1, XMFLOAT4(0.3f,0.6f,1.0f,0.3f));
    }

    // =========================================================
    // パス2: 3Dモデル（右パネル内サブビューポート）
    // =========================================================
    if (cnt > 0 && g_Cursor < cnt && g_pBody && g_pHead && g_pThruster)
    {
        const ScoreRecord& rec = recs[g_Cursor];

        const XMFLOAT3 ppEyeF3 = { 0.0f, 0.45f, 1.2f };
        const XMVECTOR ppEye   = XMLoadFloat3(&ppEyeF3);
        const XMVECTOR ppTgt   = XMVectorSet(0.0f, 0.05f, 0.0f, 1.0f);
        const XMVECTOR ppUp    = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX ppView  = XMMatrixLookAtLH(ppEye, ppTgt, ppUp);
        const XMMATRIX ppProj  = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(45.0f), PP_W / PP_H, 0.01f, 100.0f);

        Shader3d_SetViewMatrix(ppView);   Shader3d_SetProjectMatrix(ppProj);
        ShaderToon_SetViewMatrix(ppView); ShaderToon_SetProjectMatrix(ppProj);
        ShaderEdge_SetViewMatrix(ppView); ShaderEdge_SetProjectMatrix(ppProj);

        // ボディ・頭・スラスターのワールド行列
        const XMMATRIX bodyRot = XMMatrixRotationY(g_PreviewAngle + XMConvertToRadians(90.0f)) *
                                 XMMatrixRotationY(XMConvertToRadians(90.0f));
        const XMVECTOR worldFront = XMVectorSet(sinf(g_PreviewAngle), 0.0f, cosf(g_PreviewAngle), 0.0f);

        const AABB bodyAABB     = ModelGetAABB(g_pBody,     {0,0,0});
        const AABB headAABB     = ModelGetAABB(g_pHead,     {0,0,0});
        const AABB thrusterAABB = ModelGetAABB(g_pThruster, {0,0,0});

        const XMMATRIX bodyWorld     = bodyRot;
        const XMMATRIX headWorld     = bodyRot *
            XMMatrixTranslation(0.0f, bodyAABB.max.y - headAABB.min.y, 0.0f);
        const XMMATRIX thrusterWorld = XMMatrixRotationY(XMConvertToRadians(180.0f)) * bodyRot *
            XMMatrixTranslation(
                XMVectorGetX(worldFront) * 0.05f,
                bodyAABB.min.y - thrusterAABB.max.y - 0.01f,
                XMVectorGetZ(worldFront) * 0.05f);

        // 武器ワールド行列
        const XMVECTOR ppUp2      = XMVectorSet(0, 1, 0, 0);
        const XMVECTOR worldRight = XMVector3Normalize(XMVector3Cross(ppUp2, worldFront));
        auto makeWeaponWorld = [&](const WeaponDef& def, float sideSign) -> XMMATRIX
        {
            XMFLOAT3 posF3 = {
                XMVectorGetX(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetX(worldFront) * def.forwardOffset,
                bodyAABB.min.y + def.heightOffset,
                XMVectorGetZ(worldRight) * sideSign * def.sideOffset
                    + XMVectorGetZ(worldFront) * def.forwardOffset
            };
            XMVECTOR aimZ = XMVectorNegate(worldFront);
            XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(ppUp2, aimZ));
            XMVECTOR aimY = XMVector3Cross(aimZ, aimX);
            XMFLOAT3 ax, ay, az;
            XMStoreFloat3(&ax, aimX); XMStoreFloat3(&ay, aimY); XMStoreFloat3(&az, aimZ);
            XMMATRIX aimRot(ax.x,ax.y,ax.z,0, ay.x,ay.y,ay.z,0, az.x,az.y,az.z,0, 0,0,0,1);
            XMMATRIX localRot =
                XMMatrixRotationZ(XMConvertToRadians(def.flipDeg + def.leanDeg * sideSign)) *
                XMMatrixRotationX(XMConvertToRadians(def.tiltDeg));
            return localRot * aimRot * XMMatrixTranslation(posF3.x, posF3.y, posF3.z);
        };

        const XMMATRIX rWeaponWorld = makeWeaponWorld(k_WeaponDefs[rec.rightWeapon], +1.0f);
        const XMMATRIX lWeaponWorld = makeWeaponWorld(k_WeaponDefs[rec.leftWeapon],  -1.0f);
        MODEL* rModel = g_pWeaponModels[rec.rightWeapon];
        MODEL* lModel = g_pWeaponModels[rec.leftWeapon];

        // サブビューポート設定
        auto setVP = [&]()
        {
            D3D11_VIEWPORT vp{};
            vp.TopLeftX = PP_X * scaleX; vp.TopLeftY = PP_Y * scaleY;
            vp.Width    = PP_W * scaleX; vp.Height   = PP_H * scaleY;
            vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &vp);
        };
        auto setFullVP = [&]()
        {
            D3D11_VIEWPORT vp{};
            vp.TopLeftX = 0; vp.TopLeftY = 0;
            vp.Width    = (float)Direct3D_GetBackBufferWidth();
            vp.Height   = (float)Direct3D_GetBackBufferHeight();
            vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
            Direct3D_GetContext()->RSSetViewports(1, &vp);
        };

        Direct3D_SetDepthEnable(true);
        Light_SetSpecularWorld(ppEyeF3, 100.0f, {0.6f,0.5f,0.4f,1.0f});
        Light_SetAmbient({0.5f,0.5f,0.5f});

        // 法線パス
        setVP();
        ShaderEdge_BeginNormalPass();
        ShaderEdge_SetWorldMatrix(bodyWorld);     ModelDrawWithoutBegin(g_pBody,     bodyWorld);
        ShaderEdge_SetWorldMatrix(headWorld);     ModelDrawWithoutBegin(g_pHead,     headWorld);
        ShaderEdge_SetWorldMatrix(thrusterWorld); ModelDrawWithoutBegin(g_pThruster, thrusterWorld);
        if (rModel) { ShaderEdge_SetWorldMatrix(rWeaponWorld); ModelDrawWithoutBegin(rModel, rWeaponWorld); }
        if (lModel) { ShaderEdge_SetWorldMatrix(lWeaponWorld); ModelDrawWithoutBegin(lModel, lWeaponWorld); }
        ShaderEdge_EndNormalPass();

        // トゥーン
        setVP();
        ModelDrawToon(g_pBody,     bodyWorld);
        ModelDrawToon(g_pHead,     headWorld);
        ModelDrawToon(g_pThruster, thrusterWorld);
        if (rModel) ModelDrawToon(rModel, rWeaponWorld);
        if (lModel) ModelDrawToon(lModel, lWeaponWorld);

        // エッジ合成
        setFullVP();
        ShaderEdge_DrawEdge();
        Direct3D_SetDepthEnable(false);
    }

    // D3D11 RTV アンバインド（AssemblyScreen と同じパターン：D2D との共存のため）
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        ctx->Flush();
    }

    // =========================================================
    // パス3: TextLogo（ヘッダー・スコア大）
    // =========================================================

    // ヘッダー
    {
        LogoStyle s;
        s.fontSize     = 58.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.50f,1.00f,0.90f,1.0f);
        s.colorBottom  = D2D1::ColorF(0.05f,0.60f,0.80f,1.0f);
        s.outlineColor = D2D1::ColorF(0.00f,0.10f,0.15f,1.0f);
        s.outlineWidth = 2.5f;
        TextLogo_Draw(L"SCORE CHECK", sw * 0.5f, 80.0f, s);
    }

    if (cnt > 0 && g_Cursor < cnt)
    {
        const ScoreRecord& rec = recs[g_Cursor];

        // 選択スコア（大）
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%u", rec.score);
            std::wstring ws(buf, buf + strlen(buf));
            LogoStyle s;
            s.fontSize     = 64.0f;
            s.fontName     = L"Agency FB";
            s.colorTop     = D2D1::ColorF(1.0f,0.95f,0.60f,1.0f);
            s.colorBottom  = D2D1::ColorF(0.90f,0.65f,0.10f,1.0f);
            s.outlineColor = D2D1::ColorF(0.05f,0.02f,0.00f,1.0f);
            s.outlineWidth = 2.5f;
            TextLogo_Draw(ws.c_str(), RP_CX, RP_Y + 44.0f, s);
        }
    }
    else if (cnt == 0)
    {
        LogoStyle s;
        s.fontSize     = 38.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.5f,0.5f,0.5f,1.0f);
        s.colorBottom  = D2D1::ColorF(0.35f,0.35f,0.35f,1.0f);
        s.outlineColor = D2D1::ColorF(0.0f,0.0f,0.0f,1.0f);
        s.outlineWidth = 1.5f;
        TextLogo_Draw(L"NO RECORDS", RP_CX, RP_Y + RP_H * 0.5f, s);
    }

    // フッターは DirectWrite 描画後に InputHint_Draw で描く

    // =========================================================
    // パス4: DirectWrite（ランキング・腕部名称）
    // =========================================================
    if (!g_pDWRank || !g_pDWScore || !g_pDWLabel) return;

    // ランキングリスト
    g_pDWRank->SetScale(scaleX, scaleY);
    g_pDWScore->SetScale(scaleX, scaleY);
    g_pDWRank->BeginBatch();
    g_pDWScore->BeginBatch();

    for (int i = 0; i < SCORE_RECORD_MAX; ++i)
    {
        const float rowCY = LP_Y + i * ROW_H + ROW_H * 0.5f;
        const float alpha = (i < cnt) ? 1.0f : 0.25f;

        char rankBuf[8];
        snprintf(rankBuf, sizeof(rankBuf), "#%d", i + 1);
        g_pDWRank->DrawAt(rankBuf, LP_X + 30.0f, rowCY, 50.0f,
            D2D1::ColorF(0.55f, 0.55f, 0.55f, alpha));

        char scoreBuf[24];
        if (i < cnt) snprintf(scoreBuf, sizeof(scoreBuf), "%u", recs[i].score);
        else         snprintf(scoreBuf, sizeof(scoreBuf), "---");
        g_pDWScore->DrawAt(scoreBuf, LP_X + 310.0f, rowCY, 260.0f,
            D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha));
    }

    g_pDWRank->EndBatch();
    g_pDWScore->EndBatch();
    g_pDWRank->SetScale(1.0f, 1.0f);
    g_pDWScore->SetScale(1.0f, 1.0f);

    // 右パネル：R-ARM / L-ARM + パーツ名（AssemblyScreen と同フォント）
    // EndBatch() が RTV を再バインドするため、DrawString 前に再度アンバインドが必要
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        ctx->Flush();
    }

    if (cnt > 0 && g_Cursor < cnt)
    {
        const ScoreRecord& rec  = recs[g_Cursor];
        const WeaponDef&   rDef = k_WeaponDefs[rec.rightWeapon];
        const WeaponDef&   lDef = k_WeaponDefs[rec.leftWeapon];

        char rBuf[64], lBuf[64];
        snprintf(rBuf, sizeof(rBuf), "R-ARM : %s", rDef.name);
        snprintf(lBuf, sizeof(lBuf), "L-ARM : %s", lDef.name);

        // AssemblyScreen (line 738-745) と同じ設定
        FontData fd;
        fd.font       = Font::Arial;
        fd.fontSize   = 20.0f;
        fd.Color      = D2D1::ColorF(0.7f, 0.9f, 1.0f, 1.0f);
        g_pDWLabel->SetFont(&fd);
        g_pDWLabel->SetScale(scaleX, scaleY);

        g_pDWLabel->DrawString(rBuf, RP_CX - 180.0f, ARM_Y0, D2D1_DRAW_TEXT_OPTIONS_NONE);
        g_pDWLabel->DrawString(lBuf, RP_CX - 180.0f, ARM_Y1, D2D1_DRAW_TEXT_OPTIONS_NONE);

        g_pDWLabel->SetScale(1.0f, 1.0f);
    }

    // フッター：InputHint バー（DrawString 後は RTV がアンバインドされているため再バインド）
    Direct3D_BindMainRenderTarget();
    InputHint_Draw(
        "{ENTER} Select  {UP}{DOWN} Move",
        "{A} Select  {DPAD_UP}{DPAD_DN} Move");
}

//------------------------------------------------------------------------------
bool ScoreCheck_IsEnd()
{
    if (g_IsEnd) { g_IsEnd = false; return true; }
    return false;
}
