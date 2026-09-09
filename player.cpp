/*==============================================================================

   プレイヤー制御 [Player.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
==============================================================================*/
#include "Player.h"
#include "model.h"
#include "WeaponDef.h"
#include "key_logger.h"
#include "pad_logger.h"
#include "Light.h"
#include "camera.h"
#include "Player_Camera.h"
#include "map.h"
#include "cube.h"
#include "bullet.h"
#include "mouse.h"
#include "collision_obb.h"
#include "Particle.h"
#include "particle_thruster.h"
#include "texture.h"
#include "direct3d.h"
#include "shader3d.h"
#include "meshfield.h"
#include "Audio.h"
#include "HUD.h"
#include "ShaderEdge.h"
#include "score.h"
#include "game.h"
#include "billboard.h"
#include "PlayerWeapon.h"
#include "shield.h"
#include "ModelToon.h"
#include "Shadow_Map.h"
#include "Trail.h"
#include "PlayerWeaponEx.h"
#include "MechParts.h"
#include <algorithm>
#include <cfloat>
using namespace DirectX;

namespace
{
    //--------------------------------------------------------------------------
    // プレイヤー状態
    //--------------------------------------------------------------------------
    XMFLOAT3 g_PlayerPosition = {};
    XMFLOAT3 g_PlayerStartPosition = {};
    XMFLOAT3 g_PlayerFront = { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 g_PlayerVelocity = {};
    MODEL* g_pPlayerModel = nullptr;  // ボディパーツモデル
    MODEL* g_pThrusterModel = nullptr;  // スラスターパーツモデル
    MODEL* g_pHeadModel = nullptr;      // 頭パーツモデル

    MODEL* g_pBarrelModel     = nullptr;  // 右腕モデル（バレル系）
    MODEL* g_pShieldModel     = nullptr;  // シールドモデル（左右共用）
    MODEL* g_pLeftBarrelModel = nullptr;  // 左腕モデル（バレル系）

    // 近接（Blade）の発光パーツ。装備時のみロードし、刃本体に重ねて描画する。
    MODEL* g_pBarrelEdgeModel     = nullptr;  // 右腕 BladeEdge
    MODEL* g_pLeftBarrelEdgeModel = nullptr;  // 左腕 BladeEdge
    constexpr char MELEE_EDGE_MODEL_PATH[] = "resource/Models/BladeEdge.fbx";
    constexpr XMFLOAT3 EDGE_GLOW_AMBIENT = { 3.0f, 3.0f, 3.0f }; // エッジ描画時だけ上げるアンビエント（発光風）

    int g_RightWeaponIdx = WEAPON_MACHINEGUN; // 右腕武器ID（0-3）
    int g_LeftWeaponIdx  = WEAPON_SHIELD;     // 左腕武器ID（0-3）
    PlayerWeapon* g_pLeftWeapon = nullptr;    // 左腕武器インスタンス

    //--------------------------------------------------------------------------
    // 発射リコイル（バレルが銃口方向の後方へ後退し、時間で復帰する演出）
    //--------------------------------------------------------------------------
    float g_RightBarrelRecoil = 0.0f;   // 右腕バレルの後退量（ワールド単位）
    float g_LeftBarrelRecoil  = 0.0f;   // 左腕バレルの後退量
    constexpr float RECOIL_KICK         = 0.12f;  // 1発あたりの後退量
    constexpr float RECOIL_RETURN_SPEED = 14.0f;  // 復帰速度（大きいほど速く戻る）


    bool g_IsJump = false;
    bool g_PlayerEnable = true;

    //--------------------------------------------------------------------------
    // ダッシュ（Shift / PAD_B）
    //   ・エネルギーを消費して短距離バースト移動する
    //   ・ダッシュ中にシールドを装備していれば接触で敵にダメージ
    //--------------------------------------------------------------------------
    float g_DashTimer    = 0.0f;   // ダッシュ有効時間（>0 の間が「ダッシュ中」）
    float g_DashCooldown = 0.0f;   // 次にダッシュできるまでの待ち時間
    constexpr float DASH_ENERGY_COST = 300.0f;  // 1回あたりのエネルギー消費（ENERGY_MAX=3000）
    constexpr float DASH_SPEED       = 24.0f;   // バースト速度（通常の最高速 ≒ 11）
    constexpr float DASH_DURATION    = 0.30f;   // 接触ダメージが有効な時間（秒）
    constexpr float DASH_COOLDOWN    = 0.55f;   // 連続ダッシュ防止のクールダウン（秒）
    constexpr int   DASH_CONTACT_DAMAGE = 300;  // シールドダッシュ接触ダメージ量

    float g_PlayerSpeedMultiplier = 2.0f;
    int g_PlayerWhightTexID = -1;        // プレイヤー矢印テクスチャ
    int g_PlayerMarkerTexID = -1;        // プレイヤー矢印テクスチャ
    //--------------------------------------------------------------------------
    // HP / 無敵
    //--------------------------------------------------------------------------
    constexpr int PLAYER_MAX_HP = PLAYER_BASE_HP;   // 機体構成の補正前の AP（MechParts.h）
int   g_PlayerMaxHP     = PLAYER_MAX_HP;       // 機体構成の補正後の最大 AP（Player_Initialize で決める）
float g_FrameSpeedMul  = 1.0f;                // 機体構成の速度倍率
float g_FrameAttackMul = 1.0f;                // 機体構成の攻撃力倍率
    int g_PlayerHP = PLAYER_MAX_HP;      // 現在HP
    double g_InvincibleTimer = 0.0;
    constexpr double INVINCIBLE_DURATION = 1.3; // 無敵時間（秒）

    //--------------------------------------------------------------------------
    // 当たり判定サイズ
    //--------------------------------------------------------------------------
    const float PLAYER_HEIGHT = 0.10f / 2.0f;
    const float PLAYER_HALF_WIDTH_X = 0.5f / 2.0f;
    const float PLAYER_HALF_WIDTH_Z = 0.75f / 2.0f;

    static XMFLOAT3 g_PlayerModelHalfExtents = { 0.25f, 0.25f, 0.375f };
    static XMFLOAT3 g_PlayerModelCenterOffset = { 0.0f,  0.0f,  0.0f };

    // g_PlayerPosition.y から頭頂部までの実距離（初期化時に計算）
    static float s_PlayerTopOffset = PLAYER_HEIGHT;

    //--------------------------------------------------------------------------
    // 火器関連ステータス
    //--------------------------------------------------------------------------
    float g_PlayerDamageMultiplier = 1.0f;          // プレイヤーの攻撃力倍率（強化システム用）

    //--------------------------------------------------------------------------
    // シールドガード構えブレンド（0.0=通常 / 1.0=ガード構え）
    //--------------------------------------------------------------------------
    float g_ShieldGuardBlend = 0.0f;
    constexpr float SHIELD_GUARD_BLEND_SPEED = 8.0f; // 1.0 到達まで約 0.125 秒

    //--------------------------------------------------------------------------
    // 武器システム
    // ・ビーム    : 固定（右クリック専用）
    // ・通常スロット : Normal/Shotgun/Missile を E キーで切り替え
    //--------------------------------------------------------------------------
    WeaponBeam* g_pBeamWeapon = nullptr;  // ビーム（固定・右クリック）

    // g_NormalWeapons は WeaponID を添字にして参照する（SHIELD の枠は nullptr）
    // [0]Normal [1]Shotgun [2]Missile [3]---(Shield) [4]MultiMissile
    PlayerWeapon* g_NormalWeapons[WEAPON_COUNT] = {};
    int                  g_NormalWeaponIdx = 0;                       // 現在の右腕通常武器（WeaponID）

    //--------------------------------------------------------------------------
    // 武器IDからインスタンスを生成するヘルパー
    //--------------------------------------------------------------------------
    static PlayerWeapon* CreateWeaponByID(int weaponId)
    {
        switch (weaponId)
        {
        case WEAPON_MACHINEGUN:   return new WeaponNormal();
        case WEAPON_SHOTGUN:      return new WeaponShotgun();
        case WEAPON_MISSILE:      return new WeaponMissile();
        case WEAPON_MULTIMISSILE: return new WeaponMultiMissile();
        case WEAPON_TRIPLEGUN:    return new WeaponTripleGun();
        case WEAPON_MELEE:        return new WeaponMelee();
        case WEAPON_RAILGUN:      return new WeaponRailgun();
        case WEAPON_GATLING:      return new WeaponGatling();
        case WEAPON_GRENADE:      return new WeaponGrenade();
        case WEAPON_BURSTRIFLE:   return new WeaponBurstRifle();
        case WEAPON_SPREADLASER:  return new WeaponSpreadLaser();
        default:                  return nullptr;
        }
    }

    //--------------------------------------------------------------------------
    // SE関連（通常スロット切り替え音のみ。射撃SEは各武器クラスが管理）
    //--------------------------------------------------------------------------
    int g_PlayerModeSwitchToNormalSE = -1;  // 通常スロット切り替えSE
    int g_SeShieldDeploy             = -1;  // シールド展開SE
    int g_SeShieldRetract            = -1;  // シールド収納SE
    int g_SeBoost                    = -1;  // 移動ブーストSE（移動中ループ）
    double g_BoostLoopTimer          = 0.0; // ブーストSEを1秒ごとに鳴らすタイマー
    int g_SeDash                     = -1;  // ダッシュ（SHIFT/PAD_B）SE：パンチの風切り音
    static constexpr double BOOST_LOOP_INTERVAL = 1.0; // ループ間隔（秒）

    //--------------------------------------------------------------------------
    // パーティクル（スラスター）
    //--------------------------------------------------------------------------
    int g_PlayerParticleTexID = -1;                     // スラスター用テクスチャID
    ThrusterEmitter* g_PlayerThrusterEmitter = nullptr; // 後方噴射用エミッター
    ThrusterEmitter* g_PlayerSmokeEmitter    = nullptr; // HP低下時のダメージ煙

    // 近接武器の刃先トレイル（振り中のみ。色はスラスターと同じ）
    Trail g_MeleeTrailR;   // 右腕
    Trail g_MeleeTrailL;   // 左腕
    constexpr XMFLOAT4 MELEE_TRAIL_COLOR = { 1.0f, 0.5f, 2.5f, 1.0f }; // スラスターの色

    // ローカルオフセット（right/up/front 基底で組み立て）
    // Initialize() でスラスターモデルの AABB から自動計算される
    // X: 右+ / 左-  Y: 上+ / 下-  Z: 前+ / 後-
    XMFLOAT3 g_ThrusterOffsetLocal = { 0.0f, 0.30f, -0.25f };

    // モデル描画の Y オフセット（Initialize と Draw で共用）
    // ボディを足元（g_PlayerPosition.y）からどれだけ持ち上げて描くか。
    // ボディの下にスラスター（脚部）が吊り下がるため、スラスターの底面が
    // ちょうど地面に接する高さを Player_Initialize でモデルの寸法から計算する。
    //（固定値 0.15 のころはスラスターが約 10cm 地面にめり込んでいた）
    float PLAYER_HEIGHT_OFFSET = 0.15f;


    float g_ThrusterLocalYaw = XMConvertToRadians(180.0f); // FBXデフォルト向き補正
    bool  g_PlayerBodyFollowCamera = true;  // true: ボディがカメラ方向 / false: 移動方向

    static XMMATRIX Player_GetBodyRotationMatrix()
    {
        float angle = -atan2f(g_PlayerFront.z, g_PlayerFront.x) + XMConvertToRadians(180.0f);
        const float pitchDeg = 0.0f;
        const float rollDeg = 0.0f;

        XMMATRIX rotFix = XMMatrixRotationZ(XMConvertToRadians(rollDeg)) *
            XMMatrixRotationX(XMConvertToRadians(pitchDeg));
        XMMATRIX rotYawFix = XMMatrixRotationY(XMConvertToRadians(90.0f));
        XMMATRIX rotY = XMMatrixRotationY(angle);

        return rotFix * rotY * rotYawFix;
    }

    //--------------------------------------------------------------------------
    // 機体パーツの取り付け点（MechParts の値 × MECH_PART_SCALE。Player_Initialize で決める）
    //   パーツの積み上げはモデルの外形ではなく取り付け点で行う（ヒレ等で頭がずれないように）
    //--------------------------------------------------------------------------
    XMFLOAT3 g_MountBodyHead = { 0.0f,  0.38f * 0.3f, 0.0f };   // 胴体：頭を載せる点
    XMFLOAT3 g_MountBodyLegs = { 0.0f, -0.37f * 0.3f, 0.0f };   // 胴体：脚部を付ける点
    XMFLOAT3 g_MountHeadNeck = { 0.0f, -0.30f * 0.3f, 0.0f };   // 頭：首
    XMFLOAT3 g_MountLegsTop  = { 0.0f,  0.15f * 0.3f, 0.0f };   // 脚部：上面

    //--------------------------------------------------------------------------
    // 胴体の「枠」：XZ はモデルの外形、Y は 脚部の取り付け点（下）〜頭の取り付け点（上）。
    // 武器・盾・近接・頭・脚部の位置はこの枠を基準にする（以前のモデル AABB の代わり）
    //--------------------------------------------------------------------------
    static AABB Player_GetBodyFrameBox(const XMFLOAT3& bodyWorldPos)
    {
        AABB box = ModelGetAABB(g_pPlayerModel, bodyWorldPos);
        box.min.y = bodyWorldPos.y + g_MountBodyLegs.y;
        box.max.y = bodyWorldPos.y + g_MountBodyHead.y;
        return box;
    }

    static XMMATRIX Player_GetThrusterWorldMatrix()
    {
        XMMATRIX bodyRot = Player_GetBodyRotationMatrix();

        const float heightOffset = PLAYER_HEIGHT_OFFSET;
        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + heightOffset,
            g_PlayerPosition.z
        };

        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);
        const AABB thrusterLocal = ModelGetAABB(g_pThrusterModel, { 0.0f, 0.0f, 0.0f });

