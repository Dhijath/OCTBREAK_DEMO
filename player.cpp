/*==============================================================================

   プレイヤー制御 [Player.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/09
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

    int g_RightWeaponIdx = WEAPON_MACHINEGUN; // 右腕武器ID（0-3）
    int g_LeftWeaponIdx  = WEAPON_SHIELD;     // 左腕武器ID（0-3）
    PlayerWeapon* g_pLeftWeapon = nullptr;    // 左腕武器インスタンス


    bool g_IsJump = false;
    bool g_PlayerEnable = true;

    float g_PlayerSpeedMultiplier = 1.0f;
    int g_PlayerWhightTexID = -1;        // プレイヤー矢印テクスチャ
    int g_PlayerMarkerTexID = -1;        // プレイヤー矢印テクスチャ
    //--------------------------------------------------------------------------
    // HP / 無敵
    //--------------------------------------------------------------------------
    constexpr int PLAYER_MAX_HP = 100;
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

    static constexpr int NORMAL_WEAPON_COUNT = 3;
    PlayerWeapon* g_NormalWeapons[NORMAL_WEAPON_COUNT] = {};  // [0]Normal [1]Shotgun [2]Missile
    int                  g_NormalWeaponIdx = 0;                       // E キーで切り替え

    //--------------------------------------------------------------------------
    // 武器IDからインスタンスを生成するヘルパー
    //--------------------------------------------------------------------------
    static PlayerWeapon* CreateWeaponByID(int weaponId)
    {
        switch (weaponId)
        {
        case WEAPON_MACHINEGUN: return new WeaponNormal();
        case WEAPON_SHOTGUN:    return new WeaponShotgun();
        case WEAPON_MISSILE:    return new WeaponMissile();
        default:                return nullptr;
        }
    }

    //--------------------------------------------------------------------------
    // SE関連（通常スロット切り替え音のみ。射撃SEは各武器クラスが管理）
    //--------------------------------------------------------------------------
    int g_PlayerModeSwitchToNormalSE = -1;  // 通常スロット切り替えSE
    int g_SeShieldDeploy             = -1;  // シールド展開SE
    int g_SeShieldRetract            = -1;  // シールド収納SE

    //--------------------------------------------------------------------------
    // パーティクル（スラスター）
    //--------------------------------------------------------------------------
    int g_PlayerParticleTexID = -1;                     // スラスター用テクスチャID
    ThrusterEmitter* g_PlayerThrusterEmitter = nullptr; // 後方噴射用エミッター

    // ローカルオフセット（right/up/front 基底で組み立て）
    // Initialize() でスラスターモデルの AABB から自動計算される
    // X: 右+ / 左-  Y: 上+ / 下-  Z: 前+ / 後-
    XMFLOAT3 g_ThrusterOffsetLocal = { 0.0f, 0.30f, -0.25f };

    // モデル描画の Y オフセット（Initialize と Draw で共用）
    constexpr float PLAYER_HEIGHT_OFFSET = 0.15f;


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

    static XMMATRIX Player_GetThrusterWorldMatrix()
    {
        XMMATRIX bodyRot = Player_GetBodyRotationMatrix();

        const float heightOffset = PLAYER_HEIGHT_OFFSET;
        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + heightOffset,
            g_PlayerPosition.z
        };

        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);
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
            bodyAABB.min.y - thrusterLocal.max.y - 0.01f,
            g_PlayerPosition.z + XMVectorGetZ(playerFront) * THRUSTER_FORWARD_OFFSET
        );

        return thrusterLocalRot * bodyRot * thrusterTrans;
    }

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
        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);

        // バレル原点位置（ボディ底面・右側）
        const XMFLOAT3 barrelOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * BARREL_SIDE_X
                               + XMVectorGetX(playerFront) * BARREL_FORWARD_OFFSET,
            bodyAABB.min.y,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * BARREL_SIDE_X
                               + XMVectorGetZ(playerFront) * BARREL_FORWARD_OFFSET
        };
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
        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);

        const XMFLOAT3 barrelOriginPos = {
            g_PlayerPosition.x + XMVectorGetX(playerRight) * BARREL_SIDE_X
                               + XMVectorGetX(playerFront) * BARREL_FORWARD_OFFSET,
            bodyAABB.min.y,
            g_PlayerPosition.z + XMVectorGetZ(playerRight) * BARREL_SIDE_X
                               + XMVectorGetZ(playerFront) * BARREL_FORWARD_OFFSET
        };
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
        const float SHIELD_SIDE_X        = -0.30f + b * 0.15f;  // 左横 → 正面寄り
        const float SHIELD_FORWARD_OFFSET =  0.20f + b * 0.30f;  // 前方へ突き出す
        const float SHIELD_HEIGHT_OFFSET  =  0.10f + b * 0.15f;  // 少し持ち上げる

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x,
            g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET,
            g_PlayerPosition.z
        };
        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);

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
        const float SHIELD_SIDE_X        =  0.30f - b * 0.15f;  // 右横 → 正面寄り（左の逆）
        const float SHIELD_FORWARD_OFFSET =  0.20f + b * 0.30f;
        const float SHIELD_HEIGHT_OFFSET  =  0.10f + b * 0.15f;

        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR playerFront = XMVector3Normalize(XMLoadFloat3(&g_PlayerFront));
        XMVECTOR playerRight = XMVector3Normalize(XMVector3Cross(up, playerFront));

        const XMFLOAT3 bodyWorldPos = {
            g_PlayerPosition.x, g_PlayerPosition.y + PLAYER_HEIGHT_OFFSET, g_PlayerPosition.z };
        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);

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

        const AABB bodyAABB = ModelGetAABB(g_pPlayerModel, bodyWorldPos);
        const AABB headLocal = ModelGetAABB(g_pHeadModel, { 0.0f, 0.0f, 0.0f });

        XMMATRIX headTrans = XMMatrixTranslation(
            g_PlayerPosition.x,
            bodyAABB.max.y - headLocal.min.y,
            g_PlayerPosition.z
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

        for (int i = 0; i < Map_GetObjectsCount(); i++)
        {
            const MapObject* mo = Map_GetObject(i);
            if (!mo) continue;
            if (mo->KindId != 2) continue;

            OBB playerOBB = Player_ConvertPositionToOBB(*ioPos);
            Hit hit = Collision_IsHitOBB_AABB(playerOBB, mo->Aabb);
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
    g_PlayerSpeedMultiplier = 1.0f;


    g_PlayerHP = PLAYER_MAX_HP;
    g_InvincibleTimer = 0.0;

    Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
    Mouse_SetVisible(false);

    XMStoreFloat3(&g_PlayerFront, XMVector3Normalize(XMLoadFloat3(&front)));

    g_PlayerWhightTexID = Texture_Load(L"resource/texture/Player_white.png");
    g_PlayerMarkerTexID = Texture_Load(L"resource/texture/Player_white.png");
    // プレイヤーモデルを body.fbx で構成する
    g_pPlayerModel = ModelLoad("resource/Models/body.fbx", 0.3f);
    g_pThrusterModel = ModelLoad("resource/Models/Thruster.fbx", 0.3f);
    g_pBarrelModel = ModelLoad(k_WeaponDefs[g_NormalWeaponIdx].modelPath, k_WeaponDefs[g_NormalWeaponIdx].scale);
    g_pHeadModel   = ModelLoad("resource/Models/Head.fbx", 0.3f);
    g_pShieldModel = ModelLoad(k_WeaponDefs[WEAPON_SHIELD].modelPath, k_WeaponDefs[WEAPON_SHIELD].scale);

    // 左腕武器（バレル系ならモデルとインスタンスをロード）
    g_LeftWeaponIdx = WEAPON_SHIELD;
    g_pLeftBarrelModel = nullptr;
    g_pLeftWeapon      = nullptr;

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
            + bodyLocal.min.y
            - thrusterLocal.max.y
            - 0.01f;
        const float thrusterCenterY = thrusterOriginY
            + (thrusterLocal.max.y + thrusterLocal.min.y) * 0.5f;

        g_ThrusterOffsetLocal = { 0.0f, thrusterCenterY, 0.0f };
    }

    //--------------------------------------------------------------------------
    // 武器システム初期化
    //--------------------------------------------------------------------------
    // ビーム（固定）
    g_pBeamWeapon = new WeaponBeam();
    g_pBeamWeapon->Initialize();

    // 通常スロット
    g_NormalWeaponIdx = 0;
    g_NormalWeapons[0] = new WeaponNormal();
    g_NormalWeapons[1] = new WeaponShotgun();
    g_NormalWeapons[2] = new WeaponMissile();
    for (int i = 0; i < NORMAL_WEAPON_COUNT; ++i)
        g_NormalWeapons[i]->Initialize();

    //--------------------------------------------------------------------------
    // SE読み込み（通常スロット切り替えSEのみ。射撃SEは各武器クラスが管理）
    //--------------------------------------------------------------------------
    g_PlayerModeSwitchToNormalSE = LoadAudioWithVolume("resource/sound/mode_switch_normal.wav", 0.5f);
    g_SeShieldDeploy  = LoadAudio("resource/Sound/shield_deploy.wav");
    g_SeShieldRetract = LoadAudio("resource/Sound/shield_retract.wav");

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
    g_PlayerThrusterEmitter->SetLifeRange(0.18f, 0.26f);        // パーティクルの寿命範囲（最小, 最大）秒
    g_PlayerThrusterEmitter->SetConeAngleDeg(26.0f);            // 放出コーン角度（度）
    g_PlayerThrusterEmitter->SetAspectRatio(3.0f);              // 横長比率（幅/高さ）
    g_PlayerThrusterEmitter->SetColor({ 1.0f, 0.5f, 2.5f, 1.0f }); // パーティクルの色（R,G,B,A）
    g_PlayerThrusterEmitter->SetUVRect({ 0.0f, 0.0f, 80.0f, 80.0f }); // UV矩形（x, y, 幅, 高さ）
    g_PlayerThrusterEmitter->SetLocalOffset(g_ThrusterOffsetLocal); // エミッターのローカルオフセット座標
}

//==============================================================================
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

    ModelRelease(g_pShieldModel);
    g_pShieldModel = nullptr;

    ModelRelease(g_pLeftBarrelModel);
    g_pLeftBarrelModel = nullptr;
    if (g_pLeftWeapon) { g_pLeftWeapon->Finalize(); delete g_pLeftWeapon; g_pLeftWeapon = nullptr; }

    if (g_PlayerThrusterEmitter)
    {
        delete g_PlayerThrusterEmitter;
        g_PlayerThrusterEmitter = nullptr;
    }

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
    for (int i = 0; i < NORMAL_WEAPON_COUNT; ++i)
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

    const bool padJump = PadLogger_IsTrigger(PAD_A);

    if (g_PlayerEnable && (KeyLogger_IsTrigger(KK_F) || padJump) && !g_IsJump)
    {
        velocity += XMVECTOR{ 0.0f, 10.0f, 0.0f, 0.0f };
        g_IsJump = true;
    }

    XMVECTOR gravityDir = XMVECTOR{ 0.0f, -1.0f, 0.0f, 0.0f };
    velocity += gravityDir * 9.8f * 3.0f * static_cast<float>(elapsed_time);

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
                tempPos.y + PLAYER_HEIGHT,
                tempPos.z + PLAYER_HALF_WIDTH_Z
            }
        };

        for (int i = 0; i < Map_GetObjectsCount(); i++)
        {
            const MapObject* mo = Map_GetObject(i);
            if (!mo) continue;
            if (mo->KindId != 0 && mo->KindId != 1) continue;

            if (Collision_IsOverLapAABB(playerAABB, mo->Aabb))
            {
                posY = XMVectorSetY(posY, mo->Aabb.max.y);
                velocity *= XMVECTOR{ 1.0f, 0.0f, 1.0f, 1.0f };
                g_IsJump = false;
                break;
            }
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
        velocity += moveDir * static_cast<float>(2000.0 / 90.0 * elapsed_time) * g_PlayerSpeedMultiplier;
    }

    // K キーでボディの向きモード切り替え
    if (KeyLogger_IsTrigger(KK_K))
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
                          || KeyLogger_IsPressed(KK_SPACE)
                          || Player_Camera_IsMouseLeftPressed();
        if (g_LeftWeaponIdx == WEAPON_SHIELD)
            shieldPressed |= PadLogger_IsPressed(PAD_LEFT_SHOULDER);

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
    if (g_pBeamWeapon)
        g_pBeamWeapon->Update(elapsed_time);
    if (g_NormalWeapons[g_NormalWeaponIdx])
        g_NormalWeapons[g_NormalWeaponIdx]->Update(elapsed_time);
    if (g_pLeftWeapon)
        g_pLeftWeapon->Update(elapsed_time);

    //--------------------------------------------------------------------------
    // 発射ボタン判定
    //   RB(PAD_RIGHT_SHOULDER) = 右腕  LB(PAD_LEFT_SHOULDER) = 左腕
    //   RT                     = ビーム  マウス左 = 右腕  マウス右 = ビーム
    //--------------------------------------------------------------------------
    const bool padRightFire = PadLogger_IsPressed(PAD_RIGHT_SHOULDER);
    const bool padLeftFire  = PadLogger_IsPressed(PAD_LEFT_SHOULDER);
    const bool padBeamFire  = (PadLogger_GetRightTrigger() > 0.5f);
    const bool mouseAttack  = Player_Camera_IsMouseLeftPressed();
    const bool keyAttack    = KeyLogger_IsPressed(KK_SPACE);
    const bool beamAttack   = Player_Camera_IsMouseRightPressed() || padBeamFire;

    const bool rightFire = keyAttack || mouseAttack || padRightFire;
    const bool leftFire  = padLeftFire;

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
            g_NormalWeapons[g_NormalWeaponIdx]->TryFire(muzzlePos, aimDir, g_PlayerDamageMultiplier);
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

        g_pLeftWeapon->TryFire(leftMuzzlePos, leftAimDir, g_PlayerDamageMultiplier);
    }

    //--------------------------------------------------------------------------
    // ビーム発射（RT / マウス右）：胴体中心から発射
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

        g_pBeamWeapon->TryFire(beamOrigin, beamDir, g_PlayerDamageMultiplier);
    }

    //--------------------------------------------------------------------------
    // スラスターの外観をスピード倍率に応じて変化させる
    //--------------------------------------------------------------------------
    {
        // スピード倍率(1.0〜2.0)を0〜1に正規化した補間係数
        float t = std::clamp((g_PlayerSpeedMultiplier - 1.0f) / 1.0f, 0.0f, 1.0f);

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
        float speedT = std::clamp((g_PlayerSpeedMultiplier - 1.0f) / 1.0f, 0.0f, 1.0f);
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

    //--------------------------------------------------------------------------
    // スラスターパーティクル
    //--------------------------------------------------------------------------
    if (g_PlayerThrusterEmitter)
    {
        g_PlayerThrusterEmitter->Draw();
    }

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
    Shader3d_Begin(); // 

    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    ID3D11Buffer* nullCB = nullptr;
    ctx->PSSetConstantBuffers(6, 1, &nullCB);

    const float minimapY = 10.0f;

    Shader3d_SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });

    const XMMATRIX markerWorld = XMMatrixTranslation(
        g_PlayerPosition.x,
        minimapY + 1.0f,
        g_PlayerPosition.z
    );

    MeshField_DrawTile(markerWorld, Map_GetWiteTexID(), 1.0f);
    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
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

    // 両腕シールド → ダメージ無効（-100%）
    const bool rightIsShield = (g_NormalWeaponIdx >= NORMAL_WEAPON_COUNT); // 右腕がシールド
    const bool leftIsShield  = (g_LeftWeaponIdx == WEAPON_SHIELD);
    if (rightIsShield && leftIsShield && Shield_IsActive())
        return false;

    // シールドガード中はダメージ軽減（最低 1 ダメージは通す）
    if (Shield_IsActive())
    {
        damage = static_cast<int>(damage * (1.0f - Shield_GetDamageReduction()));
        if (damage < 1) damage = 1;
        Shield_NotifyHit();
    }

    g_PlayerHP -= damage;

    g_InvincibleTimer = INVINCIBLE_DURATION;

    if (g_PlayerHP <= 0)
    {
        g_PlayerHP = 0;
        Player_SetEnable(false);
    }

    return true;
}

int Player_GetHP() // 現在HPを返す
{
    return g_PlayerHP;
}

int Player_GetMaxHP() // 最大HPを返す
{
    return PLAYER_MAX_HP;
}

void Player_Heal(int amount) // HP回復（最大HPでクランプ）。amount=回復量
{
    g_PlayerHP += amount;
    if (g_PlayerHP > PLAYER_MAX_HP)
        g_PlayerHP = PLAYER_MAX_HP;
}

void Player_ResetHP() // HPと無敵時間を初期状態に戻す
{
    g_PlayerHP = PLAYER_MAX_HP;
    g_InvincibleTimer = 0.0;
}

bool Player_IsInvincible() // 無敵中かどうかを返す（trueで無敵）
{
    return g_InvincibleTimer > 0.0;
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

    if (idx != WEAPON_SHIELD)
    {
        g_NormalWeaponIdx = idx;  // 武器クラスのインデックスも更新
        g_pBarrelModel = ModelLoad(k_WeaponDefs[idx].modelPath, k_WeaponDefs[idx].scale);
    }
    // SHIELD の場合は g_pShieldModel（常時ロード済み）を右腕にも使う
}

void Player_SetLeftWeaponIndex(int idx)
{
    if (idx < 0 || idx >= WEAPON_COUNT) return;

    g_LeftWeaponIdx = idx;

    // 既存の左腕リソースを解放
    ModelRelease(g_pLeftBarrelModel); g_pLeftBarrelModel = nullptr;
    if (g_pLeftWeapon) { g_pLeftWeapon->Finalize(); delete g_pLeftWeapon; g_pLeftWeapon = nullptr; }

    if (idx != WEAPON_SHIELD)
    {
        // バレル系：モデルと武器インスタンスをロード
        g_pLeftBarrelModel = ModelLoad(k_WeaponDefs[idx].modelPath, k_WeaponDefs[idx].scale);
        g_pLeftWeapon      = CreateWeaponByID(idx);
        if (g_pLeftWeapon) g_pLeftWeapon->Initialize();
    }
    // SHIELD の場合は g_pShieldModel（常時ロード済み）をそのまま使う
}