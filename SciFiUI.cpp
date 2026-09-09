/*==============================================================================

   SF調UI部品 [SciFiUI.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   概要・使い方は SciFiUI.h を参照。

==============================================================================*/
#include "SciFiUI.h"
#include "DirectWrite.h"
#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include <map>
#include <memory>
#include <vector>
#include <cmath>

using namespace DirectX;

namespace
{
    int g_WhiteTex = -1;

    int WhiteTex()
    {
        if (g_WhiteTex < 0) g_WhiteTex = Texture_Load(L"resource/Texture/white.png");
        return g_WhiteTex;
    }

    //==========================================================================
    // フォントごとの DirectWrite インスタンス
    //==========================================================================
    struct FontEntry
    {
        std::unique_ptr<FontData> data;     // DirectWrite はこのポインタを保持し続ける
        DirectWrite*              dw = nullptr;
    };
    std::map<int, FontEntry> g_Fonts;

    struct TextItem
    {
        int          key;
        std::wstring text;
        float        cx, cy, halfW;
        D2D1_COLOR_F color;
        float        outline;
    };
    std::vector<TextItem> g_Queue;

    constexpr float TEXT_HALF_W = 1600.0f;   // 文字の描画矩形の半幅（折り返さない程度に広く）

    int MakeKey(SciFiUI::UIFont font, float size, bool bold, SciFiUI::UIAlign align)
    {
        return static_cast<int>(font) * 1000000
             + static_cast<int>(align) * 100000
             + (bold ? 10000 : 0)
             + static_cast<int>(size * 10.0f + 0.5f);
    }

    DirectWrite* GetFont(int key, SciFiUI::UIFont font, float size, bool bold, SciFiUI::UIAlign align)
    {
        auto it = g_Fonts.find(key);
        if (it != g_Fonts.end()) return it->second.dw;

        FontEntry e;
        e.data = std::make_unique<FontData>();
        e.data->font = (font == SciFiUI::UIFont::Display) ? Font::AgencyFB
                     : (font == SciFiUI::UIFont::Mono)    ? Font::Consolas
                                                          : Font::Meiryo;
        e.data->fontSize      = size;
        e.data->fontWeight    = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
        e.data->textAlignment = (align == SciFiUI::UIAlign::Center) ? DWRITE_TEXT_ALIGNMENT_CENTER
                              : (align == SciFiUI::UIAlign::Right)  ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                                                    : DWRITE_TEXT_ALIGNMENT_LEADING;
        e.data->Color = D2D1::ColorF(1, 1, 1, 1);
        e.dw = new DirectWrite(e.data.get());
        e.dw->Init();
        e.dw->SetWordWrapping(false);

        DirectWrite* dw = e.dw;
        g_Fonts.emplace(key, std::move(e));
        return dw;
    }
}

//==============================================================================
// 配色ヘルパー
//==============================================================================
D2D1_COLOR_F SciFiUI::ToD2D(const XMFLOAT4& c, float alphaScale)
{
    return D2D1::ColorF(c.x, c.y, c.z, c.w * alphaScale);
}

XMFLOAT4 SciFiUI::WithAlpha(const XMFLOAT4& c, float a)
{
    return { c.x, c.y, c.z, a };
}

//==============================================================================
// 図形
//==============================================================================
void SciFiUI::BeginSprites()
{
    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();
}

void SciFiUI::Fill(float x, float y, float w, float h, const XMFLOAT4& color)
{
    if (w <= 0.0f || h <= 0.0f) return;
    Sprite_Draw(WhiteTex(), x, y, w, h, color);
}

void SciFiUI::Frame(float x, float y, float w, float h, const XMFLOAT4& color, float thick)
{
    Fill(x,             y,             w,     thick, color);
    Fill(x,             y + h - thick, w,     thick, color);
    Fill(x,             y,             thick, h,     color);
    Fill(x + w - thick, y,             thick, h,     color);
}

void SciFiUI::Brackets(float x, float y, float w, float h, float len, const XMFLOAT4& color, float thick)
{
    // 左上・右上・左下・右下
    Fill(x,               y,               len,   thick, color);
    Fill(x,               y,               thick, len,   color);
    Fill(x + w - len,     y,               len,   thick, color);
    Fill(x + w - thick,   y,               thick, len,   color);
    Fill(x,               y + h - thick,   len,   thick, color);
    Fill(x,               y + h - len,     thick, len,   color);
    Fill(x + w - len,     y + h - thick,   len,   thick, color);
    Fill(x + w - thick,   y + h - len,     thick, len,   color);
}

void SciFiUI::LineAngle(float cx, float cy, float len, float angle, const XMFLOAT4& color, float thick)
{
    const int tex = WhiteTex();
    Sprite_Draw(tex, cx - len * 0.5f, cy - thick * 0.5f, len, thick,
                0, 0, Texture_Width(tex), Texture_Height(tex), angle, color);
}

void SciFiUI::Panel(float x, float y, float w, float h, const XMFLOAT4& fill, const XMFLOAT4& edge, float cut)
{
    // 地：右上の角を cut だけ空けた2枚の矩形で作る
    Fill(x, y + cut, w, h - cut, fill);
    Fill(x, y, w - cut, cut, fill);

    // 縁：上辺（角の手前まで）・右辺（角の下から）・左辺・下辺
    Fill(x,                y,         w - cut, 1.0f, edge);
    Fill(x + w - 1.0f,     y + cut,   1.0f,    h - cut, edge);
    Fill(x,                y,         1.0f,    h,    edge);
    Fill(x,                y + h - 1.0f, w,    1.0f, edge);

    // 角の斜線
    const float diag = cut * 1.41421356f;
    LineAngle(x + w - cut * 0.5f, y + cut * 0.5f, diag, XM_PIDIV4, edge, 1.0f);

    // 上辺のアクセント（左端の太い線）
    Fill(x, y, std::fmin(60.0f, w * 0.3f), 3.0f, WithAlpha(edge, std::fmin(1.0f, edge.w * 1.6f)));
}

