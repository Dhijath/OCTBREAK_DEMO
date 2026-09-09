/*==============================================================================

   SF調メニュー画面 [SciFiMenu.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   タイトル・中間メニュー・モード選択で共通の「ロゴ＋縦メニュー」画面を描く。
   背景（格子・走査帯・画面四隅の枠・両脇のデータ列）、ロゴ、メニュー項目、
   画面の階層表示（SYS://...）までをまとめて描画する。

   ■使い方
     SciFiMenuItem items[] = { { L"START", L"出撃準備" }, ... };
     SciFiMenu_Draw(L"SYS://TITLE", items, 3, selected, time);

   ※描画は SciFiUI を使う。入力ヒントバー（InputHint_Draw）は呼び出し側で描くこと。

==============================================================================*/
#pragma once

struct SciFiMenuItem
{
    const wchar_t* label;   // 英字の見出し（"START" など）
    const wchar_t* sub;     // 日本語の補足（"出撃準備" など）
};

void SciFiMenu_Initialize();   // 背景テクスチャの読み込み（各画面の Initialize から呼んでよい）

// path     : 左上に出す画面の階層表示（例 L"SYS://TITLE"）
// selected : 選択中の項目
// time     : 画面に入ってからの経過秒（アニメーション用）
void SciFiMenu_Draw(const wchar_t* path, const SciFiMenuItem* items, int count, int selected, float time);
