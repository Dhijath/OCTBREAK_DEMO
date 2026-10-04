/*==============================================================================

   アセンブル画面 [AssemblyScreen.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01（2026/10/03 タブ式に拡張）
--------------------------------------------------------------------------------

   ■タブ（左右キーで切り替え。ショップでは「武装」のみ）
     WEAPON   武装     : R-ARM / L-ARM の武器
     FRAME    機体     : 頭・胴体・脚部（見た目と性能が変わる）
     INTERNAL 内部パーツ: 2スロット（速度・攻撃力・AP の強化。見た目は変わらない）

   ■操作
     上下 : 項目の移動（タブ内の全セクション＋READY を通した1本の縦リスト）
     TAB / LB / RB : 次のセクションへ
     決定 : ホバー中の項目をそのセクションに装備 / READY で出撃
     ESC  : 前の画面へ（機体・内部パーツの変更は READY を押したときに確定する）

   ■レイアウト（1600×900）
     Header (y=0〜64)              : 画面名・タブ・残クレジット
     Left   (x=24,   w=250)        : セクションごとの候補リスト・READY ボタン
     Center (x=290,  w=700)        : ホバー中の項目（3Dプレビュー・性能・説明）
     Right  (x=1006, w=570)        : 組み上がった機体の3Dプレビュー・装備一覧・合計性能・予算

==============================================================================*/
#include "AssemblyScreen.h"
#include "WeaponDef.h"
#include "MechParts.h"
#include "audio.h"
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
#include "SciFiUI.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

using namespace DirectX;

//==============================================================================
// 定数
//==============================================================================
static constexpr int   INITIAL_CREDITS = 200000;
static constexpr int   MAX_SAVED_CREDITS = 99999999;   // 持ち越しクレジットの上限（表示桁あふれ防止）
static constexpr float SW = 1600.0f;
static constexpr float SH = 900.0f;

static constexpr float HEAD_H  = 64.0f;
static constexpr float PANEL_Y = 80.0f;    // パネルの上端
static constexpr float PANEL_B = 792.0f;   // パネルの下端（下部ヒントバーの上）

static constexpr float LEFT_X = 24.0f,   LEFT_W = 250.0f;
static constexpr float CNT_X  = 290.0f,  CNT_W  = 700.0f;
static constexpr float RGT_X  = 1006.0f, RGT_W  = 570.0f;

// 左パネル：リスト領域と READY ボタン
static constexpr float LP_LIST_TOP = 92.0f;
static constexpr float LP_READY_X  = LEFT_X + 12.0f;
static constexpr float LP_READY_Y  = 734.0f;
static constexpr float LP_READY_W  = LEFT_W - 24.0f;
static constexpr float LP_READY_H  = 48.0f;
static constexpr float LP_LIST_BOTTOM = LP_READY_Y - 30.0f;
static constexpr float LP_SECTION_H   = 28.0f;   // セクション見出しの高さ

// センターパネル
static constexpr float CP_NAME_Y       = 96.0f;
static constexpr float CP_DIV1_Y       = 150.0f;
static constexpr float CP_INFO_LABEL_Y = 432.0f;
static constexpr float CP_BAR_X_LABEL  = CNT_X + 24.0f;
static constexpr float CP_BAR_X_BAR    = CNT_X + 180.0f;
static constexpr float CP_BAR_W        = 400.0f;
static constexpr float CP_BAR_H        = 12.0f;
static constexpr float CP_BAR_ROW1_Y   = 474.0f;
static constexpr float CP_BAR_ROW2_Y   = 518.0f;
static constexpr float CP_BAR_ROW3_Y   = 562.0f;
static constexpr float CP_VAL_X        = CNT_X + 676.0f;
static constexpr float CP_DESC_Y       = 610.0f;
static constexpr float CP_COST_Y       = 724.0f;

// 右パネル
static constexpr float RP_TITLE_Y      = 92.0f;
static constexpr float RP_DIV_Y        = 136.0f;
static constexpr float RP_LOADOUT_Y    = 548.0f;
static constexpr float RP_STATS_Y      = 660.0f;
static constexpr float RP_CREDIT_Y     = 720.0f;

//==============================================================================
// 内部状態
//==============================================================================
namespace
{
    enum Tab { TAB_WEAPON = 0, TAB_FRAME, TAB_INTERNAL, TAB_COUNT };

    // セクションの種類（リストの中身）
    enum SectionKind { SEC_RARM, SEC_LARM, SEC_HEAD, SEC_BODY, SEC_LEGS, SEC_INT1, SEC_INT2 };

    int g_Tab = TAB_WEAPON;

    // タブ内の縦カーソル（全セクションの項目を順につないだ通し番号。最後が READY）
    int g_Focus = 0;

    // 武装：確定（装備）中
    int g_RightSelected = WEAPON_MACHINEGUN;
    int g_LeftSelected  = WEAPON_SHIELD;
    // 機体・内部パーツ：この画面での選択（READY で MechParts に確定）
    int g_FrameSel[FRAME_SLOT_COUNT]       = { 0, 0, 0 };
    int g_InternalSel[INTERNAL_SLOT_COUNT] = { 0, 0 };

    // ショップモード（サバイバルのショップから流用する際に true）
    bool g_ShopMode   = false;
    int  g_ShopBudget = 0;

    // ミッション報酬の持ち越し分（通常モードの予算に上乗せする。セーブ対象）
    int  g_SavedCredits = 0;

    // 前回選択のデフォルト値（SaveData_Load から上書きされる）
    int g_DefaultRight = WEAPON_MACHINEGUN;
    int g_DefaultLeft  = WEAPON_SHIELD;

    bool g_Decided   = false;
    bool g_Cancelled = false;

    int g_SeCursorMove = -1;
    int g_SeSelect     = -1;
    int g_SeCancel     = -1;
    int g_SeTabSwitch  = -1;

    double g_Time = 0.0;
    double g_TabTime = 10.0;   // タブを切り替えてからの時間（切り替えの演出）
    int g_BgTexID = -1;

    // プレビューモデル
    MODEL* g_pPreviewModels[WEAPON_COUNT] = {};
    MODEL* g_pMeleeEdgePreview = nullptr;
    MODEL* g_pFrameModels[FRAME_SLOT_COUNT][FRAME_PART_COUNT] = {};

    float g_PreviewAngle = 0.0f;

    //--------------------------------------------------------------------------
    // タブの構成
    //--------------------------------------------------------------------------
    struct Section { SectionKind kind; const wchar_t* title; const wchar_t* sub; int count; };

    int SectionsOf(int tab, Section out[3])
    {
        switch (tab)
        {
        case TAB_FRAME:
            out[0] = { SEC_HEAD, L"HEAD", L"頭部", FRAME_PART_COUNT };
            out[1] = { SEC_BODY, L"CORE", L"胴体", FRAME_PART_COUNT };
            out[2] = { SEC_LEGS, L"LEGS", L"脚部", FRAME_PART_COUNT };
            return 3;
        case TAB_INTERNAL:
            out[0] = { SEC_INT1, L"SLOT 1", L"内部パーツ", INTERNAL_PART_COUNT };
            out[1] = { SEC_INT2, L"SLOT 2", L"内部パーツ", INTERNAL_PART_COUNT };
            return 2;
        default:
            out[0] = { SEC_RARM, L"R-ARM", L"PRIMARY",   WEAPON_COUNT };
            out[1] = { SEC_LARM, L"L-ARM", L"SECONDARY", WEAPON_COUNT };
            return 2;
        }
    }

