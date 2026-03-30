/*==============================================================================

   エネミー制御 [Enemy.cpp]
                                                         Author : 51106
                                                         Date   : 2026/01/16
--------------------------------------------------------------------------------

==============================================================================*/

#include "Enemy.h"
#include "model.h"
#include "map.h"
#include "Player.h"
#include "bullet.h"
#include "Light.h"
#include "Player_Camera.h"
#include "collision_obb.h"
#include "EnemyAI.h"
#include "MapPatrolAI.h"
#include <algorithm>
#include <cmath>
#include "Audio.h"
#include "score.h"
#include "ItemManager.h"
#include "DamagePopup.h"
#include "Shadow_Map.h"

using namespace DirectX;

namespace
{
    int g_enemy_hitSE = -1;
    int g_enemy_deadSE = -1;
}

void Enemy_LoadSE()
{
    if (g_enemy_hitSE < 0)
        g_enemy_hitSE = LoadAudioWithVolume("resource/sound/hit.wav", 0.2f);

    if (g_enemy_deadSE < 0)
        g_enemy_deadSE = LoadAudioWithVolume("resource/sound/dead.wav", 1.0f);
}

void Enemy_UnloadSE()
{
    if (g_enemy_hitSE >= 0) { UnloadAudio(g_enemy_hitSE);  g_enemy_hitSE = -1; }
    if (g_enemy_deadSE >= 0) { UnloadAudio(g_enemy_deadSE); g_enemy_deadSE = -1; }
}

void Enemy_PlayDeathSE()
{
    PlayAudio(g_enemy_deadSE, false);
}



//==============================================================================
// 初期化
//==============================================================================
void Enemy::Initialize(const XMFLOAT3& position)
{
    m_Position = position;
    m_Velocity = { 0,0,0 };
    m_Front = { 0,0,1 };
    m_Destination = MapPatrolAI_GetReachableDestination(m_Position); // 初期目的地設定(視界内の目的地のみ設定に変更)
    m_IsAlive = true;
    m_IsGround = false;
    m_WasChasing = false;
    //＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    // HP設定
    //＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
    SetHP(300, 300);

    m_pModel = ModelLoad("resource/Models/enemy.fbx", ENEMY_SIZE);
    ComputeLockOnOffsetFromModel();

    Enemy_LoadSE();
}

//==============================================================================
// モデルAABBからロックオンYオフセットを自動計算
//==============================================================================
void Enemy::ComputeLockOnOffsetFromModel()
{
    if (!m_pModel) return;
    const XMFLOAT3 origin{ 0.0f, 0.0f, 0.0f };
    AABB aabb = ModelGetAABB(m_pModel, origin);

    // モデルのローカル中心・サイズ（スケール済み）
    const float localMinY   = aabb.min.y;
    const float localMaxY   = aabb.max.y;
    const float localCenterY = (localMinY + localMaxY) * 0.5f;
    const float halfH        = (localMaxY - localMinY) * 0.5f;
    const float halfW        = std::max(
        (aabb.max.x - aabb.min.x) * 0.5f,
        (aabb.max.z - aabb.min.z) * 0.5f);

    // ロックオンY（Draw の ENEMY_HEIGHT オフセット分を足す）
    m_lockOnCenterOffset = ENEMY_HEIGHT + localCenterY;

    // 衝突OBBをモデルAABBに合わせる
    m_obbHalfWidth  = halfW;
    m_obbHalfHeight = halfH;
    m_obbBottomY    = ENEMY_HEIGHT + localCenterY; // m_Position.y からOBB中心まで
}

//==============================================================================
// 終了処理
//==============================================================================
void Enemy::Finalize()
{
    ModelRelease(m_pModel);
    m_pModel = nullptr;
}

