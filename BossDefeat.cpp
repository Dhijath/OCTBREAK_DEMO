/*==============================================================================

   ボス撃破演出 [BossDefeat.cpp]
                                                         Author : 51106
                                                         Date   : 2026/06/11
--------------------------------------------------------------------------------
   ■フェーズ
     ZOOM_IN  (1.5秒) : ボス位置へカメラがズームイン
                         スパーク連続発生
     HOLD     (1.5秒) : アップで静止・スパーク継続
     FADE_OUT (1.0秒) : 黒フェードアウト → 演出終了
     DONE             : Player_SetEnable(true) で操作権を返す

   ■カメラ上書き
     Player_Camera_OverrideCinematic(eyePos, targetPos) を毎フレーム呼ぶ

==============================================================================*/
#include "BossDefeat.h"
#include "Player_Camera.h"
#include "Player.h"
#include "map.h"
#include "particle_spark.h"
#include "fade.h"
#include "Audio.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

namespace
{
    enum class Phase { Idle, ZoomIn, Hold, FadeOut, Done };

    static Phase    g_Phase   = Phase::Idle;
    static double   g_Timer   = 0.0;
    static XMFLOAT3 g_BossPos = {};

    static void(*g_OnBossVanish)() = nullptr; // ボス消滅タイミングのコールバック

    static int g_ExplodeSE      = -1;  // スパーク発生ごとの爆発SE
    static int g_VanishExplodeSE = -1;  // ボス消滅時の大爆発SE

    // ズーム開始時のカメラ位置
    static XMFLOAT3 g_StartEye = {};

    // ズーム終了時（ホールド中）のカメラ位置
    static XMFLOAT3 g_ZoomEndEye = {};

    // スパーク連続発生の間隔管理
    static double g_SparkTimer = 0.0;

    static constexpr double ZOOM_DURATION    = 1.5;
    static constexpr double HOLD_DURATION    = 1.5;
    static constexpr double FADE_DURATION    = 1.0;

    static constexpr float  ZOOM_END_DIST       = 10.0f;
    static constexpr float  ZOOM_START_DIST     = 18.0f;
    static constexpr float  CAMERA_HEIGHT_START =  4.5f; // 天井(5.5f)より低く
    static constexpr float  CAMERA_HEIGHT_END   =  4.0f;
    static constexpr float  LOOK_Y_OFFSET       =  1.5f; // ボスの胸あたりを注視
    static constexpr float  SPARK_INTERVAL   = 0.25;     // スパーク発生間隔（秒）
    static constexpr float  WALL_MARGIN      = 0.15f;

    static float Smoothstep(float t)
    {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    }

    static XMFLOAT3 ClipEyeToWall(const XMFLOAT3& target, const XMFLOAT3& eye)
    {
        XMFLOAT3 hitPos;
        if (!Map_RaycastWalls(target, eye, &hitPos))
            return eye;

        XMVECTOR t   = XMLoadFloat3(&target);
        XMVECTOR e   = XMLoadFloat3(&eye);
        XMVECTOR h   = XMLoadFloat3(&hitPos);
        XMVECTOR dir = XMVector3Normalize(e - t);
        XMVECTOR clipped = h - dir * WALL_MARGIN;

        XMFLOAT3 result;
        XMStoreFloat3(&result, clipped);
        return result;
    }

    // プレイヤー → ボスの方向角度（XZ平面）
    static float CalcAngleFromPlayerToBoss()
    {
        const XMFLOAT3 p = Player_GetPosition();
        return atan2f(p.x - g_BossPos.x, p.z - g_BossPos.z);
    }

    // 指定距離・高さのカメラ eye 座標を計算
    static XMFLOAT3 CalcEyeAt(float dist, float height)
    {
        const float angle = CalcAngleFromPlayerToBoss();
        return XMFLOAT3{
            g_BossPos.x + sinf(angle) * dist,
            g_BossPos.y + height,
            g_BossPos.z + cosf(angle) * dist
        };
    }
}