        // スラスターのローカル中心を計算
        const float centerX = (thrusterLocal.min.x + thrusterLocal.max.x) * 0.5f;
        const float centerY = (thrusterLocal.min.y + thrusterLocal.max.y) * 0.5f;
        const float centerZ = (thrusterLocal.min.z + thrusterLocal.max.z) * 0.5f;

        // 中心を原点に移動 → 回転 → 中心を戻す
        XMMATRIX toCenter   = XMMatrixTranslation(-centerX, -centerY, -centerZ);
        XMMATRIX rot        = XMMatrixRotationY(g_ThrusterLocalYaw);
        XMMATRIX fromCenter = XMMatrixTranslation(centerX, centerY, centerZ);

        XMMATRIX thrusterLocalRot = toCenter * rot * fromCenter;
        constexpr float THRUSTER_FORWARD_OFFSET = -0.05f;
        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));

        XMMATRIX thrusterTrans = XMMatrixTranslation(
            g_PlayerPosition.x + XMVectorGetX(playerFront) * THRUSTER_FORWARD_OFFSET,
            bodyAABB.min.y - g_MountLegsTop.y - 0.01f,   // 胴体の脚の取り付け点に脚部の上面を合わせる
            g_PlayerPosition.z + XMVectorGetZ(playerFront) * THRUSTER_FORWARD_OFFSET
        );

        return thrusterLocalRot * bodyRot * thrusterTrans;
    }

    //--------------------------------------------------------------------------
    // 近接（薙ぎ払い）姿勢の調整値【左右共通】。ここ1か所を変えれば両腕に効く。
    //--------------------------------------------------------------------------
    constexpr float MELEE_PIVOT_SIDE  = 0.10f;  // 肩の横位置
    constexpr float MELEE_PIVOT_FWD   = 0.10f;  // 肩の前方位置
    constexpr float MELEE_PIVOT_UP    = 0.55f;  // 肩（胸）の高さ比
    constexpr float MELEE_GRIP_RADIUS = 0.50f;  // 肩から武器までの距離（体からの近さ）
    constexpr float MELEE_FWD_THRUST  = 0.10f;  // 前方（照準方向）への突き出し量
    // 刃の向き調整（振り中の武器姿勢）。swingRot（-Z が外側）に対して掛ける追加回転。
    // 見ながら3軸を数値で合わせてください。既定は元の銃向き相当（Z=150）。
    constexpr float MELEE_ROT_X = 0.0f;    // X軸（銃身を上下に倒す）
    constexpr float MELEE_ROT_Y = 0.0f;    // Y軸（左右に振る）
    constexpr float MELEE_ROT_Z = 90.0f;   // Z軸ロール（=底面を外側へ向ける90°回転）

    static XMMATRIX Player_GetBarrelWorldMatrix()
    {
        constexpr float BARREL_FLIP_DEG = 180.0f; // Z軸反転（モデル上下補正）
        constexpr float BARREL_LEAN_DEG = -30.0f; // 傾き
        constexpr float BARREL_TILT_DEG =   0.0f;
        constexpr float BARREL_SIDE_X = 0.30f;  // 右横オフセット（調整可）
        constexpr float BARREL_FORWARD_OFFSET = 0.3f;  // 前方オフセット（調整可）

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        // ボディ右方向（ワールド）を計算して横オフセット適用
        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };
        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);

        // バレル原点位置（ボディ底面・右側）
        XMFLOAT3 barrelOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * BARREL_SIDE_X
                               + XMVectorGetX(playerFront) * BARREL_FORWARD_OFFSET,
            bodyAABB.min.y,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * BARREL_SIDE_X
                               + XMVectorGetZ(playerFront) * BARREL_FORWARD_OFFSET
        };
        // 近接は共通定数（WeaponDef.h）で胴体側面中心＋前へ。アセンブリと位置を一致させる。
        if (g_RightWeaponIdx == WEAPON_MELEE)
        {
            barrelOriginPos = {
                g_PlayerPosition.x + XMVectorGetX(playerRight) * MELEE_REST_SIDE
                                   + XMVectorGetX(playerFront) * MELEE_REST_FWD,
                bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_REST_UP_R,
                g_PlayerPosition.z + XMVectorGetZ(playerRight) * MELEE_REST_SIDE
                                   + XMVectorGetZ(playerFront) * MELEE_REST_FWD
            };
        }
        XMMATRIX barrelTrans = XMMatrixTranslation(
            barrelOriginPos.x, barrelOriginPos.y, barrelOriginPos.z);

        // 照準方向：カメラ追従モード or プレイヤー正面固定
        XMVECTOR aimDir;
        if (g_PlayerBodyFollowCamera)
        {
            // カメラ追従モード：ロックオン or カメラ前方
            XMFLOAT3 lockOnPos;
            if (Game_GetLockOnWorldPos(&lockOnPos))
            {
                XMVECTOR toTarget = XMLoadFloat3(&lockOnPos)
                    - XMLoadFloat3(&barrelOriginPos);
                aimDir = XMVector3Normalize(toTarget);
            }
            else
            {
                XMFLOAT3 camFront = Player_Camera_GetFront();
                aimDir = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
            }
        }
        else
        {
            // 移動方向モード：プレイヤー正面をそのまま使う
            aimDir = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        }

        // 近接武器：薙ぎ払い中は「後端(グリップ)をプレイヤー側・先端を外側」に向けて薙ぐ。
        PlayerWeapon* const rWeapon = g_NormalWeapons[g_NormalWeaponIdx];

        if (rWeapon && rWeapon->IsSwinging())
        {
            const float swingDeg = rWeapon->GetSwingAngleDeg();
            const float thrust   = rWeapon->GetSwingThrust01();

            // 先端(穂先)の向き：照準を鉛直軸まわりに swingDeg 回した水平方向
            XMVECTOR outward = XMVector3Normalize(XMVector3TransformNormal(
                aimDir, XMMatrixRotationY(XMConvertToRadians(swingDeg))));

            // 回転中心(肩)＝プレイヤー近く。
            const float pivotY = bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_PIVOT_UP;
            XMVECTOR pivot = XMVectorSet(
                g_PlayerPosition.x + XMVectorGetX(playerRight) * MELEE_PIVOT_SIDE
                                   + XMVectorGetX(playerFront) * MELEE_PIVOT_FWD,
                pivotY,
                g_PlayerPosition.z + XMVectorGetZ(playerRight) * MELEE_PIVOT_SIDE
                                   + XMVectorGetZ(playerFront) * MELEE_PIVOT_FWD,
                0.0f);

            // 振り切った時の狙い位置（肩から outward へ＋前方突き出し）
            XMVECTOR swingPos = pivot
                + outward * MELEE_GRIP_RADIUS
                + aimDir  * MELEE_FWD_THRUST;
            // 元の held 位置（通常のバレル位置）から thrust で補間＝元の位置から突き出す
            XMVECTOR restPos = XMLoadFloat3(&barrelOriginPos);
            XMVECTOR gripPos = XMVectorLerp(restPos, swingPos, thrust);

            // 目標の向き（2条件を同時に満たす）：
            //   底面(-Y) → 外側＝半径方向 R(=outward)
            //   先端(-Z) → 円周＝進行方向 T
            // 基準：-Z(先端) を outward(敵側/外側) へ。後端(+Z)は自動でプレイヤー側。
            // 上は自然（局所+Y→上）。底面を外へ向ける90°は MELEE_ROT_Z で掛ける。
            XMVECTOR sZ = XMVectorNegate(outward);
            XMVECTOR sX = XMVector3Normalize(XMVector3Cross(up, sZ));
            if (XMVectorGetX(XMVector3LengthSq(sX)) < 0.001f)
                sX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
            XMVECTOR sY = XMVector3Cross(sZ, sX);

            XMFLOAT3 sx, sy, sz;
            XMStoreFloat3(&sx, sX);
            XMStoreFloat3(&sy, sY);
            XMStoreFloat3(&sz, sZ);
            XMMATRIX swingRot(
                sx.x, sx.y, sx.z, 0.0f,
                sy.x, sy.y, sy.z, 0.0f,
                sz.x, sz.y, sz.z, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);

            // 微調整用の追加回転。右腕は鏡像なのでZロールを反転（底面の向きを左手と揃える）。
            XMMATRIX localRot =
                XMMatrixRotationX(XMConvertToRadians(MELEE_ROT_X)) *
                XMMatrixRotationY(XMConvertToRadians(MELEE_ROT_Y)) *
                XMMatrixRotationZ(XMConvertToRadians(-MELEE_ROT_Z));

            XMMATRIX gripTrans = XMMatrixTranslation(
                XMVectorGetX(gripPos), XMVectorGetY(gripPos), XMVectorGetZ(gripPos));
            return localRot * swingRot * gripTrans;
        }

        // 通常：発射リコイル（銃口方向の逆＝後方へバレルをずらす）
        {
            XMVECTOR recoil = XMVectorNegate(aimDir) * g_RightBarrelRecoil;
            barrelTrans = XMMatrixTranslation(
                barrelOriginPos.x + XMVectorGetX(recoil),
                barrelOriginPos.y + XMVectorGetY(recoil),
                barrelOriginPos.z + XMVectorGetZ(recoil));
        }

        // ローカル -Z がマズル方向なので aimZ を反転（バレルモデルのデフォルト向き対応）
        XMVECTOR aimZ = XMVectorNegate(aimDir);
        XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(up, aimZ));
        if (XMVectorGetX(XMVector3LengthSq(aimX)) < 0.001f)
            aimX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR aimY = XMVector3Cross(aimZ, aimX);

        XMFLOAT3 ax, ay, az;
        XMStoreFloat3(&ax, aimX);
        XMStoreFloat3(&ay, aimY);
        XMStoreFloat3(&az, aimZ);

        // 行ベクトル形式：ローカル X/Y/Z がワールド aimX/aimY/aimZ に対応
        XMMATRIX aimRot(
            ax.x, ax.y, ax.z, 0.0f,
            ay.x, ay.y, ay.z, 0.0f,
            az.x, az.y, az.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        XMMATRIX localRot =
            XMMatrixRotationZ(XMConvertToRadians(BARREL_FLIP_DEG + BARREL_LEAN_DEG)) *
            XMMatrixRotationX(XMConvertToRadians(BARREL_TILT_DEG));

        return localRot * aimRot * barrelTrans;
    }   

    // 左腕バレル（右腕の鏡像：サイドオフセット・リーンを反転）
    static XMMATRIX Player_GetLeftBarrelWorldMatrix()
    {
        constexpr float BARREL_FLIP_DEG          = 180.0f;
        constexpr float BARREL_LEAN_DEG          =  30.0f;  // 右腕と逆方向
        constexpr float BARREL_TILT_DEG          =   0.0f;
        constexpr float BARREL_SIDE_X            = -0.30f;  // 左側
        constexpr float BARREL_FORWARD_OFFSET    =  0.3f;


        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };
        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);

        XMFLOAT3 barrelOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * BARREL_SIDE_X
                               + XMVectorGetX(playerFront) * BARREL_FORWARD_OFFSET,
            bodyAABB.min.y,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * BARREL_SIDE_X
                               + XMVectorGetZ(playerFront) * BARREL_FORWARD_OFFSET
        };
        // 近接は共通定数で胴体側面中心＋前へ（左腕は横を反転）
        if (g_LeftWeaponIdx == WEAPON_MELEE)
        {
            barrelOriginPos = {
                g_PlayerPosition.x - XMVectorGetX(playerRight) * MELEE_REST_SIDE
                                   + XMVectorGetX(playerFront) * MELEE_REST_FWD,
                bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_REST_UP_R,
                g_PlayerPosition.z - XMVectorGetZ(playerRight) * MELEE_REST_SIDE
                                   + XMVectorGetZ(playerFront) * MELEE_REST_FWD
            };
        }
        XMMATRIX barrelTrans = XMMatrixTranslation(
            barrelOriginPos.x, barrelOriginPos.y, barrelOriginPos.z);

        XMVECTOR aimDir;
        if (g_PlayerBodyFollowCamera)
        {
            XMFLOAT3 lockOnPos;
            if (Game_GetLockOnWorldPos(&lockOnPos))
            {
                XMVECTOR toTarget = XMLoadFloat3(&lockOnPos)
                    - XMLoadFloat3(&barrelOriginPos);
                aimDir = XMVector3Normalize(toTarget);
            }
            else
            {
                XMFLOAT3 camFront = Player_Camera_GetFront();
                aimDir = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
            }
        }
        else
        {
            aimDir = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        }

        // 近接武器：薙ぎ払い中は「後端をプレイヤー側・先端を外側」に（左腕は鏡像）。
        if (g_pLeftWeapon && g_pLeftWeapon->IsSwinging())
        {
            const float swingDeg = -g_pLeftWeapon->GetSwingAngleDeg();  // 鏡像
            const float thrust   =  g_pLeftWeapon->GetSwingThrust01();

            XMVECTOR outward = XMVector3Normalize(XMVector3TransformNormal(
                aimDir, XMMatrixRotationY(XMConvertToRadians(swingDeg))));

            const float pivotY = bodyAABB.min.y + (bodyAABB.max.y - bodyAABB.min.y) * MELEE_PIVOT_UP;
            XMVECTOR pivot = XMVectorSet(
                g_PlayerPosition.x - XMVectorGetX(playerRight) * MELEE_PIVOT_SIDE     // 左肩
                                   + XMVectorGetX(playerFront) * MELEE_PIVOT_FWD,
                pivotY,
                g_PlayerPosition.z - XMVectorGetZ(playerRight) * MELEE_PIVOT_SIDE
                                   + XMVectorGetZ(playerFront) * MELEE_PIVOT_FWD,
                0.0f);

            XMVECTOR swingPos = pivot
                + outward * MELEE_GRIP_RADIUS
                + aimDir  * MELEE_FWD_THRUST;
            XMVECTOR restPos = XMLoadFloat3(&barrelOriginPos);
            XMVECTOR gripPos = XMVectorLerp(restPos, swingPos, thrust);

            // 基準：-Z(先端)→outward(敵側)、後端はプレイヤー側、上は自然（左腕も同じ）
            XMVECTOR sZ = XMVectorNegate(outward);
            XMVECTOR sX = XMVector3Normalize(XMVector3Cross(up, sZ));
            if (XMVectorGetX(XMVector3LengthSq(sX)) < 0.001f)
                sX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
            XMVECTOR sY = XMVector3Cross(sZ, sX);

            XMFLOAT3 sx, sy, sz;
            XMStoreFloat3(&sx, sX);
            XMStoreFloat3(&sy, sY);
            XMStoreFloat3(&sz, sZ);
            XMMATRIX swingRot(
                sx.x, sx.y, sx.z, 0.0f,
                sy.x, sy.y, sy.z, 0.0f,
                sz.x, sz.y, sz.z, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);

            XMMATRIX localRot =
                XMMatrixRotationX(XMConvertToRadians(MELEE_ROT_X)) *
                XMMatrixRotationY(XMConvertToRadians(MELEE_ROT_Y)) *
                XMMatrixRotationZ(XMConvertToRadians(MELEE_ROT_Z));

            XMMATRIX gripTrans = XMMatrixTranslation(
                XMVectorGetX(gripPos), XMVectorGetY(gripPos), XMVectorGetZ(gripPos));
            return localRot * swingRot * gripTrans;
        }

        // 通常：発射リコイル
        {
            XMVECTOR recoil = XMVectorNegate(aimDir) * g_LeftBarrelRecoil;
            barrelTrans = XMMatrixTranslation(
                barrelOriginPos.x + XMVectorGetX(recoil),
                barrelOriginPos.y + XMVectorGetY(recoil),
                barrelOriginPos.z + XMVectorGetZ(recoil));
        }

        XMVECTOR aimZ = XMVectorNegate(aimDir);
        XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(up, aimZ));
        if (XMVectorGetX(XMVector3LengthSq(aimX)) < 0.001f)
            aimX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR aimY = XMVector3Cross(aimZ, aimX);

        XMFLOAT3 ax, ay, az;
        XMStoreFloat3(&ax, aimX);
        XMStoreFloat3(&ay, aimY);
        XMStoreFloat3(&az, aimZ);

        XMMATRIX aimRot(
            ax.x, ax.y, ax.z, 0.0f,
            ay.x, ay.y, ay.z, 0.0f,
            az.x, az.y, az.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        XMMATRIX localRot =
            XMMatrixRotationZ(XMConvertToRadians(BARREL_FLIP_DEG + BARREL_LEAN_DEG)) *
            XMMatrixRotationX(XMConvertToRadians(BARREL_TILT_DEG));

        return localRot * aimRot * barrelTrans;
    }

    static XMMATRIX Player_GetShieldWorldMatrix()
    {
        constexpr float SHIELD_FLIP_DEG = 0.0f;   // 上下反転なし（修正済み）
        constexpr float SHIELD_LEAN_DEG = 0.0f;   // 傾きなし
        constexpr float SHIELD_TILT_DEG = 0.0f;   // ナナメ角度

        // 通常位置 → ガード構え位置をブレンドで補間
        const float b = g_ShieldGuardBlend;
        const float SHIELD_SIDE_X        = -0.3f + b * 0.15f;  // 展開時は外側へ
        const float SHIELD_FORWARD_OFFSET =  0.20f + b * 0.15f;  // 前方へ突き出す
        const float SHIELD_HEIGHT_OFFSET  =  0.10f + b * 0.15f;  // 少し持ち上げる

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };
        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);

        const XMFLOAT3 shieldOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * SHIELD_SIDE_X
                               + XMVectorGetX(playerFront) * SHIELD_FORWARD_OFFSET,
            bodyAABB.min.y + SHIELD_HEIGHT_OFFSET,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * SHIELD_SIDE_X
                               + XMVectorGetZ(playerFront) * SHIELD_FORWARD_OFFSET
        };

        XMMATRIX shieldTrans = XMMatrixTranslation(
            shieldOriginPos.x, shieldOriginPos.y, shieldOriginPos.z);

        // 照準方向：カメラ追従モード or プレイヤー正面固定
        XMVECTOR aimDir;
        if (g_PlayerBodyFollowCamera)
        {
            // カメラ追従モード：ロックオン or カメラ前方
            XMFLOAT3 lockOnPos;
            if (Game_GetLockOnWorldPos(&lockOnPos))
            {
                XMVECTOR toTarget = XMLoadFloat3(&lockOnPos)
                    - XMLoadFloat3(&shieldOriginPos);
                aimDir = XMVector3Normalize(toTarget);
            }
            else
            {
                XMFLOAT3 camFront = Player_Camera_GetFront();
                aimDir = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
            }
        }
        else
        {
            // 移動方向モード：プレイヤー正面をそのまま使う
            aimDir = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        }

        XMVECTOR aimZ = XMVectorNegate(aimDir);
        XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(up, aimZ));
        if (XMVectorGetX(XMVector3LengthSq(aimX)) < 0.001f)
            aimX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR aimY = XMVector3Cross(aimZ, aimX);

        XMFLOAT3 ax, ay, az;
        XMStoreFloat3(&ax, aimX);
        XMStoreFloat3(&ay, aimY);
        XMStoreFloat3(&az, aimZ);

        XMMATRIX aimRot(
            ax.x, ax.y, ax.z, 0.0f,
            ay.x, ay.y, ay.z, 0.0f,
            az.x, az.y, az.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        XMMATRIX localRot =
            XMMatrixRotationZ(XMConvertToRadians(SHIELD_FLIP_DEG + SHIELD_LEAN_DEG)) *
            XMMatrixRotationX(XMConvertToRadians(SHIELD_TILT_DEG));

        return localRot * aimRot * shieldTrans;
    }

    // 右腕シールド（左腕の鏡像：サイドオフセット反転）
    static XMMATRIX Player_GetRightShieldWorldMatrix()
    {
        constexpr float SHIELD_FLIP_DEG = 0.0f;
        constexpr float SHIELD_LEAN_DEG = 0.0f;
        constexpr float SHIELD_TILT_DEG = 0.0f;

        const float b = g_ShieldGuardBlend;
        const float SHIELD_SIDE_X        =  0.3f - b * 0.15f;  // 展開時は内側へ（左と対称）
        const float SHIELD_FORWARD_OFFSET =  0.20f + b * 0.15f;
        const float SHIELD_HEIGHT_OFFSET  =  0.10f + b * 0.15f;

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x, g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET, g_PlayerPosition.z };
        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);

        const XMFLOAT3 shieldOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * SHIELD_SIDE_X
                               + XMVectorGetX(playerFront) * SHIELD_FORWARD_OFFSET,
            bodyAABB.min.y + SHIELD_HEIGHT_OFFSET,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * SHIELD_SIDE_X
                               + XMVectorGetZ(playerFront) * SHIELD_FORWARD_OFFSET
        };
        XMMATRIX shieldTrans = XMMatrixTranslation(
            shieldOriginPos.x, shieldOriginPos.y, shieldOriginPos.z);

        XMVECTOR aimDir;
        if (g_PlayerBodyFollowCamera)
        {
            XMFLOAT3 lockOnPos;
            if (Game_GetLockOnWorldPos(&lockOnPos))
                aimDir = XMVector3Normalize(XMLoadFloat3(&lockOnPos) - XMLoadFloat3(&shieldOriginPos));
            else
            {
                XMFLOAT3 camFront = Player_Camera_GetFront();
                aimDir = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
            }
        }
        else
            aimDir = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));

        XMVECTOR aimZ = XMVectorNegate(aimDir);
        XMVECTOR aimX = XMVector3Normalize(XMVector3Cross(up, aimZ));
        if (XMVectorGetX(XMVector3LengthSq(aimX)) < 0.001f)
            aimX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR aimY = XMVector3Cross(aimZ, aimX);

        XMFLOAT3 ax, ay, az;
        XMStoreFloat3(&ax, aimX); XMStoreFloat3(&ay, aimY); XMStoreFloat3(&az, aimZ);
        XMMATRIX aimRot(ax.x, ax.y, ax.z, 0, ay.x, ay.y, ay.z, 0, az.x, az.y, az.z, 0, 0, 0, 0, 1);

        XMMATRIX localRot =
            XMMatrixRotationZ(XMConvertToRadians(SHIELD_FLIP_DEG + SHIELD_LEAN_DEG)) *
            XMMatrixRotationX(XMConvertToRadians(SHIELD_TILT_DEG));

        return localRot * aimRot * shieldTrans;
    }

    static XMMATRIX Player_GetHeadWorldMatrix()
    {
        XMMATRIX bodyRot = Player_GetBodyRotationMatrix();

        const XMFLOAT3 bodyWorldPos =
        {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };

        const AABB bodyAABB = Player_GetBodyFrameBox(bodyWorldPos);

        // 頭の首を胴体の頭の取り付け点に重ねる（前後左右のずれは胴体の向きで回す）
        XMFLOAT3 off;
        XMStoreFloat3(&off, XMVector3TransformNormal(
            XMVectorSet(g_MountBodyHead.x - g_MountHeadNeck.x, 0.0f, g_MountBodyHead.z - g_MountHeadNeck.z, 0.0f), bodyRot));
        XMMATRIX headTrans = XMMatrixTranslation(
            g_PlayerPosition.x + off.x,
            bodyAABB.max.y - g_MountHeadNeck.y,
            g_PlayerPosition.z + off.z
        );

        return bodyRot * headTrans;
    }


    //--------------------------------------------------------------------------
    // 壁押し戻し（現在位置での OBB vs 壁AABB を使い、押し戻しベクトルを決定）
    //--------------------------------------------------------------------------
    static bool ResolveWallCollisionAtPosition(XMVECTOR* ioPos, XMVECTOR* ioVel)
    {
        float maxPenetration = 0.0f;
        XMVECTOR bestNormal = XMVectorZero();
        bool foundCollision = false;

        // プレイヤーOBBは壁ごとに変わらないのでループ外で1回だけ計算
        OBB playerOBB = Player_ConvertPositionToOBB(*ioPos);

        const int wallCount = Map_GetWallColliderCount();
        for (int i = 0; i < wallCount; i++)
        {
            const AABB& wallAabb = *Map_GetWallCollider(i);   // 壁のみの高速リスト
            Hit hit = Collision_IsHitOBB_AABB(playerOBB, wallAabb);
            if (!hit.isHit) continue;

            if (hit.penetration > maxPenetration)
            {
                maxPenetration = hit.penetration;
                bestNormal = XMLoadFloat3(&hit.normal);
                foundCollision = true;
            }
        }

        if (foundCollision)
        {
            *ioPos -= bestNormal * maxPenetration;

            float velDotN = XMVectorGetX(XMVector3Dot(*ioVel, bestNormal));
            if (velDotN > 0.0f)
                *ioVel -= bestNormal * velDotN;
        }

        return foundCollision;
    }


    //--------------------------------------------------------------------------
    // 速度が速いときのすり抜け対策：小ステップに分割して移動＋衝突解決を繰り返す
    //--------------------------------------------------------------------------
    static void MoveWithSubSteps(XMVECTOR* ioPos, XMVECTOR* ioVel, float dt) // すり抜け防止の分割移動。ioPos=位置(入出力) ioVel=速度(入出力) dt=このフレームの経過時間(秒)
    {
        const float maxStep = 0.05f;
        const XMVECTOR delta0 = (*ioVel) * dt;

        const float dx = fabsf(XMVectorGetX(delta0));
        const float dy = fabsf(XMVectorGetY(delta0));
        const float dz = fabsf(XMVectorGetZ(delta0));

        const float len3 = sqrtf(dx * dx + dy * dy + dz * dz);

        int steps = (int)ceilf(len3 / maxStep);
        if (steps < 1)   steps = 1;
        if (steps > 128) steps = 128;

        const float dtStep = dt / (float)steps;

        int collisionCount = 0;
        const int maxCollisions = 3;

        for (int s = 0; s < steps; ++s)
        {
            *ioPos += (*ioVel) * dtStep;

            bool hit = ResolveWallCollisionAtPosition(ioPos, ioVel);

            if (hit)
            {
                collisionCount++;

                if (collisionCount >= maxCollisions)
                {
                    *ioVel = XMVectorSetX(*ioVel, 0.0f);
                    *ioVel = XMVectorSetZ(*ioVel, 0.0f);
                    break;
                }
            }
        }
    }
}

