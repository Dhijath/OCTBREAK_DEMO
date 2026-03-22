/*==============================================================================

   DirectWrite テキスト描画 [DirectWrite.h]
                                                         Author : 51106
                                                         Date   : 2026/03/11
--------------------------------------------------------------------------------
   Direct2D / DirectWrite を使って日本語を含む任意フォントのテキストを
   バックバッファに直接描画するクラス。

   ■使い方
     1. FontData を設定してコンストラクタに渡す
     2. Init() で D2D レンダーターゲット・ブラシを生成
     3. DrawString() で位置または矩形を指定して文字列を描画
     4. Release() でリソース解放

   ■フルスクリーン対応
     バックバッファサイズが変わると D2D レンダーターゲットも再生成が必要。
     SetScale(scaleX, scaleY) で仮想座標系→実ピクセル座標系のスケールを適用できる
     （AssemblyScreen.cpp などで使用）。

   ■対応フォント
     Font::Meiryo / Font::Arial / Font::MeiryoUI（FontData で指定）

==============================================================================*/
#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include "direct3d.h"

//=============================================================================
// フォントリスト
//=============================================================================
enum class Font
{
    Meiryo,
    Arial,
    MeiryoUI
};

//=============================================================================
// フォント名
//=============================================================================
namespace
{
    const WCHAR* FontList[]
    {
        L"メイリオ",
        L"Arial",
        L"Meiryo UI"
    };
}

//=============================================================================
// フォント設定
//=============================================================================
struct FontData
{
    Font font;                          // フォント名
    IDWriteFontCollection* fontCollection; // フォントコレクション
    DWRITE_FONT_WEIGHT fontWeight;      // フォントの太さ
    DWRITE_FONT_STYLE fontStyle;        // フォントスタイル
    DWRITE_FONT_STRETCH fontStretch;    // フォントの幅
    FLOAT fontSize;                     // フォントサイズ
    WCHAR const* localeName;            // ロケール名
    DWRITE_TEXT_ALIGNMENT textAlignment; // テキストの配置
    D2D1_COLOR_F Color;                 // フォントの色

    // デフォルト設定
    FontData()
    {
        font = Font::Meiryo;
        fontCollection = nullptr;
        fontWeight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_NORMAL;
        fontStyle = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL;
        fontStretch = DWRITE_FONT_STRETCH::DWRITE_FONT_STRETCH_NORMAL;
        fontSize = 20;
        localeName = L"ja-jp";
        textAlignment = DWRITE_TEXT_ALIGNMENT::DWRITE_TEXT_ALIGNMENT_LEADING;
        Color = D2D1::ColorF(D2D1::ColorF::White);
    }
};

//=============================================================================
// DirectWrite設定
//=============================================================================
class DirectWrite
{
private:
    ID2D1Factory*           pD2DFactory     = NULL;
    IDWriteFactory*         pDWriteFactory  = NULL;
    IDWriteTextFormat*      pTextFormat     = NULL;
    IDWriteTextLayout*      pTextLayout     = NULL;
    ID2D1RenderTarget*      pRT             = NULL;
    ID2D1SolidColorBrush*   pSolidBrush     = NULL;
    IDXGISurface*           pBackBuffer     = NULL;

    // フォントデータ
    FontData* Setting = new FontData();

    // stringをwstringへ変換する
    std::wstring StringToWString(std::string oString);

public:
    // デフォルトコンストラクタを制限
    DirectWrite() = delete;

    // コンストラクタ
    // 第1引数：フォント設定
    DirectWrite(FontData* set) :Setting(set) {};

    // コンストラクタ
    DirectWrite(
        Font font,
        IDWriteFontCollection* fontCollection,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        DWRITE_FONT_STRETCH fontStretch,
        FLOAT fontSize,
        WCHAR const* localeName,
        DWRITE_TEXT_ALIGNMENT textAlignment,
        D2D1_COLOR_F Color
    );

    // フォント設定（FontDataポインタ）
    void SetFont(FontData* set);

    // フォント設定（個別引数）
    void SetFont(
        Font font,
        IDWriteFontCollection* fontCollection,
        DWRITE_FONT_WEIGHT fontWeight,
        DWRITE_FONT_STYLE fontStyle,
        DWRITE_FONT_STRETCH fontStretch,
        FLOAT fontSize,
        WCHAR const* localeName,
        DWRITE_TEXT_ALIGNMENT textAlignment,
        D2D1_COLOR_F Color
    );

    // 文字描画（位置指定）
    void DrawString(std::string str, float x, float y, D2D1_DRAW_TEXT_OPTIONS options);

    // 文字描画（位置指定・ワイド文字版）
    void DrawString(std::wstring wstr, float x, float y, D2D1_DRAW_TEXT_OPTIONS options);

    // 文字描画（矩形領域指定）
    void DrawString(std::string str, D2D1_RECT_F rect, D2D1_DRAW_TEXT_OPTIONS options);

    // 初期化
    void Init();

    // スケール変換を設定（仮想座標系→実ピクセル座標系のスケール）
    void SetScale(float sx, float sy);

    // 終了処理
    void Release();
};
