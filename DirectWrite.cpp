#include <d2d1.h>
#include <dwrite.h>
#include <dxgi.h>
#include <string>
#include "direct3d.h"
#include "DirectWrite.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

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

    // テキストフォーマットの作成
    pDWriteFactory->CreateTextFormat(
        FontList[(int)Setting->font],
        Setting->fontCollection,
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
    if (pTextLayout)    { pTextLayout->Release();   pTextLayout   = NULL; }
    if (pSolidBrush)    { pSolidBrush->Release();   pSolidBrush   = NULL; }
    if (pTextFormat)    { pTextFormat->Release();   pTextFormat   = NULL; }
    if (pRT)            { pRT->Release();            pRT           = NULL; }
    if (pBackBuffer)    { pBackBuffer->Release();    pBackBuffer   = NULL; }
    if (pDWriteFactory) { pDWriteFactory->Release(); pDWriteFactory = NULL; }
    if (pD2DFactory)    { pD2DFactory->Release();   pD2DFactory   = NULL; }
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