//==============================================================================
// 初期化
//==============================================================================
void Player_Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3 front) // プレイヤーの初期化（位置/向き/HP/入力/モデル/スラスター生成）。position=初期位置 front=初期正面方向
{
    g_PlayerPosition = position;
    g_PlayerStartPosition = position;
    g_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
    g_PlayerEnable = true;


    g_ThrusterLocalYaw = XMConvertToRadians(180.0f);

    g_PlayerDamageMultiplier = 1.0f;
    g_PlayerSpeedMultiplier = 2.0f;


    // 機体構成（頭・胴体・脚部・内部パーツ）の性能を反映する
    {
        const MechStats st = MechParts_CalcStats();
        g_PlayerMaxHP     = static_cast<int>(PLAYER_MAX_HP * st.hpMul);
        g_FrameSpeedMul   = st.speedMul;
        g_FrameAttackMul  = st.attackMul;
    }
    g_PlayerHP = g_PlayerMaxHP;
    g_InvincibleTimer = 0.0;

    Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
    Mouse_SetVisible(false);

    XMStoreFloat3(&g_PlayerFront, XMVector3Normalize(XMLoadFloat3(&front)));

    g_PlayerWhightTexID = Texture_Load(L"resource/texture/Player_white.png");
    g_PlayerMarkerTexID = Texture_Load(L"resource/texture/Player_white.png");
    // プレイヤーモデルを body.fbx で構成する
    g_pPlayerModel = ModelLoad(MechParts_GetFrame(FRAME_BODY, MechParts_GetFrameSel(FRAME_BODY)).model, 0.3f);
    g_pThrusterModel = ModelLoad(MechParts_GetFrame(FRAME_LEGS, MechParts_GetFrameSel(FRAME_LEGS)).model, 0.3f);

    // 脚部（スラスター）の底面が地面に接するようにボディの描画高さを決める
    //   スラスター原点 = ボディ底面 - スラスター上端 - 0.01（Player_GetThrusterWorldMatrix と同じ）
    //   スラスター底面 = スラスター原点 + スラスター下端 = 0 になる高さ
    // 取り付け点（頭・胴体・脚部の積み上げの基準。MechParts.h を参照）
    {
        auto toF3 = [](const MechMount& m) { return XMFLOAT3{ m.x, m.y, m.z }; };
        const int body = MechParts_GetFrameSel(FRAME_BODY);
        g_MountBodyHead = toF3(MechParts_Mount(FRAME_BODY, body, false));
        g_MountBodyLegs = toF3(MechParts_Mount(FRAME_BODY, body, true));
        g_MountHeadNeck = toF3(MechParts_Mount(FRAME_HEAD, MechParts_GetFrameSel(FRAME_HEAD)));
        g_MountLegsTop  = toF3(MechParts_Mount(FRAME_LEGS, MechParts_GetFrameSel(FRAME_LEGS)));
    }
    if (g_pPlayerModel && g_pThrusterModel)
    {
        // 脚部の底が地面に接する高さ：胴体の脚の取り付け点 → 脚部の上面 → 脚部の底
        const AABB thruster = ModelGetAABB(g_pThrusterModel, { 0.0f, 0.0f, 0.0f });
        PLAYER_HEIGHT_OFFSET = -g_MountBodyLegs.y + (g_MountLegsTop.y - thruster.min.y) + 0.01f;
    }
    g_pBarrelModel = ModelLoad(k_WeaponDefs[g_NormalWeaponIdx].modelPath, k_WeaponDefs[g_NormalWeaponIdx].scale);
    if (g_NormalWeaponIdx == WEAPON_MELEE)  // 近接なら発光パーツも
        g_pBarrelEdgeModel = ModelLoad(MELEE_EDGE_MODEL_PATH, k_WeaponDefs[g_NormalWeaponIdx].scale);
    g_pHeadModel   = ModelLoad(MechParts_GetFrame(FRAME_HEAD, MechParts_GetFrameSel(FRAME_HEAD)).model, 0.3f);
    g_pShieldModel = ModelLoad(k_WeaponDefs[WEAPON_SHIELD].modelPath, k_WeaponDefs[WEAPON_SHIELD].scale);

    // 左腕武器（バレル系ならモデルとインスタンスをロード）
    g_LeftWeaponIdx = WEAPON_SHIELD;
    g_pLeftBarrelModel = nullptr;
    g_pLeftWeapon      = nullptr;

    // g_PlayerPosition.y からヘッド頂部までの実距離を計算
    // body は PLAYER_HEIGHT_OFFSET 上に描画、head は body 頂面に積まれる
    {
        const XMFLOAT3 bodyOrigin = { 0.0f, PLAYER_HEIGHT_OFFSET, 0.0f };
        const AABB bodyAABB  = Player_GetBodyFrameBox(bodyOrigin);
        AABB headLocal = ModelGetAABB(g_pHeadModel, { 0.0f, 0.0f, 0.0f });
        headLocal.min.y = g_MountHeadNeck.y;   // 頭は首（取り付け点）で胴体に載る
        // head の Y 原点 = bodyAABB.max.y - headLocal.min.y
        // head の世界頂点 = headOriginY + headLocal.max.y
        const float headOriginY = bodyAABB.max.y - headLocal.min.y;
        s_PlayerTopOffset = headOriginY + headLocal.max.y;  // g_PlayerPosition.y = 0 基準
    }

    // OBBをボディモデルのAABBから自動計算する
    {
        AABB local = ModelGetAABB(g_pPlayerModel, { 0.0f, 0.0f, 0.0f });
        float ex = (local.max.x - local.min.x) * 0.5f;
        float ey = (local.max.y - local.min.y) * 0.5f;
        float ez = (local.max.z - local.min.z) * 0.5f;
        float cx = (local.max.x + local.min.x) * 0.5f;
        float cy = (local.max.y + local.min.y) * 0.5f;
        float cz = (local.max.z + local.min.z) * 0.5f;
        // angle+180 + rotYawFix(+90) = 実質 RotY(-90): model(x,y,z) -> OBB(z, y, -x)
        // OBB front(z) = model local -X,  OBB right(x) = model local +Z
        g_PlayerModelHalfExtents = { ez, ey, ex };   // 半径は絶対値なので変わらず
        g_PlayerModelCenterOffset = { cz, cy, -cx }; // 符号が前後反転
    }



    // スラスターエミッターの位置をスラスターモデルの底面から自動計算する
    {
        const AABB bodyLocal = ModelGetAABB(g_pPlayerModel, { 0.0f, 0.0f, 0.0f });
        const AABB thrusterLocal = ModelGetAABB(g_pThrusterModel, { 0.0f, 0.0f, 0.0f });

        // スラスター中心 Y（g_PlayerPosition.y 基準）
        // スラスター原点 = PLAYER_HEIGHT_OFFSET + bodyLocal.min.y - thrusterLocal.max.y - 0.01f
        // スラスター中心 = 原点 + (max.y + min.y) / 2
        const float thrusterOriginY = PLAYER_HEIGHT_OFFSET
            + g_MountBodyLegs.y   // 胴体の脚の取り付け点
            - g_MountLegsTop.y    // 脚部の上面
            - 0.01f;
        const float thrusterCenterY = thrusterOriginY
            + (thrusterLocal.max.y + thrusterLocal.min.y) * 0.5f;

        g_ThrusterOffsetLocal = { 0.0f, thrusterCenterY, 0.0f };
    }

    //--------------------------------------------------------------------------
    // 武器システム初期化（再初期化の場合は既存リソースを先に解放）
    //--------------------------------------------------------------------------
    if (g_pBeamWeapon) { g_pBeamWeapon->Finalize(); delete g_pBeamWeapon; g_pBeamWeapon = nullptr; }
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_NormalWeapons[i]) { g_NormalWeapons[i]->Finalize(); delete g_NormalWeapons[i]; g_NormalWeapons[i] = nullptr; }
    }
    if (g_pLeftWeapon) { g_pLeftWeapon->Finalize(); delete g_pLeftWeapon; g_pLeftWeapon = nullptr; }

    // ビーム（固定）
    g_pBeamWeapon = new WeaponBeam();
    g_pBeamWeapon->Initialize();

    // 通常スロット（WeaponID を添字にして生成。SHIELD の枠は nullptr のまま）
    g_NormalWeaponIdx = 0;
    g_NormalWeapons[WEAPON_MACHINEGUN]   = new WeaponNormal();
    g_NormalWeapons[WEAPON_SHOTGUN]      = new WeaponShotgun();
    g_NormalWeapons[WEAPON_MISSILE]      = new WeaponMissile();
    g_NormalWeapons[WEAPON_MULTIMISSILE] = new WeaponMultiMissile();
    g_NormalWeapons[WEAPON_TRIPLEGUN]    = new WeaponTripleGun();
    g_NormalWeapons[WEAPON_MELEE]        = new WeaponMelee();
    g_NormalWeapons[WEAPON_RAILGUN]      = new WeaponRailgun();
    g_NormalWeapons[WEAPON_GATLING]      = new WeaponGatling();
    g_NormalWeapons[WEAPON_GRENADE]      = new WeaponGrenade();
    g_NormalWeapons[WEAPON_BURSTRIFLE]   = new WeaponBurstRifle();
    g_NormalWeapons[WEAPON_SPREADLASER]  = new WeaponSpreadLaser();
    for (int i = 0; i < WEAPON_COUNT; ++i)
        if (g_NormalWeapons[i]) g_NormalWeapons[i]->Initialize();

    //--------------------------------------------------------------------------
    // SE読み込み（通常スロット切り替えSEのみ。射撃SEは各武器クラスが管理）
    //--------------------------------------------------------------------------
    g_PlayerModeSwitchToNormalSE = LoadAudioWithVolume("resource/sound/mode_switch_normal.wav", 0.5f);
    g_SeShieldDeploy  = LoadAudio("resource/Sound/shield_deploy.wav");
    g_SeShieldRetract = LoadAudio("resource/Sound/shield_retract.wav");
    g_SeBoost         = LoadAudioWithVolume("resource/Sound/Boost2.wav", 0.40f);
    g_SeDash          = LoadAudioWithVolume("resource/Sound/dash_whoosh.wav", 0.90f); // パンチの風切り音（スローモーション）

    PadLogger_Initialize();

    Shield_Initialize();

    //--------------------------------------------------------------------------
    // パーティクル初期化（スラスター）
    //--------------------------------------------------------------------------
    g_PlayerParticleTexID = Texture_Load(L"resource/texture/effect000.jpg");

    XMVECTOR playerVec = XMLoadFloat3(&g_PlayerPosition);

    g_PlayerThrusterEmitter = new ThrusterEmitter(playerVec, 1024, true);
    g_PlayerThrusterEmitter->SetParticleTextureId(g_PlayerParticleTexID);

    // 見た目パラメータ（ここを調整して表現を作る）
    g_PlayerThrusterEmitter->SetScaleRange(0.001f, 0.11f);       // パーティクルのスケール範囲（最小, 最大）
    g_PlayerThrusterEmitter->SetSpeedRange(1.2f, 2.0f);         // パーティクルの速度範囲（最小, 最大）
    g_PlayerThrusterEmitter->SetLifeRange(0.25f, 0.42f);        // パーティクルの寿命範囲（最小, 最大）秒
    g_PlayerThrusterEmitter->SetConeAngleDeg(26.0f);            // 放出コーン角度（度）
    g_PlayerThrusterEmitter->SetAspectRatio(3.0f);              // 横長比率（幅/高さ）
    g_PlayerThrusterEmitter->SetColor({ 1.0f, 0.5f, 2.5f, 1.0f }); // パーティクルの色（R,G,B,A）
    g_PlayerThrusterEmitter->SetUVRect({ 0.0f, 0.0f, 80.0f, 80.0f }); // UV矩形（x, y, 幅, 高さ）
    g_PlayerThrusterEmitter->SetLocalOffset(g_ThrusterOffsetLocal); // エミッターのローカルオフセット座標

    // 近接トレイル（刃先の軌跡）。色はスラスターと同じ。
    g_MeleeTrailR.Initialize(24, 0.18f, 0.18f, MELEE_TRAIL_COLOR);
    g_MeleeTrailL.Initialize(24, 0.18f, 0.18f, MELEE_TRAIL_COLOR);
}

