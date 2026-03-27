/*==============================================================================

   プレイヤーカメラ制御（切替ハブ） [Player_Camera.cpp]
                                                         Author : 51106
                                                         Date   : 2025/12/19
--------------------------------------------------------------------------------

   ・旧：Player_topCamera（固定追従）
   ・新：マウス操作（yaw/pitch）＋パッド右スティック
   ・C で切替（※IsTriggerが不安定な場合に備えて立ち上がり検出を自前化）
   ・注意：Mouse_GetState は「マウスモード時のみ」ここで読む
           （main側で毎フレーム読むと二重読みで 0 掴みになりやすい）

    重要
   Mouse_SetMode は「イベントを立てるだけ」で、実際のモード切替は
   Mouse_ProcessMessage が走った時に確定する実装になっている。
   そのため EnterMouse/EnterFixed の直後に WM_MOUSEMOVE を1回流して
   切替イベントを即反映させる。

==============================================================================*/

#include "Player_Camera.h"                               // 公開API（取得/更新/切替）

#define WIN32_LEAN_AND_MEAN                              // Windowsヘッダを軽量化
#include <Windows.h>                                     // WM_MOUSEMOVE など

#include <DirectXMath.h>                                 // XMVECTOR/XMMATRIX など
#include <cmath>                                         // sinf/cosf/fabsf/sqrtf/ceilf など

#include "key_logger.h"                                  // キー入力
#include "pad_logger.h"                                  // ゲームパッド入力
#include "shader3d.h"                                    // 3D用シェーダへView/Proj設定
#include "shader_field.h"                                // 地面用シェーダへView/Proj設定
#include "shader_billboard.h"                            // ビルボード用シェーダへView/Proj設定
#include "direct3d.h"                                    // バックバッファサイズ取得
#include "Player.h"                                      // Player_GetPosition / Player_GetHeight
#include "mouse.h"                                       // マウス入力とモード切替
#include "Player_topCamera.h"                            // 旧固定追従カメラ
#include "WallShader.h"                                  // 壁用シェーダへView/Proj設定
#include "map.h"
#include "ShaderToon.h"
#include "ShaderEdge.h"

using namespace DirectX;                                 // DirectXMath名前空間

namespace                                                // ファイル内限定
{                                                        // ここから

    //==========================================================================
    // マウス左ボタン管理（トリガー判定用）
    //==========================================================================
    bool gMouseLeftNow = false;                          // 現在フレームのマウス左ボタン状態
    bool gMouseLeftPrev = false;                         // 前フレームのマウス左ボタン状態
    bool gMouseLeftTrigger = false;                      // マウス左ボタントリガー（押した瞬間）
    bool gMouseRightNow = false;                         // 現在フレームのマウス右ボタン状態

    //==========================================================================
    // 返却用（どちらのカメラでもここへ集約）
    //==========================================================================
    XMFLOAT3   g_Front = { 0.0f, 0.0f, 1.0f };           // カメラ前方向（共通返却）
    XMFLOAT3   g_Pos = { 0.0f, 0.0f, 0.0f };             // カメラ位置（共通返却）
    XMFLOAT4X4 g_View{};                                 // View行列（共通返却）
    XMFLOAT4X4 g_Proj{};                                 // Proj行列（共通返却）

    //==========================================================================
    // モード
    //==========================================================================
    PlayerCamera_Mode g_Mode = PLAYER_CAMERA_MODE_FIXED_FOLLOW; // 初期は旧固定追従

    //==========================================================================
    // マウスカメラ用パラメータ
    //==========================================================================
    float gYaw = 0.0f;                                   // ヨー（左右回転）
    float gPitch = 0.0f;                                 // ピッチ（上下回転）
    float gDistance = 1.2f;                              // 中心基準点からカメラまでの距離（小型向けに近め）
    float gHeight = 0.8f;                                // 中心基準点のYオフセット（中心＋少し上へ）
    float gMouseSens      = 0.0025f;                     // 予備（未使用でも残す：調整用）
    float gMouseSensYaw   = 0.0050f;                    // マウス左右感度（オプションから変更可）
    float gMouseSensPitch = 0.0012f;                    // マウス上下感度（オプションから変更可）
    bool  gMouseInvertY   = false;                      // Y軸反転（true=上向きで下を向く）

