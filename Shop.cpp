/*==============================================================================
   ショップ [Shop.cpp]
   Author : 51106
   Date   : 2026/06/12
==============================================================================*/
#include "Shop.h"
#include "WaveManager.h"
#include "WeaponDef.h"
#include "AssemblyScreen.h"
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
#include "model.h"
#include "ModelToon.h"
#include "Player_Camera.h"
#include "shield.h"
#include "light.h"
#include <d2d1helper.h>
#include <DirectXMath.h>
#include <cstdio>
#include <cmath>
using namespace DirectX;

namespace
{
    static XMFLOAT3 g_Pos       = {};
    static bool     g_IsOpen    = false;
    static int      g_ShopBudget = 0;   // ショップを開いた時点の所持金（購入額算出用）
    // ショップは事前アセンブリ画面と同じ選択状態を共有するため、開く前の
    // 事前アセンブリのロードアウトを退避し、閉じる時に復元する。
    // （復元しないと SaveData がショップの購入内容を「前回のアセンブリ設定」として保存してしまう）
    static int      g_PreShopR  = 0;
    static int      g_PreShopL  = 3;
    static int      g_TexShop = -1;
    static int      g_TexFont = -1;         // 「SHOP」表示用フォントアトラス（16x16グリッド）

    static MODEL*   g_pBodyModel = nullptr; // フィールド目印用ボディモデル（body.fbx）
    static double   g_AnimTime   = 0.0;     // 目印の浮遊・回転アニメ用の経過時間
    static int      g_WhiteTex = -1;
    static int      g_SeOpen   = -1;
    static int      g_SeCursor = -1;
    static int      g_SeSelect = -1;
    static int      g_SeError  = -1;

    static constexpr float INTERACT_DIST = 5.5f; // インタラクト可能距離（目印を大きくしたので広め）

    // DirectWrite 生成時の基準フォントサイズ。
    // 実際の描画サイズは DrawTextFit が枠サイズ比から拡縮するため、
    // この値自体はレイアウトに影響しない（DrawAt 内の縦中心計算の基準）。
    static constexpr float BASE_FONT_SIZE = 32.0f;

    static DirectWrite* g_pDWTitle  = nullptr;
    static DirectWrite* g_pDWItem   = nullptr;
    static DirectWrite* g_pDWInfo   = nullptr;

    // 仮想座標 (cx, cy) を中心に、仮想単位 fontSize の高さで文字列を描く。
    // SetScale は座標とグリフを同率で拡縮するため、倍率 k を掛けた分だけ
    // 座標を 1/k して渡すことで「位置はそのまま・文字だけ枠比サイズ」になる。
    // cy はグリフの縦中心（DrawAt の top = cy - 0.75*fontSize を補正済み）。
    static void DrawTextFit(DirectWrite* dw, const char* text,
                            float cx, float cy, float halfW, float fontSize,
                            const D2D1_COLOR_F& color,
                            float scaleX, float scaleY)
    {
        const float k = fontSize / BASE_FONT_SIZE;
        const float cyTop = cy + fontSize * 0.25f; // DrawAt の縦アンカーを中心へ補正
        dw->SetScale(scaleX * k, scaleY * k);
        dw->BeginBatch();
        dw->DrawAt(text, cx / k, cyTop / k, halfW / k, color);
        dw->EndBatch();
        dw->SetScale(1.0f, 1.0f);
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

    g_TexShop  = Texture_Load(L"resource/texture/white.png"); // TODO: 専用テクスチャ
    g_WhiteTex = Texture_Load(L"resource/texture/white.png");
    g_TexFont  = Texture_Load(L"Resource/Texture/consolab_ascii_512.png"); // 「SHOP」文字用

    // フィールド目印用ボディモデル（プレイヤーと同じ body.fbx を流用）
    if (!g_pBodyModel)
        g_pBodyModel = ModelLoad("resource/Models/body.fbx", 0.6f);  // フィールド目印は大きめに
    g_AnimTime = 0.0;

    if (g_SeOpen   < 0) g_SeOpen   = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeCursor < 0) g_SeCursor = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeSelect < 0) g_SeSelect = LoadAudio("resource/Sound/ui_select.wav");
    if (g_SeError  < 0) g_SeError  = LoadAudio("resource/Sound/ui_cancel.wav");

    if (!g_pDWTitle)
    {
        static FontData fd;
        fd.font = Font::AgencyFB; fd.fontWeight = DWRITE_FONT_WEIGHT_BOLD;   // メニューと同じ圧縮ミリタリー調（英字専用）
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = BASE_FONT_SIZE; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color = D2D1::ColorF(1,1,1,1);
        g_pDWTitle = new DirectWrite(&fd); g_pDWTitle->Init();
        g_pDWTitle->SetWordWrapping(false);
    }
    if (!g_pDWItem)
    {
        static FontData fd;
        fd.font = Font::Arial; fd.fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = BASE_FONT_SIZE; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        fd.Color = D2D1::ColorF(1,1,1,1);
        g_pDWItem = new DirectWrite(&fd); g_pDWItem->Init();
        g_pDWItem->SetWordWrapping(false);
    }
    if (!g_pDWInfo)
    {
        static FontData fd;
        fd.font = Font::Arial; fd.fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
        fd.fontStyle = DWRITE_FONT_STYLE_NORMAL; fd.fontStretch = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize = BASE_FONT_SIZE; fd.localeName = L"en-us";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        fd.Color = D2D1::ColorF(0.8f,0.8f,0.8f,1);
        g_pDWInfo = new DirectWrite(&fd); g_pDWInfo->Init();
        g_pDWInfo->SetWordWrapping(false);
    }
}

