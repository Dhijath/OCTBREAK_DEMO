/*==============================================================================
   チュートリアル（画像スライドショー）[Tutorial.cpp]
   Author : 51106
   Date   : 2026/04/01
--------------------------------------------------------------------------------
   resource/texture/Tutorial/ 内の 16:9 画像（png / jpg）を名前順に読み込み、
   左右キー / スティックで送る。ENTER / A で次へ、ESC / B で戻る。
   仮想解像度 1600×900 は 16:9 なので、画像は画面全体に等倍で表示される。

   ※画像は共有の Texture_Load を使わず、DirectXTex で R8G8B8A8_UNORM に変換し
     アルファを不透明化してから自前で SRV を作る。
     （24bit(アルファ無し)PNG が Texture_Load 経由だと表示できなかったため）
==============================================================================*/
#include "Tutorial.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
#include "DirectWrite.h"
#include "UIInput.h"
#include "input_hint.h"
#include "audio.h"
#include "DirectXTex.h"
#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <d2d1helper.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <cstdint>

using namespace DirectX;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while(0)
#endif

namespace
{
    std::vector<ID3D11ShaderResourceView*> g_srvs;  // 読み込んだチュートリアル画像（自前SRV）
    int    g_index = 0;             // 現在表示中のページ
    bool   g_end   = false;
    float  g_Time  = 0.0f;

    int g_WhiteTex = -1;            // 帯・余白用
    int g_BgTex    = -1;            // 背景（タイトルと同じ画像）

    int g_SeCursorMove = -1;
    int g_SeCancel     = -1;

    DirectWrite* g_pDW = nullptr;   // ページ番号 / メッセージ用

    // 拡張子が画像かどうか（.png / .jpg / .jpeg 大文字小文字問わず）
    static bool IsImageFile(const std::wstring& name)
    {
        std::wstring lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](wchar_t c) { return (wchar_t)towlower(c); });
        auto endsWith = [&](const wchar_t* ext) {
            const size_t n = wcslen(ext);
            return lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0;
        };
        return endsWith(L".png") || endsWith(L".jpg") || endsWith(L".jpeg");
    }

    // 1枚を読み込んで SRV を返す（失敗時 nullptr）
    static ID3D11ShaderResourceView* LoadImageSRV(const wchar_t* path)
    {
        TexMetadata  md{};
        ScratchImage src{};
        // SRGB として解釈させず素直に読む
        if (FAILED(LoadFromWICFile(path, WIC_FLAGS_IGNORE_SRGB, &md, src)))
            return nullptr;

        // R8G8B8A8_UNORM に統一（24bit や BGRA / SRGB の差異を吸収）
        ScratchImage conv{};
        const ScratchImage* use = &src;
        if (md.format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            if (SUCCEEDED(Convert(src.GetImages(), src.GetImageCount(), md,
                                  DXGI_FORMAT_R8G8B8A8_UNORM, TEX_FILTER_DEFAULT,
                                  TEX_THRESHOLD_DEFAULT, conv)))
                use = &conv;
        }

        // アルファを不透明(255)に強制（アルファ無しPNGが透明で消えるのを防ぐ）
        if (use->GetMetadata().format == DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            const Image* im = use->GetImage(0, 0, 0);
            if (im && im->pixels)
            {
                for (size_t y = 0; y < im->height; ++y)
                {
                    uint8_t* row = im->pixels + y * im->rowPitch;
                    for (size_t x = 0; x < im->width; ++x)
                        row[x * 4 + 3] = 0xFF;
                }
            }
        }

        ID3D11ShaderResourceView* srv = nullptr;
        if (FAILED(CreateShaderResourceView(Direct3D_GetDevice(),
                    use->GetImages(), use->GetImageCount(), use->GetMetadata(), &srv)))
            return nullptr;

        return srv;
    }
}