    //==========================================================================
    // 切替キー（C） 立ち上がり検出用
    //==========================================================================
    bool gPrevToggle = false;                            // 前フレームのC状態
}                                                        // namespace終わり

//==============================================================================
// 内部：各シェーダへ View/Proj を適用
//==============================================================================
static void ApplyViewProjToShaders(const XMMATRIX& view, const XMMATRIX& proj) // 全描画系へ反映
{                                                        // 開始
    Shader3d_SetViewMatrix(view);                        // 3DへView
    Shader_field_SetViewMatrix(view);                    // FieldへView
    Shader_Billboard_SetViewMatrix(view);                // BillboardへView
    Shader3d_SetProjectMatrix(proj);                     // 3DへProj
    Shader_field_SetProjectMatrix(proj);                 // FieldへProj
    Shader_Billboard_SetProjectMatrix(proj);             // BillboardへProj
    WallShader_SetViewMatrix(view);                      // 壁へView
    WallShader_SetProjectMatrix(proj);                   // 壁へProj


    ShaderToon_SetViewMatrix(view);                      //トゥーン用view
    ShaderToon_SetProjectMatrix(proj);                   //トゥーン用proj

    ShaderEdge_SetViewMatrix(view);
    ShaderEdge_SetProjectMatrix(proj);
}                                                        // 終了

//==============================================================================
// 内部：本編用 Projection を作成（度数法入力→弧度法変換）
//==============================================================================
static XMMATRIX MakeMainProjection()                     // 本編用Projを返す
{                                                        // 開始
    float aspect =                                       // アスペクト比
        static_cast<float>(Direct3D_GetBackBufferWidth()) /   // 幅
        static_cast<float>(Direct3D_GetBackBufferHeight());   // 高さ
    if (aspect <= 0.0f) aspect = 1.0f;                   // 念のため保険

    // 画角（FOV）を度数法で指定して弧度法へ変換する
    const float fovDeg = 80.0f;                          // 縦FOV（度数法）：プレイヤーが入りやすい標準寄り
    const float fovY = XMConvertToRadians(fovDeg);       // 縦FOV（弧度法）：行列計算用

    return XMMatrixPerspectiveFovLH(                     // 透視投影（左手系）
        fovY,                                            // 縦画角（弧度法）
        aspect,                                          // アスペクト比
        0.1f,                                            // Near
        1000.0f                                          // Far
    );                                                   // 返す
}                                                        // 終了

//==============================================================================
// 内部：Mouse_SetMode 直後に「切替イベント」を即反映させる
//  ※ mouse.cpp の仕様：モード切替は Mouse_ProcessMessage の先頭で確定する
//==============================================================================
static void FlushMouseModeEventOnce()                    // WM_MOUSEMOVEを1回流す
{                                                        // 開始
    Mouse_ProcessMessage(WM_MOUSEMOVE, 0, 0);            // モード確定用に疑似イベント
}                                                        // 終了

//==============================================================================
// 内部：旧モードへ
//==============================================================================
static void EnterFixed()                                 // 固定追従へ切替
{                                                        // 開始
    g_Mode = PLAYER_CAMERA_MODE_FIXED_FOLLOW;            // モード設定
    Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);         // 旧は絶対座標
    Mouse_SetVisible(true);                              // カーソル表示
    FlushMouseModeEventOnce();                           // 切替イベント即確定
}                                                        // 終了

//==============================================================================
// 内部：マウスモードへ
//==============================================================================
static void EnterMouse()                                 // マウス自由視点へ切替
{                                                        // 開始
    g_Mode = PLAYER_CAMERA_MODE_MOUSE_FREE;              // モード設定
    Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);         // 相対入力へ
    Mouse_SetVisible(false);                             // カーソル非表示
    FlushMouseModeEventOnce();                           // 切替イベント即確定
    Mouse_State ms{};                                    // マウス状態
    Mouse_GetState(&ms);                                 // 1回捨てて飛びを抑える
}                                                        // 終了

