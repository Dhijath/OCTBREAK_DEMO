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
#include "Player_Camera.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

namespace
{
    static constexpr int   MAX_POPUPS = 64;
    static constexpr float POPUP_LIFE = 1.0f;   // 生存時間（秒）
    static constexpr float POPUP_RISE = 1.5f;   // 上昇速度（m/s）
    static constexpr int   SRC_W      = 32;     // suji.png 1桁の幅（px）
    static constexpr int   SRC_H      = 35;     // suji.png 1桁の高さ（px）
    static constexpr float DRAW_W     = 20.0f;  // 画面上の描画幅
    static constexpr float DRAW_H     = 26.0f;  // 画面上の描画高さ

    struct Popup
    {
        XMFLOAT3 worldPos;   // ワールド座標（毎フレーム上昇）
        int      damage;     // ダメージ量
        float    life;       // 残り寿命 1.0 → 0.0
        bool     active;     // 使用中フラグ
    };

    static Popup g_Popups[MAX_POPUPS];
    static int   g_TexSuji = -1;
}

//==============================================================================
// 初期化
//==============================================================================
void DamagePopup_Initialize()
{
    for (auto& p : g_Popups)
        p.active = false;

    g_TexSuji = Texture_Load(L"resource/texture/suji.png");
}

//==============================================================================
// 終了
//==============================================================================
void DamagePopup_Finalize()
{
    for (auto& p : g_Popups)
        p.active = false;
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
    if (g_TexSuji < 0) return;

    const float W = static_cast<float>(Direct3D_GetBackBufferWidth());
    const float H = static_cast<float>(Direct3D_GetBackBufferHeight());
    const float scaleX = static_cast<float>(SPRITE_SCREEN_W) / W;
    const float scaleY = static_cast<float>(SPRITE_SCREEN_H) / H;

    XMMATRIX view = XMLoadFloat4x4(&Player_Camera_GetViewMatrix());
    XMMATRIX proj = XMLoadFloat4x4(&Player_Camera_GetProjectionMatrix());

    Sprite_Begin(); // 2D パイプラインをセット

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

        // 背面・画面外はスキップ（実座標で判定）
        if (sz <= 0.0f || sz >= 1.0f)            continue;
        if (sx < -200.0f || sx > W + 200.0f)     continue;
        if (sy < -200.0f || sy > H + 200.0f)     continue;

        // 仮想座標系に変換（スプライトは1600×900空間で描画）
        const float vsx = sx * scaleX;
        const float vsy = sy * scaleY;

        const float alpha = p.life * p.life; // 二乗フェード

        // ダメージ量による色分け
        XMFLOAT4 col;
        if      (p.damage >= 500) col = { 1.0f, 0.4f, 0.1f, alpha }; // 橙赤
        else if (p.damage >= 100) col = { 1.0f, 1.0f, 0.0f, alpha }; // 黄
        else                      col = { 1.0f, 1.0f, 1.0f, alpha }; // 白

        // 桁数配列生成（最下位から格納）
        int digitArr[10] = {};
        int dCount = 0;
        int tmp = p.damage;
        while (tmp > 0 && dCount < 10)
        {
            digitArr[dCount++] = tmp % 10;
            tmp /= 10;
        }

        // 数字列全体の中心を (vsx, vsy) に合わせて左から右へ描画
        const float totalW = dCount * DRAW_W;
        const float startX = vsx - totalW * 0.5f;

        for (int i = dCount - 1; i >= 0; --i)
        {
            const float dx = startX + static_cast<float>(dCount - 1 - i) * DRAW_W;
            Sprite_Draw(
                g_TexSuji,
                dx,
                vsy - DRAW_H * 0.5f,
                DRAW_W, DRAW_H,
                SRC_W * digitArr[i], 0, SRC_W, SRC_H,
                col
            );
        }
    }
}