void Shop_Finalize()
{
    // ショップを開いたまま終了する場合はアセンブリ画面も後始末する
    if (g_IsOpen) { AssemblyScreen_SetShopMode(false, 0); AssemblyScreen_Finalize(); }

    if (g_pBodyModel) { ModelRelease(g_pBodyModel); g_pBodyModel = nullptr; }

    if (g_pDWTitle) { g_pDWTitle->Release(); delete g_pDWTitle; g_pDWTitle = nullptr; }
    if (g_pDWItem)  { g_pDWItem->Release();  delete g_pDWItem;  g_pDWItem  = nullptr; }
    if (g_pDWInfo)  { g_pDWInfo->Release();  delete g_pDWInfo;  g_pDWInfo  = nullptr; }
    UnloadAudio(g_SeOpen);   g_SeOpen   = -1;
    UnloadAudio(g_SeCursor); g_SeCursor = -1;
    UnloadAudio(g_SeSelect); g_SeSelect = -1;
    UnloadAudio(g_SeError);  g_SeError  = -1;
    g_IsOpen = false;
}

void Shop_Update(double elapsed_time)
{
    g_AnimTime += elapsed_time;   // フィールド目印の浮遊・回転アニメ

    if (!g_IsOpen)
    {
        // 近くでEキー/Aボタン→アセンブリ画面をショップとして開く
        const bool inRange = (DistToPlayer() <= INTERACT_DIST);
        if (inRange && (KeyLogger_IsTrigger(KK_E) || PadLogger_IsTrigger(PAD_A)))
        {
            // 事前アセンブリのロードアウトを退避（閉じる時に復元してセーブ汚染を防ぐ）
            g_PreShopR = static_cast<int>(AssemblyScreen_GetRightWeapon());
            g_PreShopL = static_cast<int>(AssemblyScreen_GetLeftWeapon());

            // 装備中の武器を初期値、所持金を予算にしてショップモードで開く
            AssemblyScreen_SetDefaults(
                static_cast<WeaponID>(Player_GetRightWeaponIndex()),
                static_cast<WeaponID>(Player_GetLeftWeaponIndex()));
            AssemblyScreen_Initialize();                       // ※ SetDefaults の後
            g_ShopBudget = WaveManager_GetCredits();
            AssemblyScreen_SetShopMode(true, g_ShopBudget);    // ※ Initialize の後

            g_IsOpen = true;
            PlayAudio(g_SeOpen, false);
        }
        return;
    }

    // ショップ表示中はアセンブリ画面（ショップモード）を駆動する。
    // 開いている間はゲームが凍結されるためプレイヤーは移動できない（＝距離で閉じない）。
    if (AssemblyScreen_Update(elapsed_time))
    {
        if (!AssemblyScreen_WasCancelled())
        {
            // 購入確定：装備中から変更したぶんの差額を消費し、両腕を装備する
            const int spent = g_ShopBudget - AssemblyScreen_GetRemainingCredits();
            if (spent > 0) WaveManager_SpendCredits(spent);

            Player_SetNormalWeaponIndex(static_cast<int>(AssemblyScreen_GetRightWeapon()));
            Player_SetLeftWeaponIndex  (static_cast<int>(AssemblyScreen_GetLeftWeapon()));
            PlayAudio(g_SeSelect, false);
        }

        AssemblyScreen_Finalize();
        AssemblyScreen_SetShopMode(false, 0);
        // 事前アセンブリのロードアウトを復元（ショップの購入内容をセーブに残さない）。
        // 購入はすでに Player 側へ適用済みなので、この復元は装備に影響しない。
        AssemblyScreen_SetDefaults(static_cast<WeaponID>(g_PreShopR),
                                   static_cast<WeaponID>(g_PreShopL));
        g_IsOpen = false;
    }
}