    int TotalRows(int tab)
    {
        Section s[3];
        const int n = SectionsOf(tab, s);
        int rows = 0;
        for (int i = 0; i < n; ++i) rows += s[i].count;
        return rows;
    }

    // 縦カーソル → (セクション番号, 項目番号)。READY なら false
    bool FocusToItem(int focus, int* section, int* item)
    {
        Section s[3];
        const int n = SectionsOf(g_Tab, s);
        for (int i = 0; i < n; ++i)
        {
            if (focus < s[i].count) { *section = i; *item = focus; return true; }
            focus -= s[i].count;
        }
        return false;
    }

    int FirstRowOfSection(int section)
    {
        Section s[3];
        SectionsOf(g_Tab, s);
        int r = 0;
        for (int i = 0; i < section; ++i) r += s[i].count;
        return r;
    }

    int& SelectedRef(SectionKind k)
    {
        switch (k)
        {
        case SEC_LARM: return g_LeftSelected;
        case SEC_HEAD: return g_FrameSel[FRAME_HEAD];
        case SEC_BODY: return g_FrameSel[FRAME_BODY];
        case SEC_LEGS: return g_FrameSel[FRAME_LEGS];
        case SEC_INT1: return g_InternalSel[0];
        case SEC_INT2: return g_InternalSel[1];
        default:       return g_RightSelected;
        }
    }

    // セクション section の、選択中の項目にフォーカスする
    void FocusSelected(int section)
    {
        Section s[3];
        const int n = SectionsOf(g_Tab, s);
        if (section >= n) { g_Focus = TotalRows(g_Tab); return; }   // READY
        g_Focus = FirstRowOfSection(section) + SelectedRef(s[section].kind);
    }

    std::wstring Widen(const char* s) { return std::wstring(s, s + strlen(s)); }   // ASCII のみ

    std::wstring ItemName(SectionKind k, int i)
    {
        switch (k)
        {
        case SEC_RARM: case SEC_LARM: return Widen(k_WeaponDefs[i].name);
        case SEC_HEAD: return MechParts_GetFrame(FRAME_HEAD, i).name;
        case SEC_BODY: return MechParts_GetFrame(FRAME_BODY, i).name;
        case SEC_LEGS: return MechParts_GetFrame(FRAME_LEGS, i).name;
        default:       return MechParts_GetInternal(i).name;
        }
    }
}

//==============================================================================
// 内部ヘルパー
//==============================================================================
static int CalcRemaining()
{
    if (g_ShopMode)
    {
        // ショップ：装備中（初期値）から変更したアームぶんだけ課金する差分方式
        int spent = 0;
        if (g_RightSelected != g_DefaultRight) spent += k_WeaponDefs[g_RightSelected].cost;
        if (g_LeftSelected  != g_DefaultLeft)  spent += k_WeaponDefs[g_LeftSelected].cost;
        return g_ShopBudget - spent;
    }
    return INITIAL_CREDITS + g_SavedCredits - k_WeaponDefs[g_RightSelected].cost - k_WeaponDefs[g_LeftSelected].cost;
}

static int TotalBudget()
{
    return g_ShopMode ? g_ShopBudget : INITIAL_CREDITS + g_SavedCredits;
}

static void RestoreFullViewport()
{
    D3D11_VIEWPORT fullVP{};
    fullVP.Width    = static_cast<float>(Direct3D_GetBackBufferWidth());
    fullVP.Height   = static_cast<float>(Direct3D_GetBackBufferHeight());
    fullVP.MinDepth = 0.0f; fullVP.MaxDepth = 1.0f;
    Direct3D_GetContext()->RSSetViewports(1, &fullVP);
}

//==============================================================================
// AssemblyScreen_Initialize
//==============================================================================
void AssemblyScreen_Initialize()
{
    g_RightSelected = g_DefaultRight;
    g_LeftSelected  = g_DefaultLeft;
    for (int s = 0; s < FRAME_SLOT_COUNT; ++s)    g_FrameSel[s]    = MechParts_GetFrameSel(s);
    for (int s = 0; s < INTERNAL_SLOT_COUNT; ++s) g_InternalSel[s] = MechParts_GetInternalSel(s);

    g_Tab       = TAB_WEAPON;
    g_Focus     = g_RightSelected;   // R-ARM の装備中の武器にフォーカス
    g_ShopMode  = false;             // ショップは Initialize の後に SetShopMode する
    g_Decided   = false;
    g_Cancelled = false;
    g_Time      = 0.0;
    g_TabTime   = 10.0;

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudioWithVolume("resource/Sound/ui_cursor_move.wav", 0.5f);
    if (g_SeSelect     < 0) g_SeSelect     = LoadAudioWithVolume("resource/Sound/ui_select.wav", 0.5f);
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudioWithVolume("resource/Sound/ui_cancel.wav", 0.5f);
    if (g_SeTabSwitch  < 0) g_SeTabSwitch  = LoadAudioWithVolume("resource/Sound/ui_tab_switch.wav", 0.5f);

    if (g_BgTexID < 0) g_BgTexID = Texture_Load(L"resource/Texture/titleBg.png");

    // プレビューモデル（古いものを解放してから再ロード）
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_pPreviewModels[i]) { ModelRelease(g_pPreviewModels[i]); g_pPreviewModels[i] = nullptr; }
        g_pPreviewModels[i] = ModelLoad(k_WeaponDefs[i].modelPath, k_WeaponDefs[i].scale);
    }
    if (g_pMeleeEdgePreview) { ModelRelease(g_pMeleeEdgePreview); g_pMeleeEdgePreview = nullptr; }
    g_pMeleeEdgePreview = ModelLoad("resource/Models/BladeEdge.fbx", k_WeaponDefs[WEAPON_MELEE].scale);

    for (int s = 0; s < FRAME_SLOT_COUNT; ++s)
        for (int i = 0; i < FRAME_PART_COUNT; ++i)
        {
            if (g_pFrameModels[s][i]) { ModelRelease(g_pFrameModels[s][i]); g_pFrameModels[s][i] = nullptr; }
            g_pFrameModels[s][i] = ModelLoad(MechParts_GetFrame(s, i).model, 0.3f);
        }

    g_PreviewAngle = 0.0f;
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

    for (int i = 0; i < WEAPON_COUNT; ++i) { ModelRelease(g_pPreviewModels[i]); g_pPreviewModels[i] = nullptr; }
    ModelRelease(g_pMeleeEdgePreview); g_pMeleeEdgePreview = nullptr;
    for (int s = 0; s < FRAME_SLOT_COUNT; ++s)
        for (int i = 0; i < FRAME_PART_COUNT; ++i) { ModelRelease(g_pFrameModels[s][i]); g_pFrameModels[s][i] = nullptr; }

    if (g_BgTexID >= 0) { Texture_Release(g_BgTexID); g_BgTexID = -1; }
}

