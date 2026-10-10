/*==============================================================================

   エネミー図鑑 [EnemyDex.cpp]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   ■レイアウト（1600×900）
     一覧       : x=40〜470,   y=104〜816  全23種（分類ごとに色分け。行の高さは項目数から決める）
     プレビュー : x=500〜1080, y=104〜560  3Dモデルが回転する（パーツも動く）
     性能       : x=1100〜1560,y=104〜560  分類・脅威度・耐久・速度・撃破数
     解説       : x=500〜1560, y=580〜816  攻撃と解説

   ■3Dプレビュー
     AssemblyScreen と同じく、深度をクリアしてサブビューポートに描く。
     モデルの大きさは本体モデルのAABBから自動で枠に収める（パーツのはみ出し分は
     項目ごとの倍率で補う）。
==============================================================================*/
#include "EnemyDex.h"
#include "EnemyManager.h"
#include "EnemyBall.h"
#include "EnemyBossEx.h"
#include "EnemyParts.h"
#include "SciFiUI.h"
#include "UIInput.h"
#include "audio.h"
#include "direct3d.h"
#include "model.h"
#include "ModelToon.h"
#include "shader3d.h"
#include "ShaderToon.h"
#include "light.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DirectX;
using namespace SciFiUI;

//==============================================================================
// 図鑑データ
//==============================================================================
namespace
{
    enum class DexClass { Standard, Special, Ball, Boss };

    struct DexEntry
    {
        EnemyType      type;
        const wchar_t* name;
        const wchar_t* nameJp;
        DexClass       cls;
        int            threat;      // 脅威度 1〜5
        int            hp;
        float          speed;       // 追跡速度（m/s）
        const char*    model;       // 本体モデル（枠に収める基準）
        float          modelScale;
        bool           fbx;         // ユーザー作成のFBX（正面が -Z なので 180°回す）
        float          frame;       // 枠の倍率（パーツが本体からはみ出す分）
        float          offsetY;     // 注視点の上下の補正（m）
        const wchar_t* attack;
        const wchar_t* text;
    };