//==============================================================================
// 演出開始
//==============================================================================
void BossDefeat_Start(const XMFLOAT3& bossPos, void(*onBossVanish)())
{
    g_BossPos       = bossPos;
    g_Phase         = Phase::ZoomIn;
    g_Timer         = 0.0;
    g_SparkTimer    = 0.0;
    g_OnBossVanish  = onBossVanish;

    if (g_ExplodeSE < 0)
        g_ExplodeSE = LoadAudioWithVolume("resource/sound/cannon_01.wav", 0.6f);

    if (g_VanishExplodeSE < 0)
        g_VanishExplodeSE = LoadAudioWithVolume("resource/sound/cannon_02.wav", 1.0f);

    g_StartEye   = CalcEyeAt(ZOOM_START_DIST, CAMERA_HEIGHT_START);
    g_ZoomEndEye = CalcEyeAt(ZOOM_END_DIST,   CAMERA_HEIGHT_END);

    Player_SetEnable(false);
}

//==============================================================================
// 更新
//==============================================================================
void BossDefeat_Update(double dt)
{
    if (g_Phase == Phase::Idle || g_Phase == Phase::Done) return;

    g_Timer      += dt;
    g_SparkTimer += dt;

    // スパーク定期発生（ZoomIn・Hold 中）
    if (g_Phase == Phase::ZoomIn || g_Phase == Phase::Hold)
    {
        if (g_SparkTimer >= SPARK_INTERVAL)
        {
            g_SparkTimer = 0.0;
            // ボス位置を少しランダムにずらして複数発生
            XMFLOAT3 p = g_BossPos;
            p.x += ((rand() % 100) - 50) * 0.03f;
            p.z += ((rand() % 100) - 50) * 0.03f;
            p.y += ((rand() % 100) * 0.02f);
            SparkEffect_Create(p, 1.8f);

            if (g_ExplodeSE >= 0)
                PlayAudio(g_ExplodeSE);
        }
    }

    if (g_Phase == Phase::ZoomIn)
    {
        const float t = Smoothstep(static_cast<float>(g_Timer / ZOOM_DURATION));

        XMVECTOR eyeV = XMVectorLerp(
            XMLoadFloat3(&g_StartEye),
            XMLoadFloat3(&g_ZoomEndEye),
            t);
        XMFLOAT3 eyeF;
        XMStoreFloat3(&eyeF, eyeV);
        const XMFLOAT3 lookTarget = { g_BossPos.x, g_BossPos.y + LOOK_Y_OFFSET, g_BossPos.z };
        Player_Camera_OverrideCinematic(eyeF, lookTarget);

        if (g_Timer >= ZOOM_DURATION)
        {
            g_Phase = Phase::Hold;
            g_Timer = 0.0;
        }
    }
    else if (g_Phase == Phase::Hold)
    {
        const XMFLOAT3 lookTarget = { g_BossPos.x, g_BossPos.y + LOOK_Y_OFFSET, g_BossPos.z };
        Player_Camera_OverrideCinematic(g_ZoomEndEye, lookTarget);

        if (g_Timer >= HOLD_DURATION)
        {
            // ボス消滅：大爆発SE + スパーク一斉発生
            if (g_VanishExplodeSE >= 0)
                PlayAudio(g_VanishExplodeSE);

            for (int i = 0; i < 20; ++i)
            {
                XMFLOAT3 p = g_BossPos;
                p.x += ((rand() % 200) - 100) * 0.05f;
                p.y += ((rand() % 100)       ) * 0.04f;
                p.z += ((rand() % 200) - 100) * 0.05f;
                const float scale = 2.5f + ((rand() % 100) * 0.02f);  // 2.5〜4.5
                SparkEffect_Create(p, scale);
            }

            if (g_OnBossVanish) g_OnBossVanish();

            g_Phase = Phase::FadeOut;
            g_Timer = 0.0;
            Fade_StartOut(FADE_DURATION);
        }
    }
    else if (g_Phase == Phase::FadeOut)
    {
        const XMFLOAT3 lookTarget = { g_BossPos.x, g_BossPos.y + LOOK_Y_OFFSET, g_BossPos.z };
        Player_Camera_OverrideCinematic(g_ZoomEndEye, lookTarget);

        if (Fade_IsOutEnd())
        {
            g_Phase = Phase::Done;
            Player_SetEnable(true);
            Player_Camera_Update(0.0);
        }
    }
}

//==============================================================================
// 演出中フラグ
//==============================================================================
bool BossDefeat_IsPlaying()
{
    return (g_Phase == Phase::ZoomIn ||
            g_Phase == Phase::Hold   ||
            g_Phase == Phase::FadeOut);
}

bool BossDefeat_ShouldRemoveBoss()
{
    return (g_Phase == Phase::FadeOut || g_Phase == Phase::Done);
}
