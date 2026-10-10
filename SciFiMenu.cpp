/*==============================================================================

   SF調メニュー画面 [SciFiMenu.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   ■レイアウト（1600×900）
     画面四隅      : 大きなブラケット、左上に階層表示、右上に稼働時間と信号強度
     ロゴ          : 中央 y≈200（TextLogo のグラデーション文字＋左右の目盛り線）
     メニュー      : 中央 x=590〜1010、y=420 から 84px 間隔
     両脇          : 流れる16進のデータ列（飾り）

==============================================================================*/
#include "SciFiMenu.h"
#include "SciFiUI.h"
#include "sprite.h"
#include "texture.h"
#include "text_logo.h"
#include "UIInput.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace SciFiUI;
using DirectX::XMFLOAT4;

namespace
{
    int g_BgTex = -1;

    constexpr float SW = 1600.0f;
    constexpr float SH = 900.0f;

    constexpr float MENU_W = 420.0f;
    constexpr float MENU_H = 62.0f;
    constexpr float MENU_X = (SW - MENU_W) * 0.5f;
    constexpr float MENU_Y = 420.0f;
    constexpr float MENU_STEP = 84.0f;

    constexpr float LOGO_Y = 200.0f;

    // 決まった種から作る疑似乱数（飾りのデータ列用）
    unsigned int Hash(unsigned int x)
    {
        x ^= x >> 16; x *= 0x7feb352du;
        x ^= x >> 15; x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    // 両脇に流れる16進のデータ列
    void DataColumn(float x, float time, unsigned int seed, UIAlign align)
    {
        constexpr int   ROWS = 22;
        constexpr float ROW_H = 18.0f;
        const int scroll = static_cast<int>(time * 6.0f);
        for (int r = 0; r < ROWS; ++r)
        {
            const unsigned int h = Hash(seed + static_cast<unsigned int>(r + scroll) * 2654435761u);
            wchar_t buf[24];
            swprintf_s(buf, L"%04X  %04X  %02X", h & 0xffff, (h >> 16) & 0xffff, (h >> 8) & 0xff);
            const float a = 0.10f + 0.18f * static_cast<float>((h >> 3) & 7) / 7.0f;
            Text(buf, x, 150.0f + r * ROW_H, 12.0f, ToD2D(kCyan, a), UIFont::Mono, align);
        }
    }
}

void SciFiMenu_Initialize()
{
    if (g_BgTex < 0) g_BgTex = Texture_Load(L"resource/texture/titleBg.png");
}

bool SciFiMenu_UpdateMouse(int count, int* selected)
{
    // カーソル下の項目（シェブロンまで少し広めに当てる）
    int hit = -1;
    for (int i = 0; i < count; ++i)
    {
        if (UI_IsMouseIn(MENU_X - 20.0f, MENU_Y + i * MENU_STEP, MENU_W + 40.0f, MENU_H))
        {
            hit = i;
            break;
        }
    }
    if (hit < 0) return false;

    // 動いた時だけホバーで選択を移す（止まったカーソルがキー操作を上書きしないため）
    if (UI_IsMouseMoved()) *selected = hit;

    if (UI_IsMouseLeftTrig())
    {
        *selected = hit;
        return true;
    }
    return false;
}

void SciFiMenu_Draw(const wchar_t* path, const SciFiMenuItem* items, int count, int selected, float time)
{
    SciFiMenu_Initialize();
    const float pulse = 0.5f + 0.5f * sinf(time * 4.0f);

    BeginSprites();

    //--------------------------------------------------------------------------
    // 背景：暗い地＋元の背景画像を薄く＋格子＋走査帯
    //--------------------------------------------------------------------------
    Fill(0.0f, 0.0f, SW, SH, { 0.01f, 0.02f, 0.04f, 1.0f });
    if (g_BgTex >= 0) Sprite_Draw(g_BgTex, 0.0f, 0.0f, SW, SH, { 0.55f, 0.8f, 1.0f, 0.22f });
    Grid(0.0f, 0.0f, SW, SH, 40.0f, { 0.3f, 0.7f, 1.0f, 0.035f });
    Scanlines(0.0f, 0.0f, SW, SH, 4.0f, 0.018f);
    {
        const float sweepY = fmodf(time * 70.0f, SH + 80.0f) - 40.0f;
        Fill(0.0f, sweepY - 60.0f, SW, 60.0f, { 0.4f, 0.9f, 1.0f, 0.025f });
        Fill(0.0f, sweepY, SW, 1.0f, { 0.4f, 0.9f, 1.0f, 0.14f });
    }

    // 画面四隅の大きなブラケットと、上下の目盛り
    Brackets(24.0f, 24.0f, SW - 48.0f, SH - 48.0f, 60.0f, WithAlpha(kCyan, 0.55f), 2.0f);
    Ticks(200.0f, 30.0f, SW - 400.0f, 61, 5, WithAlpha(kCyan, 0.25f));
    Fill(24.0f, 70.0f, 260.0f, 1.0f, WithAlpha(kCyan, 0.35f));
    Fill(SW - 284.0f, 70.0f, 260.0f, 1.0f, WithAlpha(kCyan, 0.35f));

    // ロゴの左右に伸びる線
    Fill(140.0f, LOGO_Y + 4.0f, 380.0f, 1.0f, WithAlpha(kCyan, 0.45f));
    Fill(SW - 520.0f, LOGO_Y + 4.0f, 380.0f, 1.0f, WithAlpha(kCyan, 0.45f));
    Ticks(140.0f, LOGO_Y + 6.0f, 380.0f, 20, 5, WithAlpha(kCyan, 0.3f));
    Ticks(SW - 520.0f, LOGO_Y + 6.0f, 380.0f, 20, 5, WithAlpha(kCyan, 0.3f));
    Diamond(130.0f, LOGO_Y + 4.0f, 5.0f, kCyan);
    Diamond(SW - 130.0f, LOGO_Y + 4.0f, 5.0f, kCyan);

    // 信号強度のバー
    for (int i = 0; i < 5; ++i)
    {
        const float h = 6.0f + i * 3.0f;
        const bool  on = (i < 4) || (fmodf(time, 1.0f) < 0.6f);
        Fill(SW - 150.0f + i * 7.0f, 60.0f - h, 4.0f, h, WithAlpha(kGreen, on ? 0.9f : 0.2f));
    }

    //--------------------------------------------------------------------------
    // メニュー項目
    //--------------------------------------------------------------------------
    for (int i = 0; i < count; ++i)
    {
        const float y   = MENU_Y + i * MENU_STEP;
        const bool  sel = (i == selected);
        if (sel)
        {
            Panel(MENU_X, y, MENU_W, MENU_H, kPanelHi, kCyan);
            const float e = 4.0f + pulse * 4.0f;
            Brackets(MENU_X - e, y - e, MENU_W + e * 2.0f, MENU_H + e * 2.0f, 14.0f, kCyan, 2.0f);
            Fill(MENU_X + 1.0f, y + 1.0f, 4.0f, MENU_H - 2.0f, kCyan);

            // 左右のシェブロン（内側へ寄せたり離したり）
            const float off = 18.0f + pulse * 8.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float a = 0.9f - k * 0.28f;
                Diamond(MENU_X - off - k * 14.0f, y + MENU_H * 0.5f, 4.0f, WithAlpha(kCyan, a));
                Diamond(MENU_X + MENU_W + off + k * 14.0f, y + MENU_H * 0.5f, 4.0f, WithAlpha(kCyan, a));
            }
        }
        else
        {
            Panel(MENU_X, y, MENU_W, MENU_H, kPanel, WithAlpha(kCyanDim, 0.45f));
        }
    }

