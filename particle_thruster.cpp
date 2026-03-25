/*==============================================================================

   スラスターパーティクル [particle_thruster.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/15
--------------------------------------------------------------------------------
   ThrusterParticle と ThrusterEmitter の実装

==============================================================================*/
#include "particle_thruster.h"
#include "billboard.h"
#include "direct3d.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    static float Rand01(std::mt19937& mt, std::uniform_real_distribution<float>& u01)
    {
        return u01(mt);
    }

    static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    static XMVECTOR SafeNormalize3(XMVECTOR v, XMVECTOR fallback)
    {
        const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
        if (lenSq <= 1.0e-8f) return fallback;
        return XMVector3Normalize(v);
    }

    // dir（軸）を中心に、coneAngleRad の範囲でランダムに拡散した単位ベクトルを生成
    static XMVECTOR RandomConeDirection(
        std::mt19937& mt,
        std::uniform_real_distribution<float>& u01,
        XMVECTOR dirUnit,
        XMVECTOR upHintUnit,
        float coneAngleRad)
    {
        // 正規直交基底（dir, right, up）
        XMVECTOR dir = SafeNormalize3(dirUnit, XMVectorSet(0, 0, -1, 0));
        XMVECTOR upH = SafeNormalize3(upHintUnit, XMVectorSet(0, 1, 0, 0));

        XMVECTOR right = XMVector3Cross(upH, dir);
        right = SafeNormalize3(right, XMVectorSet(1, 0, 0, 0));
        XMVECTOR up = XMVector3Cross(dir, right);
        up = SafeNormalize3(up, XMVectorSet(0, 1, 0, 0));

        // 一様っぽい拡散（簡易）
        const float u = Rand01(mt, u01);
        const float v = Rand01(mt, u01);

        const float theta = XM_2PI * u;          // 周方向
        const float phi = coneAngleRad * v;      // 軸からの角度（0..cone）
        const float s = sinf(phi);
        const float c = cosf(phi);

        XMVECTOR d =
            dir * c +
            right * (s * cosf(theta)) +
            up * (s * sinf(theta));

        return SafeNormalize3(d, dir);
    }
}

//==============================================================================
// ThrusterParticle
//==============================================================================

ThrusterParticle::ThrusterParticle(
    const XMVECTOR& position,
    const XMVECTOR& velocity,
    double Life_Time,
    double Spawn_Time,
    int texture_id,
    float scale,
    float aspect)
    : Particle(position, velocity, Life_Time, Spawn_Time)
    , m_texture_id(texture_id)
    , m_scale(scale)
    , m_aspect(aspect)
{
}

void ThrusterParticle::Update(double elapsed_time)
{
    Particle::Update(elapsed_time);
}

void ThrusterParticle::Draw(const XMFLOAT4& color, const XMFLOAT4& uvRect) const
{
    XMFLOAT3 pos{};
    XMStoreFloat3(&pos, GetPosition());

    // 寿命比でスケールとアルファを落とす
    float r = static_cast<float>(GetLifeRatio());
    if (r > 1.0f) r = 1.0f;
    const float displayScale = m_scale * (1.0f - r);

    XMFLOAT4 c = color;
    c.w = c.w * (1.0f - r);

    Billboard_Draw(
        m_texture_id,
        pos,
        { displayScale * m_aspect, displayScale },
        c,
        uvRect);
}

//==============================================================================
// ThrusterEmitter
//==============================================================================

ThrusterEmitter::ThrusterEmitter(
    const XMVECTOR& position,
    double particles_per_second,
    bool is_emmit)
    : Emitter(position, particles_per_second, is_emmit)
    , m_mt(std::random_device()())
    , m_unit01(0.0f, 1.0f)
{
#if THRUSTER_TRAIL_ENABLED
    m_trail.Initialize(48, 0.18f, 0.2f, { 0.4f, 0.7f, 1.0f, 0.7f });
#endif
}

Particle* ThrusterEmitter::createParticle()
{
    if (m_particle_texture_id < 0) return nullptr;

    const float tScale = Rand01(m_mt, m_unit01);
    const float tSpeed = Rand01(m_mt, m_unit01);
    const float tLife = Rand01(m_mt, m_unit01);

    const float scale = Lerp(m_scale_min, m_scale_max, tScale);
    const float speed = Lerp(m_speed_min, m_speed_max, tSpeed);
    const float life = Lerp(m_life_min, m_life_max, tLife);

    const float coneRad = XMConvertToRadians(m_cone_angle_deg);

    XMVECTOR dir = XMLoadFloat3(&m_world_dir);
    XMVECTOR up = XMLoadFloat3(&m_world_up);

    // スラスターなので「後ろ向き」に噴く：dir は外から「後ろ」を渡す前提
    XMVECTOR d = RandomConeDirection(m_mt, m_unit01, dir, up, coneRad);

    XMVECTOR vel = XMVectorScale(d, speed);

    // 位置は Emitter の position をそのまま使う（Player 側で SetPosition 済みの想定）
    return new ThrusterParticle(
        GetPosition(),
        vel,
        static_cast<double>(life),
        0.0,
        m_particle_texture_id,
        scale,
        m_aspect);
}

void ThrusterEmitter::Update(double elapsed_time)
{
    Emitter::Update(elapsed_time);

#if THRUSTER_TRAIL_ENABLED
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, GetPosition());
    m_trail.Update(elapsed_time, pos);
#endif
}

void ThrusterEmitter::Draw()
{
    // トレイルはビルボード描画より先に描く
    // Shader_Billboard_Begin() が VS b1/b2 を未初期化バッファで上書きするため、
    // 後から描くとトレイル VS がゼロ行列を読んで全頂点 (0,0,0,0) になってしまう。
    // モデル描画直後のこのタイミングなら b1/b2 に正しいビュー/プロジェクション行列が残っている。
#if THRUSTER_TRAIL_ENABLED
    m_trail.Draw();
#endif

    Direct3D_SetBlendStateAdditive(true);
    Direct3D_SetDepthStencilStateDepthWriteDisable(false);

    const auto& particles = GetParticles();
    for (const auto* p : particles)
    {
        if (!p || p->IsDestroy()) continue;

        const ThrusterParticle* tp = dynamic_cast<const ThrusterParticle*>(p);
        if (!tp) continue;

        tp->Draw(m_color, m_uvRect);
    }

    Direct3D_SetBlendStateAdditive(false);
    Direct3D_SetDepthStencilStateDepthWriteDisable(true);
}