    const DexEntry k_Entries[] =
    {
        { EnemyType::Normal,  L"DRONE",    L"基本型",   DexClass::Standard, 1, 300, 3.2f,
          "resource/Models/enemy.fbx", 0.25f, true, 1.25f, 0.0f,
          L"体当たり（溜めてから短く突進）",
          L"最も数の多い球体型の無人機。巡回中に見つけた相手へ\n"
          L"まっすぐ寄ってきて、一瞬溜めてから体当たりする。\n"
          L"単体は弱いが、仲間に警報を回して群れで来る。" },
        { EnemyType::Tank,    L"TANK",     L"重装型",   DexClass::Standard, 3, 1600, 1.0f,
          "resource/Models/enemy_tank.fbx", 0.625f, true, 1.7f, 0.15f,
          L"体当たり / 正面の盾（耐久 300）",
          L"大型の球体に盾と砲身を備えた重装機。動きは遅いが、\n"
          L"正面の盾が弾を受け止める。盾を壊すか、横や背後へ\n"
          L"回り込んで本体を撃て。" },
        { EnemyType::Speed,   L"SPEED",    L"高機動型", DexClass::Standard, 2, 120, 8.0f,
          "resource/Models/enemy_speed.fbx", 0.175f, true, 1.05f, 0.0f,
          L"高速の体当たり",
          L"翼とブースターで高速移動する小型機。耐久は低いが、\n"
          L"足を止めるとすぐに距離を詰められる。\n"
          L"近づかれる前に連射で落とせ。" },
        { EnemyType::Sniper,  L"SNIPER",   L"狙撃型",   DexClass::Standard, 3, 150, 1.2f,
          "resource/Models/enemy_sniper.fbx", 0.2125f, true, 1.25f, 0.0f,
          L"遠距離からの狙撃（405）",
          L"灰色の球体に赤い眼の狙撃機。遠くから正確に撃ってくる。\n"
          L"遮蔽物の陰で弾をやり過ごし、撃ち終わりを狙って\n"
          L"一気に近づくのが有効。" },
        { EnemyType::Boss,    L"BEHEMOTH", L"大型兵器", DexClass::Boss, 5, 80000, 4.5f,
          "resource/Models/enemy_boss.fbx", 1.3f, true, 1.9f, 0.15f,
          L"4門の連射 / 扇状の散弾 / 突進（2360）",
          L"デフォルト作戦の最深部に待つ大型兵器。前面の4門から\n"
          L"順番に撃ち、時々突進してくる。体力が半分を切ると\n"
          L"激昂して攻撃の間隔が短くなる。" },

        { EnemyType::Bomber,    L"BOMBER",    L"自爆型", DexClass::Special, 3, 90, 6.5f,
          "resource/Models/enemy_bomber.obj", 1.0f, false, 1.15f, 0.0f,
          L"接近して自爆（半径3m・1300）",
          L"トゲの付いた機雷型。高速で寄ってきて、点滅したあと\n"
          L"爆発する。点滅を見たらすぐ離れること。\n"
          L"撃ち落とせば爆発は起きない。" },
        { EnemyType::Gunner,    L"GUNNER",    L"突撃型", DexClass::Special, 3, 260, 3.5f,
          "resource/Models/enemy_gunner.obj", 1.0f, false, 1.9f, -0.18f,
          L"予測射撃の3連射（150×3）",
          L"両腕に銃を持つ二脚機。中距離を保ちながら横へ回り込み、\n"
          L"移動先を狙って3連射してくる。\n"
          L"止まらずに動き続ければ当たりにくい。" },
        { EnemyType::Artillery, L"ARTILLERY", L"砲撃型", DexClass::Special, 4, 420, 1.2f,
          "resource/Models/enemy_artillery.obj", 1.0f, false, 1.1f, 0.0f,
          L"溜めてから重砲弾3発（480×3）",
          L"長砲身の装軌機。遠くで止まり、光って溜めてから\n"
          L"重い砲弾を扇状に撃つ。光り始めたら射線から外れろ。\n"
          L"近づくと後退する。" },
        { EnemyType::Turret,    L"TURRET",    L"固定砲台", DexClass::Special, 4, 700, 0.0f,
          "resource/Models/enemy_turret.obj", 1.0f, false, 1.1f, 0.0f,
          L"全方位弾12発（220）/ 2連の狙い撃ち（260）",
          L"動かない砲台。一定間隔で全方位に弾を撒き、\n"
          L"その合間に狙い撃ってくる。全方位弾は毎回少しずれるので\n"
          L"隙間を見て抜けること。" },
        { EnemyType::Phantom,   L"PHANTOM",   L"幻影型", DexClass::Special, 4, 200, 4.5f,
          "resource/Models/enemy_phantom.obj", 1.0f, false, 1.15f, 0.0f,
          L"瞬間移動からの斬撃（650）",
          L"刃を持つ浮遊機。見つけた相手のすぐ近くへ瞬間移動し、\n"
          L"光って溜めてから斬りかかる。現れた瞬間に横へ跳べば\n"
          L"かわせる。" },

        { EnemyType::Wing,    L"WING",    L"翼型",     DexClass::Ball, 3, 180, 5.0f,
          "resource/Models/enemy_wing_body.obj", 1.0f, false, 2.7f, 0.0f,
          L"旋回しながらの射撃（180）/ 急降下突撃（500）",
          L"高機動型の流れをくむ白い球体。赤い翼を羽ばたかせて\n"
          L"周りを旋回し、翼をたたんだら急降下の合図。\n"
          L"突撃の直後は動きが止まる。" },
        { EnemyType::Gatling, L"GATLING", L"回転砲型", DexClass::Ball, 4, 520, 2.0f,
          "resource/Models/enemy_gatling_body.obj", 1.0f, false, 1.5f, 0.0f,
          L"砲身を回して加速 → 18連射（70×18）",
          L"狙撃型と同じ灰色の球体に回転砲を付けた機体。\n"
          L"砲身が回り始めたら連射の前触れ。撃っている間は\n"
          L"向きを変えるのが遅いので、横へ回れば避けられる。" },
        { EnemyType::Orbiter, L"ORBITER", L"子機型",   DexClass::Ball, 3, 380, 3.0f,
          "resource/Models/enemy_orbiter_body.obj", 1.0f, false, 2.0f, 0.0f,
          L"子機からの交互射撃（160）/ 3機一斉射（140×9）",
          L"基本型の青い球体が3つの子機を従えたもの。子機は周りを\n"
          L"回りながら順番に撃つ。子機の回転が速くなったら\n"
          L"一斉射が来る。" },
        { EnemyType::Walker,  L"WALKER",  L"脚型",     DexClass::Ball, 4, 600, 3.2f,
          "resource/Models/enemy_walker_body.obj", 1.0f, false, 1.9f, -0.16f,
          L"跳躍からの踏みつけ（700）/ 着地の衝撃波（180×10）",
          L"4本脚で歩く赤い球体。近づくとかがんで光り、\n"
          L"跳び上がって踏みつけてくる。着地と同時に衝撃波が\n"
          L"全方位へ広がるので、跳んだら距離を取れ。" },
        { EnemyType::Halo,    L"HALO",    L"環型",     DexClass::Ball, 4, 450, 2.4f,
          "resource/Models/enemy_halo_body.obj", 1.0f, false, 1.95f, 0.0f,
          L"リングの高速回転 → 螺旋弾幕（150）",
          L"リングをまとった黄色い球体。普段はリングがゆっくり回り、\n"
          L"高速回転を始めたら螺旋状の弾幕を撒く。\n"
          L"弾の渦は回転方向へ流れるので、逆へ回り込め。" },

        { EnemyType::BossArgus,   L"ARGUS",      L"浮遊する眼", DexClass::Boss, 5, 60000, 0.0f,
          "resource/Models/boss_argus.obj", 1.0f, false, 1.35f, 0.0f,
          L"全方位弾 / 扇状弾 / 突撃型の召喚",
          L"二重のリングをまとった巨大な眼。全方位に弾を撒きながら\n"
          L"扇状の狙い撃ちを重ね、突撃型を呼び寄せる。\n"
          L"召喚された護衛から先に片付けろ。" },
        { EnemyType::BossGoliath, L"GOLIATH",    L"重装歩行機", DexClass::Boss, 5, 90000, 0.0f,
          "resource/Models/boss_goliath.obj", 1.0f, false, 1.25f, -0.45f,
          L"衝撃波 / 肩の重砲 / 突進",
          L"肩に2門の重砲を載せた四脚の歩行機。地面を踏み鳴らして\n"
          L"衝撃波を放ち、重い砲弾と突進で追い詰めてくる。\n"
          L"突進は壁に当たると止まる。" },
        { EnemyType::BossOmega,   L"OMEGA CORE", L"動力炉",     DexClass::Boss, 5, 80000, 0.0f,
          "resource/Models/boss_omega.obj", 1.0f, false, 1.6f, 0.9f,
          L"螺旋弾幕 / 連射 / 固定砲台の召喚",
          L"台座に据えられた動力炉。動かない代わりに螺旋の弾幕と\n"
          L"連射を絶え間なく撃ち、固定砲台を呼び出す。\n"
          L"弾幕の切れ目に火力を集中させろ。" },
        { EnemyType::BossHydra,   L"HYDRA",      L"三つ首",     DexClass::Boss, 5, 70000, 0.0f,
          "resource/Models/boss_hydra.obj", 1.0f, false, 1.5f, 0.6f,
          L"三つ首の一斉射 / 機雷 / 自爆型の召喚",
          L"砲を持つ首が3本ある六角形の大型兵器。三方向へ扇状に\n"
          L"一斉射し、機雷をばら撒き、自爆型を送り込んでくる。\n"
          L"首の正面に立ち続けないこと。" },
        { EnemyType::BossSpectre, L"SPECTRE",    L"刃の騎士",   DexClass::Boss, 5, 55000, 0.0f,
          "resource/Models/boss_spectre.obj", 1.0f, false, 1.15f, 0.0f,
          L"瞬間移動 / 斬撃 / 幻影型の召喚",
          L"両腕に刃を持つ細身の騎士。瞬間移動で間合いを詰めて\n"
          L"斬りかかり、幻影型を呼ぶ。耐久は大型兵器の中で\n"
          L"最も低いが、最も速い。" },
        { EnemyType::BossBastion, L"BASTION",    L"要塞機",     DexClass::Boss, 5, 100000, 0.0f,
          "resource/Models/boss_bastion.obj", 1.0f, false, 1.5f, 0.0f,
          L"交差する掃射 / 十字砲火 / ガトリング型の投下と衝撃波",
          L"両腕にガトリングを備え、3枚の盾を周回させる要塞機。\n"
          L"砲身の回転が上がったら掃射の合図。左右の弾の帯が\n"
          L"交差する瞬間を、横へ抜けてかわせ。" },
        { EnemyType::BossNest,    L"NEST",       L"母艦機",     DexClass::Boss, 5, 85000, 0.0f,
          "resource/Models/boss_nest.obj", 1.0f, false, 2.2f, 0.0f,
          L"翼型の射出 / 予告付きの絨毯爆撃 / 子機からの斉射",
          L"高く浮かんで距離を取る母艦。翼型の無人機を次々と放ち、\n"
          L"火花で示した地点を爆撃する。火花が見えたら\n"
          L"その場を離れろ。周回する子機も弾を撃ってくる。" },
        { EnemyType::BossEclipse, L"ECLIPSE",    L"日蝕",       DexClass::Boss, 5, 120000, 0.0f,
          "resource/Models/boss_eclipse.obj", 1.0f, false, 2.0f, 0.0f,
          L"二重螺旋 / 引き寄せてからの炸裂 / 衛星からの連射",
          L"3本のリングと4つの衛星を従える最終兵器。光りながら\n"
          L"機体を引き寄せ、全方位へ炸裂する。引力は走って\n"
          L"振り切れる。激昂すると瞬間移動で位置を変える。" },
    };
    constexpr int ENTRY_COUNT = static_cast<int>(sizeof(k_Entries) / sizeof(k_Entries[0]));

