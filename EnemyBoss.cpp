/*==============================================================================

   ボス型エネミー [EnemyBoss.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/09
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyBoss.h"
#include "EnemyBullet.h"
#include "MapPatrolAI.h"
#include "Player.h"
#include "Audio.h"
#include "model.h"
#include "ModelToon.h"
#include "Light.h"
#include "Player_Camera.h"
#include "BossIntro.h"
#include <cstdlib>
#include <algorithm>  // std::clamp
#include <cmath>      // acosf, sqrtf, atan2f

using namespace DirectX;

//==============================================================================
// 初期化
//
// ■役割
// ・Enemy::Initialize で基本状態をセットアップ
// ・HP・モデル・バレル・SEをボス用に上書きする
//==============================================================================
void EnemyBoss::Initialize(const XMFLOAT3& position)
{
    // 基底クラスの初期化
    Enemy::Initialize(position);

    // HP をボス用に上書き
    SetHP(HP, HP);

    // 本体モデルを差し替え
    ModelRelease(m_pModel);
    m_pModel = ModelLoad("resource/Models/enemy_tank.fbx", BOSS_SIZE);
    ComputeLockOnOffsetFromModel(); // ボスモデルのAABBから自動計算

    // X字4門バレルをロード
    for (int i = 0; i < BARREL_COUNT; ++i)
        m_pBarrel[i] = ModelLoad("resource/Models/Barrel.fbx", BOSS_SIZE * 0.5f);

    // ショットガンSEをロード
    m_shootSE = LoadAudioWithVolume("resource/sound/shotgun.wav", 0.5f);
    m_shootTimer = 0.0f;
    m_BarrelRotX = 0.0f;

    // 突進・射撃パラメータ初期化
    m_bossPhase         = BossPhase::INTRO;
    m_chargeTimer       = 0.0f;
    m_chargeCooldown    = CHARGE_INTERVAL; // イントロ終了後すぐ突進しないよう満タン
    m_chargeDir         = {};
    m_chargeDamageDealt = false;
    m_shotCount         = 0;
    m_nextShootInterval = 1.0f;
}

//==============================================================================
// 終了処理
//==============================================================================
void EnemyBoss::Finalize()
{
    UnloadAudio(m_shootSE);
    m_shootSE = -1;

    for (int i = 0; i < BARREL_COUNT; ++i)
    {
        ModelRelease(m_pBarrel[i]);
        m_pBarrel[i] = nullptr;
    }

    Enemy::Finalize();
}

//==============================================================================
// 更新処理
//
// ■役割
// ・突進フェーズに応じて速度を事前設定してから基底クラスのUpdateを呼ぶ
// ・基底クラスのUpdate（AI移動・重力・衝突・弾ヒット・死亡判定）を実行
// ・バレルをプレイヤー方向に追従させる
// ・突進ステートマシンを更新する
// ・生存中のみ射撃タイマーを進め、視線が通れば Shoot()/SpreadShoot() を呼ぶ
//==============================================================================
void EnemyBoss::Update(double elapsed_time)
{
    if (!m_IsAlive) return;

    float dt = static_cast<float>(elapsed_time > 1.0 / 30.0 ? 1.0 / 30.0 : elapsed_time);

    //==========================================================================
    // INTRO 中：バレルアニメタイマーのみ更新。AI・移動・射撃・ロックオン全スキップ。
    // m_Front は BossIntro_Start() で設定済みの方向を維持する。
    //==========================================================================
    if (m_bossPhase == BossPhase::INTRO)
    {
        m_chargeTimer += dt;
        if (m_chargeTimer >= INTRO_DURATION && !BossIntro_IsPlaying())
        {
            m_bossPhase   = BossPhase::NORMAL;
            m_chargeTimer = 0.0f;
        }
        return;
    }

    //==========================================================================
    // 突進フェーズに応じた速度の事前設定（Enemy::Update 内の EnemyAI に渡す初期値）
    // ＋ m_SpeedCap を更新して Enemy::Update 内の ClampXZSpeed 上限を制御する
    //==========================================================================
    switch (m_bossPhase)
    {
    case BossPhase::CHARGE_WINDUP:
        // 溜め中は停止（EnemyAI が lerp で若干動くが許容範囲）
        m_Velocity.x = 0.0f;
        m_Velocity.z = 0.0f;
        m_SpeedCap   = MAX_SPEED; // 通常上限を維持
        break;

    case BossPhase::CHARGING:
        // 突進方向へ高速移動
        m_Velocity.x = m_chargeDir.x * CHARGE_SPEED;
        m_Velocity.z = m_chargeDir.z * CHARGE_SPEED;
        m_SpeedCap   = CHARGE_SPEED; // 突進速度まで上限を引き上げ
        break;

    case BossPhase::NORMAL:
        // 通常 AI に任せる。距離調整は Enemy::Update() 後にソフトな斥力で行う
        m_SpeedCap = MAX_SPEED;
        break;

    default:
        break;
    }

    //==========================================================================
    // 基底クラスのUpdate（AI・移動・衝突・死亡判定）
    //==========================================================================
    Enemy::Update(elapsed_time);

    // 死亡後は以降の処理をスキップ
    if (!m_IsAlive) return;

    //==========================================================================
    // ソフト斥力（NORMAL 時のみ）
    // Enemy::Update 後に加算することで EnemyAI のチェイスと自然に競合させる。
    // 距離二乗で滑らかなフォールオフ → 遠いと無視、近いほど押し返す力が増す。
    // 「完全に近づかせない」設計ではないので接触も起こり得る。
    //==========================================================================
    if (m_bossPhase == BossPhase::NORMAL)
    {
        const XMFLOAT3& playerPos = Player_GetPosition();
        float dx   = m_Position.x - playerPos.x;
        float dz   = m_Position.z - playerPos.z;
        float dist = sqrtf(dx * dx + dz * dz);

        //----------------------------------------------------------------------
        // ① ソフト斥力：近いほど押し返す（密着時のみ強力）
        //----------------------------------------------------------------------
        if (dist < KEEP_DISTANCE && dist > 0.001f)
        {
            float t       = 1.0f - dist / KEEP_DISTANCE;
            float repulse = RETREAT_SPEED * t * t;
            m_Velocity.x += (dx / dist) * repulse;
            m_Velocity.z += (dz / dist) * repulse;
        }

        //----------------------------------------------------------------------
        // ② ストレイフ：プレイヤーを中心に接線方向へ旋回する
        // プレイヤー→ボスの方向に直交するベクトルを使い、数秒ごとに反転。
        // EnemyAI の直線チェイスに旋回成分が加わり、弧を描いて動く。
        //----------------------------------------------------------------------
        m_strafeTimer -= dt;
        if (m_strafeTimer <= 0.0f)
        {
            // 方向転換：ランダムな間隔で左右を切り替え
            m_strafeSign  = -m_strafeSign;
            m_strafeTimer = STRAFE_CHANGE_MIN +
                (rand() % static_cast<int>((STRAFE_CHANGE_MAX - STRAFE_CHANGE_MIN) * 100)) * 0.01f;
        }

        if (dist > 0.001f)
        {
            // ボス→プレイヤー方向に直交する接線（XZ平面）
            float toPlayerX = -dx / dist;  // = playerPos - bossPos (正規化)
            float toPlayerZ = -dz / dist;
            float tangentX  = -toPlayerZ * static_cast<float>(m_strafeSign);
            float tangentZ  =  toPlayerX * static_cast<float>(m_strafeSign);

            m_Velocity.x += tangentX * STRAFE_SPEED;
            m_Velocity.z += tangentZ * STRAFE_SPEED;
        }
    }

    //==========================================================================
    // ロックオン：感知時のみ本体をプレイヤー方向へ滑らかに回転する
    // ─ 突進中だけは突進方向へ固定（バレルで見た目がおかしくなるため）
    // ─ 未感知時はバレル・本体ともに最後の方向を保持
    //==========================================================================
    {
        const XMFLOAT3& playerPos = Player_GetPosition();
        float dx        = playerPos.x - m_Position.x;
        float dy        = (playerPos.y + 0.3f) - m_Position.y;
        float dz        = playerPos.z - m_Position.z;
        float horizDist = sqrtf(dx * dx + dz * dz);

        // 感知判定：視野距離(XZ平面)かつ視線が通っている
        const bool isDetected = (horizDist <= SIGHT_DIST)
                             && MapPatrolAI_HasLineOfSight(m_Position, playerPos);

        if (m_bossPhase == BossPhase::CHARGING && (m_chargeDir.x != 0.0f || m_chargeDir.z != 0.0f))
        {
            // 突進中：突進方向へ向く（バレルも水平に固定）
            m_Front      = m_chargeDir;
            m_BarrelRotX = 0.0f;
        }
        else if (isDetected && horizDist > 0.001f)
        {
            // バレル仰角：プレイヤー追従
            m_BarrelRotX = atan2f(dy, horizDist);

            // 本体水平向き：感知中は即スナップ（EnemyAI の移動向きより優先）
            m_Front = { dx / horizDist, 0.0f, dz / horizDist };
        }
    }

    //==========================================================================
    // 突進ステートマシン
    //==========================================================================
    switch (m_bossPhase)
    {
    case BossPhase::NORMAL:
    {
        // 突進クールダウンを減算
        m_chargeCooldown  -= dt;
        m_lastShotTimer   += dt;

        // ==== アクション優先順位 ====
        // 突進 > 射撃 > ストレイフ
        // 射撃直後（SHOOT_CHARGE_GAP 秒以内）は突進を抑制し、
        // プレイヤーに「射撃→即突進」の連続を押し付けない。
        bool canCharge = m_chargeCooldown <= 0.0f
                      && m_lastShotTimer  >= SHOOT_CHARGE_GAP
                      && MapPatrolAI_HasLineOfSight(m_Position, Player_GetPosition());

        if (canCharge)
        {
            m_bossPhase         = BossPhase::CHARGE_WINDUP;
            m_chargeTimer       = 0.0f;
            m_chargeDamageDealt = false;
            m_shootTimer        = 0.0f; // 溜め開始時に射撃タイマーをリセット
        }        break;
    }

    case BossPhase::CHARGE_WINDUP:
    {
        m_chargeTimer += dt;

        // 毎フレーム突進方向をプレイヤー方向に更新（狙いを定める）
        {
            const XMFLOAT3& playerPos = Player_GetPosition();
            float dx  = playerPos.x - m_Position.x;
            float dz  = playerPos.z - m_Position.z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 0.001f)
                m_chargeDir = { dx / len, 0.0f, dz / len };
        }

        // 溜め完了 → 突進へ移行
        if (m_chargeTimer >= CHARGE_WINDUP_TIME)
        {
            m_bossPhase   = BossPhase::CHARGING;
            m_chargeTimer = 0.0f;
        }
        break;
    }

    case BossPhase::CHARGING:
    {
        m_chargeTimer += dt;

        // プレイヤー接触チェック（1回のみダメージ）
        if (!m_chargeDamageDealt)
        {
            const XMFLOAT3& playerPos = Player_GetPosition();
            float dx   = playerPos.x - m_Position.x;
            float dz   = playerPos.z - m_Position.z;
            float dist = sqrtf(dx * dx + dz * dz);
            if (dist < CHARGE_DAMAGE_DIST)
            {
                Player_TakeDamage(CHARGE_DAMAGE);

                // 強ノックバック
                DirectX::XMFLOAT3* pPlayerVel = Player_GetVelocityPtr();
                if (pPlayerVel && dist > 0.001f)
                {
                    pPlayerVel->x += (dx / dist) * CHARGE_KNOCKBACK;
                    pPlayerVel->z += (dz / dist) * CHARGE_KNOCKBACK;
                    pPlayerVel->y += 6.0f;
                }
                m_chargeDamageDealt = true;
            }
        }

        // 突進時間終了 → クールダウンへ移行
        if (m_chargeTimer >= CHARGE_MOVE_TIME)
        {
            m_bossPhase      = BossPhase::COOLDOWN;
            m_chargeTimer    = 0.0f;
            m_chargeCooldown = CHARGE_INTERVAL;
        }
        break;
    }

    case BossPhase::COOLDOWN:
    {
        m_chargeTimer += dt;

        // クールダウン終了 → 通常へ戻る
        if (m_chargeTimer >= CHARGE_COOLDOWN_TIME)
        {
            m_bossPhase   = BossPhase::NORMAL;
            m_chargeTimer = 0.0f;
        }
        break;
    }
    }

    //==========================================================================
    // 射撃タイマー更新（溜め・突進中は射撃しない）
    //==========================================================================
    // ==== 射撃（突進中・溜め中は撃たない）====
    if (m_bossPhase == BossPhase::NORMAL || m_bossPhase == BossPhase::COOLDOWN)
    {
        m_shootTimer += dt;
        if (m_shootTimer >= m_nextShootInterval)
        {
            m_shootTimer        = 0.0f;
            m_nextShootInterval = 0.7f + (rand() % 90) * 0.01f;

            if (MapPatrolAI_HasLineOfSight(m_Position, Player_GetPosition()))
            {
                m_shotCount++;
                if (m_shotCount % 3 == 0)
                    SpreadShoot();
                else
                    Shoot();

                m_lastShotTimer = 0.0f; // 射撃直後フラグをリセット
            }
        }
    }
}

//==============================================================================
// 描画処理
//
// ■役割
// ・本体モデルをトゥーン描画する
// ・左右バレルをプレイヤー追従角度で描画する
//==============================================================================
void EnemyBoss::Draw()
{
    if (!m_IsAlive) return;
    if (!m_pModel)  return;

    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        4.0f,
        { 0.8f, 0.2f, 0.2f, 1.0f }  // 赤い光でボスを識別
    );

    float angle = -atan2f(m_Front.z, m_Front.x);

    XMMATRIX rot =
        XMMatrixRotationY(angle) *
        XMMatrixRotationY(XMConvertToRadians(-90.0f));

    // モデルのローカルAABBを取得して底面をm_Position.yに合わせる（地面めり込み修正）
    const AABB bodyLocal = ModelGetAABB(m_pModel, { 0.0f, 0.0f, 0.0f });

    XMMATRIX trans = XMMatrixTranslation(
        m_Position.x,
        m_Position.y - bodyLocal.min.y,
        m_Position.z
    );

    ModelDrawToon(m_pModel, rot * trans);

    // X字4門バレル描画: 正面から見て上右・上左・下右・下左の2×2配置
    // rightOffset は ±1 の正規化符号（GetBarrelWorldMatrix内でOBBスケーリング）
    // heightOffset はAABB高さに対する比率で算出
    if (m_pBarrel[0])
    {
        // [0]=前面 [1]=右面 [2]=後面 [3]=左面 — OBBの各辺サーフェスに底面を合わせて配置
        for (int i = 0; i < BARREL_COUNT; ++i)
        {
            if (m_pBarrel[i])
                ModelDrawToon(m_pBarrel[i], GetBarrelWorldMatrix(i));
        }
    }
}

//==============================================================================
// バレルのワールド行列取得
//
// ■役割
// ・OBBの各面サーフェスにバレル底面を合わせて配置する
// ・マズル（ローカル -Z）がプレイヤーを向くように aimRot を構築する
//
// ■引数
// ・faceIndex : 0=前面 / 1=右面 / 2=後面 / 3=左面
//==============================================================================
XMMATRIX EnemyBoss::GetBarrelWorldMatrix(int faceIndex)
{
    if (!m_pModel)                                return XMMatrixIdentity();
    if (faceIndex < 0 || faceIndex >= BARREL_COUNT) return XMMatrixIdentity();
    if (!m_pBarrel[faceIndex])                    return XMMatrixIdentity();

    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // ── ボス向き ──────────────────────────────────────────────────────────────
    XMVECTOR bossFront = XMVector3Normalize(
        XMVectorSet(m_Front.x, 0.0f, m_Front.z, 0.0f));
    XMVECTOR bossRight = XMVector3Normalize(XMVector3Cross(up, bossFront));

    // ── 本体OBB（Draw()と同じ回転行列を使ってAABB→OBBに変換） ────────────────
    const AABB bodyLocal = ModelGetAABB(m_pModel, { 0.0f, 0.0f, 0.0f });

    // Draw()と同じ回転：RotY(angle) * RotY(-90°)
    float angle = -atan2f(m_Front.z, m_Front.x);
    XMMATRIX bodyRot = XMMatrixRotationY(angle)
                     * XMMatrixRotationY(XMConvertToRadians(-90.0f));

    // ローカルAABB中心・半サイズ（XZ のみ使用）
    float heX = (bodyLocal.max.x - bodyLocal.min.x) * 0.5f;
    float heZ = (bodyLocal.max.z - bodyLocal.min.z) * 0.5f;

    // ワールド空間でのOBBの各軸方向（Y成分は水平面では無視）
    XMVECTOR rotLocalX = XMVector3TransformNormal(
        XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), bodyRot);
    XMVECTOR rotLocalZ = XMVector3TransformNormal(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), bodyRot);

    // ボスFront/Right方向に対するOBBの投影距離（面サーフェスまでの半幅）
    auto absDot = [](XMVECTOR a, XMVECTOR b) -> float {
        return fabsf(XMVectorGetX(XMVector3Dot(a, b)));
    };
    float frontExtent = absDot(rotLocalX, bossFront) * heX
                      + absDot(rotLocalZ, bossFront) * heZ;
    float rightExtent = absDot(rotLocalX, bossRight) * heX
                      + absDot(rotLocalZ, bossRight) * heZ;

    // ── ボディのワールド中心 ──────────────────────────────────────────────────
    // Draw()のtrans: Translation(x, y - bodyLocal.min.y, z) なので
    // ローカル中心 → ワールド：RotY後の中心 + bodyTranslation
    float localCX = (bodyLocal.min.x + bodyLocal.max.x) * 0.5f;
    float localCY = (bodyLocal.min.y + bodyLocal.max.y) * 0.5f;
    float localCZ = (bodyLocal.min.z + bodyLocal.max.z) * 0.5f;
    XMVECTOR rotCenter = XMVector3TransformNormal(
        XMVectorSet(localCX, localCY, localCZ, 0.0f), bodyRot);
    XMVECTOR worldCenter = XMVectorAdd(
        rotCenter,
        XMVectorSet(m_Position.x,
                    m_Position.y - bodyLocal.min.y,
                    m_Position.z, 0.0f));
    XMFLOAT3 wc;
    XMStoreFloat3(&wc, worldCenter);

    // ── 前面2×2配置：OBBの角に合わせる ──────────────────────────────────────
    //   [0]=上右  [1]=上左  [2]=下右  [3]=下左
    //
    //   Z : 前面OBBサーフェス（frontExtent）に合わせる
    //   X : OBBの左右端（±rightExtent）に合わせる
    //   Y : OBBの上端(max.y) or 下端(min.y) のワールドY → 角に配置
    const float sideSign[4] = { +1.0f, -1.0f, +1.0f, -1.0f };
    const bool  isUpper[4]  = { true,  true,  false, false  };

    // Z: 前面OBBサーフェス（XZ 平面）
    XMVECTOR originV = XMVectorAdd(
        worldCenter,
        XMVectorScale(bossFront, frontExtent));

    XMFLOAT3 barrelOriginPos;
    XMStoreFloat3(&barrelOriginPos, originV);

    // X: OBBの左右端（bossRight 方向）
    XMFLOAT3 rightF;
    XMStoreFloat3(&rightF, bossRight);
    barrelOriginPos.x += rightF.x * sideSign[faceIndex] * rightExtent;
    barrelOriginPos.z += rightF.z * sideSign[faceIndex] * rightExtent;

    // Y: AABB上端/下端のワールドY
    // Draw()のtrans Y = m_Position.y - bodyLocal.min.y なので
    //   ワールド下端 = m_Position.y
    //   ワールド上端 = m_Position.y - bodyLocal.min.y + bodyLocal.max.y
    const float worldBottom = m_Position.y;
    const float worldTop    = m_Position.y - bodyLocal.min.y + bodyLocal.max.y;
    barrelOriginPos.y = isUpper[faceIndex] ? worldTop : worldBottom;

    // ── INTRO 前進オフセット ───────────────────────────────────────────────────
    // 上昇フェーズ（0→INTRO_RAISE_END）で前進、復帰フェーズで後退
    // 上段・下段ともに同じ前後移動をさせてめり込みを防ぐ
    if (m_bossPhase == BossPhase::INTRO)
    {
        float anim;
        if (m_chargeTimer <= INTRO_RAISE_END)
        {
            float t = std::clamp(m_chargeTimer / INTRO_RAISE_END, 0.0f, 1.0f);
            anim = t * t * (3.0f - 2.0f * t);             // smoothstep 0→1
        }
        else
        {
            const float returnDur = INTRO_DURATION - INTRO_RAISE_END;
            float t = std::clamp((m_chargeTimer - INTRO_RAISE_END) / returnDur, 0.0f, 1.0f);
            anim = 1.0f - t * t * (3.0f - 2.0f * t);      // smoothstep 1→0
        }

        const float maxForward = 0.35f; // 前進量（m）
        XMVECTOR fwd = XMVectorScale(bossFront, maxForward * anim);
        barrelOriginPos.x += XMVectorGetX(fwd);
        barrelOriginPos.z += XMVectorGetZ(fwd);
    }

    XMMATRIX barrelTrans = XMMatrixTranslation(
        barrelOriginPos.x, barrelOriginPos.y, barrelOriginPos.z);

    // ── 照準方向 ──────────────────────────────────────────────────────────────
    XMVECTOR aimDir;

    if (m_bossPhase == BossPhase::INTRO && !isUpper[faceIndex])
    {
        // 下段：拳を中央上方へ収束させる（上昇フェーズのみ・復帰フェーズは正面へ戻す）
        float anim;
        if (m_chargeTimer <= INTRO_RAISE_END)
        {
            float t = std::clamp(m_chargeTimer / INTRO_RAISE_END, 0.0f, 1.0f);
            anim = t * t * (3.0f - 2.0f * t);
        }
        else
        {
            const float returnDur = INTRO_DURATION - INTRO_RAISE_END;
            float t = std::clamp((m_chargeTimer - INTRO_RAISE_END) / returnDur, 0.0f, 1.0f);
            anim = 1.0f - t * t * (3.0f - 2.0f * t);
        }

        XMVECTOR fistPoint = XMVectorSet(wc.x, worldTop + 0.3f, wc.z, 0.0f);
        XMVECTOR toFist    = XMVector3Normalize(
            XMVectorSubtract(fistPoint, XMLoadFloat3(&barrelOriginPos)));
        aimDir = XMVector3Normalize(XMVectorLerp(bossFront, toFist, anim));
    }
    else if (m_bossPhase == BossPhase::NORMAL || m_bossPhase == BossPhase::COOLDOWN)
    {
        // 射撃フェーズ：プレイヤー追従
        XMFLOAT3 playerPos = Player_GetPosition();
        XMFLOAT3 target    = { playerPos.x, playerPos.y + 0.3f, playerPos.z };
        XMVECTOR toTarget  = XMVectorSet(
            target.x - barrelOriginPos.x,
            target.y - barrelOriginPos.y,
            target.z - barrelOriginPos.z, 0.0f);
        aimDir = XMVectorGetX(XMVector3LengthSq(toTarget)) > 0.001f
               ? XMVector3Normalize(toTarget)
               : bossFront;
    }
    else
    {
        aimDir = bossFront; // 突進中：正面固定
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

    // 行ベクトル形式（Player.cpp の aimRot と完全同形式）
    XMMATRIX aimRot(
        ax.x, ax.y, ax.z, 0.0f,
        ay.x, ay.y, ay.z, 0.0f,
        az.x, az.y, az.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // 上段バレル：底面が下（モデル中心方向）を向くよう上下反転
    // 下段バレル：BARREL_FLIP_DEG で通常補正（底面が上＝モデル中心方向）
    XMMATRIX localRot = isUpper[faceIndex]
        ? XMMatrixRotationZ(XMConvertToRadians(BARREL_FLIP_DEG + 180.0f)) // 上段: 反転
        : XMMatrixRotationZ(XMConvertToRadians(BARREL_FLIP_DEG));          // 下段: 通常

    return localRot * aimRot * barrelTrans;
}

//==============================================================================
// 射撃処理
//
// ■役割
// ・左右バレルの先端（マズル）位置を取得する
// ・各バレルからプレイヤーの胴体に向けて弾を1発ずつ発射する
// ・発射と同時にショットガンSEを再生する
//==============================================================================
void EnemyBoss::Shoot()
{
    XMFLOAT3 playerPos = Player_GetPosition();
    XMFLOAT3 target = { playerPos.x, playerPos.y + 0.3f, playerPos.z };

    if (!m_pBarrel[0]) return;
    const AABB barrelLocal = ModelGetAABB(m_pBarrel[0], { 0.0f, 0.0f, 0.0f });

    // Drawと同じ4面配置から全門発射
    for (int i = 0; i < BARREL_COUNT; ++i)
    {
        if (!m_pBarrel[i]) continue;

        XMMATRIX barrelWorld = GetBarrelWorldMatrix(i);

        // バレル先端（ローカルZ最小＝マズル）をワールド座標に変換
        XMVECTOR muzzleLocal = XMVectorSet(0.0f, 0.0f, barrelLocal.min.z, 1.0f);
        XMVECTOR muzzleWorld = XMVector3TransformCoord(muzzleLocal, barrelWorld);

        XMFLOAT3 muzzlePos;
        XMStoreFloat3(&muzzlePos, muzzleWorld);

        XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&target) - muzzleWorld);
        XMFLOAT3 vel;
        XMStoreFloat3(&vel, dir);

        EnemyBullet_Create(muzzlePos, vel, SHOOT_DAMAGE, BOSS_BULLET_SPEED);
    }

    // ショットガンSE再生
    PlayAudio(m_shootSE, false);
}

//==============================================================================
// 散弾処理
//
// ■役割
// ・中央バレル位置から±30°の扇状に5発同時発射する
// ・Shoot() の代わりに3発に1回呼ばれる
//==============================================================================
void EnemyBoss::SpreadShoot()
{
    XMFLOAT3 playerPos = Player_GetPosition();
    XMFLOAT3 target    = { playerPos.x, playerPos.y + 0.3f, playerPos.z };

    // マズル位置を先に取得（方向計算の基点になる）
    if (!m_pBarrel[0]) { Shoot(); return; }
    const AABB barrelLocal = ModelGetAABB(m_pBarrel[0], { 0.0f, 0.0f, 0.0f });
    // 前面バレル（index 0）を基点にスプレッド射撃
    XMMATRIX   barrelCenter = GetBarrelWorldMatrix(0);
    XMVECTOR   muzzleLocal  = XMVectorSet(0.0f, 0.0f, barrelLocal.min.z, 1.0f);
    XMVECTOR   muzzleWorld  = XMVector3TransformCoord(muzzleLocal, barrelCenter);
    XMFLOAT3   muzzlePos;
    XMStoreFloat3(&muzzlePos, muzzleWorld);

    // マズル→プレイヤーの方向で基準角を計算（ボス中心ではなくマズル基点）
    float dx         = target.x - muzzlePos.x;
    float dy         = target.y - muzzlePos.y;
    float dz         = target.z - muzzlePos.z;
    float horizDist  = sqrtf(dx * dx + dz * dz);
    float baseYaw    = atan2f(dx, dz);
    float cosEl      = (horizDist > 0.001f) ? cosf(atan2f(dy, horizDist)) : 1.0f;
    float sinEl      = (horizDist > 0.001f) ? sinf(atan2f(dy, horizDist)) : 0.0f;

    // 扇状5発（±30°, ±15°, 中央）
    const float spreadAngles[] = { -30.0f, -15.0f, 0.0f, 15.0f, 30.0f };
    for (float deg : spreadAngles)
    {
        float a      = baseYaw + XMConvertToRadians(deg);
        XMFLOAT3 vel = { sinf(a) * cosEl, sinEl, cosf(a) * cosEl };
        EnemyBullet_Create(muzzlePos, vel, SHOOT_DAMAGE, BOSS_BULLET_SPEED);
    }

    PlayAudio(m_shootSE, false);
}