//==============================================================================
// AssemblyScreen_Update
//==============================================================================
bool AssemblyScreen_Update(double dt)
{
    g_Time    += dt;
    g_TabTime += dt;
    g_PreviewAngle += static_cast<float>(dt) * 0.8f;

    const int focusReady = TotalRows(g_Tab);
    const int focusTotal = focusReady + 1;

    // ── 左右：タブ切り替え（ショップは武装のみ）──
    if (!g_ShopMode && (UI_IsMoveLeft() || UI_IsMoveRight()))
    {
        g_Tab = (g_Tab + (UI_IsMoveRight() ? 1 : TAB_COUNT - 1)) % TAB_COUNT;
        FocusSelected(0);
        g_TabTime = 0.0;
        PlayAudio(g_SeTabSwitch, false);
    }

    // ── TAB / LB / RB：次のセクションへ（最後のセクションの次は READY、その次は先頭）──
    if (UI_IsTabSwitch())
    {
        int sec = 0, item = 0;
        Section s[3];
        const int n = SectionsOf(g_Tab, s);
        if (FocusToItem(g_Focus, &sec, &item)) FocusSelected(sec + 1 <= n ? sec + 1 : 0);
        else                                   FocusSelected(0);
        PlayAudio(g_SeTabSwitch, false);
    }

    // ── 上下：縦移動（端でループ）──
    if (UI_IsMoveDown()) { g_Focus = (g_Focus + 1) % focusTotal;              PlayAudio(g_SeCursorMove, false); }
    if (UI_IsMoveUp())   { g_Focus = (g_Focus + focusTotal - 1) % focusTotal; PlayAudio(g_SeCursorMove, false); }

    // ── キャンセル ──
    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_Cancelled = true;
        return true;
    }

    // ── 決定 ──
    if (UI_IsConfirm())
    {
        int sec = 0, item = 0;
        if (FocusToItem(g_Focus, &sec, &item))
        {
            Section s[3];
            SectionsOf(g_Tab, s);
            SelectedRef(s[sec].kind) = item;   // ホバー中の項目をこのセクションに装備
            PlayAudio(g_SeSelect, false);
        }
        else if (CalcRemaining() >= 0)
        {
            // READY：構成を確定して出撃
            if (!g_ShopMode)
            {
                g_DefaultRight = g_RightSelected;
                g_DefaultLeft  = g_LeftSelected;
                for (int i = 0; i < FRAME_SLOT_COUNT; ++i)    MechParts_SetFrameSel(i, g_FrameSel[i]);
                for (int i = 0; i < INTERNAL_SLOT_COUNT; ++i) MechParts_SetInternalSel(i, g_InternalSel[i]);
            }
            PlayAudio(g_SeSelect, false);
            g_Decided = true;
            return true;
        }
        else
        {
            PlayAudio(g_SeCancel, false);   // 予算超過中は出撃できない
        }
    }
    return false;
}

//==============================================================================
// 3D：ホバー中の武器・機体パーツのプレビュー（センターパネル上部）
//==============================================================================
static void DrawItemPreview(MODEL* previewModel, bool melee, bool framePart)
{
    if (!previewModel) return;

    constexpr float PV_X = CNT_X + 100.0f;
    constexpr float PV_Y = CP_DIV1_Y + 5.0f;
    constexpr float PV_W = CNT_W - 200.0f;
    constexpr float PV_H = CP_INFO_LABEL_Y - CP_DIV1_Y - 10.0f;

    // 機体パーツは武器より少し大きいので少し引いて見る
    const XMFLOAT3 eyeF3 = framePart ? XMFLOAT3{ 0.32f, 0.26f, 0.70f } : XMFLOAT3{ 0.25f, 0.18f, 0.55f };
    const XMMATRIX view  = XMMatrixLookAtLH(XMLoadFloat3(&eyeF3), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
    const XMMATRIX proj  = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), PV_W / PV_H, 0.01f, 100.0f);
    Shader3d_SetViewMatrix(view);
    Shader3d_SetProjectMatrix(proj);
    ShaderToon_SetViewMatrix(view);
    ShaderToon_SetProjectMatrix(proj);
    ShaderEdge_SetViewMatrix(view);
    ShaderEdge_SetProjectMatrix(proj);

    // 機体パーツはモデル中心で回す（正面 -Z をはじめにカメラへ向ける）
    XMMATRIX world = XMMatrixRotationY(g_PreviewAngle);
    if (framePart)
    {
        const AABB b = ModelGetAABB(previewModel, { 0, 0, 0 });
        world = XMMatrixTranslation(-(b.min.x + b.max.x) * 0.5f, -(b.min.y + b.max.y) * 0.5f, -(b.min.z + b.max.z) * 0.5f) *
                XMMatrixRotationY(g_PreviewAngle + XM_PI);
    }

    const float sx = (float)Direct3D_GetBackBufferWidth() / SW;
    const float sy = (float)Direct3D_GetBackBufferHeight() / SH;
    auto setSubVP = [&]()
    {
        D3D11_VIEWPORT vp{ PV_X * sx, PV_Y * sy, PV_W * sx, PV_H * sy, 0.0f, 1.0f };
        Direct3D_GetContext()->RSSetViewports(1, &vp);
    };

    Direct3D_SetDepthEnable(true);
    setSubVP();
    Light_SetSpecularWorld(eyeF3, 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
    Light_SetAmbient({ 0.65f, 0.65f, 0.65f });

    ShaderEdge_BeginNormalPass();
    ShaderEdge_SetWorldMatrix(world);
    ModelDrawWithoutBegin(previewModel, world);
    ShaderEdge_EndNormalPass();

    setSubVP();
    ModelDrawToon(previewModel, world);
    if (melee && g_pMeleeEdgePreview)
    {
        const XMFLOAT3 prevAmb = Light_GetAmbient();
        Light_SetAmbient({ 3.0f, 3.0f, 3.0f });
        ModelDrawToon(g_pMeleeEdgePreview, world);
        Light_SetAmbient(prevAmb);
    }

    RestoreFullViewport();
    ShaderEdge_DrawEdge();
    Direct3D_SetDepthEnable(false);
}

