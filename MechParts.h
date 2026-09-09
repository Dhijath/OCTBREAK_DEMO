/*==============================================================================

   機体パーツ・内部パーツ定義 [MechParts.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   アセンブリ画面の「機体」「内部パーツ」タブで選ぶ部品の定義と、選択中の構成。

   ■機体パーツ（見た目が変わる）
     頭・胴体・脚部（スラスター）それぞれ STANDARD（元のモデル）＋新規3種。
     モデルは元の body / Head / Thruster.fbx と同じ大きさ・色づかいで作ってある
     （modelgen4 で生成。正面 -Z・倍率 0.3）。

   ■内部パーツ（見た目は変わらない）
     2スロット。速度・攻撃力・耐久（AP）を強化する。強いものは別の性能が下がる。

   ■取り付け点（オフセットシステム）
     パーツの積み上げはモデルの外形（AABB）ではなく、パーツごとの取り付け点で行う。
       頭の原点   = 胴体原点 + (胴体.mountA - 頭.mountA)
       脚部の原点 = 胴体原点 + (胴体.mountB - 脚部.mountA)
     武器・盾の高さの基準は「胴体の mountB（腰）」、近接の高さ比は mountB〜mountA の間で取る。
     STANDARD の取り付け点は元の AABB と同じ値なので、元の見た目は変わらない。

   ■性能の合成
     各パーツの補正（%）を足し合わせて倍率にする。
       AP     = 8000 × (1 + Σhp)
       速度   = 1 + Σspeed
       攻撃力 = 1 + Σattack

==============================================================================*/
#pragma once

enum FrameSlot { FRAME_HEAD = 0, FRAME_BODY, FRAME_LEGS, FRAME_SLOT_COUNT };

constexpr int FRAME_PART_COUNT    = 4;   // 各部位の候補数（STANDARD＋3種）
constexpr int INTERNAL_PART_COUNT = 7;   // 内部パーツの候補数（NONE を含む）
constexpr int INTERNAL_SLOT_COUNT = 2;

// 取り付け点（モデルの元の単位。ゲームでは MECH_PART_SCALE を掛ける）
struct MechMount { float x, y, z; };
constexpr float MECH_PART_SCALE = 0.3f;

struct MechPartDef
{
    const char*    code;      // 型番（"HD-02" など）
    const wchar_t* name;      // 名前
    const char*    model;     // 機体パーツのモデル（内部パーツは nullptr）
    float          hp;        // AP の補正（0.10 = +10%）
    float          speed;     // 速度の補正
    float          attack;    // 攻撃力の補正
    const wchar_t* desc;      // 説明

    // 取り付け点（機体パーツのみ。モデルの見た目の端ではなく、ここを基準に積み上げる）
    //   胴体 : mountA = 頭を載せる点（コア上面の中央）  mountB = 脚部を付ける点（腰ブロックの底）
    //   頭   : mountA = 首（頭の底の中央。胴体の mountA に重ねる）
    //   脚部 : mountA = 上面の中央（胴体の mountB に重ねる）
    // ヒレや肩の張り出しがあっても頭・脚の位置はずれない。
    MechMount      mountA = { 0.0f, 0.0f, 0.0f };
    MechMount      mountB = { 0.0f, 0.0f, 0.0f };
};

struct MechStats
{
    float hpMul     = 1.0f;
    float speedMul  = 1.0f;
    float attackMul = 1.0f;
};

const MechPartDef& MechParts_GetFrame(int slot, int index);
const MechPartDef& MechParts_GetInternal(int index);

// 選択中の構成（出撃時に player.cpp が読む。SaveData で保存）
int  MechParts_GetFrameSel(int slot);
void MechParts_SetFrameSel(int slot, int index);
int  MechParts_GetInternalSel(int slot);
void MechParts_SetInternalSel(int slot, int index);

// 構成の合計性能（引数を省くと選択中の構成）
MechStats MechParts_CalcStats(const int frame[FRAME_SLOT_COUNT], const int internal[INTERNAL_SLOT_COUNT]);
MechStats MechParts_CalcStats();

// 取り付け点（ゲームの単位。MECH_PART_SCALE を掛けた値）
MechMount MechParts_Mount(int slot, int index, bool mountB = false);

constexpr int PLAYER_BASE_HP = 8000;
