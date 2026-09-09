/*==============================================================================

   ミニマップ制御 [Minimap.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

==============================================================================*/

#include "player.h"
#include "direct3d.h"
#include "player_camera.h"
#include "map.h"
#include "sprite.h"
#include "Minimap.h"
#include "texture.h"
#include "game.h"
#include "SciFiUI.h"
#include <cmath>
#include <cwchar>


//==============================================================================
// ミニマップ：3D描画
//==============================================================================

int g_MinimapframeTexID = -1;  //ミニマップのフレーム用テクスチャ

// 真上から見下ろすカメラの設定（3D描画と、範囲外マーカーの位置計算で共有する）
static constexpr float MINIMAP_CAMERA_HEIGHT = 100.0f;
static constexpr float MINIMAP_RANGE         = 60.0f;   // 平行投影の横幅の半分（m）

void Minimap_Initialize()
{
}

//==============================================================================
// 範囲外マーカー
//
// ■役割
// ・ミニマップに映っていない目標（ゴール・敵）の方角を、枠の縁の小さな四角で示す
// ・ミニマップはプレイヤー中心・北（+Z）が上・東（+X）が右
//
// ■引数
// ・target         : 目標のワールド座標
// ・sx, sy, size   : ミニマップ本体の左上座標と一辺（仮想 1600×900 座標）
// ・halfVisible    : ミニマップに映る範囲の半分（m）
// ・markSize, color: マーカーの一辺と色
//==============================================================================
static void DrawOffMapMarker(const DirectX::XMFLOAT3& target,
    float sx, float sy, float size, float halfVisible,
    float markSize, const DirectX::XMFLOAT4& color)
{
    const DirectX::XMFLOAT3 center = Player_GetPosition();
    float u = (target.x - center.x) / halfVisible;   // -1〜1 が映っている範囲
    float v = (target.z - center.z) / halfVisible;

    const float m = (fabsf(u) > fabsf(v)) ? fabsf(u) : fabsf(v);
    if (m <= 1.0f) return;   // 映っているので不要（3D側のマーカーが見えている）

    // 中心から目標へ向かう線が枠と交わる点へ寄せる
    u /= m;
    v /= m;

    const float px = sx + (u * 0.5f + 0.5f) * size;
    const float py = sy + (0.5f - v * 0.5f) * size;
    Sprite_Draw(Map_GetWiteTexID(), px - markSize * 0.5f, py - markSize * 0.5f, markSize, markSize, color);
}

void MiniMap_Render3D()
{
    Direct3D_BeginOffScreen();
    const DirectX::XMFLOAT3 center = Player_GetPosition();
    Player_Camera_SetMiniMapTopDown(center, MINIMAP_CAMERA_HEIGHT, MINIMAP_RANGE);

    Map_DrawForMinimap();
    Player_DrawMarker();
    Game_DrawEnemyMarkers();

    Player_Camera_ApplyMainViewProj();
    Direct3D_EndOffScreen();
}