//==============================================================================
// ポーズ時SE停止
//==============================================================================
void Player_OnPause()
{
    // ループ再生中のブーストSEを止める（ポーズ解除後は移動入力で自動再開）
    if (IsAudioPlaying(g_SeBoost))
        StopAudio(g_SeBoost);
}

// 終了
//==============================================================================
void Player_Finalize() // プレイヤーの終了処理（モデル解放・スラスター破棄・SE解放）
{
    ModelRelease(g_pPlayerModel);
    g_pPlayerModel = nullptr;

    ModelRelease(g_pThrusterModel);
    g_pThrusterModel = nullptr;

    ModelRelease(g_pHeadModel);
    g_pHeadModel = nullptr;

    ModelRelease(g_pBarrelModel);
    g_pBarrelModel = nullptr;
    ModelRelease(g_pBarrelEdgeModel);
    g_pBarrelEdgeModel = nullptr;

    ModelRelease(g_pShieldModel);
    g_pShieldModel = nullptr;

    ModelRelease(g_pLeftBarrelModel);
    g_pLeftBarrelModel = nullptr;
    ModelRelease(g_pLeftBarrelEdgeModel);
    g_pLeftBarrelEdgeModel = nullptr;
    if (g_pLeftWeapon) { g_pLeftWeapon->Finalize(); delete g_pLeftWeapon; g_pLeftWeapon = nullptr; }

    if (g_PlayerThrusterEmitter)
    {
        delete g_PlayerThrusterEmitter;
        g_PlayerThrusterEmitter = nullptr;
    }
    if (g_PlayerSmokeEmitter)
    {
        delete g_PlayerSmokeEmitter;
        g_PlayerSmokeEmitter = nullptr;
    }

    g_MeleeTrailR.Finalize();
    g_MeleeTrailL.Finalize();

    Texture_Release(g_PlayerParticleTexID);
    g_PlayerParticleTexID = -1;

    //--------------------------------------------------------------------------
    // 武器システム解放
    //--------------------------------------------------------------------------
    if (g_pBeamWeapon)
    {
        g_pBeamWeapon->Finalize();
        delete g_pBeamWeapon;
        g_pBeamWeapon = nullptr;
    }
    for (int i = 0; i < WEAPON_COUNT; ++i)
    {
        if (g_NormalWeapons[i])
        {
            g_NormalWeapons[i]->Finalize();
            delete g_NormalWeapons[i];
            g_NormalWeapons[i] = nullptr;
        }
    }

    Shield_Finalize();

    //--------------------------------------------------------------------------
    // SE解放（通常スロット切り替えSEのみ）
    //--------------------------------------------------------------------------
    UnloadAudio(g_PlayerModeSwitchToNormalSE);
    g_PlayerModeSwitchToNormalSE = -1;
    UnloadAudio(g_SeShieldDeploy);  g_SeShieldDeploy  = -1;
    UnloadAudio(g_SeShieldRetract); g_SeShieldRetract = -1;
    StopAudio(g_SeBoost);
    UnloadAudio(g_SeBoost);         g_SeBoost         = -1;
    UnloadAudio(g_SeDash);          g_SeDash          = -1;
}

