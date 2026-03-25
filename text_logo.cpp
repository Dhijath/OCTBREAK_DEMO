/*==============================================================================

   グラデーション塗りつぶしロゴテキスト [text_logo.cpp]
   Author : 51106
   Date   : 2026/03/25

==============================================================================*/
#include "text_logo.h"
#include "direct3d.h"
#include <dwrite.h>
#include <dxgi.h>
#include <d2d1helper.h>
#include <wincodec.h>
#include <string>
#include <cstring>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

//==============================================================================
// マスクキャッシュプール
//   テキスト / フォント名 / サイズが同じエントリを再利用する
//   （タイトル画面の START, OPTION, EXIT など複数テキストに対応）
//==============================================================================
static constexpr int MAX_CACHE = 16;

struct MaskEntry
{
    std::wstring text;
    std::wstring fontName;
    float        fontSize = 0.f;
    float        maskW    = 0.f;
    float        maskH    = 0.f;

    ID2D1BitmapRenderTarget* pBitmapRT = nullptr;
    ID2D1Bitmap*             pBitmap   = nullptr;
    bool                     valid     = false;

    void Release()
    {
        if (pBitmap)   { pBitmap->Release();   pBitmap   = nullptr; }
        if (pBitmapRT) { pBitmapRT->Release(); pBitmapRT = nullptr; }
        valid = false;
        text.clear();
        fontName.clear();
    }
};

static MaskEntry s_Pool[MAX_CACHE];

static void ClearPool()
{
    for (auto& e : s_Pool) e.Release();
}

static MaskEntry* FindEntry(const wchar_t* text, const LogoStyle& style)
{
    for (auto& e : s_Pool)
        if (e.valid && e.text == text &&
            e.fontName == style.fontName &&
            e.fontSize == style.fontSize)
            return &e;
    return nullptr;
}

static MaskEntry* GetFreeSlot()
{
    for (auto& e : s_Pool)
        if (!e.valid) return &e;
    // 全スロット埋まっていれば先頭を解放して再利用
    s_Pool[0].Release();
    return &s_Pool[0];
}

//==============================================================================
// 内部リソース
//==============================================================================
static ID2D1Factory*       s_pD2DFactory = nullptr;
static IDWriteFactory*     s_pDWFactory  = nullptr;
static ID2D1RenderTarget*  s_pRT         = nullptr;
static IWICImagingFactory* s_pWICFactory = nullptr;

// テクスチャビットマップキャッシュ（1 枚）
struct TexCache
{
    std::wstring path;
    ID2D1Bitmap* pBitmap = nullptr;
    void Release() { if (pBitmap) { pBitmap->Release(); pBitmap = nullptr; } path.clear(); }
};
static TexCache s_TexCache;

//==============================================================================
// D2D レンダーターゲット生成
//==============================================================================
static bool CreateRT()
{
    if (!s_pD2DFactory) return false;
    IDXGISurface* pSurface = nullptr;
    if (FAILED(Direct3D_GetSwapChain()->GetBuffer(
            0, __uuidof(IDXGISurface), reinterpret_cast<void**>(&pSurface))))
        return false;

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

    HRESULT hr = s_pD2DFactory->CreateDxgiSurfaceRenderTarget(pSurface, &props, &s_pRT);
    pSurface->Release();
    return SUCCEEDED(hr);
}

//==============================================================================
// 初期化 / 終了
//==============================================================================
void TextLogo_Initialize()
{
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &s_pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&s_pDWFactory));
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&s_pWICFactory));
    CreateRT();
}

void TextLogo_Finalize()
{
    ClearPool();
    s_TexCache.Release();
    if (s_pRT)         { s_pRT->Release();         s_pRT         = nullptr; }
    if (s_pWICFactory) { s_pWICFactory->Release(); s_pWICFactory = nullptr; }
    if (s_pDWFactory)  { s_pDWFactory->Release();  s_pDWFactory  = nullptr; }
    if (s_pD2DFactory) { s_pD2DFactory->Release(); s_pD2DFactory = nullptr; }
}

void TextLogo_PreResize()
{
    ClearPool();
    s_TexCache.Release();
    if (s_pRT) { s_pRT->Release(); s_pRT = nullptr; }
}

void TextLogo_PostResize()
{
    CreateRT();
}