    // 撃破数（EnemyType の値で引く）
    constexpr int TYPE_COUNT = static_cast<int>(EnemyType::BossEclipse) + 1;
    int g_Kills[TYPE_COUNT] = {};

    // 画面の状態
    int   g_Selected   = 0;
    float g_Time       = 0.0f;
    float g_ViewYaw    = 0.0f;   // プレビューの回転（左右で手動回転）
    float g_SelectTime = 0.0f;   // 選択してからの時間（切り替えの演出）
    bool  g_IsEnd      = false;
    bool  g_Dragging   = false;  // プレビューをマウスでドラッグ中
    int   g_SeCursor   = -1;
    int   g_SeCancel   = -1;

    // レイアウト
    constexpr float LIST_X = 40.0f,   LIST_Y = 104.0f, LIST_W = 430.0f, LIST_H = 712.0f;
    constexpr float ROW_Y0 = LIST_Y + 40.0f;
    constexpr float ROW_H  = std::min(33.0f, (LIST_H - 48.0f) / ENTRY_COUNT);   // 項目数に合わせて詰める
    constexpr float PV_X   = 500.0f,  PV_Y   = 104.0f, PV_W   = 580.0f, PV_H   = 456.0f;
    constexpr float DATA_X = 1100.0f, DATA_Y = 104.0f, DATA_W = 460.0f, DATA_H = 456.0f;
    constexpr float TXT_X  = 500.0f,  TXT_Y  = 580.0f, TXT_W  = 1060.0f, TXT_H = 236.0f;

    XMFLOAT4 ClassColor(DexClass c)
    {
        switch (c)
        {
        case DexClass::Special: return kAmber;
        case DexClass::Ball:    return kGreen;
        case DexClass::Boss:    return kRed;
        default:                return kCyan;
        }
    }

    const wchar_t* ClassLabel(DexClass c)
    {
        switch (c)
        {
        case DexClass::Special: return L"SPECIAL  特殊機";
        case DexClass::Ball:    return L"BALL UNIT  球体機";
        case DexClass::Boss:    return L"HEAVY WEAPON  大型兵器";
        default:                return L"STANDARD  標準機";
        }
    }

    std::wstring Grouped(int value)
    {
        std::wstring d = std::to_wstring(value), out;
        for (size_t i = 0; i < d.size(); ++i)
        {
            if (i > 0 && (d.size() - i) % 3 == 0) out += L',';
            out += d[i];
        }
        return out;
    }