//==============================================================================
// 更新処理
//==============================================================================
void Enemy::Update(double elapsed_time)
{
    if (!m_IsAlive) return;
    if (elapsed_time > 1.0 / 30.0)
        elapsed_time = 1.0 / 30.0;

    float dt = static_cast<float>(elapsed_time);

    XMVECTOR pos = XMLoadFloat3(&m_Position);
    XMVECTOR vel = XMLoadFloat3(&m_Velocity);

    // プレイヤーとの距離を先に計算（AI スキップ判定で使う）
    XMFLOAT3 playerPosEarly  = Player_GetPosition();
    XMVECTOR vPlayerPosEarly = XMLoadFloat3(&playerPosEarly);
    float    distToPlayerXZ  = sqrtf(XMVectorGetX(
                                   XMVector3LengthSq(
                                       XMVectorSetY(vPlayerPosEarly - pos, 0.0f))));

    // 　AI処理（追跡 + 索敵 + 巡回）
    // 攻撃中 または 攻撃射程内（クールダウン中も含む）は AI の速度上書きをスキップ。
    // 射程内で AI が押し続けると衝突の押し返しと競合して前後振動が発生するため。
    if (!m_IsAttacking && distToPlayerXZ >= ATTACK_RANGE)
    {
        EnemyAI_Update(
            &m_Position,
            &m_Velocity,
            &m_Front,
            &m_Destination,
            &m_WasChasing,
            &m_LastSeenPos,
            &m_InvestigateTimer,
            dt,
            CHASE_SPD,   // 通常エネミーの追跡速度
            PATROL_SPD,  // 通常エネミーの巡回速度
            SIGHT_DIST); // 視野距離
    }

    // AI処理後の速度を再ロード
    vel = XMLoadFloat3(&m_Velocity);

    //==========================================================================
    // 攻撃モーション（溜め → ダッシュ）
    //==========================================================================
    {
        XMFLOAT3 playerPosF  = Player_GetPosition();
        XMVECTOR vPlayerPos  = XMLoadFloat3(&playerPosF);
        XMVECTOR vToPlayerXZ = XMVectorSetY(vPlayerPos - pos, 0.0f);
        float    distXZ      = sqrtf(XMVectorGetX(XMVector3LengthSq(vToPlayerXZ)));

        // クールダウンカウントダウン（攻撃中は止める）
        if (!m_IsAttacking && m_AttackCooldown > 0.0f)
            m_AttackCooldown -= dt;

        // 近距離でかつ攻撃していなければ溜め開始（クールダウン中は再発動しない）
        if (!m_IsAttacking && m_AttackCooldown <= 0.0f && distXZ < ATTACK_RANGE)
        {
            m_IsAttacking = true;
            m_AttackTimer = 0.0f;
        }

        if (m_IsAttacking)
        {
            m_AttackTimer += dt;

            if (m_AttackTimer < ATTACK_WINDUP)
            {
                // 溜め中：XZ 速度を急減速（その場で止まる）
                float brake = std::max(0.0f, 1.0f - 15.0f * dt);
                vel = XMVectorSetX(vel, XMVectorGetX(vel) * brake);
                vel = XMVectorSetZ(vel, XMVectorGetZ(vel) * brake);
            }
            else if (m_AttackTimer < ATTACK_WINDUP + 0.2f)
            {
                // ダッシュ：プレイヤー方向へ高速突進
                if (XMVectorGetX(XMVector3LengthSq(vToPlayerXZ)) > 0.001f)
                {
                    XMVECTOR dir = XMVector3Normalize(vToPlayerXZ);
                    vel = XMVectorSetX(vel, XMVectorGetX(dir) * ATTACK_DASH_SPD);
                    vel = XMVectorSetZ(vel, XMVectorGetZ(dir) * ATTACK_DASH_SPD);
                }
            }
            else
            {
                // 攻撃終了 → クールダウンセット
                m_IsAttacking  = false;
                m_AttackTimer  = 0.0f;
                m_AttackCooldown = ATTACK_COOLDOWN;
            }
        }

        XMStoreFloat3(&m_Velocity, vel);
    }

    // 重力加算
    vel += XMVectorSet(0, -9.8f * GRAVITY_MUL * dt, 0, 0);

    // 速度制限（攻撃ダッシュ中は ATTACK_DASH_SPD、それ以外は m_SpeedCap まで許容）
    // m_SpeedCap はサブクラス（EnemyBoss 等）が突進時に引き上げて使う
    vel = ClampXZSpeed(vel, m_IsAttacking ? ATTACK_DASH_SPD : m_SpeedCap);

    // 摩擦
    vel += -vel * (FRICTION * dt);

    // サブステップ移動
    MoveWithSubSteps(&pos, &vel, dt);

    // 床判定
    ResolveFloorCollision(&pos, &vel);

    // 　プレイヤー衝突（ダメージ＋ノックバック）
    ResolvePlayerCollision(&pos, &vel);

    // 位置・速度を保存
    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Velocity, vel);

    // 接触ダメージクールダウン
    if (m_ContactDamageCooldown > 0.0f)
        m_ContactDamageCooldown -= dt;

    // 弾ヒット処理
    ResolveBulletHits();

    // 死亡判定
    if (IsDead()) m_IsAlive = false;
}

