/*==============================================================================

   プレイヤーカメラ制御（切替） [Player_Camera.h]
                                                         Author : 51106
                                                         Date   : 2025/12/19
--------------------------------------------------------------------------------

   ・旧（固定追従）と新（マウス操作）を切り替えるハブ
   ・ビュー行列／射影行列の更新を担当
   ・F1 等でモード切替（実装は cpp 側）

==============================================================================*/

#ifndef PLAYER_CAMERA_H
#define PLAYER_CAMERA_H

#include <DirectXMath.h>

//==============================================================================
// カメラモード（C形式 enum：スコープ演算子は付けない）
//==============================================================================
enum PlayerCamera_Mode
{
    PLAYER_CAMERA_MODE_FIXED_FOLLOW = 0,   // 旧：固定追従（プレイヤー注視）
    PLAYER_CAMERA_MODE_MOUSE_FREE,         // 新：マウス操作（前方視点）
};

//==============================================================================
// 初期化／終了
//==============================================================================
void Player_Camera_Initialize();
void Player_Camera_Finalize();

//==============================================================================
// 更新
//==============================================================================
void Player_Camera_Update(double elapsed_time);

//==============================================================================
// Getter
//==============================================================================
const DirectX::XMFLOAT3& Player_Camera_GetFront();
const DirectX::XMFLOAT3& Player_Camera_GetPosition();
const DirectX::XMFLOAT4X4& Player_Camera_GetViewMatrix();
const DirectX::XMFLOAT4X4& Player_Camera_GetProjectionMatrix();

//==============================================================================
// ミニマップ用（上から見下ろし）View/Proj を各シェーダへセット
//==============================================================================
void Player_Camera_SetMiniMapTopDown(const DirectX::XMFLOAT3& center, float height, float range);

//==============================================================================
// 本編の View/Proj（保存済み）を各シェーダへ再セット
//==============================================================================
void Player_Camera_ApplyMainViewProj();

//==============================================================================
// モード切替API
//==============================================================================
PlayerCamera_Mode Player_Camera_GetMode();
void Player_Camera_SetMode(PlayerCamera_Mode mode);
void Player_Camera_ToggleMode();

float Player_Camera_GetMouseSensitivity();           // 横感度（Yaw）
void  Player_Camera_SetMouseSensitivity(float yawSens);
float Player_Camera_GetMouseSensitivityPitch();      // 縦感度（Pitch）
void  Player_Camera_SetMouseSensitivityPitch(float pitchSens);
bool  Player_Camera_GetMouseInvertY();               // Y軸反転
void  Player_Camera_SetMouseInvertY(bool invert);

bool Player_Camera_IsMouseLeftTrigger();
bool Player_Camera_IsMouseLeftPressed();
bool Player_Camera_IsMouseRightPressed();

//==============================================================================
// シネマティック上書き（ボス演出など）
// ・指定した eye / target で View/Proj を作りすべてのシェーダへ適用する
// ・g_Pos / g_View も更新するので GetPosition 等で取得可能
//==============================================================================
void Player_Camera_OverrideCinematic(
    const DirectX::XMFLOAT3& eye,
    const DirectX::XMFLOAT3& target);

//==============================================================================
// カメラの Yaw/Pitch を直接設定（演出開始前の向きリセット用）
// ・gYaw / gPitch を上書きし、次の Update で反映される
// ・MOUSE_FREE モード時のみ有効（FIXED_FOLLOW はこの値を使わない）
//==============================================================================
void Player_Camera_SetYawPitch(float yaw, float pitch);

//==============================================================================
// Yaw/Pitch でカメラを即スナップ（マウス入力なし・g_Pos を即時更新）
// ・BossIntro_Start など演出開始直前に呼び、直後に GetPosition() で位置取得可能
// ・Update() を呼ぶと再びマウス入力が加算されるため、代わりにこちらを使う
//==============================================================================
void Player_Camera_SnapToYawPitch(float yaw, float pitch);

//==============================================================================
// パッド用ロックオンアシスト：ターゲット位置をセット（nullptr で無効化）
// ・UpdateMouseCamera 内でパッド使用中のみ gYaw/gPitch を緩やかに追尾
// ・毎フレーム呼び直すこと（Game_Update 等から）
//==============================================================================
void Player_Camera_SetLockOnAssist(const DirectX::XMFLOAT3* targetWorldPos);


#endif // PLAYER_CAMERA_H