    //==========================================================================
    // 3Dモデル（本体＋パーツ）。world はボディを置く行列（正面 +Z）
    //==========================================================================
    void DrawEntryModel(const DexEntry& e, const XMMATRIX& world, float t)
    {
        const XMMATRIX flip = XMMatrixRotationY(XM_PI);   // ユーザーFBXは正面が -Z
        MODEL* body = EnemyParts_Get(e.model, e.modelScale);

        switch (e.type)
        {
        case EnemyType::Tank:
        {
            // 本体・盾・砲身（ゲーム中と同じ揺れ）
            ModelDrawToon(body, flip * world);
            const float sway = sinf(t * 1.6f) * XMConvertToRadians(9.0f);
            const float bob  = sinf(t * 2.3f) * 0.035f;
            ModelDrawToon(EnemyParts_Get("resource/Models/Shield.fbx", 0.625f),
                          XMMatrixRotationY(XM_PI + sway) * XMMatrixTranslation(0.0f, bob, 0.7f) * world);
            ModelDraw(EnemyParts_Get("resource/Models/Barrel.fbx", 0.25f),
                      XMMatrixRotationX(XMConvertToRadians(sinf(t * 0.9f) * 8.0f)) * flip *
                      XMMatrixTranslation(0.0f, 0.79f, -0.1f) * world);
            break;
        }
        case EnemyType::Boss:
        {
            // 本体＋前面 2×2 の砲身（交互に反動で下がる）
            const float bob = 0.07f + sinf(t * 1.8f) * 0.07f;
            const XMMATRIX w = XMMatrixTranslation(0.0f, bob, 0.0f) * world;
            ModelDrawToon(body, flip * w);
            MODEL* barrel = EnemyParts_Get("resource/Models/Barrel.fbx", 0.65f);
            const float sx[4] = { 1.35f, -1.35f, 1.35f, -1.35f };   // 本体の顔が隠れないよう左右へ寄せる
            const float sy[4] = { 0.95f, 0.95f, -0.25f, -0.25f };
            for (int i = 0; i < 4; ++i)
            {
                const float phase = fmodf(t * 1.6f + i * 0.25f, 1.0f);
                const float recoil = (phase < 0.15f) ? (0.15f - phase) * 2.0f : 0.0f;
                ModelDrawToon(barrel, flip * XMMatrixTranslation(sx[i], sy[i], 0.35f - recoil) * w);
            }
            break;
        }
        //------------------------------------------------------------------
        // 本体と動くパーツに分かれた機体（ゲーム中と同じ部品・アイドル中の動き）
        //------------------------------------------------------------------
        case EnemyType::Bomber:
            ModelDraw(body, world);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_bomber_spikes.obj"), XMMatrixRotationY(t * 2.0f) * XMMatrixTranslation(0, 0.32f, 0) * world);
            break;
        case EnemyType::Gunner:
            ModelDraw(body, XMMatrixTranslation(0, fabsf(sinf(t * 6.0f)) * 0.02f, 0) * world);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_gunner_leg_l.obj"), XMMatrixRotationX(sinf(t * 6.0f) * 0.5f) * XMMatrixTranslation(-0.12f, 0.32f, 0) * world);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_gunner_leg_r.obj"), XMMatrixRotationX(-sinf(t * 6.0f) * 0.5f) * XMMatrixTranslation(0.12f, 0.32f, 0) * world);
            break;
        case EnemyType::Artillery:
        {
            ModelDraw(body, world);
            const float k = powf(std::max(0.0f, 1.0f - fmodf(t, 2.0f) * 3.0f), 2.0f);   // 2 秒ごとに発射の反動
            ModelDraw(EnemyParts_Get("resource/Models/enemy_artillery_barrel.obj"),
                      XMMatrixTranslation(0, 0, -0.16f * k) * XMMatrixRotationX(-(0.04f + 0.22f * k)) * XMMatrixTranslation(0, 0.48f, 0.1f) * world);
            break;
        }
        case EnemyType::Turret:
            ModelDraw(body, world);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_turret_dome.obj"), XMMatrixRotationY(sinf(t * 0.8f) * 0.9f) * XMMatrixTranslation(0, 0.52f, 0) * world);
            break;
        case EnemyType::Phantom:
        {
            const XMMATRIX b = XMMatrixTranslation(0, 0.04f + sinf(t * 2.4f) * 0.04f, 0) * world;
            ModelDraw(body, b);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_phantom_blades.obj"), XMMatrixRotationY(t * 1.4f) * XMMatrixTranslation(0, 0.44f, 0) * b);
            ModelDraw(EnemyParts_Get("resource/Models/enemy_phantom_ring.obj"), XMMatrixRotationY(-t * 2.2f) * XMMatrixTranslation(0, 0.2f, 0) * b);
            break;
        }
        case EnemyType::BossArgus:
        {
            ModelDraw(body, world);
            const XMMATRIX c = XMMatrixTranslation(0, 1.2f, 0) * world;
            ModelDraw(EnemyParts_Get("resource/Models/boss_argus_ring1.obj"), XMMatrixRotationY(t * 0.8f) * XMMatrixRotationX(-0.35f) * c);
            ModelDraw(EnemyParts_Get("resource/Models/boss_argus_ring2.obj"), XMMatrixRotationY(-t * 1.1f) * XMMatrixRotationZ(0.45f) * XMMatrixRotationY(1.0f) * c);
            break;
        }
        case EnemyType::BossGoliath:
        {
            const float ph = t * 3.0f;
            const XMMATRIX b = XMMatrixTranslation(0, fabsf(sinf(ph)) * 0.06f, 0) * world;
            ModelDraw(body, b);
            const float lx[4] = { 0.95f, 0.95f, -0.95f, -0.95f }, lz[4] = { 0.8f, -0.8f, -0.8f, 0.8f }, lp[4] = { 0, XM_PI, 0, XM_PI };
            for (int i = 0; i < 4; ++i)
                ModelDraw(EnemyParts_Get("resource/Models/boss_goliath_leg.obj"), XMMatrixRotationX(sinf(ph + lp[i]) * 0.35f) * XMMatrixTranslation(lx[i], 1.1f, lz[i]) * world);
            for (float s : { -1.0f, 1.0f })
                ModelDraw(EnemyParts_Get("resource/Models/boss_goliath_cannon.obj"), XMMatrixTranslation(s * 1.15f, 2.2f, 0) * b);
            break;
        }
        case EnemyType::BossOmega:
        {
            ModelDraw(body, world);
            ModelDraw(EnemyParts_Get("resource/Models/boss_omega_core.obj"), XMMatrixRotationY(t * 1.5f) * XMMatrixTranslation(0, 2.3f + sinf(t * 2.0f) * 0.1f, 0) * world);
            const XMMATRIX c = XMMatrixTranslation(0, 2.2f, 0) * world;
            ModelDraw(EnemyParts_Get("resource/Models/boss_omega_ring1.obj"), XMMatrixRotationY(t * 0.7f) * c);
            ModelDraw(EnemyParts_Get("resource/Models/boss_omega_ring2.obj"), XMMatrixRotationY(t * 1.3f) * XMMatrixRotationX(1.22f) * XMMatrixRotationY(-t * 0.4f) * c);
            break;
        }
        case EnemyType::BossHydra:
        {
            ModelDraw(body, world);
            const float yaw[3] = { 0.0f, XMConvertToRadians(-40.0f), XMConvertToRadians(40.0f) };
            for (int i = 0; i < 3; ++i)
                ModelDraw(EnemyParts_Get("resource/Models/boss_hydra_head.obj"),
                          XMMatrixRotationY(sinf(t * 1.3f + i * 2.1f) * 0.16f) * XMMatrixRotationX(sinf(t * 1.7f + i * 1.3f) * 0.08f) *
                          XMMatrixRotationY(yaw[i]) * XMMatrixTranslation(0.5f * sinf(yaw[i]), 1.4f, 0.5f * cosf(yaw[i])) * world);
            break;
        }
        case EnemyType::BossSpectre:
        {
            ModelDraw(body, world);
            const float swing = sinf(t * 1.5f) * 0.25f;
            ModelDraw(EnemyParts_Get("resource/Models/boss_spectre_arm_l.obj"), XMMatrixRotationX(swing) * XMMatrixTranslation(-0.78f, 1.45f, 0) * world);
            ModelDraw(EnemyParts_Get("resource/Models/boss_spectre_arm_r.obj"), XMMatrixRotationX(-swing) * XMMatrixTranslation(0.78f, 1.45f, 0) * world);
            break;
        }
        case EnemyType::BossBastion: EnemyBossEx::DrawBallRig(EnemyBossEx::Kind::Bastion, body, world, t, t * 4.0f, 0.0f, false); break;
        case EnemyType::BossNest:    EnemyBossEx::DrawBallRig(EnemyBossEx::Kind::Nest,    body, world, t, 0.0f,     0.0f, false); break;
        case EnemyType::BossEclipse: EnemyBossEx::DrawBallRig(EnemyBossEx::Kind::Eclipse, body, world, t, 0.0f,     0.0f, false); break;
        case EnemyType::Wing:    EnemyBall::DrawPreview(EnemyBall::Kind::Wing,    world, t); break;
        case EnemyType::Gatling: EnemyBall::DrawPreview(EnemyBall::Kind::Gatling, world, t); break;
        case EnemyType::Orbiter: EnemyBall::DrawPreview(EnemyBall::Kind::Orbiter, world, t); break;
        case EnemyType::Walker:  EnemyBall::DrawPreview(EnemyBall::Kind::Walker,  world, t); break;
        case EnemyType::Halo:    EnemyBall::DrawPreview(EnemyBall::Kind::Halo,    world, t); break;
        default:
            ModelDraw(body, e.fbx ? flip * world : world);
            break;
        }
    }

    void DrawPreview3D(const DexEntry& e)
    {
        MODEL* body = EnemyParts_Get(e.model, e.modelScale);
        if (!body) return;

        // 本体の AABB から注視点と大きさを決める
        const AABB box = ModelGetAABB(body, { 0.0f, 0.0f, 0.0f });
        XMFLOAT3 c = { (box.min.x + box.max.x) * 0.5f, (box.min.y + box.max.y) * 0.5f, (box.min.z + box.max.z) * 0.5f };
        if (e.fbx) { c.x = -c.x; c.z = -c.z; }
        c.y += e.offsetY;
        const float ext = std::max({ box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z });
        const float radius = std::max(0.2f, ext * 0.5f * e.frame);

        // 3Dの領域（パネル内、見出しの下）
        constexpr float VX = PV_X + 10.0f, VY = PV_Y + 70.0f, VW = PV_W - 20.0f, VH = PV_H - 80.0f;
        const float fov  = XMConvertToRadians(34.0f);
        const float dist = radius / tanf(fov * 0.5f) * 1.05f;
        const XMFLOAT3 eye = { dist * 0.30f, dist * 0.32f, dist * 0.92f };
        const XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
        const XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, VW / VH, 0.01f, 200.0f);

        Shader3d_SetViewMatrix(view);
        Shader3d_SetProjectMatrix(proj);
        ShaderToon_SetViewMatrix(view);
        ShaderToon_SetProjectMatrix(proj);

        const float sx = static_cast<float>(Direct3D_GetBackBufferWidth())  / 1600.0f;
        const float sy = static_cast<float>(Direct3D_GetBackBufferHeight()) / 900.0f;
        D3D11_VIEWPORT vp{ VX * sx, VY * sy, VW * sx, VH * sy, 0.0f, 1.0f };

        Direct3D_ClearDepth();
        Direct3D_SetDepthEnable(true);
        Direct3D_GetContext()->RSSetViewports(1, &vp);

        Light_SetPointLightCount(0);
        Light_SetDirectionalWorld({ 0.35f, -0.8f, 0.48f, 0.0f }, { 0.85f, 0.85f, 0.9f, 1.0f });
        Light_SetAmbient({ 0.55f, 0.55f, 0.6f });
        Light_SetSpecularWorld(eye, 20.0f, { 0.35f, 0.35f, 0.4f, 1.0f });

        // 切り替え直後は下からせり上がる
        const float rise = std::max(0.0f, 1.0f - g_SelectTime / 0.35f);
        const XMMATRIX world =
            XMMatrixTranslation(-c.x, -c.y - rise * radius * 0.6f, -c.z) *
            XMMatrixRotationY(g_ViewYaw + g_Time * 0.6f);
        DrawEntryModel(e, world, g_Time);

        D3D11_VIEWPORT full{ 0.0f, 0.0f, static_cast<float>(Direct3D_GetBackBufferWidth()),
                             static_cast<float>(Direct3D_GetBackBufferHeight()), 0.0f, 1.0f };
        Direct3D_GetContext()->RSSetViewports(1, &full);
        Direct3D_SetDepthEnable(false);
        Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
    }
}