//==============================================================================
// 描画処理
//==============================================================================
void Enemy::Draw()
{
    if (!m_IsAlive) return;
    if (!m_pModel) return;

    Light_SetSpecularWorld(
        Player_Camera_GetPosition(),
        4.0f,
        { 0.3f,0.25f,0.2f,1.0f }
    );

    float angle =
        -atan2f(m_Front.z, m_Front.x)
        + XMConvertToRadians(0.0f);

    XMMATRIX rot =
        XMMatrixRotationX(XMConvertToRadians(00.0f)) *
        XMMatrixRotationY(angle) *
        XMMatrixRotationY(XMConvertToRadians(-90.0f));

    XMMATRIX trans =
        XMMatrixTranslation(
            m_Position.x,
            m_Position.y + ENEMY_HEIGHT * 1.0f,
            m_Position.z);

    ModelDraw(m_pModel, rot * trans);
}

//==============================================================================
// シャドウパス用深度描画
//==============================================================================
void Enemy::DrawShadow()
{
    if (!m_IsAlive || !m_pModel) return;

    float angle = -atan2f(m_Front.z, m_Front.x);
    XMMATRIX rot =
        XMMatrixRotationY(angle) *
        XMMatrixRotationY(XMConvertToRadians(-90.0f));

    XMMATRIX trans =
        XMMatrixTranslation(
            m_Position.x,
            m_Position.y + ENEMY_HEIGHT * 1.0f,
            m_Position.z);

    ShadowMap::DrawModel(m_pModel, rot * trans);
}

//==============================================================================
// OBB取得
//==============================================================================
OBB Enemy::GetOBB() const
{
    XMFLOAT3 halfExtents = { m_obbHalfWidth, m_obbHalfHeight, m_obbHalfWidth };

    XMFLOAT3 center = {
        m_Position.x,
        m_Position.y + m_obbBottomY,
        m_Position.z
    };

    return OBB::CreateFromFront(center, halfExtents, m_Front);
}

//==============================================================================
// AABB取得（デバッグ用・旧コード互換用）
//==============================================================================
AABB Enemy::GetAABB() const
{
    return {
        {
            m_Position.x - ENEMY_HALF_WIDTH_X,
            m_Position.y,
            m_Position.z - ENEMY_HALF_WIDTH_Z
        },
        {
            m_Position.x + ENEMY_HALF_WIDTH_X,
            m_Position.y + ENEMY_HEIGHT,
            m_Position.z + ENEMY_HALF_WIDTH_Z
        }
    };
}

//==============================================================================
// 位置取得
//==============================================================================
const XMFLOAT3& Enemy::GetPosition() const
{
    return m_Position;
}

//==============================================================================
// 速度ポインタ取得（ノックバック用）
//==============================================================================
DirectX::XMFLOAT3* Enemy::GetVelocityPtr()
{
    return &m_Velocity;
}

//==============================================================================
// HP設定
//==============================================================================
void Enemy::SetHP(int hp, int maxHp)
{
    m_MaxHp = std::max(1, maxHp);
    m_Hp = std::clamp(hp, 0, m_MaxHp);
}

//==============================================================================
// ダメージ処理
//==============================================================================
void Enemy::Damage(int value)
{
    m_Hp -= std::max(1, value);
    if (m_Hp < 0) m_Hp = 0;
}

//==============================================================================
// HP取得
//==============================================================================
int Enemy::GetHP() const { return m_Hp; }

