/*==============================================================================

   イージング関数ユーティリティ [Easing.h]
                                                         Author : 51106
                                                         Date   : 2026/02/01
--------------------------------------------------------------------------------
   アニメーション補間に使うイージング関数を inline で提供するユーティリティヘッダ。
   実装ファイル（.cpp）は不要で、このヘッダをインクルードするだけで使える。

   ■引数・戻り値
     全関数共通: t ∈ [0.0, 1.0] → 戻り値 ∈ [0.0, 1.0]
     t=0 が開始、t=1 が終了を表す。

   ■種別
     Quad  （2乗）: 軽い加減速。UI スライドイン、フェードなど。
     Cubic （3乗）: やや強い加減速。BossIntro カメラ補間など。
     Sine  （sin）: なめらかな加減速。揺れ・呼吸アニメなど。
     Expo  （指数）: 急激な加減速。衝撃・ポップアップなど。
     Back        : 少しオーバーシュートして戻る。バウンス感を出したいとき。

==============================================================================*/
// Easing.h  ─  イージング関数ユーティリティ
// 全関数: t ∈ [0,1] → [0,1]
// 使い方: float smoothed = EaseOutQuad(t);
#pragma once

// ── Quad ──────────────────────────────────────────────
// EaseInQuad  : ゆっくり始まって速く終わる
inline float EaseInQuad(float t) { return t * t; }

// EaseOutQuad : 速く始まってゆっくり終わる（UI スライドイン、ポップアップ等）
inline float EaseOutQuad(float t) { return 1.f - (1.f - t) * (1.f - t); }

// EaseInOutQuad : 両端がなだらか（フェード、カメラ遷移等）
inline float EaseInOutQuad(float t)
{
    return t < 0.5f
        ? 2.f * t * t
        : 1.f - (-2.f * t + 2.f) * (-2.f * t + 2.f) / 2.f;
}

// ── Cubic ─────────────────────────────────────────────
// EaseInCubic  : より急な加速（突進開始等）
inline float EaseInCubic(float t) { return t * t * t; }

// EaseOutCubic : より急な減速（着地衝撃等）
inline float EaseOutCubic(float t) { float u = 1.f - t; return 1.f - u * u * u; }

// EaseInOutCubic : 両端なだらか・強め（BossIntro カメラ補間等）
inline float EaseInOutCubic(float t)
{
    return t < 0.5f
        ? 4.f * t * t * t
        : 1.f - (-2.f * t + 2.f) * (-2.f * t + 2.f) * (-2.f * t + 2.f) / 2.f;
}

// ── Back（軽いオーバーシュート）────────────────────────
// EaseOutBack : 終点をわずかに超えてから戻る（ポーズボタン選択等）
inline float EaseOutBack(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

// ── Smoothstep（Ken Perlin 式）────────────────────────
// Smoothstep : 3t²-2t³  ─ 両端ゼロ導関数の S 字カーブ
inline float Smoothstep(float t) { return t * t * (3.f - 2.f * t); }