//==============================================================================
// 撃破数
//==============================================================================
int EnemyDex_TypeCount() { return TYPE_COUNT; }

void EnemyDex_RecordKill(int type)
{
    if (type < 0 || type >= TYPE_COUNT) return;
    if (g_Kills[type] < 999999) ++g_Kills[type];
}

int EnemyDex_GetKills(int type)
{
    return (type >= 0 && type < TYPE_COUNT) ? g_Kills[type] : 0;
}

void EnemyDex_SetKills(int type, int kills)
{
    if (type >= 0 && type < TYPE_COUNT) g_Kills[type] = std::max(0, kills);
}

//==============================================================================
// 画面
//==============================================================================
void EnemyDex_Initialize()
{
    g_Selected   = 0;
    g_Time       = 0.0f;
    g_ViewYaw    = 0.0f;
    g_SelectTime = 0.0f;
    g_IsEnd      = false;
    g_Dragging   = false;
    if (g_SeCursor < 0) g_SeCursor = LoadAudio("resource/Sound/ui_cursor_move.wav");
    if (g_SeCancel < 0) g_SeCancel = LoadAudio("resource/Sound/ui_cancel.wav");

    // 最初の表示で引っかからないよう、全モデルを先に読み込んでおく
    for (const DexEntry& e : k_Entries) EnemyParts_Get(e.model, e.modelScale);
}

