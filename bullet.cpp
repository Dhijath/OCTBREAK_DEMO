/*==============================================================================

   弾丸の管理 [Bullet.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/17
--------------------------------------------------------------------------------

   ■設計
   ・BulletBase   : 基底クラス（ダメージ量保持）
   ・Bullet       : 通常弾（モデル描画）
   ・BeamBullet   : ビーム弾（パーティクル描画、壁衝突消滅、エネミー貫通）
   ・BulletManager: 全弾を一元管理
   ・グローバル関数: BulletManagerシングルトンに委譲

==============================================================================*/

#include <windows.h>
#include "bullet.h"
#include "Trail.h"
#include "model.h"
#include "bullet_hit_effect.h"
#include "collision_obb.h"
#include "texture.h"
#include "map.h"
#include "collision.h"
#include "particle_thruster.h"
#include "light.h"
#include "player_camera.h"

using namespace DirectX;

//==============================================================================
// Bullet（通常弾）実装
//==============================================================================

//==============================================================================
// コンストラクタ
//
// ■役割
// ・初期位置・速度・ダメージを設定する
//
// ■引数
// ・pos    : 初期位置
// ・vel    : 初期速度
// ・damage : ダメージ量
//==============================================================================
Bullet::Bullet(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage)
    : m_position(pos), m_prevPosition(pos), m_velocity(vel), m_damage(damage)
{
    // 暖色系のリボントレイル（弾の飛跡）
    m_trail.Initialize(20, 0.07f, 0.18f, { 1.0f, 0.7f, 0.2f, 0.9f });
}

Bullet::~Bullet()
{
    // m_trail.Finalize() は Trail::~Trail() が自動で呼ぶため不要
    // ここで明示呼び出しすると二重 Finalize になり s_RefCount が
    // 余分に 0 になって他の Trail インスタンス（スラスター等）の
    // 共有 GPU リソースが解放されてしまう
}

//==============================================================================
// 更新処理
//
// ■役割
// ・前フレーム位置を保存してから速度で位置を更新する
// ・経過時間を累積して寿命を管理する
//
// ■引数
// ・elapsed_time : 経過時間（秒）
//==============================================================================
void Bullet::Update(double elapsed_time)
{
    m_prevPosition = m_position;
    XMStoreFloat3(
        &m_position,
        XMLoadFloat3(&m_position) + XMLoadFloat3(&m_velocity) * static_cast<float>(elapsed_time)
    );
    m_accumulatedTime += elapsed_time;

    m_trail.Update(elapsed_time, m_position);
}

void Bullet::DrawTrail()
{
    m_trail.Draw();
}

//==============================================================================
// 消滅判定
//
// ■戻り値
// ・true  : 寿命切れ
// ・false : 生存中
//==============================================================================
bool Bullet::IsDestroyed() const
{
    return m_accumulatedTime >= LIFE_TIME;
}

//==============================================================================
// 現在位置取得
//
// ■戻り値
// ・現在のワールド座標
//==============================================================================
const XMFLOAT3& Bullet::GetPosition() const
{
    return m_position;
}

//==============================================================================
// 1フレーム前の位置取得
//
// ■戻り値
// ・1フレーム前のワールド座標
//==============================================================================
const XMFLOAT3& Bullet::GetPrevPosition() const
{
    return m_prevPosition;
}