    //--------------------------------------------------------------------------
    // ロゴ（TextLogo：D2D のグラデーション文字。シアン〜白）
    //--------------------------------------------------------------------------
    {
        LogoStyle s;
        s.fontSize     = 150.0f;
        s.fontName     = L"Agency FB";
        s.colorTop     = D2D1::ColorF(0.92f, 0.99f, 1.00f, 1.0f);
        s.colorBottom  = D2D1::ColorF(0.20f, 0.70f, 0.95f, 1.0f);
        s.outlineColor = D2D1::ColorF(0.00f, 0.06f, 0.10f, 1.0f);
        s.outlineWidth = 4.0f;
        TextLogo_Draw(L"OCT BREAK", SW * 0.5f, LOGO_Y, s);
    }

    //--------------------------------------------------------------------------
    // 文字
    //--------------------------------------------------------------------------
    Text(path, 48.0f, 40.0f, 15.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
    Text(L"ARMORED COMBAT SIMULATION", 48.0f, 76.0f, 12.0f, ToD2D(kCyan, 0.55f), UIFont::Mono);
    {
        const int sec = static_cast<int>(time);
        wchar_t buf[32];
        swprintf_s(buf, L"UPTIME %02d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);
        Text(buf, SW - 160.0f, 40.0f, 14.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Right, true);
        Text(L"SIGNAL", SW - 48.0f, 76.0f, 12.0f, ToD2D(kGreen, 0.8f), UIFont::Mono, UIAlign::Right);
    }

    Text(L"// TACTICAL ARMORED FRAME OPERATION SYSTEM //", SW * 0.5f, LOGO_Y + 84.0f, 14.0f,
         ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Center, true);

    DataColumn(48.0f,       time, 17u,  UIAlign::Left);
    DataColumn(SW - 48.0f,  time, 911u, UIAlign::Right);

    for (int i = 0; i < count; ++i)
    {
        const float y   = MENU_Y + i * MENU_STEP;
        const bool  sel = (i == selected);
        wchar_t idx[8];
        swprintf_s(idx, L"%02d", i + 1);
        Text(idx, MENU_X + 22.0f, y + 22.0f, 14.0f, ToD2D(kCyan, sel ? 0.9f : 0.4f), UIFont::Mono, UIAlign::Left, true);
        Text(items[i].label, MENU_X + 66.0f, y + 10.0f, 36.0f,
             sel ? D2D1::ColorF(0.92f, 0.99f, 1.0f, 1.0f) : ToD2D(kCyan, 0.6f),
             UIFont::Display, UIAlign::Left, true);
        if (items[i].sub)
            Text(items[i].sub, MENU_X + MENU_W - 22.0f, y + 24.0f, 14.0f,
                 ToD2D(kCyan, sel ? 0.85f : 0.4f), UIFont::Body, UIAlign::Right);
    }

    FlushText();
}