//==============================================================================
// 最大HP取得
//==============================================================================
int Enemy::GetMaxHP() const { return m_MaxHp; }

//==============================================================================
// 死亡判定
//==============================================================================
bool Enemy::IsDead() const { return m_Hp <= 0; }

//==============================================================================
// 内部ヘルパー関数
//==============================================================================

//==============================================================================
// 任意の位置ベクトルからエネミー用 OBB を作成
//==============================================================================
OBB Enemy::ConvertPositionToOBB(const XMVECTOR& pos) const
{
    XMFLOAT3 position;
    XMStoreFloat3(&position, pos);

    XMFLOAT3 halfExtents = { m_obbHalfWidth, m_obbHalfHeight, m_obbHalfWidth };

    XMFLOAT3 center = {
        position.x,
        position.y + m_obbBottomY,
        position.z
    };

    return OBB::CreateFromFront(center, halfExtents, m_Front);
}

//==============================================================================
// XZ速度制限
//==============================================================================
XMVECTOR Enemy::ClampXZSpeed(XMVECTOR v, float maxSpeed) const
{
    float vx = XMVectorGetX(v);
    float vz = XMVectorGetZ(v);
    float lenSq = vx * vx + vz * vz;
    if (lenSq <= maxSpeed * maxSpeed) return v;

    float inv = 1.0f / sqrtf(lenSq);
    return XMVectorSet(
        vx * inv * maxSpeed,
        XMVectorGetY(v),
        vz * inv * maxSpeed,
        0);
}

//==============================================================================
// 壁衝突解決（OBB ↔ AABB）
//==============================================================================
void Enemy::ResolveWallCollisionAtPosition(XMVECTOR* ioPos, XMVECTOR* ioVel, XMFLOAT3* ioDest)
{
    // 壁との衝突は「円（Circle）」として処理する。
    // OBB は m_Front で回転するため角が斜め方向に伸び、斜め向き時にケツが壁に刺さる。
    // 円なら向き不依存 → コーナー引っかかりが発生しない。
    constexpr float TELEPORT_THRESHOLD = 0.3f;
    constexpr float r = ENEMY_HALF_WIDTH_X; // 円半径（= 0.25f）

    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* mo = Map_GetObject(i);
        if (!mo) continue;
        if (mo->KindId != 2) continue;

        const AABB& a = mo->Aabb;
        XMFLOAT3 pos;
        XMStoreFloat3(&pos, *ioPos);

        // 壁 AABB に対するエネミー中心の最近接点を求める
        const float clampedX = std::clamp(pos.x, a.min.x, a.max.x);
        const float clampedZ = std::clamp(pos.z, a.min.z, a.max.z);
        const float dx = pos.x - clampedX;
        const float dz = pos.z - clampedZ;
        const float distSq = dx * dx + dz * dz;

        if (distSq >= r * r) continue; // 衝突なし

        // めり込み量（= 押し出すべき距離）
        const float dist  = sqrtf(distSq);
        const float push  = r - dist;

        if (push >= TELEPORT_THRESHOLD)
        {
            // 深いめり込み → 安全地点にテレポート
            XMFLOAT3 safePos = MapPatrolAI_GetNearbyDestination(pos, 3.0f);
            *ioPos  = XMLoadFloat3(&safePos);
            *ioVel  = XMVectorSet(0.0f, XMVectorGetY(*ioVel), 0.0f, 0.0f);
            *ioDest = MapPatrolAI_GetNearbyDestination(safePos, 3.0f);
            break;
        }

        if (dist > 0.001f)
        {
            // Circle 中心が AABB 外 → 最近接方向（法線）に押し出す
            const float nx = dx / dist;
            const float nz = dz / dist;
            *ioPos = XMVectorSetX(*ioPos, pos.x + nx * push);
            *ioPos = XMVectorSetZ(*ioPos, pos.z + nz * push);

            // 壁法線方向の速度成分のみ消す（平行成分は維持 → 壁沿いスライドが滑らか）
            XMFLOAT3 vel;
            XMStoreFloat3(&vel, *ioVel);
            const float vDotN = vel.x * nx + vel.z * nz;
            if (vDotN < 0.0f) // 壁に食い込む方向の速度のみ除去
            {
                vel.x -= vDotN * nx;
                vel.z -= vDotN * nz;
                *ioVel = XMLoadFloat3(&vel);
            }
        }
        else
        {
            // Circle 中心が AABB 内（最悪ケース）→ 最短軸で押し出す
            const float wallCx   = (a.min.x + a.max.x) * 0.5f;
            const float wallCz   = (a.min.z + a.max.z) * 0.5f;
            const float overlapX = (r + (a.max.x - a.min.x) * 0.5f) - fabsf(pos.x - wallCx);
            const float overlapZ = (r + (a.max.z - a.min.z) * 0.5f) - fabsf(pos.z - wallCz);
            if (overlapX < overlapZ)
            {
                const float sign = (pos.x < wallCx) ? -1.0f : 1.0f;
                *ioPos = XMVectorSetX(*ioPos, pos.x + sign * overlapX);
                *ioVel = XMVectorSetX(*ioVel, 0.0f);
            }
            else
            {
                const float sign = (pos.z < wallCz) ? -1.0f : 1.0f;
                *ioPos = XMVectorSetZ(*ioPos, pos.z + sign * overlapZ);
                *ioVel = XMVectorSetZ(*ioVel, 0.0f);
            }
        }

        // 押し戻し後の位置から目的地を更新
        XMFLOAT3 newPos;
        XMStoreFloat3(&newPos, *ioPos);
        *ioDest = MapPatrolAI_GetNearbyDestination(newPos, 3.0f);
        break;
    }
}