//==============================================================================
// 更新
//==============================================================================
void Player_Update(double elapsed_time)
{
    if (elapsed_time > (1.0 / 30.0)) elapsed_time = (1.0 / 30.0);

    if (g_InvincibleTimer > 0.0)
    {
        g_InvincibleTimer -= elapsed_time;
        if (g_InvincibleTimer < 0.0) g_InvincibleTimer = 0.0;
    }

    if (g_PlayerEnable && KeyLogger_IsTrigger(KK_P))
    {
        g_PlayerPosition = g_PlayerStartPosition;
        g_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
        g_IsJump = false;
        g_PlayerPosition.y += 0.002f;
    }

    XMVECTOR position = XMLoadFloat3(&g_PlayerPosition);
    XMVECTOR velocity = XMLoadFloat3(&g_PlayerVelocity);
    XMVECTOR gravityVelocity = XMVectorZero();

    const bool padJump     = PadLogger_IsTrigger(PAD_A);
    const bool padJumpHold = PadLogger_IsPressed(PAD_A);
    const bool kbJumpHold  = KeyLogger_IsPressed(KK_SPACE);

    // ジャンプ（トリガー）
    if (g_PlayerEnable && (KeyLogger_IsTrigger(KK_SPACE) || padJump) && !g_IsJump)
    {
        velocity += XMVECTOR{ 0.0f, 10.0f, 0.0f, 0.0f };
        g_IsJump = true;
    }

    // 重力
    static constexpr float GRAVITY        = 9.8f * 3.0f;
    static constexpr float FLIGHT_COST    = 150.0f;   // エネルギー消費量 / 秒
    XMVECTOR gravityDir = XMVECTOR{ 0.0f, -1.0f, 0.0f, 0.0f };
    velocity += gravityDir * GRAVITY * static_cast<float>(elapsed_time);

    // ホールドで飛行（重力を相殺＋エネルギー消費）
    if (g_PlayerEnable && g_IsJump && (kbJumpHold || padJumpHold))
    {
        const float beamEnergy = g_pBeamWeapon ? g_pBeamWeapon->GetEnergy() : 0.0f;
        if (beamEnergy > 0.0f)
        {
            // 重力を打ち消す上昇力を加える
            velocity += XMVectorSet(0.0f, GRAVITY * static_cast<float>(elapsed_time), 0.0f, 0.0f);
            // エネルギー消費（0未満にはしない）
            const float cost = FLIGHT_COST * static_cast<float>(elapsed_time);
            if (g_pBeamWeapon) g_pBeamWeapon->AddEnergy(-std::min(cost, beamEnergy));
        }
    }

    gravityVelocity = velocity * static_cast<float>(elapsed_time);

    {
        XMVECTOR posY = position + XMVectorSet(0.0f, XMVectorGetY(gravityVelocity), 0.0f, 0.0f);


        XMFLOAT3 tempPos;
        XMStoreFloat3(&tempPos, posY);

        AABB playerAABB =
        {
            {
                tempPos.x - PLAYER_HALF_WIDTH_X,
                tempPos.y,
                tempPos.z - PLAYER_HALF_WIDTH_Z
            },
            {
                tempPos.x + PLAYER_HALF_WIDTH_X,
                tempPos.y + s_PlayerTopOffset,   // ヘッド頂部まで
                tempPos.z + PLAYER_HALF_WIDTH_Z
            }
        };

        const float velY = XMVectorGetY(velocity);

        // 重なっている床のうち「足元から段差の高さ（STEP_UP）以内」で最も高いものに乗る。
        //（以前は最初に見つかった床で打ち切っていたため、階段では低い段が選ばれて
        //  次の段に足がめり込んだまま引っかかることがあった）
        // 足より大きく上にある床は横から入り込んだだけなので乗らない（壁が押し戻す）
        constexpr float STEP_UP = 0.45f;
        const float feetRef = std::max(XMVectorGetY(position), tempPos.y);   // 落下中は移動前の足の高さ
        float landY    = -FLT_MAX;
        float ceilingY =  FLT_MAX;

        for (int i = 0; i < Map_GetObjectsCount(); i++)
        {
            const MapObject* mo = Map_GetObject(i);
            if (!mo) continue;

            const bool isFloor   = (mo->KindId == 0 || mo->KindId == 1); // KIND_GROUND / KIND_FLOOR
            const bool isCeiling = (mo->KindId == 4);                    // KIND_CEILING
            if (!isFloor && !isCeiling) continue;
            if (!Collision_IsOverLapAABB(playerAABB, mo->Aabb)) continue;

            // 上昇中でも、足が床の底より上にあるなら「下から突き上げた」のではなく
            // 「縁から乗り込んだ」状態なので着地として扱う。
            //（ブロックステージで、上昇しながら屋上の縁に乗るときに下へ弾かれるのを防ぐ）
            const bool hitFromBelow = (velY > 0.0f && tempPos.y < mo->Aabb.min.y);

            if (isCeiling || hitFromBelow)
            {
                ceilingY = std::min(ceilingY, mo->Aabb.min.y);
            }
            else if (mo->Aabb.max.y <= feetRef + STEP_UP)
            {
                landY = std::max(landY, mo->Aabb.max.y);
            }
        }

        if (landY > -FLT_MAX)
        {
            // 床に着地（段差はここで上る）
            posY = XMVectorSetY(posY, landY);
            velocity *= XMVECTOR{ 1.0f, 0.0f, 1.0f, 1.0f };
            g_IsJump = false;
        }
        else if (ceilingY < FLT_MAX)
        {
            // 天井オブジェクト、または床の底に上昇中に当たった
            posY = XMVectorSetY(posY, ceilingY - s_PlayerTopOffset);
            velocity = XMVectorSetY(velocity, 0.0f);
        }

        position = XMVectorSetY(position, XMVectorGetY(posY));
    }

    // 入力無効中（ボスイントロ等）：重力・位置更新だけ済ませて終了
    if (!g_PlayerEnable)
    {
        velocity += -velocity * static_cast<float>(4.0f * elapsed_time);
        MoveWithSubSteps(&position, &velocity, static_cast<float>(elapsed_time));
        XMStoreFloat3(&g_PlayerPosition, position);
        XMStoreFloat3(&g_PlayerVelocity, velocity);

        // 入力無効中はブーストSEを止める
        if (g_SeBoost >= 0 && IsAudioPlaying(g_SeBoost))
            StopAudio(g_SeBoost);

        return;
    }

    XMVECTOR moveDir = XMVectorZero();

    XMFLOAT3 camFront = Player_Camera_GetFront();

    XMVECTOR front = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), front));

    if (KeyLogger_IsPressed(KK_W)) moveDir += front;
    if (KeyLogger_IsPressed(KK_S)) moveDir -= front;
    if (KeyLogger_IsPressed(KK_D)) moveDir += right;
    if (KeyLogger_IsPressed(KK_A)) moveDir -= right;

    float lx = 0.0f, ly = 0.0f;
    PadLogger_GetLeftStick(&lx, &ly);

    moveDir += front * ly;
    moveDir += right * lx;

    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0f)
    {
        moveDir = XMVector3Normalize(moveDir);
        velocity += moveDir * static_cast<float>(2000.0 / 90.0 * elapsed_time) * g_PlayerSpeedMultiplier * g_FrameSpeedMul;
    }

    //--------------------------------------------------------------------------
    // ダッシュ（Shift / PAD_B）：エネルギーを消費して短距離バースト移動
    //   ・移動入力があればその方向、なければ正面方向へ加速
    //   ・シールド装備中はダッシュ中の接触で敵にダメージ（Player_IsShieldDashing）
    //--------------------------------------------------------------------------
    if (g_DashCooldown > 0.0f) g_DashCooldown -= static_cast<float>(elapsed_time);
    if (g_DashTimer    > 0.0f) g_DashTimer    -= static_cast<float>(elapsed_time);

    const bool dashInput = KeyLogger_IsTrigger(KK_LEFTSHIFT) || PadLogger_IsTrigger(PAD_B);
    if (dashInput && g_DashCooldown <= 0.0f)
    {
        const float energy = g_pBeamWeapon ? g_pBeamWeapon->GetEnergy() : 0.0f;
        if (energy >= DASH_ENERGY_COST)
        {
            // ダッシュ方向：移動入力があればその方向、なければ正面（水平化）
            XMVECTOR dashDir = moveDir;
            if (XMVectorGetX(XMVector3LengthSq(dashDir)) < 0.0001f)
                dashDir = XMVector3Normalize(XMVectorSet(g_PlayerFront.x, 0.0f, g_PlayerFront.z, 0.0f));

            velocity += dashDir * DASH_SPEED;
            if (g_pBeamWeapon) g_pBeamWeapon->AddEnergy(-DASH_ENERGY_COST);

            g_DashTimer    = DASH_DURATION;
            g_DashCooldown = DASH_COOLDOWN;

            // ダッシュ音（パンチの風切り音を1回鳴らす）
            if (g_SeDash >= 0)
                PlayAudio(g_SeDash, false);
        }
    }

    // ── ブーストSE ──────────────────────────────────────────
    // 移動中は1秒ごとに再生。空中上昇中は音量を最大 0.80f まで上げる
    if (g_SeBoost >= 0)
    {
        const bool isMoving = XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f;
        if (isMoving)
        {
            // Y速度に応じて音量をスケール（地上 0.30f / 上昇ピーク 0.80f）
            float boostVol = 0.30f;
            if (g_IsJump)
            {
                const float velY = XMVectorGetY(velocity);
                if (velY > 0.0f)
                {
                    const float t = std::min(velY / 10.0f, 1.0f);
                    boostVol = 0.30f + 0.50f * t;  // 0.30 → 0.80
                }
            }
            SetAudioVolume(g_SeBoost, boostVol);

            // タイマーが0以下になったら再生（1秒ごとに鳴らす）
            g_BoostLoopTimer -= elapsed_time;
            if (g_BoostLoopTimer <= 0.0)
            {
                PlayAudio(g_SeBoost, false);  // 1回再生（ループなし）
                g_BoostLoopTimer = BOOST_LOOP_INTERVAL;
            }
        }
        else
        {
            // 止まったらタイマーもリセット（次に動き始めたらすぐ鳴る）
            if (IsAudioPlaying(g_SeBoost))
                StopAudio(g_SeBoost);
            g_BoostLoopTimer = 0.0;
        }
    }
    // ─────────────────────────────────────────────────────────

    // F10 キーでボディの向きモード切り替え（旧：K → スペクテイターカメラに使用）
    if (KeyLogger_IsTrigger(KK_F10))
    {
        g_PlayerBodyFollowCamera = !g_PlayerBodyFollowCamera;
    }

    // ボディの向き更新
    if (g_PlayerBodyFollowCamera)
    {
        // カメラ XZ 方向を向く（スラスターが移動方向に独立回転）
        XMStoreFloat3(&g_PlayerFront, front);
    }
    else if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0001f)
    {
        // 移動方向を向く
        XMStoreFloat3(&g_PlayerFront, moveDir);
    }

    velocity += -velocity * static_cast<float>(4.0f * elapsed_time);

    MoveWithSubSteps(&position, &velocity, static_cast<float>(elapsed_time));

    XMStoreFloat3(&g_PlayerPosition, position);
    XMStoreFloat3(&g_PlayerVelocity, velocity);

    XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
    XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), playerFront));

    float localX = XMVectorGetX(XMVector3Dot(moveDir, playerRight));
    float localZ = XMVectorGetX(XMVector3Dot(moveDir, playerFront));

    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0001f)
    {
        g_ThrusterLocalYaw = atan2f(-localX, -localZ);
    }

    //--------------------------------------------------------------------------
    // シールド更新（装備しているアームのボタンで展開）
    //   右腕シールド → RB  左腕シールド → LB  両腕 → どちらか
    //--------------------------------------------------------------------------
    {
        bool shieldPressed = KeyLogger_IsPressed(KK_G);  // キーボード G は常に有効
        if (g_RightWeaponIdx == WEAPON_SHIELD)
            shieldPressed |= PadLogger_IsPressed(PAD_RIGHT_SHOULDER)
                          || Player_Camera_IsMouseRightPressed();
        if (g_LeftWeaponIdx == WEAPON_SHIELD)
            shieldPressed |= PadLogger_IsPressed(PAD_LEFT_SHOULDER)
                          || Player_Camera_IsMouseLeftPressed();

        Shield_Update(elapsed_time, shieldPressed);

        // 展開・収納SE（立ち上がり・立ち下がりで1回だけ）
        static bool s_PrevLtPressed = false;
        if (shieldPressed && !s_PrevLtPressed) PlayAudio(g_SeShieldDeploy,  false);
        if (!shieldPressed && s_PrevLtPressed) PlayAudio(g_SeShieldRetract, false);
        s_PrevLtPressed = shieldPressed;

        // 構えアニメーション（ブレンド値を時間で補間）
        const float target = shieldPressed ? 1.0f : 0.0f;
        const float step   = SHIELD_GUARD_BLEND_SPEED * static_cast<float>(elapsed_time);
        const float diff   = target - g_ShieldGuardBlend;
        if (fabsf(diff) <= step)
            g_ShieldGuardBlend = target;  // 目標に近ければスナップ（震え防止）
        else
            g_ShieldGuardBlend += (diff > 0.0f ? 1.0f : -1.0f) * step;
    }

    //--------------------------------------------------------------------------
    // 全武器を毎フレーム更新（クールダウンを常に正確に保持する）
    //--------------------------------------------------------------------------
    // 近接：振り中は武器モデルの刃先ワールド座標を武器へ渡す（当たりを刃に載せる）。
    // Update より前に渡すことで、この振りのヒット判定が現在フレームの刃先で出る。
    if (g_NormalWeapons[g_NormalWeaponIdx] && g_NormalWeapons[g_NormalWeaponIdx]->IsSwinging() && g_pBarrelModel)
    {
        const AABB bl = ModelGetAABB(g_pBarrelModel, { 0.0f, 0.0f, 0.0f });
        const XMMATRIX bw = Player_GetBarrelWorldMatrix();
        XMFLOAT3 tip;
        XMStoreFloat3(&tip, XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, bl.min.z, 1.0f), bw));
        g_NormalWeapons[g_NormalWeaponIdx]->SetBladeWorldPos(tip);
        g_MeleeTrailR.Update(elapsed_time, tip);   // 刃先トレイル
    }
    else
    {
        g_MeleeTrailR.Clear();   // 非振り時は軌跡を消す（次の振りが繋がらないように）
    }
    if (g_pLeftWeapon && g_pLeftWeapon->IsSwinging() && g_pLeftBarrelModel)
    {
        const AABB bl = ModelGetAABB(g_pLeftBarrelModel, { 0.0f, 0.0f, 0.0f });
        const XMMATRIX bw = Player_GetLeftBarrelWorldMatrix();
        XMFLOAT3 tip;
        XMStoreFloat3(&tip, XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, bl.min.z, 1.0f), bw));
        g_pLeftWeapon->SetBladeWorldPos(tip);
        g_MeleeTrailL.Update(elapsed_time, tip);
    }
    else
    {
        g_MeleeTrailL.Clear();
    }

    if (g_pBeamWeapon)
        g_pBeamWeapon->Update(elapsed_time);
    if (g_NormalWeapons[g_NormalWeaponIdx])
        g_NormalWeapons[g_NormalWeaponIdx]->Update(elapsed_time);
    if (g_pLeftWeapon)
        g_pLeftWeapon->Update(elapsed_time);

    // 発射リコイルを0へ復帰（指数減衰）
    {
        const float t = std::min(1.0f, RECOIL_RETURN_SPEED * static_cast<float>(elapsed_time));
        g_RightBarrelRecoil -= g_RightBarrelRecoil * t;
        g_LeftBarrelRecoil  -= g_LeftBarrelRecoil  * t;
    }

    //--------------------------------------------------------------------------
    // 発射ボタン判定
    //   RB / マウス右          = 右腕
    //   LB / マウス左          = 左腕
    //   RB+LB / マウス左右同時 = ビーム（両腕シールド時のみ）
    //--------------------------------------------------------------------------
    const bool padRightFire  = PadLogger_IsPressed(PAD_RIGHT_SHOULDER);
    const bool padLeftFire   = PadLogger_IsPressed(PAD_LEFT_SHOULDER);
    const bool mouseRight    = Player_Camera_IsMouseRightPressed();
    const bool mouseLeft     = Player_Camera_IsMouseLeftPressed();
    const bool keyAttack     = KeyLogger_IsPressed(KK_F);

    const bool bothShield = (g_RightWeaponIdx == WEAPON_SHIELD && g_LeftWeaponIdx == WEAPON_SHIELD);
    const bool beamAttack = bothShield &&
        ((padRightFire && padLeftFire) || (mouseLeft && mouseRight)); // RB+LB同時 / 左右クリック同時

    const bool rightFire = keyAttack || mouseRight || padRightFire;
    const bool leftFire  = mouseLeft || padLeftFire;

    //--------------------------------------------------------------------------
    // 右腕発射
    //--------------------------------------------------------------------------
    if (rightFire)
    {
        XMFLOAT3 muzzlePos, aimDir;

        if (g_pBarrelModel)
        {
            const AABB     barrelLocal = ModelGetAABB(g_pBarrelModel, { 0.0f, 0.0f, 0.0f });
            const XMMATRIX barrelWorld = Player_GetBarrelWorldMatrix();

            const XMVECTOR muzzleLocal = XMVectorSet(0.0f, 0.0f, barrelLocal.min.z, 1.0f);
            XMStoreFloat3(&muzzlePos, XMVector3TransformCoord(muzzleLocal, barrelWorld));
            XMStoreFloat3(&aimDir,
                XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), barrelWorld)));
        }
        else
        {
            XMVECTOR vMuzzle = XMLoadFloat3(&g_PlayerPosition) + XMVectorSet(0.0f, 0.25f, 0.0f, 0.0f);
            XMStoreFloat3(&muzzlePos, vMuzzle);
            XMFLOAT3 cf = Player_Camera_GetFront();
            XMFLOAT3 lockOnPos;
            if (Game_GetLockOnWorldPos(&lockOnPos))
                XMStoreFloat3(&aimDir, XMVector3Normalize(XMLoadFloat3(&lockOnPos) - vMuzzle));
            else
                XMStoreFloat3(&aimDir, XMVector3Normalize(XMVectorSet(cf.x, 0.0f, cf.z, 0.0f)));
        }

        if (g_RightWeaponIdx != WEAPON_SHIELD && g_NormalWeapons[g_NormalWeaponIdx])
        {
            // 実際に発射できたフレームだけリコイルをキックする
            if (g_NormalWeapons[g_NormalWeaponIdx]->TryFire(muzzlePos, aimDir, g_PlayerDamageMultiplier * g_FrameAttackMul))
                g_RightBarrelRecoil = RECOIL_KICK;
        }
    }

    //--------------------------------------------------------------------------
    // 左腕発射（バレル系選択時のみ）
    //--------------------------------------------------------------------------
    if (leftFire && g_pLeftWeapon && g_pLeftBarrelModel)
    {
        const AABB     leftLocal = ModelGetAABB(g_pLeftBarrelModel, { 0.0f, 0.0f, 0.0f });
        const XMMATRIX leftWorld = Player_GetLeftBarrelWorldMatrix();

        XMFLOAT3 leftMuzzlePos, leftAimDir;
        XMStoreFloat3(&leftMuzzlePos,
            XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, leftLocal.min.z, 1.0f), leftWorld));
        XMStoreFloat3(&leftAimDir,
            XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), leftWorld)));

        if (g_pLeftWeapon->TryFire(leftMuzzlePos, leftAimDir, g_PlayerDamageMultiplier * g_FrameAttackMul))
            g_LeftBarrelRecoil = RECOIL_KICK;
    }

    //--------------------------------------------------------------------------
    // ビーム発射（RB+LB / マウス左右同時）：胴体中心から発射
    //--------------------------------------------------------------------------
    if (beamAttack && g_pBeamWeapon)
    {
        const XMFLOAT3 beamOrigin = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };
        const XMFLOAT3 camFrontBeam = Player_Camera_GetFront();
        XMFLOAT3 beamDir;
        XMStoreFloat3(&beamDir, XMVector3Normalize(
            XMVectorSet(camFrontBeam.x, 0.0f, camFrontBeam.z, 0.0f)));

        g_pBeamWeapon->TryFire(beamOrigin, beamDir, g_PlayerDamageMultiplier * g_FrameAttackMul);
    }

    //--------------------------------------------------------------------------
    // スラスターの外観をスピード倍率に応じて変化させる
    //--------------------------------------------------------------------------
    {
        // スピード倍率(2.0〜3.0)を0〜1に正規化した補間係数
        float t = std::clamp((g_PlayerSpeedMultiplier - 2.0f) / 1.0f, 0.0f, 1.0f);

        // 色：緑(t=0.0) → 青(t=0.33) → 紫(t=0.66) → 赤(t=1.0)
        float r, g, b;
        if (t < 0.33f)
        {
            float s = t / 0.33f;
            r = 1.5f * s;
            g = 1.0f;
            b = 2.5f;
        }
        else if (t < 0.66f)
        {
            float s = (t - 0.33f) / 0.33f;
            r = 2.5f;
            g = 1.0f;
            b = 2.5f * (1.0f - s);
        }
        else
        {
            float s = (t - 0.66f) / 0.34f;
            r = 1.0f;
            g = 2.5f * (1.0f - s);
            b = 2.5f * s;
        }
        g_PlayerThrusterEmitter->SetColor({ r, g, b, 1.0f });

        // スラスター置き去り対策：高速時はパーティクルを短命・高速にする
        float speedT = std::clamp((g_PlayerSpeedMultiplier - 2.0f) / 1.0f, 0.0f, 1.0f);
        float lifeMin = 0.18f * (1.0f - speedT * 0.7f);  // 速いほど短命
        float lifeMax = 0.26f * (1.0f - speedT * 0.7f);
        g_PlayerThrusterEmitter->SetLifeRange(lifeMin, lifeMax);

        float speedMin = 1.2f + speedT * 2.0f;  // 速いほど後方へ強く吹き出す
        float speedMax = 2.0f + speedT * 3.0f;
        g_PlayerThrusterEmitter->SetSpeedRange(speedMin, speedMax);
    }

    if (g_PlayerThrusterEmitter)
    {
        bool isMoveInput = false;

        if (KeyLogger_IsPressed(KK_W)) isMoveInput = true;
        if (KeyLogger_IsPressed(KK_S)) isMoveInput = true;
        if (KeyLogger_IsPressed(KK_A)) isMoveInput = true;
        if (KeyLogger_IsPressed(KK_D)) isMoveInput = true;

        float lx2 = 0.0f, ly2 = 0.0f;
        PadLogger_GetLeftStick(&lx2, &ly2);
        if (fabsf(lx2) > 0.1f || fabsf(ly2) > 0.1f)
            isMoveInput = true;

        g_PlayerThrusterEmitter->Emmit(isMoveInput);

        if (g_pThrusterModel)
        {
            const AABB thrusterLocal = ModelGetAABB(g_pThrusterModel, { 0.0f, 0.0f, 0.0f });
            const XMMATRIX thrusterWorld = Player_GetThrusterWorldMatrix();

            const float rearX = (thrusterLocal.min.x + thrusterLocal.max.x) * 0.5f;
            const float rearY = (thrusterLocal.min.y + thrusterLocal.max.y) * 0.5f;
            const float rearZ = (thrusterLocal.min.z + thrusterLocal.max.z) * 0.5f;

            const XMVECTOR localRearPos = XMVectorSet(rearX, rearY, rearZ, 1.0f);
            const XMVECTOR worldRearPos = XMVector3TransformCoord(localRearPos, thrusterWorld);

            const XMVECTOR localRearDir = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
            const XMVECTOR worldRearDir = XMVector3Normalize(
                XMVector3TransformNormal(localRearDir, thrusterWorld)
            );

            XMFLOAT3 emitDir;
            XMStoreFloat3(&emitDir, worldRearDir);

            g_PlayerThrusterEmitter->SetPosition(worldRearPos);
            g_PlayerThrusterEmitter->SetWorldDirection(emitDir);
            g_PlayerThrusterEmitter->SetWorldUp({ 0.0f, 1.0f, 0.0f });
        }

        g_PlayerThrusterEmitter->Update(elapsed_time);
    }

    //--------------------------------------------------------------------------
    // ダメージ煙：HPが40%以下のとき体の中心から上へ立ち上らせる
    //--------------------------------------------------------------------------
    if (g_PlayerSmokeEmitter)
    {
        const bool hpLow = (g_PlayerHP <= (g_PlayerMaxHP * 4) / 10);
        XMVECTOR smokePos = XMLoadFloat3(&g_PlayerPosition)
                          + XMVectorSet(0.0f, PLAYER_HEIGHT_OFFSET, 0.0f, 0.0f);
        g_PlayerSmokeEmitter->SetPosition(smokePos);
        g_PlayerSmokeEmitter->SetWorldDirection({ 0.0f, 1.0f, 0.0f }); // 上へ
        g_PlayerSmokeEmitter->SetWorldUp({ 0.0f, 0.0f, 1.0f });
        g_PlayerSmokeEmitter->Emmit(hpLow);
        g_PlayerSmokeEmitter->Update(elapsed_time);
    }
}

