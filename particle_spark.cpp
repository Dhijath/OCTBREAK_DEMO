/*==============================================================================

   火花パーティクル [particle_spark.cpp]
                                                         Author : 51106
                                                         Date   : 2026/03/23
--------------------------------------------------------------------------------
   SparkParticle / SparkEmitter の実装 + グローバル管理

==============================================================================*/
#include "particle_spark.h"
#include "billboard.h"
#include "direct3d.h"
#include "texture.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

//==============================================================================
// ローカルユーティリティ
//==============================================================================
namespace
{
    static float Rand01(std::mt19937& mt, std::uniform_real_distribution<float>& u)
    {
        return u(mt);
    }

    static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
}

//==============================================================================
// SparkParticle
//==============================================================================

SparkParticle::SparkParticle(
    const XMVECTOR&   position,
    const XMVECTOR&   velocity,
    double            life_time,
    int               texture_id,
    float             scale,
    const XMFLOAT4&   color_start,
    const XMFLOAT4&   color_end,
    float             gravity)
    : Particle(position, velocity, life_time, 0.0)
    , m_texture_id (texture_id)
    , m_scale      (scale)
    , m_color_start(color_start)
    , m_color_end  (color_end)
    , m_gravity    (gravity)
{
}

void SparkParticle::Update(double elapsed_time)
{
    // 重力を速度に加算してから基底クラスで位置に反映
    const XMVECTOR grav = XMVectorSet(0.0f, -m_gravity * static_cast<float>(elapsed_time), 0.0f, 0.0f);
    Add_Velocity(grav);
    Particle::Update(elapsed_time);
}

void SparkParticle::Draw() const
{
    if (IsDestroy()) return;

    XMFLOAT3 pos{};
    XMStoreFloat3(&pos, GetPosition());

    // 寿命比（0=生まれたて, 1=消えぎわ）
    float r = static_cast<float>(GetLifeRatio());
    if (r > 1.0f) r = 1.0f;

    // スケール：少し縮みながら消える
    const float displayScale = m_scale * (1.0f - r * 0.7f);

    // 色を線形補間
    const XMFLOAT4 color{
        m_color_start.x + (m_color_end.x - m_color_start.x) * r,
        m_color_start.y + (m_color_end.y - m_color_start.y) * r,
        m_color_start.z + (m_color_end.z - m_color_start.z) * r,
        m_color_start.w + (m_color_end.w - m_color_start.w) * r,
    };

    constexpr XMFLOAT4 uvRect{ 0.0f, 0.0f, 80.0f, 80.0f };

    Billboard_Draw(
        m_texture_id,
        pos,
        { displayScale, displayScale },
        color,
        uvRect);
}

//==============================================================================
// SparkEmitter
//==============================================================================

SparkEmitter::SparkEmitter(const XMVECTOR& position)
    : Emitter(position, 1.0 /*pps は使わない*/, false)
    , m_mt    (std::random_device{}())
    , m_unit01(0.0f, 1.0f)
{
}

Particle* SparkEmitter::createParticle()
{
    if (m_texture_id < 0) return nullptr;

    const float scale = Lerp(m_scale_min, m_scale_max, Rand01(m_mt, m_unit01));
    const float speed = Lerp(m_speed_min, m_speed_max, Rand01(m_mt, m_unit01));
    const float life  = Lerp(m_life_min,  m_life_max,  Rand01(m_mt, m_unit01));

    // 球面一様ランダム方向
    const float theta  = XM_2PI * Rand01(m_mt, m_unit01);
    const float cosPhi = 1.0f - 2.0f * Rand01(m_mt, m_unit01);
    const float sinPhi = sqrtf(std::max(0.0f, 1.0f - cosPhi * cosPhi));

    const XMVECTOR dir = XMVectorSet(
        sinPhi * cosf(theta),
        sinPhi * sinf(theta),
        cosPhi,
        0.0f);

    const XMVECTOR vel = XMVectorScale(dir, speed);

    return new SparkParticle(
        GetPosition(), vel,
        static_cast<double>(life),
        m_texture_id, scale,
        m_color_start, m_color_end,
        m_gravity);
}

