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
    MeiryoUI,
    Consolas,
    OCR_A,        // 機械読み取り文字風
    CourierNew,   // タイプライター
    AgencyFB,     // 圧縮・ミリタリー調
    LucidaConsole,
    DSEG7         // 7セグメントLED表示器風（要インストール）
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
        L"Meiryo UI",
        L"Consolas",
        L"OCR A Extended",
        L"Courier New",
        L"Agency FB",
        L"Lucida Console",
        L"DSEG7 Modern"
    };
}

//=============================================================================
// フォント設定
//=============================================================================
struct FontData
{
    Font font;                          // フォント名（FontListのインデックスに対応）
    IDWriteFontCollection* fontCollection; // フォントコレクション（通常nullptr）
    DWRITE_FONT_WEIGHT fontWeight;      // フォントの太さ
    DWRITE_FONT_STYLE fontStyle;        // フォントスタイル
    DWRITE_FONT_STRETCH fontStretch;    // フォントの幅
    FLOAT fontSize;                     // フォントサイズ
    WCHAR const* localeName;            // ロケール名
    DWRITE_TEXT_ALIGNMENT textAlignment; // テキストの配置
    D2D1_COLOR_F Color;                 // フォントの色

    // フォントファイルパス（設定するとファイルから読み込み・配布時に使用）
    // 例: L"resource/fonts/DSEG7Classic-Regular.ttf"
    // nullptr の場合はシステムフォントを使用
    const wchar_t* fontFilePath = nullptr;

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
        fontFilePath = nullptr;
    }
};

//=============================================================================
// DirectWrite設定
//=============================================================================
class DirectWrite
{
private:
    ID2D1Factory*           pD2DFactory          = NULL;
    IDWriteFactory*         pDWriteFactory        = NULL;
    IDWriteTextFormat*      pTextFormat           = NULL;
    IDWriteTextLayout*      pTextLayout           = NULL;
    ID2D1RenderTarget*      pRT                  = NULL;
    ID2D1SolidColorBrush*   pSolidBrush          = NULL;
    IDXGISurface*           pBackBuffer          = NULL;
    IDWriteFontCollection*  pCustomFontCollection = NULL; // ファイルから生成した場合

    // フォントデータ
    FontData* Setting = new FontData();

    // stringをwstringへ変換する
    std::wstring StringToWString(std::string oString);

    // リサイズ時の RT 部分だけ解放／再生成
    void ReleaseRT();
    void ReinitRT();

public:
    // デフォルトコンストラクタを制限
    DirectWrite() = delete;

    // デストラクタ（全インスタンスリストから除外）
    ~DirectWrite();

    // コンストラクタ
    // 第1引数：フォント設定
    DirectWrite(FontData* set);

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

    //--------------------------------------------------------------------------
    // バッチ描画 API（DamagePopup などで複数テキストを 1 パスで描く）
    //   BeginBatch() → DrawAt() × N → EndBatch() の順で呼ぶ
    //   BeginBatch が D3D11 RTV を自動アンバインドし、
    //   EndBatch が描画後に RTV を再バインドする
    //--------------------------------------------------------------------------
    void BeginBatch();
    void EndBatch();

    // バッチ内テキスト描画（実ピクセル座標 cx,cy を中心に halfW 幅で描く）
    // outlinePx > 0 のとき、4方向オフセット描画による縁取りを付ける
    void DrawAt(const std::string&  str,  float cx, float cy, float halfW, D2D1_COLOR_F color, float outlinePx = 0.0f);
    void DrawAt(const std::wstring& wstr, float cx, float cy, float halfW, D2D1_COLOR_F color, float outlinePx = 0.0f);

    //--------------------------------------------------------------------------
    // フルスクリーン切替対応（game_window.cpp の ToggleFullscreen から呼ぶ）
    //   PreResize  : ResizeBuffers 前 – 全インスタンスの D2D RT を解放
    //   PostResize : ResizeBuffers 後 – 全インスタンスの D2D RT を再生成
    //--------------------------------------------------------------------------
    static void PreResize();
    static void PostResize();
};
