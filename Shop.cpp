/*==============================================================================
   ショップ [Shop.cpp]
   Author : 51106
   Date   : 2026/06/12
==============================================================================*/
#include "Shop.h"
#include "WaveManager.h"
#include "WeaponDef.h"
#include "player.h"
#include "key_logger.h"
#include "pad_logger.h"
#include "UIInput.h"
#include "billboard.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "audio.h"
#include "input_hint.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>
using namespace DirectX;

namespace
{
    static XMFLOAT3 g_Pos     = {};
    static bool     g_IsOpen  = false;
    static int      g_Cursor  = 0;
    static int      g_TexShop = -1;
    static int      g_WhiteTex = -1;
    static int      g_SeOpen   = -1;
    static int      g_SeCursor = -1;
    static int      g_SeSelect = -1;
    static int      g_SeError  = -1;

    static constexpr float INTERACT_DIST = 4.0f; // インタラクト可能距離

    static DirectWrite* g_pDWTitle  = nullptr;
    static DirectWrite* g_pDWItem   = nullptr;
    static DirectWrite* g_pDWInfo   = nullptr;

    static bool IsShopping()
    {
        return WaveManager_GetPhase() == WavePhase::Shopping;
    }

    static float DistToPlayer()
    {
        const XMFLOAT3 p = Player_GetPosition();
        const float dx = p.x - g_Pos.x;
        const float dz = p.z - g_Pos.z;
        return sqrtf(dx*dx + dz*dz);
    }
}

void Shop_Initialize(const XMFLOAT3& pos)
{
    g_Pos    = pos;
    g_IsOpen = false;
    g_Cursor = 0;

    g_TexShop  = Texture_Load(L"resource/texture/white.png"); // TODO: 専用テクスチャ
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");

    if (g_SeOpen   < 0) g_SeOpen   = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCursor < 0) g_SeCursor = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect < 0) g_SeSelect = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeError  < 0) g_SeError  = LoadAudio("resource/Sound/ui_cancel.wav");

    if (!g_pDWTitle)
    {
        static FontData fd;
        fd.font = Font::Arial; fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = 36.0f; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color = D2D1::ColorF(1,1,1,1);
        g_pDWTitle = new DirectWrite(&fd); g_pDWTitle->Init();
    }
    if (!g_pDWItem)
    {
        static FontData fd;
        fd.font = Font::Arial; fd.fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = 28.0f; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        fd.Color = D2D1::ColorF(1,1,1,1);
        g_pDWItem = new DirectWrite(&fd); g_pDWItem->Init();
    }
    if (!g_pDWInfo)
    {
        static FontData fd;
        fd.font = Font::Arial; fd.fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = 22.0f; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        fd.Color = D2D1::ColorF(0.8f,0.8f,0.8f,1);
        g_pDWInfo = new DirectWrite(&fd); g_pDWInfo->Init();
    }
}

void Shop_Finalize()
{
    if (g_pDWTitle) { g_pDWTitle->Release(); delete g_pDWTitle; g_pDWTitle = nullptr; }
    if (g_pDWItem)  { g_pDWItem->Release();  delete g_pDWItem;  g_pDWItem  = nullptr; }
    if (g_pDWInfo)  { g_pDWInfo->Release();  delete g_pDWInfo;  g_pDWInfo  = nullptr; }
    UnloadAudio(g_SeOpen);   g_SeOpen   = -1;
    UnloadAudio(g_SeCursor); g_SeCursor = -1;
    UnloadAudio(g_SeSelect); g_SeSelect = -1;
    UnloadAudio(g_SeError);  g_SeError  = -1;
    g_IsOpen = false;
}

void Shop_Update(double)
{
    if (!IsShopping())
    {
        g_IsOpen = false;
        return;
    }

    const bool inRange = (DistToPlayer() <= INTERACT_DIST);

    if (!g_IsOpen)
    {
        // 近くでEキー/Aボタン→開く
        if (inRange && (KeyLogger_IsTrigger(KK_E) || PadLogger_IsTrigger(PAD_A)))
        {
            g_IsOpen = true;
            g_Cursor = 0;
            PlayAudio(g_SeOpen, false);
        }
        return;
    }

    // ショップ操作
    if (UI_IsMoveUp())
    {
        g_Cursor = (g_Cursor + WEAPON_COUNT - 1) % WEAPON_COUNT;
        PlayAudio(g_SeCursor, false);
    }
    if (UI_IsMoveDown())
    {
        g_Cursor = (g_Cursor + 1) % WEAPON_COUNT;
        PlayAudio(g_SeCursor, false);
    }

    if (UI_IsConfirm())
    {
        const int cost = k_WeaponDefs[g_Cursor].cost;
        if (WaveManager_SpendCredits(cost))
        {
            // 右腕に装備（後で左右選択も追加できる）
            Player_SetNormalWeaponIndex(g_Cursor);
            PlayAudio(g_SeSelect, false);
        }
        else
        {
            PlayAudio(g_SeError, false); // クレジット不足
        }
    }

    if (UI_IsCancel())
    {
        g_IsOpen = false;
    }
}