void SparkEmitter::Burst(int count)
{
    for (int i = 0; i < count; ++i)
    {
        Particle* p = createParticle();
        if (p) m_particles.push_back(p);
    }
}

void SparkEmitter::Update(double elapsed_time)
{
    // 連続放出はしない。パーティクルの更新と死亡削除のみ。
    for (auto* p : m_particles)
    {
        if (p && !p->IsDestroy())
            p->Update(elapsed_time);
    }
    RemoveDeadParticles();
}

void SparkEmitter::Draw()
{
    Direct3D_SetBlendStateAdditive(true);
    Direct3D_SetDepthStencilStateDepthWriteDisable(false);

    for (const auto* p : GetParticles())
    {
        if (!p || p->IsDestroy()) continue;
        static_cast<const SparkParticle*>(p)->Draw();
    }

    Direct3D_SetBlendStateAdditive(false);
    Direct3D_SetDepthStencilStateDepthWriteDisable(true);
}

//==============================================================================
// グローバル管理
//==============================================================================
namespace
{
    static constexpr int MAX_SPARK_EMITTERS = 64;
    static SparkEmitter* g_pEmitters[MAX_SPARK_EMITTERS]{};
    static int           g_EmitterCount = 0;
    static int           g_TexID        = -1;
}

void SparkEffect_Initialize()
{
    g_TexID       = Texture_Load(L"Resource/Texture/effect000.jpg");
    g_EmitterCount = 0;
}

void SparkEffect_Finalize()
{
    for (int i = 0; i < g_EmitterCount; i++)
    {
        delete g_pEmitters[i];
        g_pEmitters[i] = nullptr;
    }
    g_EmitterCount = 0;
    Texture_Release(g_TexID);
    g_TexID = -1;
}

void SparkEffect_ClearAll()
{
    for (int i = 0; i < g_EmitterCount; i++)
    {
        delete g_pEmitters[i];
        g_pEmitters[i] = nullptr;
    }
    g_EmitterCount = 0;
}

void SparkEffect_Update(double elapsed_time)
{
    for (int i = 0; i < g_EmitterCount; i++)
        g_pEmitters[i]->Update(elapsed_time);

    // 全パーティクルが消えたエミッターを削除（Swap & Pop）
    for (int i = 0; i < g_EmitterCount; i++)
    {
        if (g_pEmitters[i]->IsFinished())
        {
            delete g_pEmitters[i];
            g_pEmitters[i]             = g_pEmitters[g_EmitterCount - 1];
            g_pEmitters[g_EmitterCount - 1] = nullptr;
            g_EmitterCount--;
            i--;
        }
    }
}

void SparkEffect_Draw()
{
    for (int i = 0; i < g_EmitterCount; i++)
        g_pEmitters[i]->Draw();
}

void SparkEffect_Create(const XMFLOAT3& position, float scale)
{
    if (g_EmitterCount >= MAX_SPARK_EMITTERS) return;
    if (g_TexID < 0) return;

    const bool isMissile = (scale > 1.5f);

    // ミサイルは上限を大きく取る
    int count = static_cast<int>(12.0f * scale * scale);
    count = (count < 8)           ?   8 : count;
    count = (isMissile && count > 120) ? 120 : count;
    count = (!isMissile && count > 60) ?  60 : count;

    SparkEmitter* e = new SparkEmitter(XMLoadFloat3(&position));
    e->SetTextureId (g_TexID);
    e->SetScaleRange(isMissile ? 0.20f * scale : 0.05f * scale,
                     isMissile ? 0.50f * scale : 0.13f * scale);
    e->SetSpeedRange(isMissile ?  2.0f * scale : 2.0f,
                     isMissile ?  6.0f * scale : 5.0f);
    e->SetLifeRange (isMissile ? 0.50f : 0.15f,
                     isMissile ? 1.20f + 0.20f * scale : 0.30f + 0.15f * scale);
    e->SetGravity   (isMissile ? 4.0f : 9.8f);  // ミサイルは重力弱め＝長く飛び散る
    e->Burst(count);

    g_pEmitters[g_EmitterCount++] = e;
}