//==============================================================================
// WIC テクスチャ読み込み
//==============================================================================
static ID2D1Bitmap* LoadTextureBitmap(const wchar_t* path)
{
    if (!s_pWICFactory || !s_pRT || !path) return nullptr;
    if (s_TexCache.pBitmap && s_TexCache.path == path) return s_TexCache.pBitmap;

    s_TexCache.Release();

    IWICBitmapDecoder*     pDecoder   = nullptr;
    IWICBitmapFrameDecode* pFrame     = nullptr;
    IWICFormatConverter*   pConverter = nullptr;

    HRESULT hr = s_pWICFactory->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pFrame);
    if (SUCCEEDED(hr)) hr = s_pWICFactory->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) hr = pConverter->Initialize(pFrame,
        GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
        nullptr, 0.0, WICBitmapPaletteTypeMedianCut);

    ID2D1Bitmap* pBitmap = nullptr;
    if (SUCCEEDED(hr)) s_pRT->CreateBitmapFromWicBitmap(pConverter, nullptr, &pBitmap);

    if (pConverter) pConverter->Release();
    if (pFrame)     pFrame->Release();
    if (pDecoder)   pDecoder->Release();

    if (pBitmap) { s_TexCache.path = path; s_TexCache.pBitmap = pBitmap; }
    return pBitmap;
}

//==============================================================================
// アルファマスクビットマップを構築してプールに登録
//==============================================================================
static MaskEntry* BuildMask(const wchar_t* text, const LogoStyle& style)
{
    if (!s_pRT || !s_pDWFactory || !s_pD2DFactory) return nullptr;

    IDWriteTextFormat* pFmt = nullptr;
    if (FAILED(s_pDWFactory->CreateTextFormat(
        style.fontName, nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        style.fontSize, L"en-us", &pFmt))) return nullptr;

    pFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    pFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const UINT32 len = (UINT32)wcslen(text);

    // バウンディングボックス計測
    IDWriteTextLayout* pLayout = nullptr;
    s_pDWFactory->CreateTextLayout(text, len, pFmt, 1600.f, 400.f, &pLayout);

    DWRITE_TEXT_METRICS m{};
    if (pLayout) pLayout->GetMetrics(&m);
    if (pLayout) pLayout->Release();

    const float pad  = style.outlineWidth * 2.f + 6.f;
    const float maskW = m.width  + pad * 2.f;
    const float maskH = m.height + pad * 2.f;

    // Compatible RT（アルファマスク用）生成
    ID2D1BitmapRenderTarget* pBitmapRT = nullptr;
    if (FAILED(s_pRT->CreateCompatibleRenderTarget(
        D2D1::SizeF(maskW, maskH), &pBitmapRT)))
    {
        pFmt->Release();
        return nullptr;
    }

    // 白文字を透明背景に描画
    ID2D1SolidColorBrush* pWhite = nullptr;
    pBitmapRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &pWhite);

    IDWriteTextLayout* pLayout2 = nullptr;
    s_pDWFactory->CreateTextLayout(text, len, pFmt, maskW, maskH, &pLayout2);
    if (pLayout2)
    {
        pLayout2->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        pLayout2->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    pBitmapRT->BeginDraw();
    pBitmapRT->Clear(D2D1::ColorF(0, 0, 0, 0));
    if (pLayout2 && pWhite)
        pBitmapRT->DrawTextLayout(D2D1::Point2F(0, 0), pLayout2, pWhite);
    pBitmapRT->EndDraw();

    ID2D1Bitmap* pBitmap = nullptr;
    pBitmapRT->GetBitmap(&pBitmap);

    if (pLayout2) pLayout2->Release();
    if (pWhite)   pWhite->Release();
    pFmt->Release();

    if (!pBitmap) { pBitmapRT->Release(); return nullptr; }

    // スロットに登録
    MaskEntry* slot  = GetFreeSlot();
    slot->text       = text;
    slot->fontName   = style.fontName;
    slot->fontSize   = style.fontSize;
    slot->maskW      = maskW;
    slot->maskH      = maskH;
    slot->pBitmapRT  = pBitmapRT;
    slot->pBitmap    = pBitmap;
    slot->valid      = true;
    return slot;
}