//==============================================================================
// モードAPI
//==============================================================================
PlayerCamera_Mode Player_Camera_GetMode()                // 現在モード取得
{                                                        // 開始
    return g_Mode;                                       // 返す
}                                                        // 終了

void Player_Camera_SetMode(PlayerCamera_Mode mode)       // モード設定
{                                                        // 開始
    if (g_Mode == mode) return;                          // 同じなら何もしない
    (mode == PLAYER_CAMERA_MODE_MOUSE_FREE) ? EnterMouse() : EnterFixed(); // 指定へ
}                                                        // 終了

void Player_Camera_ToggleMode()                          // モード切替（トグル）
{                                                        // 開始
    Player_Camera_SetMode(                               // SetModeへ委譲
        (g_Mode == PLAYER_CAMERA_MODE_MOUSE_FREE)        // 現在がマウスなら
        ? PLAYER_CAMERA_MODE_FIXED_FOLLOW                // 固定へ
        : PLAYER_CAMERA_MODE_MOUSE_FREE                  // それ以外はマウスへ
    );                                                   // 終了
}                                                        // 終了

//==============================================================================
// 初期化／終了
//==============================================================================
void Player_Camera_Initialize()                          // 初期化
{                                                        // 開始
    Player_topCamera_Initialize();                       // 旧カメラ側も初期化
    EnterMouse();                                        // 初期はマウス自由視点
    PadLogger_Initialize();                              // パッドロガー初期化
    gMouseInvertY = false;                               // Y軸反転は常にOFF起動
}                                                        // 終了

void Player_Camera_Finalize()                            // 終了
{                                                        // 開始
    Player_topCamera_Finalize();                         // 旧カメラ終了
}                                                        // 終了

//==============================================================================
// 内部：旧カメラ更新（委譲）
//==============================================================================
static void UpdateFixedCamera(double elapsed_time)       // 固定追従を更新
{                                                        // 開始
    Player_topCamera_Update(elapsed_time);               // 旧カメラ更新
    g_Front = Player_topCamera_GetFront();               // 前方向をコピー
    g_Pos = Player_topCamera_GetPosition();              // 位置をコピー
    g_View = Player_topCamera_GetViewMatrix();           // Viewをコピー
    g_Proj = Player_topCamera_GetProjectionMatrix();     // Projをコピー
}                                                        // 終了

