/*==============================================================================

   武器定義テーブル [WeaponDef.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
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
    WEAPON_MULTIMISSILE,   // マルチミサイル（ベジェ曲線で拡散する誘導ミサイル）
    WEAPON_TRIPLEGUN,      // トリプルマシンガン（横3連バレル・1発の威力40%減）
    WEAPON_MELEE,          // 近接（白兵）：前方を薙ぎ払い範囲ダメージ・弾なし
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

//------------------------------------------------------------------------------
// 近接（WEAPON_MELEE）の初期(握り)位置：胴体側面の中心を基準に前へ出す。
// ゲーム中(player.cpp)とアセンブリ(AssemblyScreen.cpp)の両方から参照し、位置を一致させる。
//   SIDE  : 胴体側面までの横距離
//   FWD   : 側面中心から前に出す量（7:3の“3”＝控えめ）
//   UP_R  : 高さ比（0=ボディ底, 1=ボディ頭, 0.5=中央）
//------------------------------------------------------------------------------
constexpr float MELEE_REST_SIDE   = 0.30f;
constexpr float MELEE_REST_FWD    = 0.10f;
constexpr float MELEE_REST_UP_R   = 0.50f;
