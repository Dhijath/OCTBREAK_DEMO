/*==============================================================================

   武器定義テーブル [WeaponDef.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01

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

    /* WEAPON_MULTIMISSILE  実値: damage=90/発 ×5発  interval=1.20s  explosionR=6.0 */
    {
        "MULTI MISSILE", 90,   1.20f,  3.0f, 150000,
        "resource/Models/Weapon_MultiMissile_pod.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.23f,  0.08f,  0.40f,    // dmg=90/385≈0.23  rate≈0.09/1.20≈0.08  exp=3.0/7.5=0.40
        L"複数のミサイルを一斉に撃ち出す拡散ミサイル。\n弧を描いて標的へ殺到し、広範囲の敵を巻き込む。"
    },

    /* WEAPON_TRIPLEGUN  実値: damage=27/発（=45×0.6）×3列  interval=0.09s */
    {
        "TRIPLE MG",   27,   0.09f,  0.0f,  90000,
        "resource/Models/Weapon_TripleMachineGun.fbx",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.21f,  1.00f,  0.00f,    // dmg=27×3=81/385≈0.21  rate=1.00（MG同等）  exp=0.00
        L"3連バレルから弾幕を叩き込む速射マシンガン。\n1発の威力は控えめだが、圧倒的な手数で押し切る。"
    },

    /* WEAPON_MELEE  実値: damage=920/振り  interval=0.85s  弾なし・前方範囲ヒット（DPS≈1082＝トリプルMGの約1.2倍） */
    {
        "MELEE BLADE", 920,   0.85f,  0.0f,  70000,
        "resource/Models/Blade.fbx",  0.15f,   // 刃モデル（発光の BladeEdge を重ねて描画）
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        1.00f,  0.11f,  0.00f,    // dmg=920（385超のためバー最大1.0）  rate≈0.09/0.85≈0.11  exp=0.00
        L"接近して前方を薙ぎ払う白兵武器。\n射程は短いが近距離で高火力の範囲ダメージを与える。"
    },

    /* WEAPON_RAILGUN  実値: damage=520/発  interval=1.40s  貫通ビーム */
    {
        "RAIL CANNON", 520,   1.40f,  0.0f, 140000,
        "resource/Models/Weapon_Railgun.obj",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        1.00f,  0.06f,  0.00f,    // dmg=520（385超で1.0）  rate=0.09/1.40≈0.06
        L"2本のレールで弾体を加速する高威力砲。\n連射は遅いが、一撃で敵をまとめて撃ち抜く。"
    },

    /* WEAPON_GATLING  実値: damage=24/発  interval=0.05s  拡散±2.5° */
    {
        "GATLING",      24,   0.05f,  0.0f, 110000,
        "resource/Models/Weapon_Gatling.obj",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.06f,  1.00f,  0.00f,    // dmg=24/385≈0.06  rate=1.00（MG以上）
        L"6本の砲身を回して弾をばら撒く。\n1発は軽いが、毎秒20発の弾幕で押し切る。"
    },

    /* WEAPON_GRENADE  実値: damage=260/発  interval=1.00s  explosionR=4.5 */
    {
        "GRENADE",     260,   1.00f,  4.5f, 120000,
        "resource/Models/Weapon_Grenade.obj",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.68f,  0.09f,  0.60f,    // dmg=260/385≈0.68  rate=0.09  exp=4.5/7.5=0.60
        L"放物線を描いて飛ぶ炸裂弾。\n遮蔽物の向こうや群れの中心へ撃ち込める。"
    },

    /* WEAPON_BURSTRIFLE  実値: damage=70×3発=210  interval=0.60s */
    {
        "BURST RIFLE", 210,   0.60f,  0.0f,  95000,
        "resource/Models/Weapon_BurstRifle.obj",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.55f,  0.15f,  0.00f,    // dmg=210/385≈0.55  rate=0.09/0.60=0.15
        L"引き金1回で3発を撃つ精密ライフル。\n弾がまっすぐ飛び、遠くの敵にも当てやすい。"
    },

    /* WEAPON_SPREADLASER  実値: damage=60×5本=300  interval=0.50s */
    {
        "SPREAD LASER", 300,  0.50f,  0.0f, 130000,
        "resource/Models/Weapon_SpreadLaser.obj",  0.15f,
        180.0f, -30.0f, 0.0f,  0.30f, 0.30f, 0.0f,
        0.78f,  0.18f,  0.00f,    // dmg=300/385≈0.78  rate=0.09/0.50=0.18
        L"5つの発振器から扇状にビームを放つ。\n正面の広い範囲を一度に薙ぎ払う。"
    },
};