//==============================================================================
// 内部：マウスカメラ更新
//==============================================================================
static void UpdateMouseCamera(double elapsed_time)       // 自由視点を更新
{                                                        // 開始
    float rx = 0.0f, ry = 0.0f;                          // 右スティック入力
    PadLogger_GetRightStick(&rx, &ry);                   // 右スティック取得

    const float PAD_YAW_SPEED = 3.0f;                    // ヨー速度（rad/sec）
    const float PAD_PITCH_SPEED = 2.0f;                  // ピッチ速度（rad/sec）

    gYaw += rx * PAD_YAW_SPEED * static_cast<float>(elapsed_time); // パッドでヨー加算
    gPitch += ry * PAD_PITCH_SPEED * static_cast<float>(elapsed_time); // パッドでピッチ加算

    Mouse_State ms{};                                    // マウス状態
    Mouse_GetState(&ms);                                 // マウス取得（相対想定）

    gMouseLeftPrev = gMouseLeftNow;                      // 前フレーム状態を保存
    gMouseLeftNow = ms.leftButton;                       // 現在フレーム状態を取得
    gMouseLeftTrigger = (gMouseLeftNow && !gMouseLeftPrev); // トリガー判定（立ち上がり）
    gMouseRightNow = ms.rightButton;                     // 右ボタン状態を取得

    // 感度はnamespace変数から参照（オプション画面で変更可）

    if (ms.positionMode != MOUSE_POSITION_MODE_RELATIVE) // 念のため相対でなければ
    {                                                    // 開始
        Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);     // 相対へ
        Mouse_SetVisible(false);                         // 非表示へ
        FlushMouseModeEventOnce();                       // イベント確定
        Mouse_GetState(&ms);                             // 再取得
    }                                                    // 終了

    gYaw   +=  ms.x * gMouseSensYaw;                                           // マウスXでヨー加算
    gPitch -= (ms.y * gMouseSensPitch) * (gMouseInvertY ? -1.0f : 1.0f);      // マウスYでピッチ加算（反転対応・OFFが通常）

    constexpr float PITCH_LIMIT = XMConvertToRadians(80.0f); // ピッチ上限（弧度法）
    if (gPitch > PITCH_LIMIT) gPitch = PITCH_LIMIT;      // 上を向きすぎない
    if (gPitch < -PITCH_LIMIT) gPitch = -PITCH_LIMIT;    // 下を向きすぎない

    const float cy = cosf(gYaw);                         // cos(yaw)
    const float sy = sinf(gYaw);                         // sin(yaw)
    const float cp = cosf(gPitch);                       // cos(pitch)
    const float sp = sinf(gPitch);                       // sin(pitch)

    XMVECTOR forward = XMVector3Normalize(               // 前方向ベクトル（正規化）
        XMVectorSet(sy * cp, sp, cy * cp, 0.0f)          // LH：Z+前、Y+上
    );                                                   // 終了
    XMStoreFloat3(&g_Front, forward);                    // 前方向を保存

    XMFLOAT3 p = Player_GetPosition();                   // プレイヤー位置（基準）
    const float centerY = p.y + (Player_GetHeight() * 0.5f); // プレイヤー中心Y（胸ではなく中心）
    XMVECTOR pivot = XMVectorSet(p.x, centerY + gHeight, p.z, 0.0f); // 中心基準点（中心＋オフセット）

    XMVECTOR eye = pivot - forward * gDistance;          // 視点：中心から後ろへ距離分
    XMVECTOR target = pivot + forward * 1.0f;            // 注視点：中心から少し前


    {
        XMFLOAT3 pivotF, eyeF, hitPos;
        XMStoreFloat3(&pivotF, pivot);
        XMStoreFloat3(&eyeF, eye);

        // カメラマージン（壁からこの距離だけ手前に置く）
        const float camMargin = 0.15f;

        if (Map_RaycastWalls(pivotF, eyeF, &hitPos))
        {
            // 交差点からpivot方向へmargin分戻した位置をeyeにする
            const float hx = hitPos.x - pivotF.x;
            const float hy = hitPos.y - pivotF.y;
            const float hz = hitPos.z - pivotF.z;
            const float len = sqrtf(hx * hx + hy * hy + hz * hz);

            if (len > camMargin)
            {
                const float safeLen = len - camMargin;
                hitPos.x = pivotF.x + hx / len * safeLen;
                hitPos.y = pivotF.y + hy / len * safeLen;
                hitPos.z = pivotF.z + hz / len * safeLen;
            }
            else
            {
                hitPos = pivotF;
            }

            eye = XMLoadFloat3(&hitPos);
        }
    }


    XMStoreFloat3(&g_Pos, eye);                          // 視点位置を保存

    XMMATRIX view = XMMatrixLookAtLH(                    // View行列
        eye,                                             // 視点
        target,                                          // 注視点
        XMVectorSet(0, 1, 0, 0)                          // 上方向
    );                                                   // 終了
    XMMATRIX proj = MakeMainProjection();                // Proj行列（度→弧度のFOV）

    XMStoreFloat4x4(&g_View, view);                      // View保存
    XMStoreFloat4x4(&g_Proj, proj);                      // Proj保存

    ApplyViewProjToShaders(view, proj);                  // 全シェーダへ適用
}                                                        // 終了


