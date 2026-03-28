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

    //   dmgBar  : ショットガン1射総ダメージ385を最大として正規化
    //   rateBar : マシンガン(0.09s)を最大として正規化 → 1/interval / (1/0.09)
    //   expBar  : ミサイル爆発半径7.5mを最大として正規化

    /* WEAPON_MACHINEGUN  実値: damage=45/shot  interval=0.09s */
    {
        "MACHINEGUN",  45,   0.09f,  0.0f,  50000,
        "resource/Models/Weapon_MachineGun.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.12f,  1.00f,  0.00f,    // dmg=45/385≈0.12  rate=1.00  exp=0.00
        L"連射速度が高い速射型の武器。\n単体の敵に対して高い継続火力を発揮する。"
    },

    /* WEAPON_SHOTGUN  実値: damage=35×11pellet=385/shot  interval=0.70s */
    {
        "SHOTGUN",    385,   0.70f,  0.0f,  80000,
        "resource/Models/Weapon_ShotGun.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        1.00f,  0.13f,  0.00f,    // dmg=385/385=1.00  rate≈0.09/0.70≈0.13  exp=0.00
        L"11発のペレットを扇状に発射する散弾型武器。\n近距離で全弾命中すれば最大火力。"
    },

    /* WEAPON_MISSILE  実値: damage=140/shot  interval=0.85s  explosionR=7.5 */
    {
        "MISSILE",    140,   0.85f,  7.5f, 100000,
        "resource/Models/Weapon_Missile_pod.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.36f,  0.11f,  1.00f,    // dmg=140/385≈0.36  rate≈0.09/0.85≈0.11  exp=1.00
        L"爆発を伴う高火力のミサイル。\n爆発範囲内の複数の敵にダメージを与える。"
    },

    /* WEAPON_SHIELD  攻撃力なし */
    {
        "SHIELD",       0,   0.00f,  0.0f,  30000,
        "resource/Models/Weapon_Round_Shield.fbx",  0.15f,
          0.0f,   0.0f, 0.0f,  0.30f, 0.20f, 0.10f,
        0.00f,  0.00f,  0.00f,
        L"ボタン長押しでガード、被ダメージ50%軽減。\n両腕装備時は両ボタン同時押しでビーム発射。"
    },
};