//==============================================================================
// 3D：組み上がった機体のプレビュー（右パネル）
//==============================================================================
static void DrawPlayerPreview()
{
    MODEL* body     = g_pFrameModels[FRAME_BODY][g_FrameSel[FRAME_BODY]];
    MODEL* head     = g_pFrameModels[FRAME_HEAD][g_FrameSel[FRAME_HEAD]];
    MODEL* thruster = g_pFrameModels[FRAME_LEGS][g_FrameSel[FRAME_LEGS]];
    if (!body || !head || !thruster) return;

    constexpr float PP_X = RGT_X + 10.0f;
    constexpr float PP_Y = RP_DIV_Y + 5.0f;
    constexpr float PP_W = RGT_W - 20.0f;
    constexpr float PP_H = RP_LOADOUT_Y - RP_DIV_Y - 20.0f;

    const XMFLOAT3 ppEyeF3 = { 0.0f, 0.55f, 1.7f };
    const XMMATRIX ppView  = XMMatrixLookAtLH(XMLoadFloat3(&ppEyeF3), XMVectorSet(0.0f, 0.1f, 0.0f, 1.0f), XMVectorSet(0, 1, 0, 0));
    const XMMATRIX ppProj  = XMMatrixPerspectiveFovLH(XMConvertToRadians(40.0f), PP_W / PP_H, 0.01f, 100.0f);
    Shader3d_SetViewMatrix(ppView);
    Shader3d_SetProjectMatrix(ppProj);
    ShaderToon_SetViewMatrix(ppView);
    ShaderToon_SetProjectMatrix(ppProj);
    ShaderEdge_SetViewMatrix(ppView);
    ShaderEdge_SetProjectMatrix(ppProj);

    // ボディ回転（player.cpp の rotY * rotYawFix と同じ）。初期角度でボディ前面(-Z)がカメラを向く
    const XMMATRIX bodyRot = XMMatrixRotationY(g_PreviewAngle + XMConvertToRadians(90.0f)) *
                             XMMatrixRotationY(XMConvertToRadians(90.0f));
    const XMVECTOR worldFront = XMVectorSet(sinf(g_PreviewAngle), 0.0f, cosf(g_PreviewAngle), 0.0f);

    // 取り付け点で積み上げる（player.cpp と同じ。MechParts.h を参照）
    const MechMount mBodyHead = MechParts_Mount(FRAME_BODY, g_FrameSel[FRAME_BODY], false);
    const MechMount mBodyLegs = MechParts_Mount(FRAME_BODY, g_FrameSel[FRAME_BODY], true);
    const MechMount mHeadNeck = MechParts_Mount(FRAME_HEAD, g_FrameSel[FRAME_HEAD]);
    const MechMount mLegsTop  = MechParts_Mount(FRAME_LEGS, g_FrameSel[FRAME_LEGS]);

    // 胴体の「枠」：Y は脚の取り付け点〜頭の取り付け点（武器の高さの基準）
    AABB bodyAABB = ModelGetAABB(body, { 0, 0, 0 });
    bodyAABB.min.y = mBodyLegs.y;
    bodyAABB.max.y = mBodyHead.y;

    constexpr float THRUSTER_FORWARD_OFFSET = +0.05f;
    const XMMATRIX bodyWorld = bodyRot;
    XMFLOAT3 headOff;
    XMStoreFloat3(&headOff, XMVector3TransformNormal(XMVectorSet(mBodyHead.x - mHeadNeck.x, 0.0f, mBodyHead.z - mHeadNeck.z, 0.0f), bodyRot));
    const XMMATRIX headWorld = bodyRot * XMMatrixTranslation(headOff.x, mBodyHead.y - mHeadNeck.y, headOff.z);
    const XMMATRIX thrusterWorld = XMMatrixRotationY(XM_PI) * bodyRot *
        XMMatrixTranslation(XMVectorGetX(worldFront) * THRUSTER_FORWARD_OFFSET,
                            mBodyLegs.y - mLegsTop.y - 0.01f,
                            XMVectorGetZ(worldFront) * THRUSTER_FORWARD_OFFSET);

    const XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    const XMVECTOR worldRight = XMVector3Normalize(XMVector3Cross(up, worldFront));

    auto makeWeaponWorld = [&](const WeaponDef& def, float sideSign, bool isMelee) -> XMMATRIX
    {
        XMFLOAT3 p = {
            XMVectorGetX(worldRight) * sideSign * def.sideOffset + XMVectorGetX(worldFront) * def.forwardOffset,
            bodyAABB.min.y + def.heightOffset,
            XMVectorGetZ(worldRight) * sideSign * def.sideOffset + XMVectorGetZ(worldFront) * def.forwardOffset };
        if (isMelee)
        {
            p = { XMVectorGetX(worldRight) * sideSign * MELEE_REST_SIDE + XMVectorGetX(worldFront) * MELEE_REST_FWD,
                  bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_REST_UP_R,
                  XMVectorGetZ(worldRight) * sideSign * MELEE_REST_SIDE + XMVectorGetZ(worldFront) * MELEE_REST_FWD };
        }
        XMVECTOR aimZ = XMVectorNegate(worldFront);
        XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(up, aimZ));
        XMVECTOR aimY = XMVector3Cross(aimZ, aimX);
        XMFLOAT3 ax, ay, az;
        XMStoreFloat3(&ax, aimX); XMStoreFloat3(&ay, aimY); XMStoreFloat3(&az, aimZ);
        XMMATRIX aimRot(ax.x, ax.y, ax.z, 0, ay.x, ay.y, ay.z, 0, az.x, az.y, az.z, 0, 0, 0, 0, 1);
        XMMATRIX localRot = XMMatrixRotationZ(XMConvertToRadians(def.flipDeg + def.leanDeg * sideSign)) *
                            XMMatrixRotationX(XMConvertToRadians(def.tiltDeg));
        return localRot * aimRot * XMMatrixTranslation(p.x, p.y, p.z);
    };

    const XMMATRIX rWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_RightSelected], +1.0f, g_RightSelected == WEAPON_MELEE);
    const XMMATRIX lWeaponWorld = makeWeaponWorld(k_WeaponDefs[g_LeftSelected],  -1.0f, g_LeftSelected  == WEAPON_MELEE);
    MODEL* rWeaponModel = g_pPreviewModels[g_RightSelected];
    MODEL* lWeaponModel = g_pPreviewModels[g_LeftSelected];

    const float sx = (float)Direct3D_GetBackBufferWidth() / SW;
    const float sy = (float)Direct3D_GetBackBufferHeight() / SH;
    auto setPlayerVP = [&]()
    {
        D3D11_VIEWPORT vp{ PP_X * sx, PP_Y * sy, PP_W * sx, PP_H * sy, 0.0f, 1.0f };
        Direct3D_GetContext()->RSSetViewports(1, &vp);
    };

    Direct3D_SetDepthEnable(true);
    Light_SetSpecularWorld(ppEyeF3, 100.0f, { 0.6f, 0.5f, 0.4f, 1.0f });
    Light_SetAmbient({ 0.65f, 0.65f, 0.65f });

    setPlayerVP();
    ShaderEdge_BeginNormalPass();
    ShaderEdge_SetWorldMatrix(bodyWorld);     ModelDrawWithoutBegin(body, bodyWorld);
    ShaderEdge_SetWorldMatrix(headWorld);     ModelDrawWithoutBegin(head, headWorld);
    ShaderEdge_SetWorldMatrix(thrusterWorld); ModelDrawWithoutBegin(thruster, thrusterWorld);
    if (rWeaponModel) { ShaderEdge_SetWorldMatrix(rWeaponWorld); ModelDrawWithoutBegin(rWeaponModel, rWeaponWorld); }
    if (lWeaponModel) { ShaderEdge_SetWorldMatrix(lWeaponWorld); ModelDrawWithoutBegin(lWeaponModel, lWeaponWorld); }
    ShaderEdge_EndNormalPass();

    setPlayerVP();
    ModelDrawToon(body, bodyWorld);
    ModelDrawToon(head, headWorld);
    ModelDrawToon(thruster, thrusterWorld);
    if (rWeaponModel) ModelDrawToon(rWeaponModel, rWeaponWorld);
    if (lWeaponModel) ModelDrawToon(lWeaponModel, lWeaponWorld);
    if ((g_RightSelected == WEAPON_MELEE || g_LeftSelected == WEAPON_MELEE) && g_pMeleeEdgePreview)
    {
        const XMFLOAT3 prevAmb = Light_GetAmbient();
        Light_SetAmbient({ 3.0f, 3.0f, 3.0f });
        if (g_RightSelected == WEAPON_MELEE) ModelDrawToon(g_pMeleeEdgePreview, rWeaponWorld);
        if (g_LeftSelected  == WEAPON_MELEE) ModelDrawToon(g_pMeleeEdgePreview, lWeaponWorld);
        Light_SetAmbient(prevAmb);
    }

    RestoreFullViewport();
    ShaderEdge_DrawEdge();
    Direct3D_SetDepthEnable(false);
}

