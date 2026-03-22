/*==============================================================================

   武器定義テーブル [WeaponDef.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/22

==============================================================================*/
#include "WeaponDef.h"

//------------------------------------------------------------------------------
// 武器パラメータ実体
//   sideOffset: 右腕に取り付ける際の横距離（正値）
//               左腕スロットでは player.cpp 側で符号を反転して使用
//------------------------------------------------------------------------------
const WeaponDef k_WeaponDefs[WEAPON_COUNT] =
{
    // name          dmg  interval  expR  cost
    // modelPath                  scale
    // flip  lean  tilt  side   fwd   height
    // dmgBar rateBar expBar

    /* WEAPON_MACHINEGUN */
    {
        "MACHINEGUN",  45,   0.09f,  0.0f,  50000,
        "resource/Models/Weapon_MachineGun.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.45f,  1.00f,  0.00f,
        L"連射速度が高い速射型の武器。\n単体の敵に対して高い継続火力を発揮する。"
    },

    /* WEAPON_SHOTGUN */
    {
        "SHOTGUN",     45,   0.70f,  0.0f,  80000,
        "resource/Models/Weapon_ShotGun.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.45f,  0.30f,  0.00f,
        L"複数の弾を扇状に発射する散弾型武器。\n近距離での制圧力が高い。"
    },

    /* WEAPON_MISSILE */
    {
        "MISSILE",    100,   1.00f,  7.5f, 100000,
        "resource/Models/Weapon_Missile_pod.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        1.00f,  0.09f,  1.00f,
        L"爆発を伴う高火力のミサイル。\n爆発範囲内の複数の敵にダメージを与える。"
    },

    /* WEAPON_SHIELD */
    {
        "SHIELD",       0,   0.00f,  0.0f,  30000,
        "resource/Models/Weapon_Round_Shield.fbx",  0.15f,
          0.0f,   0.0f, 0.0f,  0.30f, 0.20f, 0.10f,
        0.00f,  0.00f,  0.00f,
        L"構えている間、被ダメージを軽減する防御装備。\nLTボタンでガード姿勢を取る。"
    },
};