void EnemyDex_Finalize()
{
    UnloadAudio(g_SeCursor); g_SeCursor = -1;
    UnloadAudio(g_SeCancel); g_SeCancel = -1;
}

void EnemyDex_Update(double elapsed_time)
{
    const float dt = static_cast<float>(elapsed_time);
    g_Time       += dt;
    g_SelectTime += dt;

    // ホイールは一覧の上でだけ効かせる（奥に回すと上の項目へ）
    const int wheel = UI_IsMouseIn(LIST_X, LIST_Y, LIST_W, LIST_H) ? UI_GetMouseWheel() : 0;

    if (UI_IsMoveUp() || wheel > 0)
    {
        g_Selected = (g_Selected + ENTRY_COUNT - 1) % ENTRY_COUNT;
        g_SelectTime = 0.0f;
        PlayAudio(g_SeCursor, false);
    }
    if (UI_IsMoveDown() || wheel < 0)
    {
        g_Selected = (g_Selected + 1) % ENTRY_COUNT;
        g_SelectTime = 0.0f;
        PlayAudio(g_SeCursor, false);
    }

    // マウス：一覧の行をクリックで選ぶ
    for (int i = 0; i < ENTRY_COUNT; ++i)
    {
        if (i != g_Selected && UI_IsClickIn(LIST_X + 8.0f, ROW_Y0 + i * ROW_H, LIST_W - 16.0f, ROW_H - 3.0f))
        {
            g_Selected = i;
            g_SelectTime = 0.0f;
            PlayAudio(g_SeCursor, false);
        }
    }

    if (UI_IsMoveLeftHeld())  g_ViewYaw -= 2.2f * dt;
    if (UI_IsMoveRightHeld()) g_ViewYaw += 2.2f * dt;

    // マウス：プレビューを左ドラッグで回す
    if (UI_IsMouseLeftTrig() && UI_IsMouseIn(PV_X, PV_Y, PV_W, PV_H)) g_Dragging = true;
    if (!UI_IsMouseLeftHeld()) g_Dragging = false;
    if (g_Dragging) g_ViewYaw += UI_GetMouseDeltaX() * 0.012f;

    if (UI_IsCancel())
    {
        PlayAudio(g_SeCancel, false);
        g_IsEnd = true;
    }
}

bool EnemyDex_IsEnd()
{
    return g_IsEnd;
}