//==============================================================================
// フィールド上のショップ目印（body＋シールド球）を描画する。
//
// ■必ず Game_Draw() の「3Dワールドパス内」（プレイヤー描画の直後・HUD/2Dパスの前）
//   から呼ぶこと。理由：
//   ・壁の深度がまだ深度バッファに残っている＝壁に正しく隠れる
//     （HUD はミニプレビュー用に Direct3D_ClearDepth() で深度を消すため、
//      それより後に描くと壁を透過してしまう）
//   ・2Dパスやブレンド/深度ステートを壊さない（ポーズ画面の暗転が正しく効く）
//==============================================================================
void Shop_DrawWorld()
{
    if (g_IsOpen)      return;   // メニュー表示中は世界の目印は不要
    if (!g_pBodyModel) return;

    // ── 浮遊アニメ（ゆっくり回転＋上下バウンド）──
    const float t    = static_cast<float>(g_AnimTime);
    const float bob  = sinf(t * 2.0f) * 0.2f;       // 上下の揺れ幅 ±0.2m
    const float spin = t * 0.8f;                     // Y回転 約0.8rad/秒
    const float baseY = g_Pos.y + 0.5f + bob;        // 全体的に低めに配置

    // ライティング（アセンブリ画面のプレビューと同系）
    Light_SetSpecularWorld(Player_Camera_GetPosition(), 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
    Light_SetAmbient({ 0.5f, 0.5f, 0.5f });

    // ボディをショップ位置に描画（Y回転で回す）
    const XMMATRIX world =
        XMMatrixRotationY(spin) *
        XMMatrixTranslation(g_Pos.x, baseY, g_Pos.z);
    ModelDrawToon(g_pBodyModel, world);

    // 胴体を包むシールド球体（body の AABB から中心・半径を算出）
    {
        const AABB  bb  = ModelGetAABB(g_pBodyModel, { g_Pos.x, baseY, g_Pos.z });
        const float cy  = (bb.min.y + bb.max.y) * 0.5f;
        float ext = bb.max.x - bb.min.x;
        const float ey  = bb.max.y - bb.min.y;
        const float ez  = bb.max.z - bb.min.z;
        if (ey > ext) ext = ey;
        if (ez > ext) ext = ez;

        const XMFLOAT3 center = { g_Pos.x, cy, g_Pos.z };
        const float    radius = ext * 0.75f;

        Shield_DrawAt(center,
                      Player_Camera_GetViewMatrix(),
                      Player_Camera_GetProjectionMatrix(),
                      radius);
    }

    // ── 「SHOP」を3Dビルボードで表示 ──
    // フォントアトラス（16x16グリッド）の各文字をビルボード描画する。
    // 3Dパスなので深度が効き（壁で隠れる）、遠近でサイズが変わり、HUD/UIより奥に描かれる。
    // 加算合成：アトラスの黒背景を透過させ、光る文字にする。
    if (g_TexFont >= 0)
    {
        const char* label = "SHOP";
        const int   n  = 4;
        const float GW = 0.7f, GH = 0.95f, SP = 0.55f;   // 文字サイズ・間隔（ワールド単位）
        const float labelY = g_Pos.y + 2.0f + bob;        // 目印の上（下げ済み）

        // カメラの水平右ベクトル（文字を横一列に並べる）
        const XMFLOAT3 camF = Player_Camera_GetFront();
        XMVECTOR fwd   = XMVector3Normalize(XMVectorSet(camF.x, 0.0f, camF.z, 0.0f));
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), fwd));
        XMFLOAT3 r; XMStoreFloat3(&r, right);

        Direct3D_SetDepthEnable(true);
        Direct3D_SetBlendStateAdditive(true);
        for (int i = 0; i < n; ++i)
        {
            const int   idx = static_cast<int>(label[i]) - ' ';      // アトラスは ' ' 始まり
            const float u   = static_cast<float>(idx % 16) * 32.0f;  // 512/16=32px セル
            const float v   = static_cast<float>(idx / 16) * 32.0f;
            const float off = (static_cast<float>(i) - (n - 1) * 0.5f) * SP;
            const XMFLOAT3 pos = { g_Pos.x + r.x * off, labelY, g_Pos.z + r.z * off };
            Billboard_Draw(g_TexFont, pos, XMFLOAT2{ GW, GH },
                           XMFLOAT4{ 0.5f, 0.9f, 1.0f, 1.0f },
                           XMFLOAT4{ u, v, 32.0f, 32.0f });
        }
        Direct3D_SetBlendStateAdditive(false);
    }

    // ライトを既定へ戻す（Player_Draw 末尾と同様）
    Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
}

