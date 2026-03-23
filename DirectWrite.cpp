/*==============================================================================

   DirectWrite テキスト描画 実装 [DirectWrite.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/11
--------------------------------------------------------------------------------
   Direct2D / DirectWrite を使い、日本語フォントを含む任意テキストを
   DXGI バックバッファサーフェスに直接描画する。

   ■初期化フロー
     Init() → D2D1Factory 生成 → DXGI サーフェス取得 →
     DXGI サーフェス RenderTarget 生成 → IDWriteFactory 生成 →
     TextFormat / ブラシ生成

   ■描画フロー
     DrawString() → BeginDraw() → DrawTextLayout/DrawText() → EndDraw()
     ※ BeginDraw/EndDraw はコール毎に呼ぶ（他の D3D 描画と混在注意）

   ■フルスクリーン対応
     バックバッファ再生成後は Init() を再度呼ぶか、SetScale() で
     仮想座標系→実ピクセル座標系への行列変換を適用する。

==============================================================================*/
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <dxgi.h>
#include <string>
#include <vector>
#include <algorithm>
#include "direct3d.h"
#include "DirectWrite.h"

//==============================================================================
// 全インスタンスの追跡リスト（PreResize / PostResize 用）
//==============================================================================
static std::vector<DirectWrite*> s_AllInstances;

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

//==============================================================================
// コンストラクタ（FontData ポインタ）
//==============================================================================
DirectWrite::DirectWrite(FontData* set) : Setting(set)
{
    s_AllInstances.push_back(this);
}

//==============================================================================
// デストラクタ（全インスタンスリストから除外）
//==============================================================================
DirectWrite::~DirectWrite()
{
    auto it = std::find(s_AllInstances.begin(), s_AllInstances.end(), this);
    if (it != s_AllInstances.end()) s_AllInstances.erase(it);
}

//=================================================================================================================================
// コンストラクタ（個別引数）
//=================================================================================================================================
DirectWrite::DirectWrite(
    Font font,
    IDWriteFontCollection* fontCollection,
    DWRITE_FONT_WEIGHT fontWeight,
    DWRITE_FONT_STYLE fontStyle,
    DWRITE_FONT_STRETCH fontStretch,
    FLOAT fontSize,
    WCHAR const* localeName,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    D2D1_COLOR_F Color)
{
    Setting->font           = font;
    Setting->fontCollection = fontCollection;
    Setting->fontWeight     = fontWeight;
    Setting->fontStyle      = fontStyle;
    Setting->fontStretch    = fontStretch;
    Setting->fontSize       = fontSize;
    Setting->localeName     = localeName;
    Setting->textAlignment  = textAlignment;
    Setting->Color          = Color;
    s_AllInstances.push_back(this);
}

//=============================================================================
// フォント設定（FontDataポインタ）
//=============================================================================
void DirectWrite::SetFont(FontData* set)
{
    pDWriteFactory->CreateTextFormat(
        FontList[(int)set->font],
        set->fontCollection,
        set->fontWeight,
        set->fontStyle,
        set->fontStretch,
        set->fontSize,
        set->localeName,
        &pTextFormat);

    pTextFormat->SetTextAlignment(set->textAlignment);
    pRT->CreateSolidColorBrush(set->Color, &pSolidBrush);
}

//=================================================================================================================================
// フォント設定（個別引数）
//=================================================================================================================================
void DirectWrite::SetFont(
    Font font,
    IDWriteFontCollection* fontCollection,
    DWRITE_FONT_WEIGHT fontWeight,
    DWRITE_FONT_STYLE fontStyle,
    DWRITE_FONT_STRETCH fontStretch,
    FLOAT fontSize,
    WCHAR const* localeName,
    DWRITE_TEXT_ALIGNMENT textAlignment,
    D2D1_COLOR_F Color)
{
    pDWriteFactory->CreateTextFormat(
        FontList[(int)font],
        fontCollection,
        fontWeight,
        fontStyle,
        fontStretch,
        fontSize,
        localeName,
        &pTextFormat);

    pTextFormat->SetTextAlignment(textAlignment);
    pRT->CreateSolidColorBrush(Color, &pSolidBrush);
}

//=============================================================================
// 文字描画（位置指定）
//=============================================================================
void DirectWrite::DrawString(std::string str, float x, float y, D2D1_DRAW_TEXT_OPTIONS options)
{
    // 文字列の変換
    std::wstring wstr = StringToWString(str.c_str());

    // ターゲットサイズの取得
    D2D1_SIZE_F TargetSize = pRT->GetSize();

    // テキストレイアウトを作成
    pDWriteFactory->CreateTextLayout(
        wstr.c_str(),
        (UINT32)wstr.size(),
        pTextFormat,
        TargetSize.width,
        TargetSize.height,
        &pTextLayout);

    // 描画位置
    D2D1_POINT_2F point;
    point.x = x;
    point.y = y;

    // 描画
    pRT->BeginDraw();
    pRT->DrawTextLayout(point, pTextLayout, pSolidBrush, options);
    pRT->EndDraw();
}

