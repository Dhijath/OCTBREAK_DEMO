/*==============================================================================

   武器定義テーブル [WeaponDef.h]
                                                         Author : 51106
                                                         Date   : 2026/03/22
--------------------------------------------------------------------------------

   全武器の静的パラメータを一元管理する。
   WeaponDef.cpp に実体 k_WeaponDefs[] を定義。

==============================================================================*/
#pragma once

enum WeaponID
{
    WEAPON_MACHINEGUN = 0,
    WEAPON_SHOTGUN,
    WEAPON_MISSILE,
    WEAPON_SHIELD,
    WEAPON_COUNT
};

struct WeaponDef
{
    const char*  name;          // 表示名（ASCII）
    int          damage;        // 基礎ダメージ
    float        fireInterval;  // 発射間隔（秒）
    float        explosionR;    // 爆発半径（m）。爆発なし=0
    int          cost;          // クレジットコスト

    const char*  modelPath;     // ModelLoad に渡すパス
    float        scale;         // モデルスケール

    float        flipDeg;       // Z軸回転（上下反転）
    float        leanDeg;       // Z軸回転（傾き）
    float        tiltDeg;       // X軸回転（仰角）

    float        sideOffset;    // 横オフセット（右腕=+, 左腕は符号を反転して使う）
    float        forwardOffset; // 前方オフセット
    float        heightOffset;  // 高さオフセット

    // アセンブル画面のステータスバー（0.0 〜 1.0）
    float        dmgBar;
    float        rateBar;
    float        expBar;

    const wchar_t* description;   // アセンブル画面の説明文
};

extern const WeaponDef k_WeaponDefs[WEAPON_COUNT];