void Player_Draw() // プレイヤー描画（無敵点滅の考慮、モデル描画、スラスター描画）
{
    // ※g_PlayerEnable が false でも描画は行う（BossIntro 中は入力のみロック）

    if (g_InvincibleTimer > 0.0)
    {
        int cycle = static_cast<int>(g_InvincibleTimer * 10.0);
        if (cycle % 2 == 0) return;
    }

    //--------------------------------------------------------------------------
    // ワールド行列を先に計算（法線パスと通常描画で共用）
    //--------------------------------------------------------------------------
    float angle = -atan2f(g_PlayerFront.z, g_PlayerFront.x) + XMConvertToRadians(180.0f);
    const float pitchDeg = 0.0f;
    const float rollDeg = 0.0f;

    XMMATRIX rotFix = XMMatrixRotationZ(XMConvertToRadians(rollDeg)) *
        XMMatrixRotationX(XMConvertToRadians(pitchDeg));
    XMMATRIX rotYawFix = XMMatrixRotationY(XMConvertToRadians(90.0f));
    XMMATRIX rotY = XMMatrixRotationY(angle);
    XMMATRIX rot = rotFix * rotY * rotYawFix;

    const float heightOffset = PLAYER_HEIGHT_OFFSET;

    XMMATRIX t = XMMatrixTranslation(
        g_PlayerPosition.x,
        g_PlayerPosition.y + heightOffset,
        g_PlayerPosition.z
    );

    XMMATRIX world = rot * t;

    // 各パーツのワールド行列を事前取得（法線パス・通常描画で共用）
    const XMMATRIX headWorld = g_pHeadModel ? Player_GetHeadWorldMatrix() : XMMatrixIdentity();
    const XMMATRIX thrusterWorld = g_pThrusterModel ? Player_GetThrusterWorldMatrix() : XMMatrixIdentity();
    const XMMATRIX barrelWorld      = g_pBarrelModel     ? Player_GetBarrelWorldMatrix()      : XMMatrixIdentity();
    const XMMATRIX shieldWorld      = g_pShieldModel     ? Player_GetShieldWorldMatrix()      : XMMatrixIdentity();
    const XMMATRIX rightShieldWorld = (g_pShieldModel && g_RightWeaponIdx == WEAPON_SHIELD)
                                        ? Player_GetRightShieldWorldMatrix() : XMMatrixIdentity();
    const XMMATRIX leftBarrelWorld  = g_pLeftBarrelModel ? Player_GetLeftBarrelWorldMatrix()  : XMMatrixIdentity();

    //--------------------------------------------------------------------------
    // 法線パス（エッジ検出用）
    //--------------------------------------------------------------------------
    ShaderEdge_BeginNormalPass();

    ShaderEdge_SetWorldMatrix(world);
    ModelDrawWithoutBegin(g_pPlayerModel, world);

    if (g_pHeadModel)
    {
        ShaderEdge_SetWorldMatrix(headWorld);
        ModelDrawWithoutBegin(g_pHeadModel, headWorld);
    }
    if (g_pThrusterModel)
    {
        ShaderEdge_SetWorldMatrix(thrusterWorld);
        ModelDrawWithoutBegin(g_pThrusterModel, thrusterWorld);
    }
    // 右腕：バレル or シールド
    if (g_RightWeaponIdx == WEAPON_SHIELD && g_pShieldModel)
    {
        ShaderEdge_SetWorldMatrix(rightShieldWorld);
        ModelDrawWithoutBegin(g_pShieldModel, rightShieldWorld);
    }
    else if (g_pBarrelModel)
    {
        ShaderEdge_SetWorldMatrix(barrelWorld);
        ModelDrawWithoutBegin(g_pBarrelModel, barrelWorld);
        // ※発光エッジ(BladeEdge)はアウトライン検出に含めない（後段で加算描画）
    }
    // 左腕：バレル or シールド
    if (g_pLeftBarrelModel)
    {
        ShaderEdge_SetWorldMatrix(leftBarrelWorld);
        ModelDrawWithoutBegin(g_pLeftBarrelModel, leftBarrelWorld);
    }
    else if (g_pShieldModel)
    {
        ShaderEdge_SetWorldMatrix(shieldWorld);
        ModelDrawWithoutBegin(g_pShieldModel, shieldWorld);
    }

    ShaderEdge_EndNormalPass();

    //--------------------------------------------------------------------------
    // ライティング設定
    //--------------------------------------------------------------------------
    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        100.0f,
        { 0.6f, 0.5f, 0.4f, 1.0f }
    );
    Light_SetAmbient({ 0.5f, 0.5f, 0.5f });

    //--------------------------------------------------------------------------
    // 通常描画（トゥーン）
    //--------------------------------------------------------------------------
    ModelDrawToon(g_pPlayerModel, world);

    if (g_pHeadModel)
    {
        ModelDrawToon(g_pHeadModel, headWorld);
    }
    if (g_pThrusterModel)
    {
        ModelDrawToon(g_pThrusterModel, thrusterWorld);
    }
    // 右腕：バレル or シールド
    if (g_RightWeaponIdx == WEAPON_SHIELD && g_pShieldModel)
        ModelDrawToon(g_pShieldModel, rightShieldWorld);
    else if (g_pBarrelModel)
        ModelDrawToon(g_pBarrelModel, barrelWorld);

    // 左腕：バレル or シールド
    if (g_pLeftBarrelModel)
        ModelDrawToon(g_pLeftBarrelModel, leftBarrelWorld);
    else if (g_pShieldModel)
        ModelDrawToon(g_pShieldModel, shieldWorld);

    // 近接の発光エッジ（BladeEdge）：この描画のときだけアンビエントを上げて
    // 明るく＝光っているように見せる。直後に元のアンビエントへ戻し他に影響させない。
    if (g_pBarrelEdgeModel || g_pLeftBarrelEdgeModel)
    {
        const XMFLOAT3 prevAmbient = Light_GetAmbient();
        Light_SetAmbient(EDGE_GLOW_AMBIENT);   // 発光風の明るさ
        if (g_pBarrelEdgeModel)     ModelDrawToon(g_pBarrelEdgeModel,     barrelWorld);
        if (g_pLeftBarrelEdgeModel) ModelDrawToon(g_pLeftBarrelEdgeModel, leftBarrelWorld);
        Light_SetAmbient(prevAmbient);         // 元に戻す
    }

    //--------------------------------------------------------------------------
    // スラスターパーティクル
    //--------------------------------------------------------------------------
    if (g_PlayerThrusterEmitter)
    {
        g_PlayerThrusterEmitter->Draw();
    }
    if (g_PlayerSmokeEmitter)
    {
        g_PlayerSmokeEmitter->Draw();
    }

    // 近接の刃先トレイル（加算ブレンド。不透明描画の後）
    g_MeleeTrailR.Draw();
    g_MeleeTrailL.Draw();

    //--------------------------------------------------------------------------
    // エッジを重ねる
    //--------------------------------------------------------------------------
    ShaderEdge_DrawEdge();

    //--------------------------------------------------------------------------
    // シールドドーム（半透明・エッジ後に描画）
    //--------------------------------------------------------------------------
    Shield_Draw({ g_PlayerPosition.x, g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET, g_PlayerPosition.z });

    //--------------------------------------------------------------------------
    // ミニマップ用プレイヤーマーカー（赤い四角）
    //--------------------------------------------------------------------------
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();

        ID3D11Buffer* nullCB = nullptr;
        ctx->PSSetConstantBuffers(6, 1, &nullCB);

        const float minimapY = 10.0f;

        Shader3d_Begin();
        Shader3d_SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });

        const XMMATRIX markerWorld = XMMatrixTranslation(
            g_PlayerPosition.x,
            minimapY + 1.0f,
            g_PlayerPosition.z
        );

        MeshField_DrawTile(markerWorld, Map_GetWiteTexID(), 1.0f);
        Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
}

