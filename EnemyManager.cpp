/*==============================================================================

   エネミー管理 [EnemyManager.cpp]
                                                         Author : 51106
                                                         Date   : 2026/01/13
--------------------------------------------------------------------------------

==============================================================================*/

#include "EnemyManager.h"
#include "EnemyTank.h"
#include "EnemySpeed.h"
#include "EnemySniper.h"
#include "EnemyBoss.h"
#include <cassert>
#include <algorithm>
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
    case EnemyType::Normal:
    default:
        e = std::make_unique<Enemy>();
        break;
    }

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