//==============================================================================
// 進行方向取得
//
// ■役割
// ・速度ベクトルを正規化して向きを返す
//
// ■戻り値
// ・正規化された進行方向ベクトル
//==============================================================================
XMFLOAT3 Bullet::GetFront() const
{
    XMVECTOR v = XMLoadFloat3(&m_velocity);
    if (XMVectorGetX(XMVector3LengthSq(v)) < 0.0001f) return { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 f;
    XMStoreFloat3(&f, XMVector3Normalize(v));
    return f;
}

//==============================================================================
// BeamBullet（ビーム弾）実装
//==============================================================================

//==============================================================================
// コンストラクタ
//
// ■役割
// ・初期位置・速度・ダメージを設定する
// ・速度をBEAM_SPEEDに正規化する
// ・ThrusterEmitterを生成してパーティクルの見た目を設定する
//
// ■引数
// ・pos       : 初期位置
// ・vel       : 初期速度
// ・damage    : ダメージ量
// ・beamTexID : パーティクル用テクスチャID
//==============================================================================
BeamBullet::BeamBullet(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage, int beamTexID)
    : m_position(pos), m_prevPosition(pos), m_velocity(vel), m_damage(damage)
{
    // 速度を正規化してBEAM_SPEEDに合わせる
    XMVECTOR v = XMLoadFloat3(&vel);
    if (XMVectorGetX(XMVector3LengthSq(v)) > 0.0001f)
    {
        XMStoreFloat3(&m_velocity, XMVector3Normalize(v) * BEAM_SPEED);
    }

    // パーティクルエミッター生成
    m_emitter = new ThrusterEmitter(XMLoadFloat3(&m_position), 300.0, true);
    m_emitter->SetParticleTextureId(beamTexID);
    m_emitter->SetScaleRange(0.5f, 0.8f);       // スケール範囲
    m_emitter->SetSpeedRange(0.5f, 1.5f);         // 速度範囲
    m_emitter->SetLifeRange(0.08f, 0.15f);        // 寿命範囲
    m_emitter->SetConeAngleDeg(8.0f);             // 噴射の広がり角度
    m_emitter->SetColor({ 0.1f, 0.1f, 3.0f, 1.0f });  // パーティクル色（青白）
    m_emitter->SetUVRect({ 0.0f, 0.0f, 256.0f, 64.0f });  // テクスチャUV範囲
}

//==============================================================================
// デストラクタ
//
// ■役割
// ・ThrusterEmitterを解放する
//==============================================================================
BeamBullet::~BeamBullet()
{
    delete m_emitter;
}

//==============================================================================
// 壁衝突チェック
//
// ■役割
// ・現在位置がマップの壁AABBと重なっていたら消滅フラグを立てる
// ・エフェクトを前フレーム位置で生成する
//==============================================================================
void BeamBullet::CheckWallCollision()
{
    // 現在位置の簡易AABB
    AABB beamAABB
    {
        { m_position.x - BEAM_WIDTH, m_position.y - BEAM_HEIGHT, m_position.z - BEAM_WIDTH },
        { m_position.x + BEAM_WIDTH, m_position.y + BEAM_HEIGHT, m_position.z + BEAM_WIDTH }
    };

    // 全マップオブジェクトと衝突チェック
    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* mo = Map_GetObject(i);
        if (!mo) continue;
        if (mo->KindId != 2) continue;  // 壁のみチェック

        if (Collision_IsOverLapAABB(beamAABB, mo->Aabb))
        {
            // エフェクト生成
            BulletHitEffect_Create(m_prevPosition);

            // 消滅フラグを立てる
            m_destroyed = true;

            // パーティクル放出を停止
            m_emitter->Emmit(false);
            return;
        }
    }
}

//==============================================================================
// 更新処理
//
// ■役割
// ・前フレーム位置を保存してから速度で位置を更新する
// ・壁衝突チェックを行い、衝突時に消滅フラグを立てる
// ・エミッター位置と向きを更新する
//
// ■引数
// ・elapsed_time : 経過時間（秒）
//==============================================================================
void BeamBullet::Update(double elapsed_time)
{
    const float dt = static_cast<float>(elapsed_time);

    m_prevPosition = m_position;

    // 消滅していなければ位置更新
    if (!m_destroyed)
    {
        XMStoreFloat3(
            &m_position,
            XMLoadFloat3(&m_position) + XMLoadFloat3(&m_velocity) * dt
        );

        m_accumulatedTime += elapsed_time;

        // 壁衝突チェック
        CheckWallCollision();
    }

    // エミッター位置と向きを更新
    if (m_emitter)
    {
        m_emitter->SetPosition(XMLoadFloat3(&m_position));

        XMFLOAT3 front = GetFront();
        m_emitter->SetWorldDirection(front);
        m_emitter->SetWorldUp({ 0.0f, 1.0f, 0.0f });

        // パーティクル更新
        m_emitter->Update(elapsed_time);
    }
}

//==============================================================================
// 描画
//
// ■役割
// ・ThrusterEmitterのパーティクルを描画する
//==============================================================================
void BeamBullet::Draw()
{
    if (m_emitter) m_emitter->Draw();
}

//==============================================================================
// 消滅判定
//
// ■戻り値
// ・true  : 壁衝突 or 寿命切れ
// ・false : 生存中
//==============================================================================
bool BeamBullet::IsDestroyed() const
{
    return m_destroyed || m_accumulatedTime >= LIFE_TIME;
}

//==============================================================================
// 現在位置取得
//
// ■戻り値
// ・現在のワールド座標
//==============================================================================
const XMFLOAT3& BeamBullet::GetPosition() const
{
    return m_position;
}

