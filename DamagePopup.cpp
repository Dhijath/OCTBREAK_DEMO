/*==============================================================================

   ダメージポップアップ [DamagePopup.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/13
--------------------------------------------------------------------------------
   ・suji.png (32×35px × 10桁) で数字をスクリーン描画
   ・ワールド座標 → スクリーン変換: XMVector3Project
   ・上昇速度 1.5 m/s、生存 1.0 秒、alpha = life^2（後半に向けて急速フェード）
   ・色: ~99 = 白、100~499 = 黄、500~ = 橙赤

==============================================================================*/
#include "DamagePopup.h"
#include "sprite.h"
#include "texture.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "Player_Camera.h"
#include <DirectXMath.h>
#include <d2d1helper.h>
#include <cmath>
#include <cstdio>

using namespace DirectX;

namespace
{
    static constexpr int   MAX_POPUPS = 64;
    static constexpr float POPUP_LIFE = 1.0f;   // 生存時間（秒）
    static constexpr float POPUP_RISE = 1.5f;   // 上昇速度（m/s）

    struct Popup
    {
        XMFLOAT3 worldPos;   // ワールド座標（毎フレーム上昇）
        int      damage;     // ダメージ量
        float    life;       // 残り寿命 1.0 → 0.0
        bool     active;     // 使用中フラグ
    };

    static Popup       g_Popups[MAX_POPUPS];
    static DirectWrite* g_pDW = nullptr;    // ダメージ数字描画用 DirectWrite
}

//==============================================================================
// 初期化
//==============================================================================
void DamagePopup_Initialize()
{
    for (auto& p : g_Popups)
        p.active = false;

    if (!g_pDW)
    {
        static FontData fd;
        fd.font          = Font::Arial;
        fd.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fd.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fd.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize      = 22.0f;
        fd.localeName    = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDW = new DirectWrite(&fd);
        g_pDW->Init();
    }
}

//==============================================================================
// 終了
//==============================================================================
void DamagePopup_Finalize()
{
    for (auto& p : g_Popups)
        p.active = false;

    if (g_pDW) { g_pDW->Release(); delete g_pDW; g_pDW = nullptr; }
}

//==============================================================================
// ポップアップ追加
//==============================================================================
void DamagePopup_Add(const XMFLOAT3& worldPos, int damage)
{
    // 空きスロットに登録
    for (auto& p : g_Popups)
    {
        if (!p.active)
        {
            p.worldPos = worldPos;
            p.damage   = (damage > 0) ? damage : 1;
            p.life     = 1.0f;
            p.active   = true;
            return;
        }
    }
    // スロット満杯時は先頭を上書き
    g_Popups[0].worldPos = worldPos;
    g_Popups[0].damage   = (damage > 0) ? damage : 1;
    g_Popups[0].life     = 1.0f;
    g_Popups[0].active   = true;
}

//==============================================================================
// 更新（上昇 + 寿命減算）
//==============================================================================
void DamagePopup_Update(float dt)
{
    for (auto& p : g_Popups)
    {
        if (!p.active) continue;
        p.life -= dt / POPUP_LIFE;
        if (p.life <= 0.0f) { p.active = false; continue; }
        p.worldPos.y += POPUP_RISE * dt;   // 上昇
    }
}

//==============================================================================
// 描画（ワールド→スクリーン変換 + UV カット描画）
//==============================================================================
void DamagePopup_Draw()
{
    if (!g_pDW) return;

    const float W = static_cast<float>(Direct3D_GetBackBufferWidth());
    const float H = static_cast<float>(Direct3D_GetBackBufferHeight());

    XMMATRIX view = XMLoadFloat4x4(&Player_Camera_GetViewMatrix());
    XMMATRIX proj = XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix());

    // D3D11 RTV をアンバインド → D2D BeginDraw
    // （BeginBatch 内で自動的に OMSetRenderTargets(0,...) を呼ぶ）
    g_pDW->BeginBatch();

    for (auto& p : g_Popups)
    {
        if (!p.active) continue;

        // ワールド → スクリーン変換（実解像度で投影）
        XMVECTOR wPos = XMLoadFloat3(&p.worldPos);
        XMVECTOR sc   = XMVector3Project(
            wPos, 0.0f, 0.0f, W, H, 0.0f, 1.0f,
            proj, view, XMMatrixIdentity()
        );
        const float sx = XMVectorGetX(sc);
        const float sy = XMVectorGetY(sc);
        const float sz = XMVectorGetZ(sc);

        // 背面・画面外はスキップ（実ピクセル座標で判定）
        if (sz <= 0.0f || sz >= 1.0f)        continue;
        if (sx < -200.0f || sx > W + 200.0f) continue;
        if (sy < -200.0f || sy > H + 200.0f) continue;

        const float alpha = p.life * p.life; // 二乗フェード（後半急速フェード）

        // ダメージ量による色分け
        D2D1_COLOR_F col;
        if      (p.damage >= 500) col = D2D1::ColorF(1.0f, 0.4f, 0.1f, alpha); // 橙赤
        else if (p.damage >= 100) col = D2D1::ColorF(1.0f, 1.0f, 0.0f, alpha); // 黄
        else                      col = D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha); // 白

        // 数字文字列を生成して実ピクセル座標に描画
        // （DirectWrite は実ピクセル空間で描くため仮想座標変換不要）
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", p.damage);
        g_pDW->DrawAt(buf, sx, sy, 80.0f, col, 1.5f); // 縁取り付き（背景に溶け込まない）
    }

    // D2D EndDraw → D3D11 RTV を再バインド
    g_pDW->EndBatch();
}