//=============================================================================
// 文字描画（位置指定・ワイド文字版）
//=============================================================================
void DirectWrite::DrawString(std::wstring wstr, float x, float y, D2D1_DRAW_TEXT_OPTIONS options)
{
    D2D1_SIZE_F TargetSize = pRT->GetSize();

    pDWriteFactory->CreateTextLayout(
        wstr.c_str(),
        (UINT32)wstr.size(),
        pTextFormat,
        TargetSize.width,
        TargetSize.height,
        &pTextLayout);

    D2D1_POINT_2F point;
    point.x = x;
    point.y = y;

    pRT->BeginDraw();
    pRT->DrawTextLayout(point, pTextLayout, pSolidBrush, options);
    pRT->EndDraw();
}

//=============================================================================
// 文字描画（矩形領域指定）
//=============================================================================
void DirectWrite::DrawString(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options)
{
    // 文字列の変換
    std::wstring wstr = StringToWString(str.c_str());

    // 描画
    pRT->BeginDraw();
    pRT->DrawText(wstr.c_str(), (UINT32)wstr.size(), pTextFormat, rect, pSolidBrush, options);
    pRT->EndDraw();
}

//=============================================================================
// 初期化
//=============================================================================
void DirectWrite::Init()
{
    // Direct2D ファクトリの作成
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);

    // バックバッファの取得
    Direct3D_GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

    // レンダーターゲットプロパティ（DPIは96固定）
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);

    // サーフェスに描画するレンダーターゲットを作成
    pD2DFactory->CreateDxgiSurfaceRenderTarget(pBackBuffer, &props, &pRT);

    // アンチエイリアシングモード
    pRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    // IDWriteFactory の作成
    DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&pDWriteFactory));

    // フォントコレクションの決定
    // fontFilePath が指定されている場合はファイルから読み込む（配布exe対応）
    IDWriteFontCollection* collectionToUse = Setting->fontCollection;
    if (Setting->fontFilePath != nullptr)
    {
        IDWriteFactory3* factory3 = nullptr;
        pDWriteFactory->QueryInterface(__uuidof(IDWriteFactory3),
            reinterpret_cast<void**>(&factory3));
        if (factory3)
        {
            IDWriteFontSetBuilder* builder = nullptr;
            factory3->CreateFontSetBuilder(&builder);
            if (builder)
            {
                // AddFontFile は IDWriteFontSetBuilder1 のメンバーなので QI で取得
                IDWriteFontSetBuilder1* builder1 = nullptr;
                builder->QueryInterface(__uuidof(IDWriteFontSetBuilder1),
                    reinterpret_cast<void**>(&builder1));

                // 相対パスを絶対パスに変換（exeからの相対パスを解決）
                wchar_t absPath[MAX_PATH] = {};
                GetFullPathNameW(Setting->fontFilePath, MAX_PATH, absPath, nullptr);

                IDWriteFontFile* fontFile = nullptr;
                factory3->CreateFontFileReference(absPath, nullptr, &fontFile);
                if (fontFile)
                {
                    if (builder1) builder1->AddFontFile(fontFile);
                    fontFile->Release();
                }
                IDWriteFontSet* fontSet = nullptr;
                if (builder1) { builder1->CreateFontSet(&fontSet); builder1->Release(); }
                else            builder->CreateFontSet(&fontSet);
                builder->Release();
                if (fontSet)
                {
                    IDWriteFontCollection1* col1 = nullptr;
                    factory3->CreateFontCollectionFromFontSet(fontSet, &col1);
                    fontSet->Release();
                    pCustomFontCollection = col1;
                    collectionToUse = col1;
                }
            }
            factory3->Release();
        }
    }

    // テキストフォーマットの作成
    pDWriteFactory->CreateTextFormat(
        FontList[(int)Setting->font],
        collectionToUse,
        Setting->fontWeight,
        Setting->fontStyle,
        Setting->fontStretch,
        Setting->fontSize,
        Setting->localeName,
        &pTextFormat);

    // テキスト配置
    pTextFormat->SetTextAlignment(Setting->textAlignment);

    // ブラシの作成
    pRT->CreateSolidColorBrush(Setting->Color, &pSolidBrush);
}

//=============================================================================
// スケール変換（仮想座標系→実ピクセル座標系）
//=============================================================================
void DirectWrite::SetScale(float sx, float sy)
{
    if (pRT)
        pRT->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy));
}

//=============================================================================
// 終了処理
//=============================================================================
void DirectWrite::Release()
{
    if (pTextLayout)         { pTextLayout->Release();          pTextLayout          = NULL; }
    if (pSolidBrush)         { pSolidBrush->Release();          pSolidBrush          = NULL; }
    if (pTextFormat)         { pTextFormat->Release();          pTextFormat          = NULL; }
    if (pCustomFontCollection){ pCustomFontCollection->Release(); pCustomFontCollection = NULL; }
    if (pRT)                 { pRT->Release();                  pRT                  = NULL; }
    if (pBackBuffer)         { pBackBuffer->Release();          pBackBuffer          = NULL; }
    if (pDWriteFactory)      { pDWriteFactory->Release();       pDWriteFactory       = NULL; }
    if (pD2DFactory)         { pD2DFactory->Release();          pD2DFactory          = NULL; }
}

