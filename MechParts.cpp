/*==============================================================================

   機体パーツ・内部パーツ定義 [MechParts.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03

==============================================================================*/
#include "MechParts.h"

namespace
{
    //--------------------------------------------------------------------------
    // 機体パーツ [部位][候補]
    //--------------------------------------------------------------------------
    const MechPartDef k_Frame[FRAME_SLOT_COUNT][FRAME_PART_COUNT] =
    {
        // code     name           model                                   hp      speed   attack
        //   desc
        //   mountA（頭:首 / 胴体:頭を載せる点 / 脚:上面）  mountB（胴体:脚を付ける点）
        {   // 頭（mountA = 首）
            { "HD-01", L"STANDARD",  "resource/Models/Head.fbx",          0.00f,  0.00f, 0.00f,
              L"標準の頭部。赤い双眼のセンサー。\n癖がなく、どの構成にも合う。",
              { 0.0f, -0.30f, 0.0f } },
            { "HD-02", L"HAWK",      "resource/Models/Head_Hawk.obj",    -0.03f,  0.03f, 0.04f,
              L"前へ反ったトサカと灰色のV字フェイス。\n照準が速く、わずかに攻撃と速度が上がる。",
              { 0.0f, -0.265f, 0.0f } },
            { "HD-03", L"BULWARK",   "resource/Models/Head_Bulwark.obj",  0.08f, -0.02f, 0.00f,
              L"厚い眉庇と頬当てで覆った重頭部。\n一文字のバイザーの奥にセンサーがある。",
              { 0.0f, -0.28f, 0.0f } },
            { "HD-04", L"ORACLE",    "resource/Models/Head_Oracle.obj",  -0.04f,  0.00f, 0.08f,
              L"単眼の大型センサーと側面のポッドを\n持つ索敵型。火器管制で攻撃力が上がる。",
              { 0.0f, -0.28f, 0.0f } },
        },
        {   // 胴体（mountA = 頭を載せる点 / mountB = 脚部を付ける点）
            { "CR-01", L"STANDARD",  "resource/Models/body.fbx",          0.00f,  0.00f, 0.00f,
              L"標準のコア。装甲・機動・積載の\nバランスが取れている。",
              { 0.0f, 0.38f, 0.0f }, { 0.0f, -0.37f, 0.0f } },
            { "CR-02", L"STRIKER",   "resource/Models/Body_Striker.obj", -0.10f,  0.08f, 0.00f,
              L"鋭く尖った細身のコアと後部の2枚ヒレ。\n装甲を削って身軽さを取った高機動型。",
              { 0.0f, 0.30f, 0.0f }, { 0.0f, -0.385f, 0.0f } },
            { "CR-03", L"FORTRESS",  "resource/Models/Body_Fortress.obj", 0.25f, -0.10f, 0.00f,
              L"高い肩ブロックで頭を囲う重コア。\n被弾に強いが重く、当たり判定も大きい。",
              { 0.0f, 0.34f, 0.0f }, { 0.0f, -0.43f, 0.0f } },
            { "CR-04", L"VECTOR",    "resource/Models/Body_Vector.obj",   0.05f, -0.03f, 0.05f,
              L"V字に尖った前面と後退翼のような側板を\n持つ攻撃寄りのコア。冷却に優れ火力が出る。",
              { 0.0f, 0.32f, 0.0f }, { 0.0f, -0.40f, 0.0f } },
        },
        {   // 脚部（スラスター。mountA = 上面）
            { "LG-01", L"STANDARD",  "resource/Models/Thruster.fbx",      0.00f,  0.00f, 0.00f,
              L"標準のホバースラスター。",
              { 0.0f, 0.15f, 0.0f } },
            { "LG-02", L"SPRINT",    "resource/Models/Legs_Sprint.obj",  -0.05f,  0.12f, 0.00f,
              L"左右に長いナセルを抱えた高速型。\n最高速が大きく伸びる。",
              { 0.0f, 0.13f, 0.0f } },
            { "LG-03", L"TREAD",     "resource/Models/Legs_Tread.obj",    0.12f, -0.08f, 0.00f,
              L"幅広で厚いホバープレート。機体を低く\n安定させ、装甲を増やせる。",
              { 0.0f, 0.14f, 0.0f } },
            { "LG-04", L"GLIDE",     "resource/Models/Legs_Glide.obj",    0.00f,  0.06f, 0.03f,
              L"左右に後退翼を張った滑空型スラスター。\n速度と姿勢制御に優れる。",
              { 0.0f, 0.12f, 0.0f } },
        },
    };
    //--------------------------------------------------------------------------
    // 内部パーツ
    //--------------------------------------------------------------------------
    const MechPartDef k_Internal[INTERNAL_PART_COUNT] =
    {
        { "---",    L"NONE",          nullptr,  0.00f,  0.00f, 0.00f, L"内部パーツを積まない。" },
        { "IP-SPD", L"SPEED BOOSTER", nullptr,  0.00f,  0.10f, 0.00f, L"ジェネレーターの出力を推進系へ回す。\n速度 +10%" },
        { "IP-ATK", L"ATTACK AMP",    nullptr,  0.00f,  0.00f, 0.10f, L"火器の出力を増幅する。\n攻撃力 +10%" },
        { "IP-ARM", L"ARMOR PLATE",   nullptr,  0.15f,  0.00f, 0.00f, L"内側に追加の装甲板を張る。\nAP +15%" },
        { "IP-OVD", L"OVERDRIVE",     nullptr, -0.10f,  0.20f, 0.00f, L"リミッターを外した推進系。\n速度 +20% / AP -10%" },
        { "IP-BSK", L"BERSERKER",     nullptr, -0.10f,  0.00f, 0.20f, L"冷却を捨てて火力を引き出す。\n攻撃力 +20% / AP -10%" },
        { "IP-HVY", L"HEAVY ARMOR",   nullptr,  0.30f, -0.10f, 0.00f, L"重い複合装甲。\nAP +30% / 速度 -10%" },
    };