void EnemyDex_Draw()
{
    const DexEntry& sel = k_Entries[g_Selected];
    const XMFLOAT4 accent = ClassColor(sel.cls);
    const int kills = EnemyDex_GetKills(static_cast<int>(sel.type));

    int recorded = 0, totalKills = 0;
    for (const DexEntry& e : k_Entries)
    {
        const int k = EnemyDex_GetKills(static_cast<int>(e.type));
        if (k > 0) ++recorded;
        totalKills += k;
    }

    BeginSprites();

    //--------------------------------------------------------------------------
    // 背景
    //--------------------------------------------------------------------------
    Fill(0.0f, 0.0f, 1600.0f, 900.0f, { 0.010f, 0.018f, 0.032f, 1.0f });
    Grid(0.0f, 0.0f, 1600.0f, 900.0f, 40.0f, WithAlpha(kCyan, 0.04f));
    Scanlines(0.0f, 0.0f, 1600.0f, 900.0f, 4.0f, 0.03f);
    Brackets(20.0f, 20.0f, 1560.0f, 860.0f, 40.0f, WithAlpha(kCyan, 0.5f), 2.0f);
    Fill(40.0f, 84.0f, 1520.0f, 1.0f, WithAlpha(kCyan, 0.35f));
    Ticks(40.0f, 86.0f, 1520.0f, 76, 4, WithAlpha(kCyan, 0.3f));

    //--------------------------------------------------------------------------
    // 一覧
    //--------------------------------------------------------------------------
    Panel(LIST_X, LIST_Y, LIST_W, LIST_H, kPanel, WithAlpha(kCyan, 0.5f), 16.0f);
    Fill(LIST_X + 1.0f, LIST_Y + 32.0f, LIST_W - 2.0f, 1.0f, WithAlpha(kCyan, 0.3f));
    for (int i = 0; i < ENTRY_COUNT; ++i)
    {
        const DexEntry& e = k_Entries[i];
        const float y = ROW_Y0 + i * ROW_H;
        const XMFLOAT4 c = ClassColor(e.cls);
        const bool on = (i == g_Selected);
        if (on)
        {
            Fill(LIST_X + 8.0f, y, LIST_W - 16.0f, ROW_H - 3.0f, WithAlpha(c, 0.16f));
            Frame(LIST_X + 8.0f, y, LIST_W - 16.0f, ROW_H - 3.0f, WithAlpha(c, 0.8f), 1.0f);
            Fill(LIST_X + 8.0f, y, 3.0f, ROW_H - 3.0f, c);
        }
        else if (UI_IsMouseIn(LIST_X + 8.0f, y, LIST_W - 16.0f, ROW_H - 3.0f))
        {
            // マウスが乗っている行（クリックで選べることを示す）
            Frame(LIST_X + 8.0f, y, LIST_W - 16.0f, ROW_H - 3.0f, WithAlpha(c, 0.5f), 1.0f);
        }
        Fill(LIST_X + 18.0f, y + 9.0f, 4.0f, ROW_H - 21.0f, WithAlpha(c, on ? 1.0f : 0.55f));
        // 分類の切れ目
        if (i > 0 && k_Entries[i - 1].cls != e.cls)
            Fill(LIST_X + 14.0f, y - 2.0f, LIST_W - 28.0f, 1.0f, WithAlpha(kCyan, 0.18f));
    }

    //--------------------------------------------------------------------------
    // プレビュー枠
    //--------------------------------------------------------------------------
    Panel(PV_X, PV_Y, PV_W, PV_H, kPanel, WithAlpha(accent, 0.55f), 18.0f);
    Brackets(PV_X - 5.0f, PV_Y - 5.0f, PV_W + 10.0f, PV_H + 10.0f, 14.0f, WithAlpha(accent, 0.85f));
    Fill(PV_X + 1.0f, PV_Y + 64.0f, PV_W - 2.0f, 1.0f, WithAlpha(accent, 0.3f));
    Grid(PV_X + 10.0f, PV_Y + 70.0f, PV_W - 20.0f, PV_H - 80.0f, 32.0f, WithAlpha(accent, 0.07f));
    // 照準風の円（ひし形）と走査線
    Diamond(PV_X + PV_W * 0.5f, PV_Y + 70.0f + (PV_H - 80.0f) * 0.55f, 150.0f, WithAlpha(accent, 0.05f));
    const float scanY = PV_Y + 70.0f + fmodf(g_Time * 90.0f, PV_H - 80.0f);
    Fill(PV_X + 10.0f, scanY, PV_W - 20.0f, 2.0f, WithAlpha(accent, 0.18f));

    //--------------------------------------------------------------------------
    // 性能
    //--------------------------------------------------------------------------
    Panel(DATA_X, DATA_Y, DATA_W, DATA_H, kPanel, WithAlpha(accent, 0.5f), 16.0f);
    Fill(DATA_X + 1.0f, DATA_Y + 32.0f, DATA_W - 2.0f, 1.0f, WithAlpha(accent, 0.3f));
    for (int i = 0; i < 5; ++i)   // 脅威度
        Diamond(DATA_X + 40.0f + i * 30.0f, DATA_Y + 132.0f, 9.0f,
                (i < sel.threat) ? accent : WithAlpha(accent, 0.15f));
    // 耐久（通常機は 1600、大型兵器は 90000 を満タンとする）
    const float hpMax = (sel.cls == DexClass::Boss) ? 90000.0f : 1600.0f;
    SegmentBar(DATA_X + 24.0f, DATA_Y + 210.0f, DATA_W - 48.0f, 10.0f, 24,
               std::min(1.0f, sel.hp / hpMax), WithAlpha(accent, 0.9f), WithAlpha(accent, 0.12f));
    SegmentBar(DATA_X + 24.0f, DATA_Y + 290.0f, DATA_W - 48.0f, 10.0f, 24,
               std::min(1.0f, sel.speed / 8.0f), WithAlpha(kCyan, 0.9f), WithAlpha(kCyan, 0.12f));
    Fill(DATA_X + 24.0f, DATA_Y + 340.0f, DATA_W - 48.0f, 1.0f, WithAlpha(accent, 0.25f));

    //--------------------------------------------------------------------------
    // 解説
    //--------------------------------------------------------------------------
    Panel(TXT_X, TXT_Y, TXT_W, TXT_H, kPanel, WithAlpha(accent, 0.45f), 16.0f);
    Fill(TXT_X + 1.0f, TXT_Y + 32.0f, TXT_W - 2.0f, 1.0f, WithAlpha(accent, 0.3f));
    Fill(TXT_X + 24.0f, TXT_Y + 46.0f, 3.0f, 26.0f, accent);

    //--------------------------------------------------------------------------
    // 3Dモデル
    //--------------------------------------------------------------------------
    DrawPreview3D(sel);
    BeginSprites();   // 3D描画で変わった状態を2Dに戻す

    //--------------------------------------------------------------------------
    // 文字
    //--------------------------------------------------------------------------
    const D2D1_COLOR_F white = D2D1::ColorF(0.93f, 0.97f, 1.0f, 1.0f);
    wchar_t buf[96];

    Text(L"ENEMY DATABASE", 44.0f, 32.0f, 36.0f, ToD2D(kCyan), UIFont::Display, UIAlign::Left, true, 1.0f);
    Text(L"エネミー図鑑", 300.0f, 44.0f, 18.0f, ToD2D(kCyan, 0.75f), UIFont::Body);
    swprintf_s(buf, L"RECORDED %02d / %02d     TOTAL KILLS %s", recorded, ENTRY_COUNT, Grouped(totalKills).c_str());
    Text(buf, 1556.0f, 50.0f, 15.0f, ToD2D(kCyan, 0.85f), UIFont::Mono, UIAlign::Right, true);
    Text(L"SYS://ARCHIVE/HOSTILE", 1556.0f, 28.0f, 13.0f, ToD2D(kCyan, 0.55f), UIFont::Mono, UIAlign::Right);

    Text(L"HOSTILE LIST", LIST_X + 16.0f, LIST_Y + 8.0f, 15.0f, ToD2D(kCyan), UIFont::Mono, UIAlign::Left, true);
    Text(L"KILLS", LIST_X + LIST_W - 18.0f, LIST_Y + 9.0f, 13.0f, ToD2D(kCyan, 0.6f), UIFont::Mono, UIAlign::Right, true);
    for (int i = 0; i < ENTRY_COUNT; ++i)
    {
        const DexEntry& e = k_Entries[i];
        const float y = ROW_Y0 + i * ROW_H;
        const bool on = (i == g_Selected);
        const XMFLOAT4 c = ClassColor(e.cls);
        const int k = EnemyDex_GetKills(static_cast<int>(e.type));
        swprintf_s(buf, L"%02d", i + 1);
        Text(buf, LIST_X + 32.0f, y + 7.0f, 13.0f, ToD2D(c, on ? 0.9f : 0.45f), UIFont::Mono, UIAlign::Left, true);
        Text(e.name, LIST_X + 62.0f, y + 3.0f, 20.0f, on ? white : ToD2D(c, k > 0 ? 0.8f : 0.5f),
             UIFont::Display, UIAlign::Left, true);
        Text(e.nameJp, LIST_X + 200.0f, y + 7.0f, 13.0f, D2D1::ColorF(0.75f, 0.82f, 0.9f, on ? 1.0f : 0.55f), UIFont::Body);
        Text(k > 0 ? Grouped(k) : std::wstring(L"---"), LIST_X + LIST_W - 22.0f, y + 6.0f, 15.0f,
             ToD2D(k > 0 ? c : kCyanDim, on ? 1.0f : 0.7f), UIFont::Mono, UIAlign::Right, true);
    }

    // プレビュー見出し
    Text(sel.name, PV_X + 22.0f, PV_Y + 8.0f, 44.0f, ToD2D(accent), UIFont::Display, UIAlign::Left, true, 1.5f);
    Text(sel.nameJp, PV_X + PV_W - 22.0f, PV_Y + 14.0f, 20.0f, white, UIFont::Body, UIAlign::Right, true);
    Text(ClassLabel(sel.cls), PV_X + PV_W - 22.0f, PV_Y + 42.0f, 12.0f, ToD2D(accent, 0.75f), UIFont::Mono, UIAlign::Right, true);
    Text(L"<  ROTATE / DRAG  >", PV_X + PV_W * 0.5f, PV_Y + PV_H - 26.0f, 12.0f, ToD2D(accent, 0.5f), UIFont::Mono, UIAlign::Center, true);

    // 性能
    Text(L"SPECIFICATION", DATA_X + 18.0f, DATA_Y + 8.0f, 15.0f, ToD2D(accent), UIFont::Mono, UIAlign::Left, true);
    Text(L"性能", DATA_X + DATA_W - 22.0f, DATA_Y + 8.0f, 15.0f, ToD2D(accent, 0.7f), UIFont::Body, UIAlign::Right);
    Text(L"CLASS", DATA_X + 24.0f, DATA_Y + 48.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    Text(ClassLabel(sel.cls), DATA_X + 24.0f, DATA_Y + 64.0f, 18.0f, white, UIFont::Body, UIAlign::Left, true);
    Text(L"THREAT", DATA_X + 24.0f, DATA_Y + 100.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    swprintf_s(buf, L"LV.%d", sel.threat);
    Text(buf, DATA_X + DATA_W - 24.0f, DATA_Y + 118.0f, 24.0f, ToD2D(accent), UIFont::Display, UIAlign::Right, true);

    Text(L"ARMOR", DATA_X + 24.0f, DATA_Y + 168.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    Text(Grouped(sel.hp), DATA_X + DATA_W - 24.0f, DATA_Y + 160.0f, 30.0f, white, UIFont::Display, UIAlign::Right, true);
    Text(L"MOBILITY", DATA_X + 24.0f, DATA_Y + 248.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    if (sel.speed > 0.0f) swprintf_s(buf, L"%.1f m/s", sel.speed);
    else                  swprintf_s(buf, L"%s", sel.cls == DexClass::Boss ? L"PATTERN" : L"FIXED");
    Text(buf, DATA_X + DATA_W - 24.0f, DATA_Y + 240.0f, 30.0f, white, UIFont::Display, UIAlign::Right, true);

    Text(L"KILLS", DATA_X + 24.0f, DATA_Y + 356.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    Text(kills > 0 ? Grouped(kills) : std::wstring(L"0"), DATA_X + 24.0f, DATA_Y + 372.0f, 44.0f,
         ToD2D(kills > 0 ? accent : kCyanDim), UIFont::Display, UIAlign::Left, true);
    Text(kills > 0 ? L"撃破記録あり" : L"未撃破", DATA_X + DATA_W - 24.0f, DATA_Y + 392.0f, 16.0f,
         kills > 0 ? white : ToD2D(kCyanDim), UIFont::Body, UIAlign::Right, true);

    // 解説
    Text(L"ANALYSIS", TXT_X + 18.0f, TXT_Y + 8.0f, 15.0f, ToD2D(accent), UIFont::Mono, UIAlign::Left, true);
    Text(L"解析データ", TXT_X + TXT_W - 22.0f, TXT_Y + 8.0f, 15.0f, ToD2D(accent, 0.7f), UIFont::Body, UIAlign::Right);
    Text(L"ATTACK", TXT_X + 38.0f, TXT_Y + 44.0f, 12.0f, ToD2D(kCyan, 0.7f), UIFont::Mono, UIAlign::Left, true);
    Text(sel.attack, TXT_X + 38.0f, TXT_Y + 58.0f, 18.0f, ToD2D(accent), UIFont::Body, UIAlign::Left, true);
    Text(sel.text, TXT_X + 38.0f, TXT_Y + 100.0f, 18.0f, white, UIFont::Body, UIAlign::Left);

    FlushText();
}