void Shop_Draw()
{
    // ショップのビルボード表示（スポーン地点に目印）
    if (g_TexShop >= 0)
    {
        const XMFLOAT3 drawPos = { g_Pos.x, g_Pos.y + 2.0f, g_Pos.z };
        Billboard_Draw(g_TexShop, drawPos,
                       XMFLOAT2{ 2.0f, 2.0f },
                       XMFLOAT4{ 0.3f, 0.8f, 1.0f, 0.9f },
                       XMFLOAT4{ 0, 0,
                           (float)Texture_Width(g_TexShop),
                           (float)Texture_Height(g_TexShop) });
    }
}

void Shop_DrawUI()
{
    if (!g_IsOpen) return;
    if (!g_pDWTitle || !g_pDWItem || !g_pDWInfo) return;

    const float sw = (float)SPRITE_SCREEN_W;
    const float sh = (float)SPRITE_SCREEN_H;
    const float scaleX = (float)Direct3D_GetBackBufferWidth()  / 1600.0f;
    const float scaleY = (float)Direct3D_GetBackBufferHeight() / 900.0f;

    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();

    // パネル（画面比率で管理）
    const float PNL_W   = sw * 0.38f;
    const float PNL_H   = sh * 0.70f;
    const float px      = sw * 0.5f - PNL_W * 0.5f;
    const float py      = sh * 0.5f - PNL_H * 0.5f;
    const float cx      = sw * 0.5f;
    const float ROW_H   = PNL_H / (WEAPON_COUNT + 1.5f);
    const float TITLE_H = ROW_H * 0.9f;
    const float LIST_Y  = py + TITLE_H + ROW_H * 0.3f;
    const float LABEL_X = px + PNL_W * 0.12f;
    const float PRICE_X = px + PNL_W * 0.88f;

    if (g_WhiteTex >= 0)
    {
        Sprite_Draw(g_WhiteTex, px, py, PNL_W, PNL_H, XMFLOAT4(0,0,0,0.8f));
        Sprite_Draw(g_WhiteTex, px,           py,          PNL_W, 2, XMFLOAT4(1,1,1,0.5f));
        Sprite_Draw(g_WhiteTex, px,           py+PNL_H-2,  PNL_W, 2, XMFLOAT4(1,1,1,0.5f));
        Sprite_Draw(g_WhiteTex, px,           py,          2, PNL_H, XMFLOAT4(1,1,1,0.5f));
        Sprite_Draw(g_WhiteTex, px+PNL_W-2,  py,          2, PNL_H, XMFLOAT4(1,1,1,0.5f));
        // タイトル下区切り線
        Sprite_Draw(g_WhiteTex, px + PNL_W*0.05f, py + TITLE_H,
                    PNL_W * 0.9f, 1, XMFLOAT4(1,1,1,0.3f));
    }

    // タイトル（所持金表示）
    char creditBuf[32];
    snprintf(creditBuf, sizeof(creditBuf), "SHOP   CR: %d", WaveManager_GetCredits());
    g_pDWTitle->SetScale(scaleX, scaleY);
    g_pDWTitle->BeginBatch();
    g_pDWTitle->DrawAt(creditBuf, cx, py + TITLE_H * 0.25f, PNL_W * 0.45f,
                       D2D1::ColorF(0.4f, 0.9f, 1.0f, 1.0f), 1.0f);
    g_pDWTitle->EndBatch();
    g_pDWTitle->SetScale(1.0f, 1.0f);

    // 武器リスト
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        const float ry  = LIST_Y + i * ROW_H;
        const bool  sel = (i == g_Cursor);

        if (sel && g_WhiteTex >= 0)
            Sprite_Draw(g_WhiteTex, px + 2, ry, PNL_W - 4, ROW_H - 2, XMFLOAT4(1,1,1,0.12f));

        const D2D1_COLOR_F nameCol = sel
            ? D2D1::ColorF(1.0f, 0.9f, 0.3f, 1.0f)
            : D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);

        char priceBuf[32];
        snprintf(priceBuf, sizeof(priceBuf), "CR %d", k_WeaponDefs[i].cost);

        const float textY = ry + ROW_H * 0.25f;

        g_pDWItem->SetScale(scaleX, scaleY);
        g_pDWItem->BeginBatch();
        g_pDWItem->DrawAt(k_WeaponDefs[i].name, LABEL_X + PNL_W*0.15f, textY,
                          PNL_W * 0.28f, nameCol, 1.0f);
        g_pDWItem->EndBatch();
        g_pDWItem->SetScale(1.0f, 1.0f);

        g_pDWInfo->SetScale(scaleX, scaleY);
        g_pDWInfo->BeginBatch();
        g_pDWInfo->DrawAt(priceBuf, PRICE_X, textY, PNL_W * 0.22f,
                          D2D1::ColorF(0.7f, 0.9f, 0.5f, 1.0f), 1.0f);
        g_pDWInfo->EndBatch();
        g_pDWInfo->SetScale(1.0f, 1.0f);
    }

    Direct3D_SetDepthEnable(true);

    // ヒント
    Direct3D_BindMainRenderTarget();
    InputHint_Draw(
        "{UP}{DOWN} Move    {ENTER} Buy    {ESC} Close",
        "{DPAD_UP}{DPAD_DN} Move    {A} Buy    {B} Close");
}

bool Shop_IsOpen() { return g_IsOpen; }