//==============================================================================
// シャドウパス用深度描画
//==============================================================================
void Player_DrawShadow()
{
    if (!g_pPlayerModel) return;

    float angle = -atan2f(g_PlayerFront.z, g_PlayerFront.x) + XMConvertToRadians(180.0f);
    XMMATRIX rotFix = XMMatrixIdentity();
    XMMATRIX rotYawFix = XMMatrixRotationY(XMConvertToRadians(90.0f));
    XMMATRIX rotY = XMMatrixRotationY(angle);
    XMMATRIX rot = rotFix * rotY * rotYawFix;

    XMMATRIX t = XMMatrixTranslation(
        g_PlayerPosition.x,
        g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
        g_PlayerPosition.z
    );
    ShadowMap::DrawModel(g_pPlayerModel, rot * t);

    if (g_pHeadModel)
        ShadowMap::DrawModel(g_pHeadModel, Player_GetHeadWorldMatrix());
    if (g_pThrusterModel)
        ShadowMap::DrawModel(g_pThrusterModel, Player_GetThrusterWorldMatrix());
}

void Player_DrawMarker()
{
    Shader3d_Begin();

    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    ID3D11Buffer* nullCB = nullptr;
    ctx->PSSetConstantBuffers(6, 1, &nullCB);

    // 標準アルファブレンド（Map_DrawGoal と同じ）
    ID3D11BlendState* pOldBlend = nullptr;
    FLOAT oldFactor[4];
    UINT  oldMask;
    ctx->OMGetBlendState(&pOldBlend, oldFactor, &oldMask);
    Direct3D_SetBlendState(true);

    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    // プレイヤーの向きに合わせてマーカーを回転
    const float angle = -atan2f(g_PlayerFront.z, g_PlayerFront.x) + XMConvertToRadians(270.0f);

    const float minimapY = 10.0f;
    const XMMATRIX markerWorld =
        XMMatrixRotationY(angle) *
        XMMatrixTranslation(g_PlayerPosition.x, minimapY + 1.0f, g_PlayerPosition.z);

    MeshField_DrawTile(markerWorld, g_PlayerMarkerTexID, 10.0f);
    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

    ctx->OMSetBlendState(pOldBlend, oldFactor, oldMask);
    SAFE_RELEASE(pOldBlend);
}