//==============================================================================
// 1フレーム前の位置取得
//
// ■戻り値
// ・1フレーム前のワールド座標
//==============================================================================
const XMFLOAT3& BeamBullet::GetPrevPosition() const
{
    return m_prevPosition;
}

//==============================================================================
// 進行方向取得
//
// ■役割
// ・速度ベクトルを正規化して向きを返す
//
// ■戻り値
// ・正規化された進行方向GetFront
//==============================================================================
XMFLOAT3 BeamBullet::GetFront() const
{
    XMVECTOR v = XMLoadFloat3(&m_velocity);
    if (XMVectorGetX(XMVector3LengthSq(v)) < 0.0001f) return { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 f;
    XMStoreFloat3(&f, XMVector3Normalize(v));
    return f;
}

//==============================================================================
// MissileBullet（ミサイル弾）実装
//==============================================================================

//==============================================================================
// コンストラクタ
//==============================================================================
MissileBullet::MissileBullet(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage, float explosionRadius)
    : m_position(pos), m_prevPosition(pos), m_velocity(vel), m_damage(damage), m_explosionRadius(explosionRadius)
{
}

//==============================================================================
// 壁衝突チェック
//
// ■役割
// ・現在位置がマップの壁AABBと重なっていたら消滅フラグを立てる
//==============================================================================
void MissileBullet::CheckWallCollision()
{
    AABB missileAABB
    {
        { m_position.x - MISSILE_SIZE, m_position.y - MISSILE_SIZE, m_position.z - MISSILE_SIZE },
        { m_position.x + MISSILE_SIZE, m_position.y + MISSILE_SIZE, m_position.z + MISSILE_SIZE }
    };

    for (int i = 0; i < Map_GetObjectsCount(); ++i)
    {
        const MapObject* mo = Map_GetObject(i);
        if (!mo) continue;
        if (mo->KindId != 2) continue;  // 壁のみチェック

        if (Collision_IsOverLapAABB(missileAABB, mo->Aabb))
        {
            m_destroyed = true;
            return;
        }
    }
}

//==============================================================================
// 更新処理
//==============================================================================
void MissileBullet::Update(double elapsed_time)
{
    m_prevPosition = m_position;

    if (!m_destroyed)
    {
        XMStoreFloat3(
            &m_position,
            XMLoadFloat3(&m_position) + XMLoadFloat3(&m_velocity) * static_cast<float>(elapsed_time)
        );
        m_accumulatedTime += elapsed_time;
        CheckWallCollision();
    }
}

//==============================================================================
// 消滅判定
//==============================================================================
bool MissileBullet::IsDestroyed() const
{
    return m_destroyed || m_accumulatedTime >= LIFE_TIME;
}

//==============================================================================
// 現在位置取得
//==============================================================================
const XMFLOAT3& MissileBullet::GetPosition() const
{
    return m_position;
}

//==============================================================================
// 1フレーム前の位置取得
//==============================================================================
const XMFLOAT3& MissileBullet::GetPrevPosition() const
{
    return m_prevPosition;
}

//==============================================================================
// 進行方向取得
//==============================================================================
XMFLOAT3 MissileBullet::GetFront() const
{
    XMVECTOR v = XMLoadFloat3(&m_velocity);
    if (XMVectorGetX(XMVector3LengthSq(v)) < 0.0001f) return { 0.0f, 0.0f, 1.0f };
    XMFLOAT3 f;
    XMStoreFloat3(&f, XMVector3Normalize(v));
    return f;
}

//==============================================================================
// BulletManager 実装
//==============================================================================

//==============================================================================
// コンストラクタ
//==============================================================================
BulletManager::BulletManager() = default;

//==============================================================================
// デストラクタ
//
// ■役割
// ・終了処理を呼び出す
//==============================================================================
BulletManager::~BulletManager()
{
    Finalize();
}

//==============================================================================
// 初期化
//
// ■役割
// ・弾モデルとビームテクスチャのロード
// ・既存弾の全削除
//==============================================================================
void BulletManager::Initialize()
{
    // 再初期化に備えて既存リソースを解放
    Texture_Release(m_beamTexID);
    m_beamTexID = -1;
    ModelRelease(m_pModel);
    m_pModel = nullptr;

    for (int i = 0; i < m_count; i++)
    {
        delete m_bullets[i];
        m_bullets[i] = nullptr;
    }
    m_count = 0;
    m_explosionCount = 0;

    m_pModel = ModelLoad("Resource/Models/bullet.fbx", 0.15f);
    m_beamTexID = Texture_Load(L"Resource/Texture/effect000.jpg");
}

//==============================================================================
// 終了処理
//
// ■役割
// ・モデル解放と残弾の全削除
//==============================================================================
void BulletManager::Finalize()
{
    Texture_Release(m_beamTexID);
    m_beamTexID = -1;

    ModelRelease(m_pModel);
    m_pModel = nullptr;

    for (int i = 0; i < m_count; i++)
    {
        delete m_bullets[i];
        m_bullets[i] = nullptr;
    }
    m_count = 0;
    m_explosionCount = 0;
}

//==============================================================================
// 全弾クリア（アセット解放なし・ルーム遷移時用）
//==============================================================================
void BulletManager::ClearAll()
{
    for (int i = 0; i < m_count; i++)
    {
        delete m_bullets[i];
        m_bullets[i] = nullptr;
    }
    m_count = 0;
    m_explosionCount = 0;
}

//==============================================================================
// 全弾の更新
//
// ■役割
// ・全弾を更新し、消滅した弾をエフェクト生成後に削除する
//
// ■引数
// ・elapsed_time : 経過時間（秒）
//==============================================================================
void BulletManager::Update(double elapsed_time)
{
    for (int i = 0; i < m_count; i++)
    {
        // 弾を更新
        m_bullets[i]->Update(elapsed_time);

        // 消滅判定
        if (m_bullets[i]->IsDestroyed())
        {
            const BulletType type = m_bullets[i]->GetType();

            if (type == BulletType::Normal)
            {
                // 通常弾のみ寿命切れエフェクト（ビームは内部で生成済み）
                BulletHitEffect_Create(m_bullets[i]->GetPrevPosition());
            }
            else if (type == BulletType::Missile)
            {
                // ミサイル：爆発登録 + ヒットエフェクト
                auto* m = static_cast<MissileBullet*>(m_bullets[i]);
                AddExplosion(m->GetPrevPosition(), m->GetExplosionRadius(), m->GetDamage());
                BulletHitEffect_Create(m->GetPrevPosition());
            }
            // BulletType::Beam はCheckWallCollision()内でエフェクト生成済み

            // 弾を削除して配列を詰める
            delete m_bullets[i];
            m_bullets[i] = m_bullets[m_count - 1];
            m_bullets[m_count - 1] = nullptr;
            m_count--;
            i--;  // 詰めた分、インデックスを戻す
        }
    }
}

//==============================================================================
// 通常弾の描画（モデル）
//
// ■役割
// ・通常弾をモデルで描画する
//
// ■引数
// ・index : 描画する弾のインデックス
//==============================================================================

void BulletManager::DrawNormal(int index)
{
    Light_SetAmbient({ 10.0, 5.5f, 0.0f });

    const XMFLOAT3& posF = m_bullets[index]->GetPosition();

    // 弾自身の進行方向（速度の正規化）を使用
    const XMFLOAT3 bulletFrontF = m_bullets[index]->GetFront();

    XMVECTOR pos = XMLoadFloat3(&posF);
    XMVECTOR front = XMLoadFloat3(&bulletFrontF);

    // 念のため正規化（Player側で正規化済みでもOK）
    if (XMVectorGetX(XMVector3LengthSq(front)) < 0.0001f)
        front = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    else
        front = XMVector3Normalize(front);

    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // 「+Zをfrontに向ける」回転行列を作る（LookToの逆を使う）
    const XMMATRIX viewLike = XMMatrixLookToLH(XMVectorZero(), front, up);
    const XMMATRIX rot = XMMatrixInverse(nullptr, viewLike);

    const XMMATRIX trans = XMMatrixTranslationFromVector(pos);

    const XMMATRIX world = rot * trans;

    ModelDraw(m_pModel, world);

    Light_SetAmbient({ 1.0, 1.0, 1.0 });
}
//==============================================================================
// ビーム弾の描画（パーティクル）
//
// ■役割
// ・ビーム弾のパーティクルを描画する
//
// ■引数
// ・index : 描画する弾のインデックス
//==============================================================================
void BulletManager::DrawBeam(int index)
{
    BeamBullet* beam = static_cast<BeamBullet*>(m_bullets[index]);
    beam->Draw();
}

//==============================================================================
// 全弾の描画
//
// ■役割
// ・通常弾はモデル、ビームはパーティクルで描画する
//==============================================================================
void BulletManager::Draw()
{
    // ① 不透明モデル描画
    for (int i = 0; i < m_count; i++)
    {
        const BulletType type = m_bullets[i]->GetType();
        if (type == BulletType::Normal || type == BulletType::Missile)
            DrawNormal(i);
        else
            DrawBeam(i);
    }

    // ② トレイル描画（加算ブレンド・不透明物の後）
    for (int i = 0; i < m_count; i++)
        m_bullets[i]->DrawTrail();
}

//==============================================================================
// 通常弾生成
//
// ■役割
// ・通常弾を配列に追加する
//
// ■引数
// ・pos    : 初期位置
// ・vel    : 初期速度
// ・damage : ダメージ量
//==============================================================================
void BulletManager::CreateNormal(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage)
{
    if (m_count >= MAX_BULLET) return;  // 上限チェック
    m_bullets[m_count++] = new Bullet(pos, vel, damage);
}

//==============================================================================
// ビーム弾生成
//
// ■役割
// ・ビーム弾を配列に追加する
//
// ■引数
// ・pos    : 初期位置
// ・vel    : 初期速度
// ・damage : ダメージ量
//==============================================================================
void BulletManager::CreateBeam(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage)
{
    if (m_count >= MAX_BULLET) return;  // 上限チェック
    m_bullets[m_count++] = new BeamBullet(pos, vel, damage, m_beamTexID);
}

//==============================================================================
// 弾削除
//
// ■役割
// ・指定インデックスの弾を削除し、エフェクトを生成する
//
// ■引数
// ・index : 削除する弾のインデックス
//==============================================================================
void BulletManager::Destroy(int index)
{
    if (index < 0 || index >= m_count) return;  // 範囲チェック

    // エフェクト生成
    BulletHitEffect_Create(m_bullets[index]->GetPrevPosition());

    // ミサイルの場合は爆発イベントも登録（敵への直撃時）
    if (m_bullets[index]->GetType() == BulletType::Missile)
    {
        auto* m = static_cast<MissileBullet*>(m_bullets[index]);
        AddExplosion(m->GetPrevPosition(), m->GetExplosionRadius(), m->GetDamage());
    }

    // 弾を削除して配列を詰める
    delete m_bullets[index];
    m_bullets[index] = m_bullets[m_count - 1];
    m_bullets[m_count - 1] = nullptr;
    m_count--;
}

//==============================================================================
// 弾数取得
//
// ■戻り値
// ・現在存在する弾の総数
//==============================================================================
int BulletManager::GetCount() const
{
    return m_count;
}

//==============================================================================
// ダメージ量取得
//
// ■引数
// ・index : 弾のインデックス
//
// ■戻り値
// ・指定弾のダメージ量（範囲外の場合は0）
//==============================================================================
int BulletManager::GetDamage(int index) const
{
    if (index < 0 || index >= m_count) return 0;
    return m_bullets[index]->GetDamage();
}

//==============================================================================
// ビーム弾判定
//
// ■引数
// ・index : 弾のインデックス
//
// ■戻り値
// ・true  : ビーム弾
// ・false : 通常弾
//==============================================================================
bool BulletManager::IsBeam(int index) const
{
    if (index < 0 || index >= m_count) return false;
    return m_bullets[index]->GetType() == BulletType::Beam;
}

//==============================================================================
// AABB取得（デバッグ・旧コード互換用）
//
// ■引数
// ・index : 弾のインデックス
//
// ■戻り値
// ・軸平行なAABB
//==============================================================================
AABB BulletManager::GetAABB(int index) const
{
    return ModelGetAABB(m_pModel, m_bullets[index]->GetPosition());
}

//==============================================================================
// OBB取得
//
// ■役割
// ・弾の種類に応じたサイズのOBBを生成して返す
//
// ■引数
// ・index : 弾のインデックス
//
// ■戻り値
// ・弾のOBB（進行方向を考慮した当たり判定）
//==============================================================================
const XMFLOAT3& BulletManager::GetPrevPosition(int index) const
{
    static const XMFLOAT3 kZero{ 0.0f, 0.0f, 0.0f };
    if (index < 0 || index >= m_count) return kZero;
    return m_bullets[index]->GetPrevPosition();
}

OBB BulletManager::GetOBB(int index) const
{
    if (index < 0 || index >= m_count) return OBB();  // 範囲外はデフォルトOBB

    const XMFLOAT3& pos = m_bullets[index]->GetPosition();
    const XMFLOAT3  front = m_bullets[index]->GetFront();

    const BulletType btype = m_bullets[index]->GetType();

    // 弾の種類に応じたサイズ設定（ビームのみ細長いOBB、他は通常サイズ）
    const XMFLOAT3 halfExtents = (btype == BulletType::Beam)
        ? XMFLOAT3{ BEAM_HALF_WIDTH_X,   BEAM_HALF_WIDTH_Y,   BEAM_HALF_LENGTH_Z }
    : XMFLOAT3{ BULLET_HALF_WIDTH_X, BULLET_HALF_WIDTH_Y, BULLET_HALF_LENGTH_Z };

    return OBB::CreateFromFront(pos, halfExtents, front);
}

//==============================================================================
// シングルトン取得
//
// ■役割
// ・BulletManagerのシングルトンインスタンスを返す
//
// ■戻り値
// ・BulletManagerの参照
//==============================================================================
static BulletManager& GetManager()
{
    static BulletManager instance;
    return instance;
}

//==============================================================================
// グローバル関数（外部インターフェース）
//
// ■役割
// ・Player.cpp / Enemy.cpp から呼ばれるグローバル関数
// ・すべてBulletManagerのシングルトンに委譲する
//==============================================================================

void Bullet_Initialize()
{
    GetManager().Initialize();
}

void Bullet_Finalize()
{
    GetManager().Finalize();
}

void Bullet_ClearAll()
{
    GetManager().ClearAll();
}

void Bullet_Update(double elapsed_time)
{
    GetManager().Update(elapsed_time);
}

void Bullet_Draw()
{
    GetManager().Draw();
}

void Bullet_Create(const XMFLOAT3& pos, const XMFLOAT3& vel, int dmg)
{
    GetManager().CreateNormal(pos, vel, dmg);
}

void Bullet_CreateBeam(const XMFLOAT3& pos, const XMFLOAT3& vel, int dmg)
{
    GetManager().CreateBeam(pos, vel, dmg);
}

void Bullet_Destroy(int index)
{
    GetManager().Destroy(index);
}

int Bullet_GetCount()
{
    return GetManager().GetCount();
}

int Bullet_GetDamage(int index)
{
    return GetManager().GetDamage(index);
}

AABB Bullet_GetAABB(int index)
{
    return GetManager().GetAABB(index);
}

const XMFLOAT3& Bullet_GetPrevPosition(int index)
{
    return GetManager().GetPrevPosition(index);
}

OBB Bullet_GetOBB(int index)
{
    return GetManager().GetOBB(index);
}

bool Bullet_IsBeam(int index)
{
    return GetManager().IsBeam(index);
}

//==============================================================================
// BulletManager：ミサイル関連メソッド実装
//==============================================================================

//==============================================================================
// ミサイル弾生成
//==============================================================================
void BulletManager::CreateMissile(const XMFLOAT3& pos, const XMFLOAT3& vel, int damage, float radius)
{
    if (m_count >= MAX_BULLET) return;
    m_bullets[m_count++] = new MissileBullet(pos, vel, damage, radius);
}

//==============================================================================
// 爆発イベントをキューに追加
//==============================================================================
void BulletManager::AddExplosion(const XMFLOAT3& pos, float radius, int damage)
{
    if (m_explosionCount >= MAX_EXPLOSIONS) return;
    m_pendingExplosions[m_explosionCount++] = { pos, radius, damage };
}

//==============================================================================
// 未処理の爆発イベント数を返す
//==============================================================================
int BulletManager::GetPendingExplosionCount() const
{
    return m_explosionCount;
}

//==============================================================================
// 未処理の爆発イベントを取得
//==============================================================================
ExplosionEvent BulletManager::GetPendingExplosion(int i) const
{
    if (i < 0 || i >= m_explosionCount) return {};
    return m_pendingExplosions[i];
}

//==============================================================================
// 未処理の爆発イベントをすべてクリア
//==============================================================================
void BulletManager::ClearPendingExplosions()
{
    m_explosionCount = 0;
}

//==============================================================================
// グローバル関数（ミサイル関連）
//==============================================================================

void Bullet_CreateMissile(const XMFLOAT3& pos, const XMFLOAT3& vel, int dmg, float radius)
{
    GetManager().CreateMissile(pos, vel, dmg, radius);
}

int Bullet_GetPendingExplosionCount()
{
    return GetManager().GetPendingExplosionCount();
}

ExplosionEvent Bullet_GetPendingExplosion(int i)
{
    return GetManager().GetPendingExplosion(i);
}

void Bullet_ClearPendingExplosions()
{
    GetManager().ClearPendingExplosions();
}
