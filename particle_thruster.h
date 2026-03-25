/*==============================================================================

   スラスターパーティクル [particle_thruster.h]
                                                         Author : 51106
                                                         Date   : 2026/02/15
--------------------------------------------------------------------------------
   ThrusterParticle と ThrusterEmitter の定義
   ・プレイヤー後方へ噴射するパーティクル用（ビルボード描画想定）

==============================================================================*/
#ifndef PARTICLE_THRUSTER_H
#define PARTICLE_THRUSTER_H

#include "Particle.h"
#include "Trail.h"
#include <random>
#include <DirectXMath.h>

// スラスタートレイルの有効/無効（コメントアウトで切り替え）
#define THRUSTER_TRAIL_ENABLED 1

//==============================================================================
// ThrusterParticle クラス（ビルボード描画用）
//==============================================================================
class ThrusterParticle : public Particle
{
private:
    int   m_texture_id = -1;
    float m_scale = 1.0f;
    float m_aspect = 1.0f;  // 横長比率（幅 / 高さ）

public:
    ThrusterParticle(
        const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& velocity,
        double Life_Time,
        double Spawn_Time,
        int texture_id,
        float scale,
        float aspect = 1.0f);

    virtual ~ThrusterParticle() = default;

    virtual void Update(double elapsed_time) override;
    void Draw(const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4& uvRect) const;

    float GetScale() const { return m_scale; }
};

//==============================================================================
// ThrusterEmitter クラス（スラスター噴出機）
//==============================================================================
class ThrusterEmitter : public Emitter
{
private:
    // RNG
    std::mt19937 m_mt;
    std::uniform_real_distribution<float> m_unit01;

    // パラメータ（外から調整する想定）
    int   m_particle_texture_id = -1;

    float m_scale_min = 0.20f;
    float m_scale_max = 0.40f;
    float m_aspect = 1.0f;   // 横長比率（幅 / 高さ）

    float m_speed_min = 2.0f;
    float m_speed_max = 6.0f;

    float m_life_min = 0.20f;
    float m_life_max = 0.50f;

    float m_cone_angle_deg = 18.0f; // 噴射の広がり（度）

    DirectX::XMFLOAT3 m_world_dir = { 0.0f, 0.0f, -1.0f };  // 噴射方向（ワールド）
    DirectX::XMFLOAT3 m_world_up = { 0.0f, 1.0f,  0.0f };  // 上方向（基準）
    DirectX::XMFLOAT3 m_offset_local = { 0.0f, 0.2f, -0.6f }; // プレイヤー原点からのローカルオフセット

    DirectX::XMFLOAT4 m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 m_uvRect = { 0.0f, 0.0f, 80.0f, 80.0f };

#if THRUSTER_TRAIL_ENABLED
    Trail m_trail;
#endif

protected:
    virtual Particle* createParticle() override;

public:
    ThrusterEmitter(
        const DirectX::XMVECTOR& position,
        double particles_per_second,
        bool is_emmit = false);

    virtual ~ThrusterEmitter() = default;

    virtual void Update(double elapsed_time) override;
    virtual void Draw() override;

    // ---- 外部から設定するための API ----
    void SetParticleTextureId(int texId) { m_particle_texture_id = texId; }

    void SetScaleRange(float smin, float smax) { m_scale_min = smin; m_scale_max = smax; }
    void SetSpeedRange(float vmin, float vmax) { m_speed_min = vmin; m_speed_max = vmax; }
    void SetLifeRange(float lmin, float lmax) { m_life_min = lmin; m_life_max = lmax; }

    void SetConeAngleDeg(float deg) { m_cone_angle_deg = deg; }

    // プレイヤーの向きに合わせて毎フレーム更新する用途
    void SetWorldDirection(const DirectX::XMFLOAT3& dir) { m_world_dir = dir; }
    void SetWorldUp(const DirectX::XMFLOAT3& up) { m_world_up = up; }

    // プレイヤー座標系オフセット（後ろに下げる等）
    void SetLocalOffset(const DirectX::XMFLOAT3& offsetLocal) { m_offset_local = offsetLocal; }

    void SetColor(const DirectX::XMFLOAT4& color)
    {
        m_color = color;
#if THRUSTER_TRAIL_ENABLED
        // トレイルの色をスラスターと同期（アルファは 0.7 で固定）
        m_trail.SetColor({ color.x, color.y, color.z, 0.7f });
#endif
    }
    void SetUVRect(const DirectX::XMFLOAT4& uvRect) { m_uvRect = uvRect; }
    void SetAspectRatio(float aspect) { m_aspect = aspect; }

    // ルーム遷移時にトレイル履歴を即時クリア
    void ClearTrail()
    {
#if THRUSTER_TRAIL_ENABLED
        m_trail.Clear();
#endif
    }
};

#endif // PARTICLE_THRUSTER_H