/*==============================================================================

   エネミー管理 [EnemyManager.cpp]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyManager.h"
#include "EnemyTank.h"
#include "EnemySpeed.h"
#include "EnemySniper.h"
#include "EnemyBoss.h"
#include "EnemyEx.h"
#include "EnemyBossEx.h"
#include "EnemyBall.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include "shader3d.h"
#include "direct3d.h"
#include "Meshfield.h"
#include "texture.h"
#include <DirectXMath.h>

//==============================================================================
// 初期化
//==============================================================================
void EnemyManager::Initialize()
{
    m_Enemies.clear();                 // 全削除
}

//==============================================================================
// 終了処理
//==============================================================================
void EnemyManager::Finalize()
{
    for (auto& e : m_Enemies)          // 全Enemy
        e->Finalize();                 // リソース解放

    m_Enemies.clear();                 // 配列クリア
}

//==============================================================================
// 全体更新
//==============================================================================
void EnemyManager::Update(double elapsed_time)
{
    for (auto& e : m_Enemies)          // 全Enemy
        e->Update(elapsed_time);       // 更新

    ResolveSeparation();               // エネミー同士のめり込みを解消
}

//==============================================================================
// エネミー同士の押し合い
//
// ■役割
// ・2体の衝突半径の和の 85% より近ければ、互いに押し離す（15% の重なりは許容）
// ・ボスは押されず、相手だけを押し出す
// ・高さが大きく違う（別の階にいる）組は無視する
// ・1フレームで解消しきらず、重なりの半分ずつ動かして振動を防ぐ
//==============================================================================
void EnemyManager::ResolveSeparation()
{
    constexpr float OVERLAP_ALLOW = 0.85f;
    constexpr float LEVEL_GAP     = 1.5f;

    const int n = static_cast<int>(m_Enemies.size());
    for (int i = 0; i < n; ++i)
    {
        Enemy& a = *m_Enemies[i];
        if (!a.IsAlive()) continue;
        const float ra = a.GetCollisionRadius();

        for (int j = i + 1; j < n; ++j)
        {
            Enemy& b = *m_Enemies[j];
            if (!b.IsAlive()) continue;

            const DirectX::XMFLOAT3& pa = a.GetPosition();
            const DirectX::XMFLOAT3& pb = b.GetPosition();
            if (fabsf(pa.y - pb.y) > LEVEL_GAP) continue;

            const float minDist = (ra + b.GetCollisionRadius()) * OVERLAP_ALLOW;
            const float dx = pb.x - pa.x;
            const float dz = pb.z - pa.z;
            const float d2 = dx * dx + dz * dz;
            if (d2 >= minDist * minDist) continue;

            float d = sqrtf(d2);
            float nx = 1.0f, nz = 0.0f;
            if (d > 0.0001f) { nx = dx / d; nz = dz / d; }
            else             { d = 0.0f; nx = (i & 1) ? 1.0f : -1.0f; }   // 完全に重なったら適当な向きへ

            const float push = (minDist - d) * 0.5f;
            const bool  aBoss = a.IsBoss(), bBoss = b.IsBoss();
            const float shareA = aBoss ? 0.0f : (bBoss ? 1.0f : 0.5f);
            const float shareB = bBoss ? 0.0f : (aBoss ? 1.0f : 0.5f);

            a.Nudge(-nx * push * shareA, -nz * push * shareA);
            b.Nudge( nx * push * shareB,  nz * push * shareB);
        }
    }
}

//==============================================================================
// 全体描画
//==============================================================================
void EnemyManager::Draw()
{
    for (auto& e : m_Enemies)          // 全Enemy
        e->Draw();                     // 描画
}

//==============================================================================
// ミニマップ用エネミーマーカー描画（頭上に赤タイルを配置）
//==============================================================================
void EnemyManager::DrawMarkers()
{
    static int s_TexID = Texture_Load(L"resource/texture/Enemy_Maker.png");

    Shader3d_Begin();

    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    ID3D11Buffer* nullCB = nullptr;
    ctx->PSSetConstantBuffers(6, 1, &nullCB);

    ID3D11BlendState* pOldBlend = nullptr;
    FLOAT oldFactor[4];
    UINT  oldMask;
    ctx->OMGetBlendState(&pOldBlend, oldFactor, &oldMask);
    Direct3D_SetBlendState(true);

    Shader3d_SetColor({ 1.0f, 0.1f, 0.1f, 1.0f });

    int idx = 0;
    for (auto& e : m_Enemies)
    {
        const DirectX::XMFLOAT3& pos = e->GetPosition();
        const float markerY = 10.0f + 1.2f + idx * 0.01f;
        const DirectX::XMMATRIX world =
            DirectX::XMMatrixTranslation(pos.x, markerY, pos.z);
        MeshField_DrawTile(world, s_TexID, 2.0f);
        ++idx;
    }

    Shader3d_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    ctx->OMSetBlendState(pOldBlend, oldFactor, oldMask);
    SAFE_RELEASE(pOldBlend);
}

//==============================================================================
// シャドウパス用深度描画
//==============================================================================
void EnemyManager::DrawShadow()
{
    for (auto& e : m_Enemies)
        e->DrawShadow();
}

//==============================================================================
// エネミー生成
//==============================================================================
int EnemyManager::Spawn(const DirectX::XMFLOAT3& position, EnemyType type)
{
    std::unique_ptr<Enemy> e;

    // 種別に応じた子クラスを生成する
    switch (type)
    {
    case EnemyType::Tank:
        e = std::make_unique<EnemyTank>();
        break;
    case EnemyType::Speed:
        e = std::make_unique<EnemySpeed>();
        break;
    case EnemyType::Sniper:
        e = std::make_unique<EnemySniper>();
        break;
    case EnemyType::Boss:
        e = std::make_unique<EnemyBoss>();
        break;
    case EnemyType::Bomber:    e = std::make_unique<EnemyEx>(EnemyEx::Kind::Bomber);    break;
    case EnemyType::Gunner:    e = std::make_unique<EnemyEx>(EnemyEx::Kind::Gunner);    break;
    case EnemyType::Artillery: e = std::make_unique<EnemyEx>(EnemyEx::Kind::Artillery); break;
    case EnemyType::Turret:    e = std::make_unique<EnemyEx>(EnemyEx::Kind::Turret);    break;
    case EnemyType::Phantom:   e = std::make_unique<EnemyEx>(EnemyEx::Kind::Phantom);   break;
    case EnemyType::Wing:      e = std::make_unique<EnemyBall>(EnemyBall::Kind::Wing);    break;
    case EnemyType::Gatling:   e = std::make_unique<EnemyBall>(EnemyBall::Kind::Gatling); break;
    case EnemyType::Orbiter:   e = std::make_unique<EnemyBall>(EnemyBall::Kind::Orbiter); break;
    case EnemyType::Walker:    e = std::make_unique<EnemyBall>(EnemyBall::Kind::Walker);  break;
    case EnemyType::Halo:      e = std::make_unique<EnemyBall>(EnemyBall::Kind::Halo);    break;
    case EnemyType::BossArgus:   e = std::make_unique<EnemyBossEx>(EnemyBossEx::Kind::Argus);   break;
    case EnemyType::BossGoliath: e = std::make_unique<EnemyBossEx>(EnemyBossEx::Kind::Goliath); break;
    case EnemyType::BossOmega:   e = std::make_unique<EnemyBossEx>(EnemyBossEx::Kind::Omega);   break;
    case EnemyType::BossHydra:   e = std::make_unique<EnemyBossEx>(EnemyBossEx::Kind::Hydra);   break;
    case EnemyType::BossSpectre: e = std::make_unique<EnemyBossEx>(EnemyBossEx::Kind::Spectre); break;
    case EnemyType::Normal:
    default:
        e = std::make_unique<Enemy>();
        break;
    }

    e->SetTypeId(static_cast<int>(type));   // 種別（エネミー図鑑の撃破数の記録に使う）
    e->Initialize(position);          // 初期化
    m_Enemies.push_back(std::move(e)); // 配列に追加
    return static_cast<int>(m_Enemies.size() - 1); // インデックス返却
}

//==============================================================================
// 死亡エネミーの削除
//==============================================================================
void EnemyManager::RemoveDead()
{
    // IsAlive() が false のエネミーを削除する
    m_Enemies.erase(
        std::remove_if(m_Enemies.begin(), m_Enemies.end(),
            [](const std::unique_ptr<Enemy>& e) { return !e->IsAlive(); }),
        m_Enemies.end()
    );
}

//==============================================================================
// 生存数取得
//==============================================================================
int EnemyManager::GetCount() const
{
    return static_cast<int>(m_Enemies.size()); // 数を返す
}

//==============================================================================
// Enemy参照
//==============================================================================
Enemy& EnemyManager::GetEnemy(int index)
{
    assert(index >= 0 && index < GetCount()); // 範囲チェック
    return *m_Enemies[index];                 // 参照返却
}

//==============================================================================
// const参照
//==============================================================================
const Enemy& EnemyManager::GetEnemy(int index) const
{
    assert(index >= 0 && index < GetCount()); // 範囲チェック
    return *m_Enemies[index];                 // const参照返却
}

//==============================================================================
// AABB取得
//==============================================================================
AABB EnemyManager::GetAABBAt(int index) const
{
    return GetEnemy(index).GetAABB();          // AABB取得
}

//==============================================================================
// 位置取得
//==============================================================================
const DirectX::XMFLOAT3& EnemyManager::GetPositionAt(int index) const
{
    return GetEnemy(index).GetPosition();      // 位置取得
}