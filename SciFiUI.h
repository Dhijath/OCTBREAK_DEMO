/*==============================================================================

   SF調UI部品 [SciFiUI.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ミッション選択画面・作戦中HUD・ミニマップで共通して使う、近未来風の描画部品。
   テクスチャは白1枚だけを使い、矩形の組み合わせで枠・ブラケット・走査線などを描く。

   ■使い方
     // 図形（スプライト）：その場で描画される
     SciFiUI::BeginSprites();
     SciFiUI::Panel(x, y, w, h);
     SciFiUI::Brackets(x, y, w, h, 14.0f, SciFiUI::kCyan);

     // 文字：いったん溜めて、Flush でまとめて描画する
     SciFiUI::Text(L"MISSION", x, y, 24.0f, SciFiUI::kCyan, SciFiUI::UIFont::Display);
     SciFiUI::FlushText();

   ■文字の描画について
     フォント（種類・大きさ・太さ・揃え）ごとに DirectWrite のインスタンスを作って
     使い回す。DirectWrite::SetFont を毎フレーム呼ぶとテキスト形式が作り直されて
     解放されないため、ここでは一度作ったものを最後まで保持する。

   ■座標系
     すべて仮想 1600×900 座標。

==============================================================================*/
#pragma once
#include <DirectXMath.h>
#include <d2d1.h>
#include <string>

namespace SciFiUI
{
    //==========================================================================
    // 配色
    //==========================================================================
    constexpr DirectX::XMFLOAT4 kCyan     = { 0.35f, 0.90f, 1.00f, 1.00f };
    constexpr DirectX::XMFLOAT4 kCyanDim  = { 0.20f, 0.55f, 0.70f, 0.60f };
    constexpr DirectX::XMFLOAT4 kAmber    = { 1.00f, 0.70f, 0.20f, 1.00f };
    constexpr DirectX::XMFLOAT4 kRed      = { 1.00f, 0.25f, 0.20f, 1.00f };
    constexpr DirectX::XMFLOAT4 kGreen    = { 0.35f, 1.00f, 0.55f, 1.00f };
    constexpr DirectX::XMFLOAT4 kPanel    = { 0.02f, 0.05f, 0.09f, 0.72f };
    constexpr DirectX::XMFLOAT4 kPanelHi  = { 0.05f, 0.18f, 0.28f, 0.80f };

    // XMFLOAT4 → D2D の色（alpha を掛け直せる）
    D2D1_COLOR_F ToD2D(const DirectX::XMFLOAT4& c, float alphaScale = 1.0f);
    DirectX::XMFLOAT4 WithAlpha(const DirectX::XMFLOAT4& c, float a);

    //==========================================================================
    // 図形（白テクスチャのスプライト）
    //==========================================================================
    void BeginSprites();   // 2D描画状態にする（深度オフ・αブレンド）

    void Fill(float x, float y, float w, float h, const DirectX::XMFLOAT4& color);
    void Frame(float x, float y, float w, float h, const DirectX::XMFLOAT4& color, float thick = 1.0f);

    // 四隅だけの L 字ブラケット
    void Brackets(float x, float y, float w, float h, float len, const DirectX::XMFLOAT4& color, float thick = 2.0f);

    // 回転した線（中心 cx,cy・長さ len・角度 rad）
    void LineAngle(float cx, float cy, float len, float angle, const DirectX::XMFLOAT4& color, float thick = 1.0f);

    // 右上の角を斜めに落としたパネル（地・縁・角の斜線・上辺のアクセント）
    void Panel(float x, float y, float w, float h,
               const DirectX::XMFLOAT4& fill = kPanel, const DirectX::XMFLOAT4& edge = kCyanDim, float cut = 14.0f);

    // 横方向の走査線（spacing ごとに細い線）
    void Scanlines(float x, float y, float w, float h, float spacing, float alpha);

    // 格子
    void Grid(float x, float y, float w, float h, float cell, const DirectX::XMFLOAT4& color);

    // 目盛り（水平。major 本ごとに長い目盛り）
    void Ticks(float x, float y, float w, int count, int major, const DirectX::XMFLOAT4& color);

    // 区切りのあるゲージ（ratio 0〜1）
    void SegmentBar(float x, float y, float w, float h, int segments, float ratio,
                    const DirectX::XMFLOAT4& on, const DirectX::XMFLOAT4& off);

    // ひし形（中心・半径）
    void Diamond(float cx, float cy, float r, const DirectX::XMFLOAT4& color);

    // 警告帯（色の区画が横に流れる）
    void HazardTape(float x, float y, float w, float h, float scroll, const DirectX::XMFLOAT4& color);

    //==========================================================================
    // 文字
    //==========================================================================
    enum class UIFont
    {
        Display,   // 見出し・数値（Agency FB）
        Mono,      // ラベル・コード（Consolas）
        Body,      // 日本語本文（メイリオ）
    };
    enum class UIAlign { Left, Center, Right };

    // (x, y) は文字の左上（Center は上辺中央、Right は右上）
    void Text(const std::wstring& text, float x, float y, float size, const D2D1_COLOR_F& color,
              UIFont font = UIFont::Body, UIAlign align = UIAlign::Left, bool bold = false, float outline = 0.0f);

    // 溜めた文字をまとめて描画する（図形を描き終えてから呼ぶ）
    void FlushText();

    // リソース解放（アプリ終了時）
    void Finalize();
}