void SciFiUI::Scanlines(float x, float y, float w, float h, float spacing, float alpha)
{
    const XMFLOAT4 c = { 0.6f, 0.9f, 1.0f, alpha };
    for (float yy = y; yy < y + h; yy += spacing)
        Fill(x, yy, w, 1.0f, c);
}

void SciFiUI::Grid(float x, float y, float w, float h, float cell, const XMFLOAT4& color)
{
    for (float xx = x; xx <= x + w + 0.5f; xx += cell) Fill(xx, y, 1.0f, h, color);
    for (float yy = y; yy <= y + h + 0.5f; yy += cell) Fill(x, yy, w, 1.0f, color);
}

void SciFiUI::Ticks(float x, float y, float w, int count, int major, const XMFLOAT4& color)
{
    if (count < 2) return;
    const float step = w / static_cast<float>(count - 1);
    for (int i = 0; i < count; ++i)
    {
        const bool isMajor = (major > 0) && (i % major == 0);
        Fill(x + step * i, y, 1.0f, isMajor ? 8.0f : 4.0f, color);
    }
}

void SciFiUI::SegmentBar(float x, float y, float w, float h, int segments, float ratio,
                         const XMFLOAT4& on, const XMFLOAT4& off)
{
    if (segments < 1) return;
    constexpr float GAP = 3.0f;
    const float segW = (w - GAP * (segments - 1)) / static_cast<float>(segments);
    const int   lit  = static_cast<int>(std::round(ratio * segments));
    for (int i = 0; i < segments; ++i)
        Fill(x + i * (segW + GAP), y, segW, h, (i < lit) ? on : off);
}

void SciFiUI::Diamond(float cx, float cy, float r, const XMFLOAT4& color)
{
    const int   tex  = WhiteTex();
    const float side = r * 1.41421356f;
    Sprite_Draw(tex, cx - side * 0.5f, cy - side * 0.5f, side, side,
                0, 0, Texture_Width(tex), Texture_Height(tex), XM_PIDIV4, color);
}

void SciFiUI::HazardTape(float x, float y, float w, float h, float scroll, const XMFLOAT4& color)
{
    // 色付きの区画と暗い区画を交互に並べ、scroll で横に流す
    constexpr float SEG = 28.0f;
    const float offset = std::fmod(scroll, SEG * 2.0f);
    for (float xx = x - SEG * 2.0f + offset; xx < x + w; xx += SEG * 2.0f)
    {
        const float x0 = std::fmax(xx, x);
        const float x1 = std::fmin(xx + SEG, x + w);
        if (x1 > x0) Fill(x0, y, x1 - x0, h, color);
    }
}

//==============================================================================
// 文字
//==============================================================================
void SciFiUI::Text(const std::wstring& text, float x, float y, float size, const D2D1_COLOR_F& color,
                   UIFont font, UIAlign align, bool bold, float outline)
{
    if (text.empty()) return;

    const int key = MakeKey(font, size, bold, align);
    GetFont(key, font, size, bold, align);

    // DrawAt は (cx, cy) を中心に幅 2*halfW・高さ 1.5*size の矩形へ描く。
    // 指定の (x, y) が矩形の左上（揃えに応じて上辺中央・右上）になるよう中心を求める
    const float cx = (align == UIAlign::Left)   ? x + TEXT_HALF_W
                   : (align == UIAlign::Right)  ? x - TEXT_HALF_W
                                                : x;
    const float cy = y + size * 0.75f;

    g_Queue.push_back({ key, text, cx, cy, TEXT_HALF_W, color, outline });
}

void SciFiUI::FlushText()
{
    if (g_Queue.empty()) return;

    const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / static_cast<float>(SPRITE_SCREEN_W);
    const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / static_cast<float>(SPRITE_SCREEN_H);

    // フォントごとにまとめて描く（積んだ順序はフォント内で保たれる）
    for (auto& [key, entry] : g_Fonts)
    {
        bool any = false;
        for (const TextItem& t : g_Queue)
            if (t.key == key) { any = true; break; }
        if (!any) continue;

        entry.dw->SetScale(scaleX, scaleY);
        entry.dw->BeginBatch();
        for (const TextItem& t : g_Queue)
            if (t.key == key)
                entry.dw->DrawAt(t.text, t.cx, t.cy, t.halfW, t.color, t.outline);
        entry.dw->EndBatch();
        entry.dw->SetScale(1.0f, 1.0f);
    }

    g_Queue.clear();
    Sprite_Begin();   // EndBatch でメインRTVが再バインドされた後、スプライト描画を続けられるようにする
}

void SciFiUI::Finalize()
{
    for (auto& [key, entry] : g_Fonts)
    {
        if (entry.dw) { entry.dw->Release(); delete entry.dw; entry.dw = nullptr; }
    }
    g_Fonts.clear();
    g_Queue.clear();
    g_WhiteTex = -1;   // テクスチャは他のモジュールと共有しているので解放しない
}