//==============================================================================
// サブステップ移動（トンネリング対策）
//==============================================================================
void Enemy::MoveWithSubSteps(XMVECTOR* ioPos, XMVECTOR* ioVel, float dt)
{
    XMVECTOR delta = (*ioVel) * dt;
    float len = sqrtf(
        powf(XMVectorGetX(delta), 2.0f) +
        powf(XMVectorGetZ(delta), 2.0f)
    );

    int steps = static_cast<int>(ceilf(len / SUBSTEP_MAX_STEP));
    if (steps < 1) steps = 1;
    if (steps > SUBSTEP_MAX_COUNT) steps = SUBSTEP_MAX_COUNT;

    float stepDt = dt / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i)
    {
        *ioPos += (*ioVel) * stepDt;
        ResolveWallCollisionAtPosition(ioPos, ioVel, &m_Destination);
    }
}

//==============================================================================
// 床衝突解決（OBB ↔ AABB）
//==============================================================================
void Enemy::ResolveFloorCollision(XMVECTOR* ioPos, XMVECTOR* ioVel)
{
    OBB enemyOBB = ConvertPositionToOBB(*ioPos);
    float supportY = 0.0f;
    bool foundFloor = false;

    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* mo = Map_GetObject(i);
        if (!mo) continue;
        if (mo->KindId != 1) continue; // KIND_FLOOR

        const AABB& floor = mo->Aabb;

        // XZ重なり判定（簡易）
        bool overlapX = (enemyOBB.center.x - ENEMY_HALF_WIDTH_X <= floor.max.x &&
            enemyOBB.center.x + ENEMY_HALF_WIDTH_X >= floor.min.x);
        bool overlapZ = (enemyOBB.center.z - ENEMY_HALF_WIDTH_Z <= floor.max.z &&
            enemyOBB.center.z + ENEMY_HALF_WIDTH_Z >= floor.min.z);

        if (!overlapX || !overlapZ) continue;

        const float eps = 0.02f;
        float enemyBottom = XMVectorGetY(*ioPos);

        if (enemyBottom <= floor.max.y + eps)
        {
            if (!foundFloor || floor.max.y > supportY)
            {
                supportY = floor.max.y;
                foundFloor = true;
            }
        }
    }

    if (foundFloor)
    {
        float currentY = XMVectorGetY(*ioPos);
        if (currentY <= supportY)
        {
            *ioPos = XMVectorSetY(*ioPos, supportY);
            *ioVel = XMVectorSetY(*ioVel, 0.0f);
            m_IsGround = true;
        }
    }
    else
    {
        m_IsGround = false;
    }
}