void Shop_DrawUI()
{
    if (!g_IsOpen) return;

    // ショップ本体はアセンブリ画面（ショップモード）をそのまま描画する
    AssemblyScreen_Draw();

    // 操作ヒント（アセンブリ準拠。ショップなので Back → Close）
    InputHint_Draw(
        "{W}{S} Move    {ENTER} Set / Buy    {TAB} Switch    {ESC} Close",
        "{DPAD_UP}{DPAD_DN} Move    {A} Set / Buy    {LB}{RB} Switch    {B} Close");
}

//==============================================================================
// 近接プロンプト：ショップの近くにいてメニュー未表示のとき、
// 「E / A で開く」案内を画面2Dに表示する。
//==============================================================================
void Shop_DrawPrompt()
{
    if (g_IsOpen) return;                          // メニュー表示中は不要
    if (DistToPlayer() > INTERACT_DIST) return;    // 近接時のみ

    // 中央ロックオン照準（画面中心 800,450）の右下・外側にオフセット配置。
    // OFFSET_X / OFFSET_Y を増やすほど照準から外（右・下）へ離れる。
    constexpr float CENTER_X = 800.0f;   // 画面中央X（仮想1600の半分＝照準中心）
    constexpr float CENTER_Y = 450.0f;   // 画面中央Y（仮想900の半分＝照準中心）
    constexpr float OFFSET_X = 220.0f;   // 右方向オフセット（外へ）
    constexpr float OFFSET_Y = 200.0f;   // 下方向オフセット（外へ）
    const float px = CENTER_X + OFFSET_X;
    const float py = CENTER_Y + OFFSET_Y;

    // チュートリアル風：文言＋ボタンアイコン。KB=Eキー / パッド=Aボタンで自動切替。
    // scale で下部バーより大きめに表示。
    InputHint_DrawAt("{E} SHOP", "{A} SHOP", px, py, 1.6f);
}

bool Shop_IsOpen() { return g_IsOpen; }