//==============================================================================
// ロゴ描画
//   cx, cy : 中心座標（仮想 1600×900 空間）
//   scale  : 追加スケール（選択アニメ等に使用、省略時 1.0f）
//==============================================================================
void TextLogo_Draw(const wchar_t* text, float cx, float cy,
                   const LogoStyle& style, float scale)
{
    if (!s_pRT || !text || !text[0]) return;

    MaskEntry* entry = FindEntry(text, style);
    if (!entry) entry = BuildMask(text, style);
    if (!entry) return;

    const float maskW = entry->maskW * scale;
    const float maskH = entry->maskH * scale;

    const float scaleX = (float)Direct3D_GetBackBufferWidth()  / 1600.f;
    const float scaleY = (float)Direct3D_GetBackBufferHeight() / 900.f;

    const float dstX = (cx - maskW * 0.5f) * scaleX;
    const float dstY = (cy - maskH * 0.5f) * scaleY;
    const float dstW = maskW * scaleX;
    const float dstH = maskH * scaleY;

    const D2D1_RECT_F destRect = D2D1::RectF(dstX, dstY, dstX + dstW, dstY + dstH);
    const D2D1_RECT_F srcRect  = D2D1::RectF(0, 0, entry->maskW, entry->maskH);

    s_pRT->BeginDraw();
    s_pRT->SetTransform(D2D1::IdentityMatrix());

    // ── アウトライン（8方向オフセット）──────────────────────────────────
    if (style.outlineWidth > 0.f)
    {
        ID2D1SolidColorBrush* pOL = nullptr;
        s_pRT->CreateSolidColorBrush(style.outlineColor, &pOL);
        if (pOL)
        {
            const float ow = style.outlineWidth * scaleX;
            const float oh = style.outlineWidth * scaleY;
            const D2D1_POINT_2F offsets[] = {
                {-ow,-oh},{0,-oh},{ow,-oh},
                {-ow, 0},        {ow, 0},
                {-ow, oh},{0, oh},{ow, oh},
            };
            s_pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            for (auto& o : offsets)
            {
                D2D1_RECT_F r = D2D1::RectF(destRect.left+o.x, destRect.top+o.y,
                                             destRect.right+o.x, destRect.bottom+o.y);
                s_pRT->FillOpacityMask(entry->pBitmap, pOL,
                    D2D1_OPACITY_MASK_CONTENT_TEXT_NATURAL, &r, &srcRect);
            }
            s_pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            pOL->Release();
        }
    }

    // ── 塗りつぶし（テクスチャ or グラデーション）────────────────────────
    ID2D1Brush* pFill = nullptr;
    ID2D1BitmapBrush*            pBBrush = nullptr;
    ID2D1LinearGradientBrush*    pGBrush = nullptr;
    ID2D1GradientStopCollection* pStops  = nullptr;

    if (style.texturePath)
    {
        ID2D1Bitmap* pTex = LoadTextureBitmap(style.texturePath);
        if (pTex)
        {
            s_pRT->CreateBitmapBrush(pTex, &pBBrush);
            if (pBBrush)
            {
                const D2D1_SIZE_F sz = pTex->GetSize();
                pBBrush->SetTransform(
                    D2D1::Matrix3x2F::Scale(dstW / sz.width, dstH / sz.height) *
                    D2D1::Matrix3x2F::Translation(dstX, dstY));
                pBBrush->SetExtendModeX(D2D1_EXTEND_MODE_CLAMP);
                pBBrush->SetExtendModeY(D2D1_EXTEND_MODE_CLAMP);
                pFill = pBBrush;
            }
        }
    }

    if (!pFill)
    {
        D2D1_GRADIENT_STOP stops[] = {{0.f, style.colorTop},{1.f, style.colorBottom}};
        s_pRT->CreateGradientStopCollection(stops, 2, &pStops);
        if (pStops)
        {
            s_pRT->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(destRect.left, destRect.top),
                    D2D1::Point2F(destRect.left, destRect.bottom)),
                pStops, &pGBrush);
            pFill = pGBrush;
        }
    }

    if (pFill)
    {
        s_pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        s_pRT->FillOpacityMask(entry->pBitmap, pFill,
            D2D1_OPACITY_MASK_CONTENT_TEXT_NATURAL, &destRect, &srcRect);
        s_pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }

    if (pGBrush) pGBrush->Release();
    if (pStops)  pStops->Release();
    if (pBBrush) pBBrush->Release();

    s_pRT->EndDraw();
    Direct3D_BindMainRenderTarget();
}