//==============================================================================
// 更新（切替＋各モード更新）
//==============================================================================
void Player_Camera_Update(double elapsed_time)           // 毎フレーム更新
{                                                        // 開始


    // カメラモード切替（Cキー または STARTボタン）
    const bool nowToggle = KeyLogger_IsPressed(KK_C) || PadLogger_IsPressed(PAD_START); // C or START押下状態
    if (nowToggle && !gPrevToggle)                       // 立ち上がり検出
    {                                                    // 開始
        Player_Camera_ToggleMode();                      // モード切替
    }                                                    // 終了
    gPrevToggle = nowToggle;                             // 前状態更新

    if (g_Mode == PLAYER_CAMERA_MODE_MOUSE_FREE)         // マウス自由視点なら
        UpdateMouseCamera(elapsed_time);                 // 自由視点更新
    else                                                 // それ以外
        UpdateFixedCamera(elapsed_time);                 // 固定追従更新
}                                                        // 終了                                                     // 終了

//==============================================================================
// Getter
//==============================================================================
const XMFLOAT3& Player_Camera_GetFront() { return g_Front; } // 前方向取得
const XMFLOAT3& Player_Camera_GetPosition() { return g_Pos; } // 位置取得
const XMFLOAT4X4& Player_Camera_GetViewMatrix() { return g_View; } // View取得
const XMFLOAT4X4& Player_Camera_GetProjectionMatrix() { return g_Proj; } // Proj取得

//==============================================================================
// ミニマップ用：上から見下ろす View/Proj を各シェーダへセット
//==============================================================================
void Player_Camera_SetMiniMapTopDown(const XMFLOAT3& center, float height, float range) // ミニマップ設定
{                                                        // 開始
    XMVECTOR target = XMLoadFloat3(&center);             // 注視点
    XMVECTOR eye = XMVectorSet(center.x, center.y + height, center.z, 0.0f); // 視点（上から）
    XMVECTOR up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);   // 上方向（上から見下ろし用）

    XMMATRIX view = XMMatrixLookAtLH(eye, target, up);   // View行列

    float aspect =                                       // アスペクト比
        static_cast<float>(Direct3D_GetBackBufferWidth()) /
        static_cast<float>(Direct3D_GetBackBufferHeight());
    if (aspect <= 0.0f) aspect = 1.0f;                   // 念のため保険

    float w = range * 2.0f;                              // 横幅（範囲×2）
    float h = w / aspect;                                // 縦幅（比率補正）

    XMMATRIX proj = XMMatrixOrthographicLH(w, h, 0.1f, 500.0f); // 平行投影

    ApplyViewProjToShaders(view, proj);                  // シェーダへ適用
}                                                        // 終了

//==============================================================================
// 本編用：保存済み View/Proj を各シェーダへ再セット
//==============================================================================
void Player_Camera_ApplyMainViewProj()                   // 本編のView/Projを復帰
{                                                        // 開始
    XMMATRIX view = XMLoadFloat4x4(&g_View);             // 保存Viewをロード
    XMMATRIX proj = XMLoadFloat4x4(&g_Proj);             // 保存Projをロード
    ApplyViewProjToShaders(view, proj);                  // シェーダへ適用
}                                                        // 終了

bool Player_Camera_IsMouseLeftTrigger() { return gMouseLeftTrigger; } // マウス左トリガー判定

bool Player_Camera_IsMouseLeftPressed() { return gMouseLeftNow; } // マウス左押下判定

bool Player_Camera_IsMouseRightPressed() { return gMouseRightNow; } // マウス右押下判定

//==============================================================================
// Yaw/Pitch 直接設定（演出開始前の向きリセット用）
//==============================================================================
void Player_Camera_SetYawPitch(float yaw, float pitch)           // 向きを直接設定
{                                                                 // 開始
    constexpr float PITCH_LIMIT = XMConvertToRadians(80.0f);     // ピッチ上限（再適用）
    gYaw   = yaw;                                                 // Yaw を上書き
    gPitch = (pitch > PITCH_LIMIT) ? PITCH_LIMIT                 // 上限クランプ
           : (pitch < -PITCH_LIMIT) ? -PITCH_LIMIT               // 下限クランプ
           : pitch;                                               // そのまま
}                                                                 // 終了

