/*==============================================================================

   グラデーション塗りつぶしロゴテキスト [text_logo.h]
   Author : 51106
   Date   : 2026/03/25

--------------------------------------------------------------------------------
   ■概要
   Direct2D の FillOpacityMask を使い、文字のアルファ形状を
   グラデーションブラシで塗りつぶすロゴ描画システム。

   ■仕組み
     1. テキストを Compatible RT に白文字で描画 → アルファマスクビットマップ化
     2. FillOpacityMask でグラデーションブラシをマスク形状に流し込む
     3. アウトラインはオフセット重ね描きで実現

   ■使い方
     // main.cpp
     TextLogo_Initialize();
     ...
     TextLogo_Finalize();

     // game_window.cpp の ToggleFullscreen 内
     TextLogo_PreResize();
     ... ResizeBuffers ...
     TextLogo_PostResize();

     // 描画（Sprite_Begin() / Direct3D_SetBlendState(true) の後）
     LogoStyle style;
     style.fontSize    = 90.0f;
     style.fontName    = L"Agency FB";
     style.colorTop    = D2D1::ColorF(1.0f, 0.85f, 0.1f, 1.0f);  // ゴールド上
     style.colorBottom = D2D1::ColorF(0.85f, 0.4f, 0.0f, 1.0f);  // ゴールド下
     style.outlineColor = D2D1::ColorF(0.05f, 0.02f, 0.0f, 1.0f);
     style.outlineWidth = 3.0f;
     TextLogo_Draw(L"GAME TITLE", 800.0f, 200.0f, style);

==============================================================================*/
#pragma once

#include <d2d1.h>
#include <d2d1helper.h>

//==============================================================================
// ロゴスタイル設定
//==============================================================================
struct LogoStyle
{
    float           fontSize     = 90.0f;
    const wchar_t*  fontName     = L"Agency FB";

    // ── グラデーション塗り（texturePath が nullptr の場合に使用） ──────────
    D2D1_COLOR_F    colorTop     = D2D1::ColorF(1.0f,  0.85f, 0.1f,  1.0f);
    D2D1_COLOR_F    colorBottom  = D2D1::ColorF(0.85f, 0.40f, 0.0f,  1.0f);

    // ── テクスチャ塗り（設定するとテクスチャを文字形状でクリップ） ─────────
    // PNG / JPG など WIC が読める形式のファイルパスを指定する
    // 例: L"resource/texture/fire_texture.png"
    // nullptr のままにするとグラデーション塗りになる
    const wchar_t*  texturePath  = nullptr;

    // ── アウトライン ─────────────────────────────────────────────────────
    D2D1_COLOR_F    outlineColor = D2D1::ColorF(0.05f, 0.02f, 0.0f,  1.0f);
    float           outlineWidth = 3.0f;   // 0.0f でアウトラインなし
};

// 初期化 / 終了（main.cpp で Direct3D 初期化後に呼ぶ）
void TextLogo_Initialize();
void TextLogo_Finalize();

// フルスクリーン切替時の RT 再生成
void TextLogo_PreResize();
void TextLogo_PostResize();

// ロゴ描画
//   text  : 表示文字列
//   cx,cy : 画面上の中心座標（仮想 1600×900 空間）
//   style : フォント・グラデーション・アウトライン設定
//   scale : 追加スケール（選択アニメ等に使用、省略時 1.0f）
void TextLogo_Draw(const wchar_t* text, float cx, float cy, const LogoStyle& style, float scale = 1.0f);