//==============================================================================
// AssemblyScreen_Draw
//==============================================================================
void AssemblyScreen_Draw()
{
    using namespace SciFiUI;

    const float t     = static_cast<float>(g_Time);
    const float pulse = 0.5f + 0.5f * sinf(t * 4.0f);

    const int  remaining  = CalcRemaining();
    const bool overBudget = (remaining < 0);

    // タブの構成とホバー中の項目
    Section secs[3];
    const int secCount = SectionsOf(g_Tab, secs);
    int hoverSec = -1, hoverItem = 0;
    const bool onItem = FocusToItem(g_Focus, &hoverSec, &hoverItem);
    // READY にいるときは最初のセクションの選択中の項目を表示する
    const SectionKind hoverKind = onItem ? secs[hoverSec].kind : secs[0].kind;
    if (!onItem) hoverItem = SelectedRef(hoverKind);
    const bool hoverWeapon   = (hoverKind == SEC_RARM || hoverKind == SEC_LARM);
    const bool hoverFrame    = (hoverKind == SEC_HEAD || hoverKind == SEC_BODY || hoverKind == SEC_LEGS);
    const int  hoverSlot     = (hoverKind == SEC_HEAD) ? FRAME_HEAD : (hoverKind == SEC_BODY) ? FRAME_BODY : FRAME_LEGS;

    // 左パネルのリストの行間（項目数から自動で決める）
    int rows = 0;
    for (int i = 0; i < secCount; ++i) rows += secs[i].count;
    const float rowStep = std::min(30.0f, (LP_LIST_BOTTOM - LP_LIST_TOP - secCount * LP_SECTION_H) / rows);
    const float rowH    = rowStep - 4.0f;
    auto sectionTop = [&](int s)
    {
        float y = LP_LIST_TOP;
        for (int i = 0; i < s; ++i) y += LP_SECTION_H + secs[i].count * rowStep;
        return y;
    };

    // 構成の合計性能（この画面での選択）
    const MechStats stats = MechParts_CalcStats(g_FrameSel, g_InternalSel);

    //--------------------------------------------------------------------------
    // (1) 背景・パネル
    //--------------------------------------------------------------------------
    BeginSprites();
    Fill(0.0f, 0.0f, SW, SH, { 0.01f, 0.02f, 0.04f, g_ShopMode ? 0.80f : 1.0f });
    if (g_BgTexID >= 0 && !g_ShopMode) Sprite_Draw(g_BgTexID, 0.0f, 0.0f, SW, SH, { 0.55f, 0.8f, 1.0f, 0.12f });
    Grid(0.0f, 0.0f, SW, SH, 40.0f, { 0.3f, 0.7f, 1.0f, 0.03f });
    Scanlines(0.0f, 0.0f, SW, SH, 4.0f, 0.015f);

    Fill(0.0f, 0.0f, SW, HEAD_H, { 0.0f, 0.03f, 0.06f, 0.9f });
    Fill(0.0f, HEAD_H, SW, 1.0f, kCyanDim);
    Ticks(0.0f, HEAD_H + 1.0f, SW, 81, 5, WithAlpha(kCyan, 0.3f));
    Fill(24.0f, 14.0f, 4.0f, 36.0f, kCyan);

    // タブ
    static const wchar_t* TAB_LABEL[TAB_COUNT] = { L"WEAPON", L"FRAME", L"INTERNAL" };
    static const wchar_t* TAB_SUB[TAB_COUNT]   = { L"武装", L"機体", L"内部パーツ" };
    constexpr float TAB_X = 560.0f, TAB_W = 170.0f, TAB_Y = 12.0f, TAB_HT = 42.0f;
    if (!g_ShopMode)
    {
        for (int i = 0; i < TAB_COUNT; ++i)
        {
            const float x = TAB_X + i * (TAB_W + 10.0f);
            const bool on = (i == g_Tab);
            Panel(x, TAB_Y, TAB_W, TAB_HT, on ? kPanelHi : kPanel, WithAlpha(kCyan, on ? 0.95f : 0.35f), 10.0f);
            if (on) Fill(x + 1.0f, TAB_Y + TAB_HT - 3.0f, TAB_W - 2.0f, 3.0f, kCyan);
        }
    }

    Panel(LEFT_X, PANEL_Y, LEFT_W, PANEL_B - PANEL_Y);
    Panel(CNT_X,  PANEL_Y, CNT_W,  PANEL_B - PANEL_Y);
    Panel(RGT_X,  PANEL_Y, RGT_W,  PANEL_B - PANEL_Y);

    Fill(CNT_X + 10.0f, CP_DIV1_Y, CNT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.3f));
    Fill(CNT_X + 10.0f, CP_INFO_LABEL_Y + 26.0f, CNT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.3f));
    Fill(CNT_X + 10.0f, CP_COST_Y - 14.0f, CNT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.2f));
    Fill(RGT_X + 10.0f, RP_DIV_Y, RGT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.3f));
    Fill(RGT_X + 10.0f, RP_LOADOUT_Y - 8.0f, RGT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.2f));
    Fill(RGT_X + 10.0f, RP_STATS_Y - 6.0f, RGT_W - 20.0f, 1.0f, WithAlpha(kCyan, 0.2f));

    // 左パネル：セクションと項目
    for (int s = 0; s < secCount; ++s)
    {
        const float top = sectionTop(s);
        const bool  active = onItem && (s == hoverSec);
        if (s > 0) Fill(LEFT_X + 8.0f, top - 3.0f, LEFT_W - 16.0f, 1.0f, WithAlpha(kCyan, 0.3f));
        if (active) Fill(LEFT_X + 1.0f, top, LEFT_W - 2.0f, 24.0f, WithAlpha(kCyan, 0.18f));

        const int sel = SelectedRef(secs[s].kind);
        for (int i = 0; i < secs[s].count; ++i)
        {
            const float y = top + LP_SECTION_H + i * rowStep;
            if (i == sel) Fill(LEFT_X + 6.0f, y, LEFT_W - 12.0f, rowH, WithAlpha(kGreen, 0.10f));
            if (active && i == hoverItem)
            {
                Fill(LEFT_X + 6.0f, y, LEFT_W - 12.0f, rowH, WithAlpha(kCyan, 0.20f));
                Fill(LEFT_X + 6.0f, y, 3.0f, rowH, kCyan);
                Brackets(LEFT_X + 4.0f, y - 2.0f, LEFT_W - 8.0f, rowH + 4.0f, 8.0f, WithAlpha(kCyan, 0.6f + 0.4f * pulse), 1.0f);
            }
        }
    }

    // READY ボタン
    {
        const bool focused = !onItem;
        const XMFLOAT4 edge = overBudget ? kRed : kGreen;
        const XMFLOAT4 fill = overBudget
            ? XMFLOAT4{ 0.35f, 0.05f, 0.05f, focused ? 0.75f : 0.45f }
            : XMFLOAT4{ 0.05f, 0.35f, 0.18f, focused ? (0.45f + 0.40f * pulse) : 0.30f };
        Panel(LP_READY_X, LP_READY_Y, LP_READY_W, LP_READY_H, fill, WithAlpha(edge, focused ? 1.0f : 0.6f), 10.0f);
        if (focused)
        {
            const float e = 3.0f + pulse * 3.0f;
            Brackets(LP_READY_X - e, LP_READY_Y - e, LP_READY_W + e * 2.0f, LP_READY_H + e * 2.0f, 10.0f, edge, 2.0f);
        }
    }

    // センター：性能ゲージ
    const MechPartDef* part = nullptr;
    if (hoverFrame)                      part = &MechParts_GetFrame(hoverSlot, hoverItem);
    else if (!hoverWeapon)               part = &MechParts_GetInternal(hoverItem);
    const float barYs[3] = { CP_BAR_ROW1_Y, CP_BAR_ROW2_Y, CP_BAR_ROW3_Y };
    if (hoverWeapon)
    {
        const WeaponDef& wd = k_WeaponDefs[hoverItem];
        const float bars[3] = { wd.dmgBar, wd.rateBar, wd.expBar };
        for (int r = 0; r < 3; ++r)
            SegmentBar(CP_BAR_X_BAR, barYs[r] + 4.0f, CP_BAR_W, CP_BAR_H, 20, bars[r], kCyan, WithAlpha(kCyan, 0.10f));
    }
    else if (part)
    {
        // 補正（±30% を端とする）：中央から右へ伸びれば強化（緑）、左なら低下（赤）
        const float mods[3] = { part->hp, part->speed, part->attack };
        for (int r = 0; r < 3; ++r)
        {
            const float cx = CP_BAR_X_BAR + CP_BAR_W * 0.5f;
            const float y  = barYs[r] + 4.0f;
            Fill(CP_BAR_X_BAR, y, CP_BAR_W, CP_BAR_H, WithAlpha(kCyan, 0.08f));
            Fill(cx - 1.0f, y - 4.0f, 2.0f, CP_BAR_H + 8.0f, WithAlpha(kCyan, 0.6f));
            const float w = std::min(1.0f, fabsf(mods[r]) / 0.30f) * CP_BAR_W * 0.5f;
            if (mods[r] > 0.0f) Fill(cx, y, w, CP_BAR_H, kGreen);
            if (mods[r] < 0.0f) Fill(cx - w, y, w, CP_BAR_H, kRed);
        }
        if (!hoverFrame)
        {
            // 内部パーツの「見た目」：基板風のチップ
            const float px = CNT_X + 230.0f, py = CP_DIV1_Y + 40.0f, pw = 240.0f, ph = 200.0f;
            Panel(px, py, pw, ph, kPanelHi, WithAlpha(kCyan, 0.7f), 14.0f);
            Grid(px + 8.0f, py + 8.0f, pw - 16.0f, ph - 16.0f, 16.0f, WithAlpha(kCyan, 0.12f));
            for (int k = 0; k < 6; ++k)
            {
                Fill(px - 14.0f, py + 24.0f + k * 28.0f, 14.0f, 4.0f, WithAlpha(kAmber, 0.7f));   // ピン
                Fill(px + pw,    py + 24.0f + k * 28.0f, 14.0f, 4.0f, WithAlpha(kAmber, 0.7f));
            }
            Diamond(px + pw * 0.5f, py + ph * 0.5f, 46.0f + 4.0f * pulse, WithAlpha(kCyan, 0.25f));
            Diamond(px + pw * 0.5f, py + ph * 0.5f, 24.0f, WithAlpha(kCyan, 0.8f));
        }
    }

    // 右：予算ゲージ
    {
        const float used = 1.0f - static_cast<float>(remaining) / static_cast<float>(std::max(1, TotalBudget()));
        SegmentBar(RGT_X + 24.0f, RP_CREDIT_Y + 40.0f, RGT_W - 48.0f, 8.0f, 24,
                   std::fmin(std::fmax(used, 0.0f), 1.0f), overBudget ? kRed : kAmber, WithAlpha(kCyan, 0.10f));
    }

    //--------------------------------------------------------------------------
    // (2) 3Dプレビュー
    //--------------------------------------------------------------------------
    if (hoverWeapon)     DrawItemPreview(g_pPreviewModels[hoverItem], hoverItem == WEAPON_MELEE, false);
    else if (hoverFrame) DrawItemPreview(g_pFrameModels[hoverSlot][hoverItem], false, true);
    DrawPlayerPreview();

    //--------------------------------------------------------------------------
    // (3) プレビューに重ねる枠
    //--------------------------------------------------------------------------
    BeginSprites();
    Brackets(CNT_X + 100.0f, CP_DIV1_Y + 8.0f, CNT_W - 200.0f, CP_INFO_LABEL_Y - CP_DIV1_Y - 16.0f, 16.0f, WithAlpha(kCyan, 0.55f), 1.0f);
    Brackets(RGT_X + 16.0f, RP_DIV_Y + 10.0f, RGT_W - 32.0f, RP_LOADOUT_Y - RP_DIV_Y - 30.0f, 18.0f, WithAlpha(kCyan, 0.55f), 1.0f);
    {
        const float h  = CP_INFO_LABEL_Y - CP_DIV1_Y - 16.0f;
        const float sy = CP_DIV1_Y + 8.0f + fmodf(t * 60.0f, h);
        Fill(CNT_X + 100.0f, sy, CNT_W - 200.0f, 1.0f, WithAlpha(kCyan, 0.25f));
    }
    // タブ切り替えの直後は左パネルが明滅する
    if (g_TabTime < 0.25)
        Fill(LEFT_X, PANEL_Y, LEFT_W, PANEL_B - PANEL_Y, WithAlpha(kCyan, 0.12f * static_cast<float>(1.0 - g_TabTime / 0.25)));

    //--------------------------------------------------------------------------
    // (4) 文字
    //--------------------------------------------------------------------------
    wchar_t buf[160];
    const D2D1_COLOR_F white = D2D1::ColorF(0.92f, 0.97f, 1.0f, 1.0f);
    const D2D1_COLOR_F label = ToD2D(kCyan, 0.65f);
    const D2D1_COLOR_F cyan  = ToD2D(kCyan);
    auto pct = [](float v) { wchar_t b[16]; swprintf_s(b, L"%+d%%", static_cast<int>(roundf(v * 100.0f))); return std::wstring(b); };

    // ヘッダ
    Text(g_ShopMode ? L"SYS://SUPPLY DEPOT" : L"SYS://ASSEMBLY", 40.0f, 10.0f, 13.0f, label, UIFont::Mono, UIAlign::Left, true);
    Text(g_ShopMode ? L"FIELD REFIT" : L"FRAME CONFIGURATION", 40.0f, 24.0f, 32.0f, cyan, UIFont::Display, UIAlign::Left, true);
    if (!g_ShopMode)
    {
        for (int i = 0; i < TAB_COUNT; ++i)
        {
            const float x = TAB_X + i * (TAB_W + 10.0f);
            const bool on = (i == g_Tab);
            Text(TAB_LABEL[i], x + 16.0f, TAB_Y + 6.0f, 22.0f, on ? white : label, UIFont::Display, UIAlign::Left, true);
            Text(TAB_SUB[i], x + TAB_W - 14.0f, TAB_Y + 14.0f, 13.0f, on ? cyan : label, UIFont::Body, UIAlign::Right);
        }
        Text(L"<  >  TAB SWITCH", TAB_X + 3 * (TAB_W + 10.0f) + 6.0f, TAB_Y + 14.0f, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
    }
    else
    {
        Text(L"補給・換装：武器を替えたアームだけ代金がかかる", 420.0f, 30.0f, 14.0f, label, UIFont::Body);
    }
    swprintf_s(buf, L"CREDIT  %d c", remaining);
    Text(buf, SW - 32.0f, 18.0f, 26.0f, overBudget ? ToD2D(kRed) : ToD2D(kAmber), UIFont::Display, UIAlign::Right, true);

    // 左パネル
    for (int s = 0; s < secCount; ++s)
    {
        const float top = sectionTop(s);
        const bool  active = onItem && (s == hoverSec);
        Text(secs[s].title, LEFT_X + 14.0f, top + 4.0f, 15.0f, active ? cyan : label, UIFont::Mono, UIAlign::Left, true);
        Text(secs[s].sub, LEFT_X + LEFT_W - 14.0f, top + 6.0f, 11.0f, label,
             (secs[s].kind == SEC_RARM || secs[s].kind == SEC_LARM) ? UIFont::Mono : UIFont::Body, UIAlign::Right);

        const int sel = SelectedRef(secs[s].kind);
        const float textH = std::min(17.0f, rowH - 2.0f);
        for (int i = 0; i < secs[s].count; ++i)
        {
            const float y = top + LP_SECTION_H + i * rowStep + (rowH - textH) * 0.5f - 1.0f;
            const bool cur = active && (i == hoverItem);
            const bool eqp = (i == sel);
            const D2D1_COLOR_F col = cur ? white : eqp ? ToD2D(kGreen) : D2D1::ColorF(0.55f, 0.65f, 0.72f, 1.0f);
            Text(ItemName(secs[s].kind, i), LEFT_X + 18.0f, y, textH, col, UIFont::Mono, UIAlign::Left, cur || eqp);
            if (eqp) Text(L"EQP", LEFT_X + LEFT_W - 16.0f, y + 2.0f, 11.0f, ToD2D(kGreen, 0.9f), UIFont::Mono, UIAlign::Right, true);
        }
    }
    Text(L"READY", LP_READY_X + LP_READY_W * 0.5f, LP_READY_Y + 7.0f, 30.0f,
         overBudget ? D2D1::ColorF(1.0f, 0.7f, 0.7f, 1.0f) : D2D1::ColorF(0.85f, 1.0f, 0.9f, 1.0f), UIFont::Display, UIAlign::Center, true);
    if (overBudget)
        Text(L"BUDGET EXCEEDED", LP_READY_X + LP_READY_W * 0.5f, LP_READY_Y - 20.0f, 12.0f, ToD2D(kRed), UIFont::Mono, UIAlign::Center, true);

    // センターパネル
    if (hoverWeapon)
    {
        const WeaponDef& wd = k_WeaponDefs[hoverItem];
        Text(hoverKind == SEC_LARM ? L"PART DATA  //  L-ARM" : L"PART DATA  //  R-ARM", CNT_X + 24.0f, PANEL_Y + 8.0f, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(Widen(wd.name), CNT_X + 24.0f, CP_NAME_Y, 36.0f, cyan, UIFont::Display, UIAlign::Left, true);
        Text(L"PERFORMANCE", CNT_X + 24.0f, CP_INFO_LABEL_Y, 13.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(L"DAMAGE",    CP_BAR_X_LABEL, CP_BAR_ROW1_Y, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(L"FIRE RATE", CP_BAR_X_LABEL, CP_BAR_ROW2_Y, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(L"EXPLOSION", CP_BAR_X_LABEL, CP_BAR_ROW3_Y, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
        swprintf_s(buf, L"%d", wd.damage);
        Text(buf, CP_VAL_X, CP_BAR_ROW1_Y - 3.0f, 20.0f, white, UIFont::Display, UIAlign::Right, true);
        if (wd.fireInterval > 0.0f) swprintf_s(buf, L"%.2fs", wd.fireInterval); else swprintf_s(buf, L"---");
        Text(buf, CP_VAL_X, CP_BAR_ROW2_Y - 3.0f, 20.0f, white, UIFont::Display, UIAlign::Right, true);
        if (wd.explosionR > 0.0f) swprintf_s(buf, L"%.1fm", wd.explosionR); else swprintf_s(buf, L"---");
        Text(buf, CP_VAL_X, CP_BAR_ROW3_Y - 3.0f, 20.0f, white, UIFont::Display, UIAlign::Right, true);
        Text(wd.description, CP_BAR_X_LABEL, CP_DESC_Y, 16.0f, D2D1::ColorF(0.75f, 0.85f, 0.9f, 1.0f), UIFont::Body);
        Text(L"COST", CP_BAR_X_LABEL, CP_COST_Y + 6.0f, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
        swprintf_s(buf, L"%d c", wd.cost);
        Text(buf, CP_VAL_X, CP_COST_Y, 28.0f, ToD2D(kAmber), UIFont::Display, UIAlign::Right, true);
    }
    else if (part)
    {
        static const wchar_t* SLOT_NAME[FRAME_SLOT_COUNT] = { L"HEAD", L"CORE", L"LEGS" };
        swprintf_s(buf, L"PART DATA  //  %s", hoverFrame ? SLOT_NAME[hoverSlot] : L"INTERNAL");
        Text(buf, CNT_X + 24.0f, PANEL_Y + 8.0f, 12.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(part->name, CNT_X + 24.0f, CP_NAME_Y, 36.0f, cyan, UIFont::Display, UIAlign::Left, true);
        Text(Widen(part->code), CNT_X + CNT_W - 24.0f, CP_NAME_Y + 14.0f, 18.0f, label, UIFont::Mono, UIAlign::Right, true);
        Text(L"MODIFIER", CNT_X + 24.0f, CP_INFO_LABEL_Y, 13.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(hoverFrame ? L"外見が変わる" : L"外見は変わらない", CNT_X + CNT_W - 24.0f, CP_INFO_LABEL_Y, 13.0f, label, UIFont::Body, UIAlign::Right);
        const wchar_t* names[3] = { L"AP", L"SPEED", L"ATTACK" };
        const float mods[3] = { part->hp, part->speed, part->attack };
        for (int r = 0; r < 3; ++r)
        {
            Text(names[r], CP_BAR_X_LABEL, barYs[r], 14.0f, label, UIFont::Mono, UIAlign::Left, true);
            Text(pct(mods[r]), CP_VAL_X, barYs[r] - 3.0f, 20.0f,
                 mods[r] > 0.0f ? ToD2D(kGreen) : mods[r] < 0.0f ? ToD2D(kRed) : white, UIFont::Display, UIAlign::Right, true);
        }
        Text(part->desc, CP_BAR_X_LABEL, CP_DESC_Y, 16.0f, D2D1::ColorF(0.75f, 0.85f, 0.9f, 1.0f), UIFont::Body);
        Text(L"COST", CP_BAR_X_LABEL, CP_COST_Y + 6.0f, 14.0f, label, UIFont::Mono, UIAlign::Left, true);
        Text(L"FREE", CP_VAL_X, CP_COST_Y, 28.0f, ToD2D(kGreen), UIFont::Display, UIAlign::Right, true);
    }

    // 右パネル：機体
    Text(g_ShopMode ? L"FRAME  //  CURRENT LOADOUT" : L"FRAME  //  PREVIEW", RGT_X + 20.0f, RP_TITLE_Y, 13.0f, label, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"%S / %S / %S",
               MechParts_GetFrame(FRAME_HEAD, g_FrameSel[FRAME_HEAD]).code,
               MechParts_GetFrame(FRAME_BODY, g_FrameSel[FRAME_BODY]).code,
               MechParts_GetFrame(FRAME_LEGS, g_FrameSel[FRAME_LEGS]).code);
    Text(buf, RGT_X + 20.0f, RP_TITLE_Y + 16.0f, 22.0f, cyan, UIFont::Display, UIAlign::Left, true);

    swprintf_s(buf, L"R-ARM   %S", k_WeaponDefs[g_RightSelected].name);
    Text(buf, RGT_X + 24.0f, RP_LOADOUT_Y, 15.0f, white, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"L-ARM   %S", k_WeaponDefs[g_LeftSelected].name);
    Text(buf, RGT_X + 24.0f, RP_LOADOUT_Y + 24.0f, 15.0f, white, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"FRAME   %s / %s / %s",
               MechParts_GetFrame(FRAME_HEAD, g_FrameSel[FRAME_HEAD]).name,
               MechParts_GetFrame(FRAME_BODY, g_FrameSel[FRAME_BODY]).name,
               MechParts_GetFrame(FRAME_LEGS, g_FrameSel[FRAME_LEGS]).name);
    Text(buf, RGT_X + 24.0f, RP_LOADOUT_Y + 48.0f, 15.0f, white, UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"INTERNAL %s / %s", MechParts_GetInternal(g_InternalSel[0]).name, MechParts_GetInternal(g_InternalSel[1]).name);
    Text(buf, RGT_X + 24.0f, RP_LOADOUT_Y + 72.0f, 15.0f, white, UIFont::Mono, UIAlign::Left, true);

    // 合計性能
    swprintf_s(buf, L"AP %d", static_cast<int>(PLAYER_BASE_HP * stats.hpMul));
    Text(buf, RGT_X + 24.0f, RP_STATS_Y + 4.0f, 22.0f, ToD2D(stats.hpMul >= 1.0f ? kCyan : kRed), UIFont::Display, UIAlign::Left, true);
    swprintf_s(buf, L"SPD %s", pct(stats.speedMul - 1.0f).c_str());
    Text(buf, RGT_X + RGT_W * 0.5f, RP_STATS_Y + 4.0f, 22.0f, ToD2D(stats.speedMul >= 1.0f ? kCyan : kRed), UIFont::Display, UIAlign::Center, true);
    swprintf_s(buf, L"ATK %s", pct(stats.attackMul - 1.0f).c_str());
    Text(buf, RGT_X + RGT_W - 24.0f, RP_STATS_Y + 4.0f, 22.0f, ToD2D(stats.attackMul >= 1.0f ? kCyan : kRed), UIFont::Display, UIAlign::Right, true);

    Text(L"BUDGET", RGT_X + 24.0f, RP_CREDIT_Y + 4.0f, 13.0f, label, UIFont::Mono, UIAlign::Left, true);
    if (!g_ShopMode && g_SavedCredits > 0)
    {
        swprintf_s(buf, L"+ REWARD %d c", g_SavedCredits);
        Text(buf, RGT_X + 24.0f, RP_CREDIT_Y + 22.0f, 11.0f, ToD2D(kGreen, 0.85f), UIFont::Mono, UIAlign::Left, true);
    }
    swprintf_s(buf, L"%d / %d c", remaining, TotalBudget());
    Text(buf, RGT_X + RGT_W - 24.0f, RP_CREDIT_Y, 22.0f, overBudget ? ToD2D(kRed) : ToD2D(kAmber), UIFont::Display, UIAlign::Right, true);

    FlushText();
}

//==============================================================================
// Getter
//==============================================================================
WeaponID AssemblyScreen_GetRightWeapon()     { return static_cast<WeaponID>(g_RightSelected); }
WeaponID AssemblyScreen_GetLeftWeapon()      { return static_cast<WeaponID>(g_LeftSelected);  }
int      AssemblyScreen_GetRemainingCredits(){ return CalcRemaining(); }
bool     AssemblyScreen_WasCancelled()       { return g_Cancelled; }

void AssemblyScreen_SetShopMode(bool on, int budget)
{
    g_ShopMode   = on;
    g_ShopBudget = budget;
    if (on) { g_Tab = TAB_WEAPON; g_Focus = g_RightSelected; }
}

bool AssemblyScreen_IsShopMode() { return g_ShopMode; }

int AssemblyScreen_GetSavedCredits() { return g_SavedCredits; }

void AssemblyScreen_SetSavedCredits(int credits)
{
    g_SavedCredits = std::clamp(credits, 0, MAX_SAVED_CREDITS);
}

void AssemblyScreen_AddSavedCredits(int amount)
{
    if (amount <= 0) return;
    g_SavedCredits = std::min(MAX_SAVED_CREDITS, g_SavedCredits + std::min(amount, MAX_SAVED_CREDITS));
}

void AssemblyScreen_SetDefaults(WeaponID right, WeaponID left)
{
    // 壊れたセーブ（範囲外）は既定値に戻す
    g_DefaultRight = (right >= 0 && right < WEAPON_COUNT) ? static_cast<int>(right) : WEAPON_MACHINEGUN;
    g_DefaultLeft  = (left  >= 0 && left  < WEAPON_COUNT) ? static_cast<int>(left)  : WEAPON_SHIELD;
    // Initialize() が呼ばれない QuickStart でも正しい値を返せるように確定武器も更新
    g_RightSelected = g_DefaultRight;
    g_LeftSelected  = g_DefaultLeft;
}