    int g_FrameSel[FRAME_SLOT_COUNT]       = { 0, 0, 0 };
    int g_InternalSel[INTERNAL_SLOT_COUNT] = { 0, 0 };

    int Clamp(int v, int count) { return (v < 0 || v >= count) ? 0 : v; }
}

const MechPartDef& MechParts_GetFrame(int slot, int index)
{
    return k_Frame[Clamp(slot, FRAME_SLOT_COUNT)][Clamp(index, FRAME_PART_COUNT)];
}

const MechPartDef& MechParts_GetInternal(int index)
{
    return k_Internal[Clamp(index, INTERNAL_PART_COUNT)];
}

int  MechParts_GetFrameSel(int slot)               { return g_FrameSel[Clamp(slot, FRAME_SLOT_COUNT)]; }
void MechParts_SetFrameSel(int slot, int index)    { g_FrameSel[Clamp(slot, FRAME_SLOT_COUNT)] = Clamp(index, FRAME_PART_COUNT); }
int  MechParts_GetInternalSel(int slot)            { return g_InternalSel[Clamp(slot, INTERNAL_SLOT_COUNT)]; }
void MechParts_SetInternalSel(int slot, int index) { g_InternalSel[Clamp(slot, INTERNAL_SLOT_COUNT)] = Clamp(index, INTERNAL_PART_COUNT); }

MechStats MechParts_CalcStats(const int frame[FRAME_SLOT_COUNT], const int internal[INTERNAL_SLOT_COUNT])
{
    float hp = 0.0f, speed = 0.0f, attack = 0.0f;
    for (int s = 0; s < FRAME_SLOT_COUNT; ++s)
    {
        const MechPartDef& p = MechParts_GetFrame(s, frame[s]);
        hp += p.hp; speed += p.speed; attack += p.attack;
    }
    for (int s = 0; s < INTERNAL_SLOT_COUNT; ++s)
    {
        const MechPartDef& p = MechParts_GetInternal(internal[s]);
        hp += p.hp; speed += p.speed; attack += p.attack;
    }
    MechStats st;
    st.hpMul     = (1.0f + hp     < 0.3f) ? 0.3f : 1.0f + hp;
    st.speedMul  = (1.0f + speed  < 0.3f) ? 0.3f : 1.0f + speed;
    st.attackMul = (1.0f + attack < 0.3f) ? 0.3f : 1.0f + attack;
    return st;
}

MechStats MechParts_CalcStats()
{
    return MechParts_CalcStats(g_FrameSel, g_InternalSel);
}

MechMount MechParts_Mount(int slot, int index, bool mountB)
{
    const MechPartDef& p = MechParts_GetFrame(slot, index);
    const MechMount& m = mountB ? p.mountB : p.mountA;
    return { m.x * MECH_PART_SCALE, m.y * MECH_PART_SCALE, m.z * MECH_PART_SCALE };
}
