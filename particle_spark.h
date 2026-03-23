/*==============================================================================

   火花パーティクル [particle_spark.h]
                                                         Author : 51106
                                                         Date   : 2026/03/23
--------------------------------------------------------------------------------
   衝突時に一瞬バースト放出する火花パーティクルシステム。
   通常弾ヒット（scale=1.0）とミサイル爆発（scale=3.0）で共用する。

   ■公開 API
     SparkEffect_Initialize  : テクスチャ読み込み
     SparkEffect_Finalize    : リソース解放
     SparkEffect_ClearAll    : 全エフェクトをアセット解放なしでクリア（ルーム遷移用）
     SparkEffect_Update      : 毎フレーム更新
     SparkEffect_Draw        : 加算ブレンドで描画
     SparkEffect_Create      : 指定座標に火花を生成（scale で大きさ調整）

==============================================================================*/
#pragma once
#include "Particle.h"
#include <DirectXMath.h>
#include <random>

//==============================================================================
// SparkParticle – 重力あり・色フェードありのビルボードパーティクル
//==============================================================================
class SparkParticle : public Particle
{
private:
    int                   m_texture_id  = -1;
    float                 m_scale       = 0.2f;
    DirectX::XMFLOAT4     m_color_start = { 1.0f, 0.8f, 0.2f, 1.0f };
    DirectX::XMFLOAT4     m_color_end   = { 0.8f, 0.1f, 0.0f, 0.0f };
    float                 m_gravity     = 9.8f;

public:
    SparkParticle(
        const DirectX::XMVECTOR& position,
        const DirectX::XMVECTOR& velocity,
        double life_time,
        int    texture_id,
        float  scale,
        const DirectX::XMFLOAT4& color_start,
        const DirectX::XMFLOAT4& color_end,
        float  gravity = 9.8f);

    virtual ~SparkParticle() = default;

    virtual void Update(double elapsed_time) override;
    void Draw() const;
};

//==============================================================================
// SparkEmitter – バースト専用エミッター（連続放出なし）
//==============================================================================
class SparkEmitter : public Emitter
{
private:
    std::mt19937                          m_mt;
    std::uniform_real_distribution<float> m_unit01;

    int               m_texture_id  = -1;
    float             m_scale_min   = 0.10f;
    float             m_scale_max   = 0.25f;
    float             m_speed_min   = 3.0f;
    float             m_speed_max   = 8.0f;
    float             m_life_min    = 0.30f;
    float             m_life_max    = 0.80f;
    float             m_gravity     = 9.8f;
    DirectX::XMFLOAT4 m_color_start = { 1.0f, 0.8f, 0.2f, 1.0f };
    DirectX::XMFLOAT4 m_color_end   = { 0.8f, 0.1f, 0.0f, 0.0f };

protected:
    virtual Particle* createParticle() override;

public:
    explicit SparkEmitter(const DirectX::XMVECTOR& position);
    virtual ~SparkEmitter() = default;

    // 一度に count 個のパーティクルを放出する
    void Burst(int count);

    // 全パーティクルが消えたら true
    bool IsFinished() const { return GetParticles().empty(); }

    virtual void Update(double elapsed_time) override;
    virtual void Draw() override;

    // ---- パラメータ設定 ----
    void SetTextureId(int id)                          { m_texture_id  = id; }
    void SetScaleRange(float a, float b)               { m_scale_min   = a; m_scale_max  = b; }
    void SetSpeedRange(float a, float b)               { m_speed_min   = a; m_speed_max  = b; }
    void SetLifeRange (float a, float b)               { m_life_min    = a; m_life_max   = b; }
    void SetGravity   (float g)                        { m_gravity     = g; }
    void SetColorStart(const DirectX::XMFLOAT4& c)    { m_color_start = c; }
    void SetColorEnd  (const DirectX::XMFLOAT4& c)    { m_color_end   = c; }
};

//==============================================================================
// グローバル API
//==============================================================================
void SparkEffect_Initialize();
void SparkEffect_Finalize();
void SparkEffect_ClearAll();
void SparkEffect_Update(double elapsed_time);
void SparkEffect_Draw();

// scale=1.0 : 通常弾ヒット
// scale=3.0 : ミサイル爆発
void SparkEffect_Create(const DirectX::XMFLOAT3& position, float scale = 1.0f);