//==============================================================================
// プレイヤー衝突処理（ダメージ＋ノックバック）
//==============================================================================
void Enemy::ResolvePlayerCollision(XMVECTOR* ioPos, XMVECTOR* ioVel)
{
    if (!Player_IsEnable()) return;

    OBB enemyOBB  = ConvertPositionToOBB(*ioPos);
    OBB playerOBB = Player_GetOBB();

    // OBB同士の衝突判定
    if (!Collision_IsOverlapOBB(enemyOBB, playerOBB))
    {
        return;

    }

    // 接触ダメージ（クールダウン中はダメージとSEをスキップ）
    constexpr int   ENEMY_DAMAGE            = 90;
    constexpr float CONTACT_DAMAGE_INTERVAL = 0.5f; // 0.5秒に1回ダメージ
    if (m_ContactDamageCooldown <= 0.0f)
    {
        Player_TakeDamage(ENEMY_DAMAGE);
        PlayAudio(g_enemy_hitSE, false);
        m_ContactDamageCooldown = CONTACT_DAMAGE_INTERVAL;
    }

    // 　ノックバック処理（プレイヤーを押し出す）
    XMFLOAT3* playerVelPtr = Player_GetVelocityPtr();
    XMFLOAT3 playerPosF = Player_GetPosition();

    // プレイヤーからエネミーへのベクトル（逆向き = 押し出し方向）
    XMVECTOR toEnemy = XMLoadFloat3(&m_Position) - XMLoadFloat3(&playerPosF);
    toEnemy = XMVectorSetY(toEnemy, 0.0f); // Y成分無視（水平方向のみ）

    // 正規化してノックバック速度を加算
    if (XMVectorGetX(XMVector3LengthSq(toEnemy)) > 0.0001f)
    {
        constexpr float KNOCKBACK_STRENGTH = 15.0f; // ノックバック強度（5.0f = 弱い / 15.0f = 強い / 25.0f = 超強い）
        XMVECTOR knockback = XMVector3Normalize(toEnemy) * -KNOCKBACK_STRENGTH;
        XMVECTOR currentVel = XMLoadFloat3(playerVelPtr);
        XMStoreFloat3(playerVelPtr, currentVel + knockback);
    }
}

//==============================================================================
// 弾ヒット処理（OBB重なり + レイキャストによるすり抜け防止）
//==============================================================================
void Enemy::ResolveBulletHits()
{
    if (!m_IsAlive) return;

    for (int i = 0; i < Bullet_GetCount(); ++i)
    {
        OBB bulletOBB = Bullet_GetOBB(i);
        OBB enemyOBB  = GetOBB();

        // ① 通常のOBB重なり判定
        bool hit = Collision_IsOverlapOBB(bulletOBB, enemyOBB);

        // ② すり抜け防止：前フレーム位置→現在位置のレイキャスト判定
        if (!hit)
        {
            const XMFLOAT3& prevPos = Bullet_GetPrevPosition(i);
            hit = OBB_RaySegmentIntersect(enemyOBB, prevPos, bulletOBB.center);
        }

        if (!hit)
            continue;

        //ヒットSE
        PlayAudio(g_enemy_hitSE, false);

        // ダメージ量を取得
        const int bulletDamage = Bullet_GetDamage(i);

        // ビーム弾は貫通するため削除しない
        if (!Bullet_IsBeam(i))
        {
            Bullet_Destroy(i);
            --i;
        }

        Damage(bulletDamage);

        // ダメージポップアップ（被弾位置にランダムオフセットで表示）
        {
            XMFLOAT3 popupPos = bulletOBB.center;
            popupPos.x += (rand() % 41 - 20) * 0.01f;  // ±0.2f ランダムずれ
            popupPos.z += (rand() % 41 - 20) * 0.01f;
            DamagePopup_Add(popupPos, bulletDamage);
        }

        if (IsDead())
        {
            //死亡SE
            PlayAudio(g_enemy_deadSE, false);
            Score_Addscore(1000);//1000点追加

            ItemManager_SpawnRandom(m_Position);//アイテムをランダムでスポーン
            break;
        }
    }
}
