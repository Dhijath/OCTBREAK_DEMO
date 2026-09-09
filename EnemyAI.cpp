/*==============================================================================

   エネミーAI処理 [EnemyAI.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyAI.h"
#include "Player.h"
#include "MapPatrolAI.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>

using namespace DirectX;

namespace
{
    //==========================================================================
    // AIパラメータ（全エネミー共通・速度は引数で個別指定）
    //==========================================================================
    float g_RotationSpeed   = 270.0f; // 回転速度（度/秒）
    float g_ArrivalDistance = 1.0f;   // 到達判定距離（メートル）
    float g_SightMultiplier = 1.0f;   // 視野距離の倍率（ミッションの難易度で変える）

    //==========================================================================
    // 警報（どれか1体がプレイヤーを視認すると、近くの仲間にも位置が伝わる）
    //==========================================================================
    constexpr float ALERT_RADIUS   = 18.0f;  // 警報が届く範囲（視認した敵からの距離）
    constexpr float ALERT_LIFETIME = 1.0f;   // 警報の有効時間（秒）
    constexpr float ALERT_INVESTIGATE = 4.0f;// 警報を受けた敵が最終視認位置へ急行する時間

    DirectX::XMFLOAT3 g_AlertSource   = {};  // 視認した敵の位置
    DirectX::XMFLOAT3 g_AlertTarget   = {};  // そのときのプレイヤー位置
    float             g_AlertAge      = 99.0f; // 発令からの経過時間（フレームごとに加算）
}

//==============================================================================
// 警報の経過時間を進める（毎フレーム1回。game.cpp から呼ぶ）
//==============================================================================
void EnemyAI_UpdateAlert(float dt)
{
    g_AlertAge += dt;
}

//==============================================================================
// AI更新処理（追跡 + 索敵 + 巡回）
//==============================================================================
void EnemyAI_Update(
    DirectX::XMFLOAT3* ioPosition,
    DirectX::XMFLOAT3* ioVelocity,
    DirectX::XMFLOAT3* ioFront,
    DirectX::XMFLOAT3* ioDestination,
    bool*              ioWasChasing,
    DirectX::XMFLOAT3* ioLastSeenPos,
    float*             ioInvestigateTimer,
    float              dt,
    float              chaseSpeed,
    float              patrolSpeed,
    float              sightDistance)
{
    // プレイヤー位置取得
    const XMFLOAT3& playerPos = Player_GetPosition();

    // エネミー→プレイヤーのベクトル
    XMVECTOR vEnemyPos  = XMLoadFloat3(ioPosition);
    XMVECTOR vPlayerPos = XMLoadFloat3(&playerPos);
    XMVECTOR vToPlayer  = vPlayerPos - vEnemyPos;

    // 距離計算（XZ平面のみ・Y無視）
    XMVECTOR vToPlayerXZ      = XMVectorSetY(vToPlayer, 0.0f);
    float    distanceToPlayer = sqrtf(XMVectorGetX(XMVector3LengthSq(vToPlayerXZ)));

    //==========================================================================
    // モード選択：プレイヤーが視野内にいるか？
    //==========================================================================

    // ミッションの難易度による視野距離の倍率
    sightDistance *= g_SightMultiplier;

    // 追跡中は視野距離を少し広げてチャタリングを防ぐ
    const float sightEnterDist = sightDistance;
    const float sightExitDist  = sightDistance * 1.3f;
    const float effectiveDist  = (*ioWasChasing) ? sightExitDist : sightEnterDist;

    bool isPlayerInSight =
        (distanceToPlayer <= effectiveDist) &&
        MapPatrolAI_HasLineOfSight(*ioPosition, playerPos);

    XMVECTOR vTargetDir;
    float    moveSpeed;

    if (isPlayerInSight)
    {
        //======================================================================
        // 追跡モード：プレイヤーを追う
        //======================================================================
        *ioWasChasing       = true;
        *ioLastSeenPos      = playerPos;   // 最終視認位置を更新
        *ioInvestigateTimer = 0.0f;        // 索敵タイマーをリセット

        // 近くの仲間へ警報を出す
        g_AlertSource = *ioPosition;
        g_AlertTarget = playerPos;
        g_AlertAge    = 0.0f;

        // isPlayerInSight の時点で視線は通っているので、そのまま直進する
        //（以前はここで同じ視線判定をもう一度行っていた）
        vTargetDir = XMVector3Normalize(vToPlayerXZ);
        moveSpeed = chaseSpeed;
    }
    else
    {
        //======================================================================
        // 視野外：索敵（Investigate）または巡回（Patrol）
        //======================================================================

        if (*ioWasChasing)
        {
            // 追跡→索敵へ切り替え：最終視認位置を目的地にセット
            *ioDestination       = *ioLastSeenPos;
            *ioInvestigateTimer  = 3.0f;
            *ioWasChasing        = false;
        }
        else if (*ioInvestigateTimer <= 0.0f && g_AlertAge < ALERT_LIFETIME)
        {
            // 巡回中に仲間の警報を受けたら、そのときのプレイヤー位置へ急行する（同じ階の敵だけ）
            const float ax = g_AlertSource.x - ioPosition->x;
            const float az = g_AlertSource.z - ioPosition->z;
            if (ax * ax + az * az <= ALERT_RADIUS * ALERT_RADIUS &&
                fabsf(g_AlertSource.y - ioPosition->y) < 1.5f)
            {
                *ioLastSeenPos      = g_AlertTarget;
                *ioDestination      = g_AlertTarget;
                *ioInvestigateTimer = ALERT_INVESTIGATE;
            }
        }

        if (*ioInvestigateTimer > 0.0f)
        {
            //==================================================================
            // 索敵モード：最終視認位置へ急行
            //==================================================================
            *ioInvestigateTimer -= dt;

            XMVECTOR vDest      = XMLoadFloat3(ioDestination);
            XMVECTOR vToDestXZ  = XMVectorSetY(vDest - vEnemyPos, 0.0f);
            float    distToDest = sqrtf(XMVectorGetX(XMVector3LengthSq(vToDestXZ)));

            if (distToDest < g_ArrivalDistance || *ioInvestigateTimer <= 0.0f)
            {
                // 到着またはタイムアウト → 巡回へ移行
                *ioInvestigateTimer = 0.0f;
                *ioDestination      = MapPatrolAI_GetReachableDestination(*ioPosition);
                vDest               = XMLoadFloat3(ioDestination);
                vToDestXZ           = XMVectorSetY(vDest - vEnemyPos, 0.0f);
            }

            vTargetDir = XMVector3Normalize(vToDestXZ);
            moveSpeed  = chaseSpeed; // 索敵中も追跡速度で移動
        }
        else
        {
            //==================================================================
            // 巡回モード：目的地へ向かう
            //==================================================================
            XMVECTOR vDest      = XMLoadFloat3(ioDestination);
            XMVECTOR vToDestXZ  = XMVectorSetY(vDest - vEnemyPos, 0.0f);
            float    distToDest = sqrtf(XMVectorGetX(XMVector3LengthSq(vToDestXZ)));

            if (distToDest < g_ArrivalDistance)
            {
                *ioDestination = MapPatrolAI_GetReachableDestination(*ioPosition);
                vDest          = XMLoadFloat3(ioDestination);
                vToDestXZ      = XMVectorSetY(vDest - vEnemyPos, 0.0f);
            }

            vTargetDir = XMVector3Normalize(vToDestXZ);
            moveSpeed  = patrolSpeed;
        }
    }

    //==========================================================================
    // 向き変更（滑らかに回転）
    //==========================================================================

    XMVECTOR vCurrentFront = XMVector3Normalize(XMVectorSetY(XMLoadFloat3(ioFront), 0.0f));

    // 目標方向との内角（ラジアン）
    float dot      = std::clamp(XMVectorGetX(XMVector3Dot(vCurrentFront, vTargetDir)), -1.0f, 1.0f);
    float angleRad = acosf(dot);

    // 1フレームで回せる最大角度（ラジアン）
    float maxRotRad = XMConvertToRadians(g_RotationSpeed) * dt;

    XMVECTOR vNewFront;

    if (angleRad <= maxRotRad)
    {
        // 既に目標方向へ向いている
        vNewFront = vTargetDir;
    }
    else
    {
        // 左右どちらに回すか判断して少しずつ回転する
        float     crossY  = XMVectorGetY(XMVector3Cross(vCurrentFront, vTargetDir));
        XMMATRIX  rotMtx  = XMMatrixRotationY(crossY >= 0.0f ? maxRotRad : -maxRotRad);
        vNewFront = XMVector3Normalize(XMVector3TransformNormal(vCurrentFront, rotMtx));
    }

    XMStoreFloat3(ioFront, vNewFront);

    //==========================================================================
    // 移動：向いている方向に加速
    //==========================================================================
    XMVECTOR vVelocity      = XMLoadFloat3(ioVelocity);
    XMVECTOR vTargetVelocity = vNewFront * moveSpeed;

    // 現在のXZ速度を目標速度へ補間する（急停止・急発進を防ぐ）
    const float lerpSpeed = 10.0f;
    float       lerpT     = std::min(1.0f, lerpSpeed * dt);

    float newVx = XMVectorGetX(vVelocity) + (XMVectorGetX(vTargetVelocity) - XMVectorGetX(vVelocity)) * lerpT;
    float newVz = XMVectorGetZ(vVelocity) + (XMVectorGetZ(vTargetVelocity) - XMVectorGetZ(vVelocity)) * lerpT;

    vVelocity = XMVectorSetX(vVelocity, newVx);
    vVelocity = XMVectorSetZ(vVelocity, newVz);

    XMStoreFloat3(ioVelocity, vVelocity);
}

//==============================================================================
// 回転速度設定
//==============================================================================
void EnemyAI_SetRotationSpeed(float degreesPerSecond)
{
    g_RotationSpeed = std::max(1.0f, degreesPerSecond);
}

//==============================================================================
// 到達判定距離設定
//==============================================================================
void EnemyAI_SetArrivalDistance(float distance)
{
    g_ArrivalDistance = std::max(0.1f, distance);
}

//==============================================================================
// 視野距離倍率設定
//==============================================================================
void EnemyAI_SetSightMultiplier(float multiplier)
{
    g_SightMultiplier = std::max(0.1f, multiplier);
}

float EnemyAI_GetSightMultiplier()
{
    return g_SightMultiplier;
}