//==============================================================================
// バッチ描画 ─ BeginBatch
//   D3D11 RTV をアンバインドしてから D2D BeginDraw を呼ぶ
//==============================================================================
void DirectWrite::BeginBatch()
{
    if (!pRT) return;
    // D3D11 と D2D が同じ DXGI サーフェスを共有するため、
    // D2D 描画前に D3D11 RTV をアンバインドしておく（コンテキスト汚染防止）
    Direct3D_GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
    pRT->BeginDraw();
}

//==============================================================================
// バッチ描画 ─ EndBatch
//   D2D EndDraw 後に D3D11 メイン RTV を再バインドする
//==============================================================================
void DirectWrite::EndBatch()
{
    if (!pRT) return;
    pRT->EndDraw();
    Direct3D_BindMainRenderTarget();
}

//==============================================================================
// バッチ描画 ─ DrawAt（実ピクセル座標・中心揃え）
//   cx,cy : 描画中心（実ピクセル座標）
//   halfW : テキスト矩形の半幅（数字桁数に合わせて調整）
//   color : RGBA（alpha はフェード等に使用）
//==============================================================================
void DirectWrite::DrawAt(const std::string& str, float cx, float cy, float halfW, D2D1_COLOR_F color, float outlinePx)
{
    DrawAt(StringToWString(str), cx, cy, halfW, color, outlinePx);
}

void DirectWrite::DrawAt(const std::wstring& wstr, float cx, float cy, float halfW, D2D1_COLOR_F color, float outlinePx)
{
    if (!pRT || !pTextFormat || !pSolidBrush) return;
    const float halfH = Setting ? Setting->fontSize * 0.75f : 16.0f;

    // 同じ矩形を指定色でまとめて描く補助ラムダ
    auto draw = [&](float ox, float oy, D2D1_COLOR_F col)
    {
        pSolidBrush->SetColor(col);
        pRT->DrawText(
            wstr.c_str(), (UINT32)wstr.size(), pTextFormat,
            D2D1::RectF(cx - halfW + ox, cy - halfH + oy,
                        cx + halfW + ox, cy + halfH + oy),
            pSolidBrush, D2D1_DRAW_TEXT_OPTIONS_NONE);
    };

    if (outlinePx > 0.0f)
    {
        // 縁取り：4方向オフセットで黒（テキストと同 alpha）を先に描く
        const D2D1_COLOR_F outline = D2D1::ColorF(0.0f, 0.0f, 0.0f, color.a);
        const float o = outlinePx;
        draw(-o, -o, outline);
        draw( o, -o, outline);
        draw(-o,  o, outline);
        draw( o,  o, outline);
    }

    // 本体テキストを最後に描いて縁の上に重ねる
    draw(0.0f, 0.0f, color);
}

//==============================================================================
// リサイズ対応 ─ ReleaseRT / ReinitRT（インスタンス単位）
//==============================================================================
void DirectWrite::ReleaseRT()
{
    if (pSolidBrush) { pSolidBrush->Release(); pSolidBrush = NULL; }
    if (pRT)         { pRT->Release();          pRT         = NULL; }
    if (pBackBuffer) { pBackBuffer->Release();  pBackBuffer = NULL; }
}

void DirectWrite::ReinitRT()
{
    if (!pD2DFactory) return;
    Direct3D_GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);
    pD2DFactory->CreateDxgiSurfaceRenderTarget(pBackBuffer, &props, &pRT);
    if (pRT)
    {
        pRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        if (Setting)
            pRT->CreateSolidColorBrush(Setting->Color, &pSolidBrush);
    }
}

//==============================================================================
// リサイズ対応 ─ PreResize / PostResize（全インスタンス一括）
//   ResizeBuffers は全ての D2D RT（= DXGI サーフェス参照）を
//   解放してから呼ばないと失敗するため、一括 Release → Resize → 一括 Reinit
//==============================================================================
void DirectWrite::PreResize()
{
    for (auto* inst : s_AllInstances)
        inst->ReleaseRT();
}

void DirectWrite::PostResize()
{
    for (auto* inst : s_AllInstances)
        inst->ReinitRT();
}

//=============================================================================
// stringをwstringへ変換する
//=============================================================================
std::wstring DirectWrite::StringToWString(std::string oString)
{
    int iBufferSize = MultiByteToWideChar(CP_ACP, 0, oString.c_str(), -1, (wchar_t*)NULL, 0);
    wchar_t* cpUCS2 = new wchar_t[iBufferSize];
    MultiByteToWideChar(CP_ACP, 0, oString.c_str(), -1, cpUCS2, iBufferSize);
    std::wstring oRet(cpUCS2, cpUCS2 + iBufferSize - 1);
    delete[] cpUCS2;
    return oRet;
}