//==============================================================================
// Yaw/Pitch で即スナップ：マウス入力なしで g_Pos / g_Front を直計算
// （UpdateMouseCamera と同式だが Mouse_GetState は呼ばない）
//==============================================================================
void Player_Camera_SnapToYawPitch(float yaw, float pitch)        // 即スナップ
{                                                                 // 開始
    constexpr float PITCH_LIMIT = XMConvertToRadians(80.0f);     // ピッチ上限
    gYaw   = yaw;                                                 // Yaw を設定
    gPitch = (pitch > PITCH_LIMIT) ? PITCH_LIMIT                 // 上限クランプ
           : (pitch < -PITCH_LIMIT) ? -PITCH_LIMIT               // 下限クランプ
           : pitch;                                               // そのまま

    const float sy = sinf(gYaw),  cy = cosf(gYaw);               // Yaw の sin/cos
    const float cp = cosf(gPitch), sp = sinf(gPitch);            // Pitch の sin/cos

    XMVECTOR forward = XMVector3Normalize(                        // 前方向（正規化）
        XMVectorSet(sy * cp, sp, cy * cp, 0.0f));                 // LH: Z+前, Y+上

    XMStoreFloat3(&g_Front, forward);                             // 前方向を保存

    XMFLOAT3 p = Player_GetPosition();                            // プレイヤー基準位置
    const float centerY = p.y + (Player_GetHeight() * 0.5f);     // プレイヤー中心 Y
    XMVECTOR pivot = XMVectorSet(                                 // 基準点
        p.x, centerY + gHeight, p.z, 0.0f);                      //   中心 + 高さオフセット
    XMVECTOR eye = pivot - forward * gDistance;                   // 視点 = 基準から後ろ

    XMStoreFloat3(&g_Pos, eye);                                   // 位置を即反映
}                                                                 // 終了

//==============================================================================
// シネマティック上書き：eye と target から View/Proj を構築してシェーダへ適用
//==============================================================================
void Player_Camera_OverrideCinematic(                        // シネマティック上書き
    const XMFLOAT3& eye,                                     // カメラ視点
    const XMFLOAT3& target)                                  // 注視点
{                                                            // 開始
    XMVECTOR eyeV    = XMLoadFloat3(&eye);                   // eye をロード
    XMVECTOR targetV = XMLoadFloat3(&target);                // target をロード
    XMVECTOR upV     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // 上方向

    XMMATRIX view = XMMatrixLookAtLH(eyeV, targetV, upV);   // View 行列生成
    XMMATRIX proj = MakeMainProjection();                    // Proj 行列生成（本編と同 FOV）

    XMStoreFloat3(&g_Pos, eyeV);                             // カメラ位置を更新
    XMStoreFloat4x4(&g_View, view);                          // View を保存
    XMStoreFloat4x4(&g_Proj, proj);                          // Proj を保存

    ApplyViewProjToShaders(view, proj);                      // 全シェーダへ適用
}                                                            // 終了

//==============================================================================
// マウス感度 getter / setter（オプション画面から操作）
// ・YawとPitchを常に4:1比で連動させる
//==============================================================================
float Player_Camera_GetMouseSensitivity()                    // 横感度取得（Yaw）
{
    return gMouseSensYaw;
}

void Player_Camera_SetMouseSensitivity(float yawSens)       // 感度設定（Yaw/Pitch 連動）
{
    gMouseSensYaw   = yawSens;
    gMouseSensPitch = yawSens * 0.24f;                      // Pitch = Yaw × 0.24 で連動
}

float Player_Camera_GetMouseSensitivityPitch()               // 縦感度取得（Pitch）
{
    return gMouseSensPitch;
}

void Player_Camera_SetMouseSensitivityPitch(float pitchSens) // 縦感度設定（Pitch）
{
    gMouseSensPitch = pitchSens;
}

bool Player_Camera_GetMouseInvertY()                         // Y軸反転取得
{
    return gMouseInvertY;
}

void Player_Camera_SetMouseInvertY(bool invert)              // Y軸反転設定
{
    gMouseInvertY = invert;
}