void Tutorial_Initialize()
{
    g_index = 0;
    g_end   = false;
    g_Time  = 0.0f;

    g_WhiteTex = Texture_Load(L"resource/texture/white.png");
    g_BgTex    = Texture_Load(L"resource/texture/titleBg.png");   // タイトルと同じ背景

    if (g_SeCursorMove < 0) g_SeCursorMove = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeCancel     < 0) g_SeCancel     = LoadAudio("resource/Sound/ui_cancel.wav");

    if (!g_pDW)
    {
        static FontData fd;
        fd.font          = Font::Arial;
        fd.fontWeight    = DWRITE_FONT_WEIGHT_BOLD;
        fd.fontStyle     = DWRITE_FONT_STYLE_NORMAL;
        fd.fontStretch   = DWRITE_FONT_STRETCH_NORMAL;
        fd.fontSize      = 24.0f;
        fd.localeName    = L"ja-jp";
        fd.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        fd.Color         = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        g_pDW = new DirectWrite(&fd);
        g_pDW->Init();
    }

    //--------------------------------------------------------------------------
    // resource/texture/Tutorial/ 内の画像を名前順に読み込む
    //--------------------------------------------------------------------------
    g_srvs.clear();
    {
        std::vector<std::wstring> files;
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(L"resource\\texture\\Tutorial\\*.*", &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const std::wstring name = fd.cFileName;
                if (IsImageFile(name))
                    files.push_back(L"resource\\texture\\Tutorial\\" + name);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }

        std::sort(files.begin(), files.end());   // ファイル名順（例: 1, 2, ...）

        for (const auto& f : files)
        {
            ID3D11ShaderResourceView* srv = LoadImageSRV(f.c_str());
            if (srv) g_srvs.push_back(srv);
        }
    }
}

void Tutorial_Finalize()
{
    for (auto& srv : g_srvs) SAFE_RELEASE(srv);
    g_srvs.clear();

    if (g_pDW) { g_pDW->Release(); delete g_pDW; g_pDW = nullptr; }

    UnloadAudio(g_SeCursorMove); g_SeCursorMove = -1;
    UnloadAudio(g_SeCancel);     g_SeCancel     = -1;
}

void Tutorial_Update(double elapsed_time)
{
    g_Time += static_cast<float>(elapsed_time);

    const int count = static_cast<int>(g_srvs.size());

    if (count > 0)
    {
        if (UI_IsMoveLeft())
        {
            g_index = (g_index - 1 + count) % count;
            PlayAudio(g_SeCursorMove, false);
        }
        if (UI_IsMoveRight() || UI_IsConfirm())
        {
            g_index = (g_index + 1) % count;
            PlayAudio(g_SeCursorMove, false);
        }
    }

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_end = true;
    }
}

void Tutorial_Draw()
{
    Direct3D_SetDepthEnable(false);
    Direct3D_SetBlendState(true);
    Sprite_Begin();

    const float SW = static_cast<float>(SPRITE_SCREEN_W);   // 1600
    const float SH = static_cast<float>(SPRITE_SCREEN_H);   // 900（16:9）

    // 背景（タイトルと同じ画像）
    if (g_BgTex >= 0)
        Sprite_Draw(g_BgTex, 0.0f, 0.0f, SW, SH, XMFLOAT4(1, 1, 1, 1));

    const int count = static_cast<int>(g_srvs.size());

    // 現在ページの画像を 1280×720 で中央に表示（自前SRVを直接描画）
    if (count > 0 && g_index >= 0 && g_index < count && g_srvs[g_index])
    {
        constexpr float IMG_W = 1280.0f;
        constexpr float IMG_H = 720.0f;
        const float ix = (SW - IMG_W) * 0.5f;   // 160
        const float iy = (SH - IMG_H) * 0.5f;   // 90
        Sprite_DrawSRV(g_srvs[g_index], ix, iy, IMG_W, IMG_H, XMFLOAT4(1, 1, 1, 1));
    }

    // ── テキスト（ページ番号 or 画像なしメッセージ）──
    Direct3D_BindMainRenderTarget();
    if (g_pDW)
    {
        const float scaleX = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
        const float scaleY = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;

        g_pDW->SetScale(scaleX, scaleY);
        g_pDW->BeginBatch();
        if (count > 0)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d / %d", g_index + 1, count);
            g_pDW->DrawAt(buf, 800.0f, 852.0f, 100.0f,
                          D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f), 2.0f);
        }
        else
        {
            g_pDW->DrawAt(std::wstring(L"チュートリアル画像がありません（resource/texture/Tutorial/）"),
                          800.0f, 450.0f, 700.0f,
                          D2D1::ColorF(0.8f, 0.85f, 1.0f, 1.0f), 2.0f);
        }
        g_pDW->EndBatch();
        g_pDW->SetScale(1.0f, 1.0f);
    }

    // フッター（操作ヒント）
    InputHint_Draw(
        "{K_A}{K_D} / {LEFT}{RIGHT} Page    {ENTER} Next    {ESC} Back",
        "{DPAD_LR} Page    {A} Next    {B} Back");
}

bool Tutorial_IsEnd()
{
    if (g_end) { g_end = false; return true; }
    return false;
}