//==============================================================================
// ミニマップ：2D描画
//==============================================================================
void MiniMap_Draw2D()
{
    //============================================================
    // 2D描画用にレンダーステートを調整
    //============================================================

    // ミニマップはUIなので深度テストを無効化
    // → 3DオブジェクトとのZ競合を防ぐ
    Direct3D_SetDepthEnable(false);

    // PNGフレームやミニマップ表示のためαブレンドを有効化
    Direct3D_SetBlendState(true);

    //============================================================
    // ミニマップ描画元（オフスクリーン）のSRV取得
    //============================================================

    // 事前に描画してあるミニマップ用オフスクリーンテクスチャ
    ID3D11ShaderResourceView* srv = Direct3D_GetOffScreenSRV();

    // SRVが無効な場合は何も描画せず終了
    if (!srv) return;

    //============================================================
    // 画面サイズ・配置パラメータ計算
    //============================================================

    // バックバッファ横幅（右上配置用）
    const float screenW = (float)SPRITE_SCREEN_W;

    // ミニマップ本体の表示サイズ（正方形）
    const float mapSize = 270.0f;

    // 画面端からの余白
    const float margin = 20.0f;

    // ミニマップ本体の左上座標（右上配置）
    const float sx = screenW - mapSize - margin;
    const float sy = margin;

    // フレーム用の余白サイズ
    // フレームをミニマップより一回り大きくするための値
    const float fsxy = 100.0f;

    // フレーム全体の描画サイズ
    const float frameSize = mapSize + fsxy;

    // フレームの左上座標
    // ミニマップの中心にフレームが来るよう半分ずらす
    const float frameSx = sx - fsxy / 2;
    const float frameSy = sy - fsxy / 2;



    //============================================================
    // ミニマップ本体描画（SRV直接描画）
    //============================================================

    // → UV指定描画に対応した描画モード
    Sprite_BeginSquare();

    // オフスクリーンが16:9想定のため、
    // 横方向をトリミングして正方形に切り出す
    float uvLeft = (1.0f - 9.0f / 16.0f) * 0.5f;
    float uvRight = 1.0f - uvLeft;

    // SRVを指定UV範囲で描画
    // αブレンドでオフスクリーンを合成
    ID3D11Device* dev = Direct3D_GetDevice();
    ID3D11DeviceContext* ctx = Direct3D_GetContext();

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* additiveBlend = nullptr;
    dev->CreateBlendState(&blendDesc, &additiveBlend);

    ID3D11BlendState* oldBlend = nullptr;
    FLOAT oldFactor[4];
    UINT  oldMask;
    ctx->OMGetBlendState(&oldBlend, oldFactor, &oldMask);

    ctx->OMSetBlendState(additiveBlend, nullptr, 0xFFFFFFFF);

    Sprite_DrawSRV_UV(
        srv,
        sx, sy,
        mapSize, mapSize,
        uvLeft, 0.0f,
        uvRight, 1.0f,
        { 1,1,1,1 }
    );

    // ブレンドステートを元に戻す
    ctx->OMSetBlendState(oldBlend, oldFactor, oldMask);
    SAFE_RELEASE(additiveBlend);
    SAFE_RELEASE(oldBlend);
    // 通常スプライト描画開始
    Sprite_Begin();

    //============================================================
    // 範囲外マーカー（映っていないゴール・敵の方角を枠の縁に表示）
    //============================================================
    {
        // オフスクリーンは横 MINIMAP_RANGE×2 を映し、上の UV 指定で中央の 9/16 を
        // 切り出して表示しているので、実際に映る範囲はその 9/16 になる
        const float halfVisible = MINIMAP_RANGE * (9.0f / 16.0f);

        DirectX::XMFLOAT3 enemyPos;
        for (int i = 0; Game_GetEnemyPosition(i, &enemyPos); ++i)
            DrawOffMapMarker(enemyPos, sx, sy, mapSize, halfVisible, 7.0f, { 1.0f, 0.25f, 0.2f, 0.9f });

        // ゴールは敵より大きく、点滅させて目立たせる
        if (Map_HasGoal())
        {
            static int s_Blink = 0;
            s_Blink = (s_Blink + 1) % 40;
            const float a = (s_Blink < 28) ? 1.0f : 0.35f;
            DrawOffMapMarker(Map_GetGoalPosition(), sx, sy, mapSize, halfVisible, 12.0f, { 0.3f, 0.95f, 1.0f, a });
        }

        //============================================================
        // SF調の重ね表示：照準線・目盛り・四隅のブラケット・方位と範囲
        //============================================================
        using namespace SciFiUI;
        const float cx = sx + mapSize * 0.5f;
        const float cy = sy + mapSize * 0.5f;
        Fill(sx, cy, mapSize, 1.0f, WithAlpha(kCyan, 0.18f));
        Fill(cx, sy, 1.0f, mapSize, WithAlpha(kCyan, 0.18f));
        Ticks(sx, sy + mapSize - 8.0f, mapSize, 11, 5, WithAlpha(kCyan, 0.45f));
        Brackets(sx - 6.0f, sy - 6.0f, mapSize + 12.0f, mapSize + 12.0f, 14.0f, WithAlpha(kCyan, 0.85f));

        wchar_t range[32];
        swprintf_s(range, L"RADAR  ±%dm", static_cast<int>(halfVisible));
        Text(L"N", cx, sy + 4.0f, 14.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Center, true, 1.0f);
        Text(range, sx + mapSize, sy + mapSize + 10.0f, 12.0f, ToD2D(kCyan, 0.8f), UIFont::Mono, UIAlign::Right, true);
        FlushText();
    }


    //============================================================
    // レンダーステート復帰
    //============================================================

    // 以降の3D描画のため深度テストを再度有効化
    Direct3D_SetDepthEnable(true);
}