//==============================================================================
// 当たり判定取得
//==============================================================================
OBB Player_GetOBB()
{
    const float heightOffset = 0.5f;
    XMVECTOR vFront = XMLoadFloat3(&g_PlayerFront);
    XMVECTOR vRight = XMVector3Cross(XMVectorSet(0, 1, 0, 0), vFront);
    XMVECTOR wo = vRight * g_PlayerModelCenterOffset.x + vFront * g_PlayerModelCenterOffset.z;
    XMFLOAT3 woF; XMStoreFloat3(&woF, wo);
    XMFLOAT3 center = {
        g_PlayerPosition.x + woF.x,
        g_PlayerPosition.y + heightOffset + g_PlayerModelCenterOffset.y,
        g_PlayerPosition.z + woF.z
    };
    return OBB::CreateFromFront(center, g_PlayerModelHalfExtents, g_PlayerFront);
}

OBB Player_ConvertPositionToOBB(const DirectX::XMVECTOR& position)
{
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, position);

    const float heightOffset = 0.25f;
    XMVECTOR vFront = XMLoadFloat3(&g_PlayerFront);
    XMVECTOR vRight = XMVector3Cross(XMVectorSet(0, 1, 0, 0), vFront);
    XMVECTOR wo = vRight * g_PlayerModelCenterOffset.x + vFront * g_PlayerModelCenterOffset.z;
    XMFLOAT3 woF; XMStoreFloat3(&woF, wo);
    XMFLOAT3 center = {
        pos.x + woF.x,
        pos.y + heightOffset + g_PlayerModelCenterOffset.y,
        pos.z + woF.z
    };
    return OBB::CreateFromFront(center, g_PlayerModelHalfExtents, g_PlayerFront);
}

AABB Player_GetAABB() // 現在のプレイヤー位置から AABB（軸揃え当たり判定）を生成して返す（床判定などに使用）
{
    return {
        {
            g_PlayerPosition.x - PLAYER_HALF_WIDTH_X,
            g_PlayerPosition.y,
            g_PlayerPosition.z - PLAYER_HALF_WIDTH_Z
        },
        {
            g_PlayerPosition.x + PLAYER_HALF_WIDTH_X,
            g_PlayerPosition.y + PLAYER_HEIGHT,
            g_PlayerPosition.z + PLAYER_HALF_WIDTH_Z
        }
    };
}

//==============================================================================
// ゲッター
//==============================================================================
const DirectX::XMFLOAT3& Player_GetPosition() // プレイヤー現在位置の参照を返す
{
    return g_PlayerPosition;
}

const DirectX::XMFLOAT3& Player_GetFront() // プレイヤー正面方向ベクトルの参照を返す
{
    return g_PlayerFront;
}

float Player_GetHalfWidthX() // 当たり判定のX方向半幅を返す
{
    return PLAYER_HALF_WIDTH_X;
}

float Player_GetHalfWidthZ() // 当たり判定のZ方向半幅を返す
{
    return PLAYER_HALF_WIDTH_Z;
}

float Player_GetHeight() // 当たり判定の高さを返す
{
    return PLAYER_HEIGHT;
}

bool Player_IsEnable() // プレイヤー操作/更新が有効かどうかを返す
{
    return g_PlayerEnable;
}

//==============================================================================
// セッター
//==============================================================================
void Player_SetEnable(bool enable) // プレイヤー操作/更新の有効無効を切り替える。enable=trueで有効 falseで無効
{
    g_PlayerEnable = enable;

    if (!g_PlayerEnable)
    {
        g_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
        g_IsJump = false;
    }
}

void Player_ClearParticles() // スラスターパーティクル＆トレイルを全消去（ルーム遷移時用）
{
    if (g_PlayerThrusterEmitter)
    {
        g_PlayerThrusterEmitter->ClearAll();    // パーティクル消去
        g_PlayerThrusterEmitter->ClearTrail();  // トレイル軌跡も消去
    }
    if (g_PlayerSmokeEmitter)
    {
        g_PlayerSmokeEmitter->ClearAll();
        g_PlayerSmokeEmitter->ClearTrail();
    }
}

void Player_SetPosition(const DirectX::XMFLOAT3& position, bool setAsStart) // プレイヤー位置を強制設定する。position=設定位置 setAsStart=trueで開始位置も更新
{
    g_PlayerPosition = position;

    if (setAsStart)
        g_PlayerStartPosition = position;

    g_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
    g_IsJump = false;
    g_PlayerEnable = true;
}

void Player_SetFront(const DirectX::XMFLOAT3& front)
{
    g_PlayerFront = front;
}

DirectX::XMFLOAT3* Player_GetVelocityPtr() // プレイヤー速度ベクトルのポインタを返す（外部から速度を書き換える用途）
{
    return &g_PlayerVelocity;
}

//==============================================================================
// ダメージ / HP
//==============================================================================
bool Player_TakeDamage(int damage) // ダメージ処理（無敵中は無効、HP減算、無敵時間付与、HP0で無効化）。damage=受けるダメージ量 戻り値=trueでダメージ適用
{
    if (g_InvincibleTimer > 0.0 || !g_PlayerEnable)
        return false;

    // シールド展開中：ダメージを一部 EN（ビームエネルギー）に肩代わりさせる。
    //   片手：HP 50% / EN 50%    両手：HP 25% / EN 75%
    //   EN が足りない分は HP に回す（無効化はしない）。
    const bool rightIsShield = (g_RightWeaponIdx == WEAPON_SHIELD);
    const bool leftIsShield  = (g_LeftWeaponIdx == WEAPON_SHIELD);
    if (Shield_IsActive() && (rightIsShield || leftIsShield))
    {
        const bool  dual   = rightIsShield && leftIsShield;
        const float enFrac = dual ? 0.75f : 0.50f;      // EN が肩代わりする割合

        int enPortion = static_cast<int>(damage * enFrac);
        int hpPortion = damage - enPortion;

        // EN で肩代わりできる分だけ EN を消費、足りない分は HP へ
        const int en      = static_cast<int>(Player_GetBeamEnergy());
        const int covered = (enPortion < en) ? enPortion : en;
        if (covered > 0) Player_AddBeamEnergy(-static_cast<float>(covered));
        hpPortion += (enPortion - covered);

        damage = hpPortion;
        Shield_NotifyHit();
    }

    g_PlayerHP -= damage;
    Score_AddDamageTaken(damage);

    //g_InvincibleTimer = INVINCIBLE_DURATION; // TODO: 無敵時間 一時無効化中

    if (g_PlayerHP <= 0)
    {
        g_PlayerHP = 0;
        Player_SetEnable(false);
    }

    // 被弾カメラシェイク（ダメージ量で強さをスケール：0.15〜0.5）
    {
        const float mag = 0.15f + std::min(damage / 2000.0f, 1.0f) * 0.35f;
        Player_Camera_AddShake(mag, 0.25f);
    }

    return true;
}

int Player_GetHP() // 現在HPを返す
{
    return g_PlayerHP;
}

int Player_GetMaxHP() // 最大HPを返す
{
    return g_PlayerMaxHP;
}

void Player_Heal(int amount) // HP回復（最大HPでクランプ）。amount=回復量
{
    g_PlayerHP += amount;
    if (g_PlayerHP > g_PlayerMaxHP)
        g_PlayerHP = g_PlayerMaxHP;
}

void Player_ResetHP() // HPと無敵時間を初期状態に戻す
{
    g_PlayerHP = g_PlayerMaxHP;
    g_InvincibleTimer = 0.0;
}

bool Player_IsInvincible() // 無敵中かどうかを返す（trueで無敵）
{
    return g_InvincibleTimer > 0.0;
}

bool Player_IsDashing() // ダッシュ中かどうかを返す（trueでダッシュ中）
{
    return g_DashTimer > 0.0f;
}

// ダッシュ中 かつ シールドを装備している（左右どちらかのアーム）
// → エネミー側がこれを見て「接触ダメージを与える側」に切り替える
bool Player_IsShieldDashing()
{
    if (g_DashTimer <= 0.0f) return false;
    // 盾を「展開中」のときだけダッシュ接触ダメージを出す（装備だけでは出さない）
    if (!Shield_IsActive()) return false;
    return (g_RightWeaponIdx == WEAPON_SHIELD) || (g_LeftWeaponIdx == WEAPON_SHIELD);
}

int Player_GetDashContactDamage()
{
    return DASH_CONTACT_DAMAGE;
}

//==============================================================================
// 攻撃力倍率取得
//==============================================================================
float Player_GetDamageMultiplier() // 攻撃力倍率を返す
{
    return g_PlayerDamageMultiplier;
}

//==============================================================================
// 攻撃力倍率設定
//==============================================================================
void Player_SetDamageMultiplier(float multiplier) // 攻撃力倍率を設定する。multiplier=設定する倍率
{
    g_PlayerDamageMultiplier = multiplier;
}

//==============================================================================
// ビームエネルギー取得（WeaponBeam に委譲）
//==============================================================================
float Player_GetBeamEnergy()
{
    return g_pBeamWeapon ? g_pBeamWeapon->GetEnergy() : 0.0f;
}

//==============================================================================
// ビームエネルギー最大値取得（WeaponBeam に委譲）
//==============================================================================
float Player_GetBeamEnergyMax()
{
    return g_pBeamWeapon ? g_pBeamWeapon->GetEnergyMax() : 0.0f;
}

//==============================================================================
// ビームエネルギー回復（WeaponBeam に委譲）
//==============================================================================
void Player_AddBeamEnergy(float amount)
{
    if (g_pBeamWeapon) g_pBeamWeapon->AddEnergy(amount);
}

float Player_GetSpeedMultiplier() // スピード倍率を返す
{
    return g_PlayerSpeedMultiplier;
}

int Player_GetNormalWeaponIndex()
{
    return g_NormalWeaponIdx;   // 通常武器スロット（0-2）
}

int Player_GetRightWeaponIndex()
{
    return g_RightWeaponIdx;    // 実際の右腕武器ID（シールド含む 0-3）
}

int Player_GetLeftWeaponIndex()
{
    return g_LeftWeaponIdx;
}

void Player_SetSpeedMultiplier(float m) // スピード倍率を設定する。m=設定する倍率
{
    g_PlayerSpeedMultiplier = m;
}

//------------------------------------------------------------------------------
// Player_SetNormalWeaponIndex
//   武器選択画面（WeaponSelect）から呼ばれ、ゲーム開始時の通常スロット武器を設定する
//   ※ Player_Initialize() の後に呼ぶこと
//------------------------------------------------------------------------------
void Player_SetNormalWeaponIndex(int idx)
{
    if (idx < 0 || idx >= WEAPON_COUNT) return;

    g_RightWeaponIdx = idx;

    // バレル系ならモデルをロード、シールドならバレルを解放
    ModelRelease(g_pBarrelModel);
    g_pBarrelModel = nullptr;
    ModelRelease(g_pBarrelEdgeModel);
    g_pBarrelEdgeModel = nullptr;

    if (idx != WEAPON_SHIELD)
    {
        g_NormalWeaponIdx = idx;  // 武器クラスのインデックスも更新
        g_pBarrelModel = ModelLoad(k_WeaponDefs[idx].modelPath, k_WeaponDefs[idx].scale);
        // 近接は発光パーツ（BladeEdge）も同スケールでロード（重ね描画用）
        if (idx == WEAPON_MELEE)
            g_pBarrelEdgeModel = ModelLoad(MELEE_EDGE_MODEL_PATH, k_WeaponDefs[idx].scale);
    }
    // SHIELD の場合は g_pShieldModel（常時ロード済み）を右腕にも使う
}

void Player_SetLeftWeaponIndex(int idx)
{
    if (idx < 0 || idx >= WEAPON_COUNT) return;

    g_LeftWeaponIdx = idx;

    // 既存の左腕リソースを解放
    ModelRelease(g_pLeftBarrelModel); g_pLeftBarrelModel = nullptr;
    ModelRelease(g_pLeftBarrelEdgeModel); g_pLeftBarrelEdgeModel = nullptr;
    if (g_pLeftWeapon) { g_pLeftWeapon->Finalize(); delete g_pLeftWeapon; g_pLeftWeapon = nullptr; }

    if (idx != WEAPON_SHIELD)
    {
        // バレル系：モデルと武器インスタンスをロード
        g_pLeftBarrelModel = ModelLoad(k_WeaponDefs[idx].modelPath, k_WeaponDefs[idx].scale);
        g_pLeftWeapon      = CreateWeaponByID(idx);
        if (g_pLeftWeapon) g_pLeftWeapon->Initialize();
        // 近接は発光パーツ（BladeEdge）も重ね描画用にロード
        if (idx == WEAPON_MELEE)
            g_pLeftBarrelEdgeModel = ModelLoad(MELEE_EDGE_MODEL_PATH, k_WeaponDefs[idx].scale);
    }
    // SHIELD の場合は g_pShieldModel（常時ロード済み）をそのまま使う
}