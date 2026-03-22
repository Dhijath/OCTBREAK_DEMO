/*==============================================================================

   ゲーム用カメラ制御 [game_camera.cpp]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   ゲームプレイ中に使用するビュー行列・透視投影行列を管理するカメラモジュール。
   camera.cpp（汎用カメラ）と構造は共通だが、ゲーム専用の設定を持つ。

   ■機能
     - Game_Camera_Initialize : カメラ位置・向きを初期化
     - Game_Camera_Update     : 毎フレームのカメラ更新
     - Game_Camera_GetMatrix  : ビュー行列を取得
     - Game_Camera_GetPerspectiveMatrix : 透視投影行列を取得
     - Game_Camera_GetPosition          : カメラワールド位置を取得

==============================================================================*/
#include "game_camera.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "shader3d.h"
#include "direct3d.h"
#include "key_logger.h"
#include <sstream>
#include "debug_text.h"

static XMFLOAT3 g_Game_CameraPosition{ 0.0f, 0.0f, 0.0f };
static XMFLOAT3 g_Game_CameraFront{ 0.0f, 0.0f, 1.0f };
static XMFLOAT3 g_Game_CameraUp{ 0.0f, 1.0f, 0.0f };
static XMFLOAT3 g_Game_CameraRight{ 1.0f, 0.0f, 0.0f };
static constexpr float GAME_CAMERA_MOVE_SPEED = 6.0f;
static constexpr float GAME_CAMERA_ROTATION_SPEED = XMConvertToRadians(45);
static XMFLOAT4X4 g_Game_CameraMatrix;
static XMFLOAT4X4 g_PerspectiveMatrix;
static float g_Yaw = 0.0f;
static float g_Pitch = 0.0f;
static hal::DebugText* g_pDT = nullptr;

void Game_Camera_Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
    Game_Camera_Initialize();

    g_Game_CameraPosition = position;
    XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&front));
    XMVECTOR r = XMVector3Normalize(XMLoadFloat3(&right) * XMVECTOR { 1.0f, 0.0f, 1.0f });
    XMVECTOR u = XMVector3Normalize(XMVector3Cross(f, r));
    XMStoreFloat3(&g_Game_CameraFront, f);
    XMStoreFloat3(&g_Game_CameraRight, r);
    XMStoreFloat3(&g_Game_CameraUp, u);
}

void Game_Camera_Initialize()
{
    g_Game_CameraPosition = { -4.0f, 2.0f, 0.0f };
    g_Game_CameraFront = { 0.0f, 0.0f, 1.0f };
    g_Game_CameraUp = { 0.0f, 1.0f, 0.0f };
    g_Game_CameraRight = { 1.0f, 0.0f, 0.0f };

    XMStoreFloat4x4(&g_Game_CameraMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&g_PerspectiveMatrix, XMMatrixIdentity());

#if defined(DEBUG) || defined(_DEBUG)
    g_pDT = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetContext(),
        L"consolab_ascii_512.png",
        Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
        0.0f, 0.0f,
        0, 0,
        0.0f, 14.0f);
#endif
}

void Game_Camera_Finalize()
{
    delete g_pDT;
}

void Game_Camera_Update(double elapsed_time)
{
    XMVECTOR position = XMLoadFloat3(&g_Game_CameraPosition);

    // --- 入力で角度を更新 ---
    if (KeyLogger_IsPressed(KK_RIGHT)) g_Yaw -= GAME_CAMERA_ROTATION_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_LEFT)) g_Yaw += GAME_CAMERA_ROTATION_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_UP)) g_Pitch += GAME_CAMERA_ROTATION_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_DOWN)) g_Pitch -= GAME_CAMERA_ROTATION_SPEED * elapsed_time;

    // ピッチ角を制限（裏返り防止）
    const float limit = XMConvertToRadians(89.0f);
    if (g_Pitch > limit) g_Pitch = limit;
    if (g_Pitch < -limit) g_Pitch = -limit;

    // --- 角度から front/right/up を計算 ---
    float cosPitch = cosf(g_Pitch);
    XMVECTOR front = XMVectorSet(
        cosPitch * cosf(g_Yaw),
        sinf(g_Pitch),
        cosPitch * sinf(g_Yaw),
        0.0f
    );
    front = XMVector3Normalize(front);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), front));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(front, right));

    // --- 移動 ---
    if (KeyLogger_IsPressed(KK_W)) position += front * GAME_CAMERA_MOVE_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_S)) position -= front * GAME_CAMERA_MOVE_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_A)) position -= right * GAME_CAMERA_MOVE_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_D)) position += right * GAME_CAMERA_MOVE_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_Q)) position += up * GAME_CAMERA_MOVE_SPEED * elapsed_time;
    if (KeyLogger_IsPressed(KK_E)) position -= up * GAME_CAMERA_MOVE_SPEED * elapsed_time;

    // --- 結果を保存 ---
    XMStoreFloat3(&g_Game_CameraPosition, position);
    XMStoreFloat3(&g_Game_CameraFront, front);
    XMStoreFloat3(&g_Game_CameraRight, right);
    XMStoreFloat3(&g_Game_CameraUp, up);

    // ビュー行列
    XMMATRIX view = XMMatrixLookAtLH(position, position + front, up);
    XMStoreFloat4x4(&g_Game_CameraMatrix, view);
    Shader3d_SetViewMatrix(view);

    // プロジェクション行列
    float aspect = (float)Direct3D_GetBackBufferWidth() / Direct3D_GetBackBufferHeight();
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);
    XMStoreFloat4x4(&g_PerspectiveMatrix, proj);
    Shader3d_SetProjectMatrix(proj);
}


const DirectX::XMFLOAT4X4& Game_Camera_GetMatrix()
{
    return g_Game_CameraMatrix;
}

const DirectX::XMFLOAT4X4& Game_Camera_GetPerspectiveMatrix()
{
    return g_PerspectiveMatrix;
}


void Game_Camera_DebugDraw()
{
#if defined(DEBUG) || defined(_DEBUG)
    std::stringstream ps;
    ps << "Game_Camera Position : x = " << g_Game_CameraPosition.x;
    ps << " : y = " << g_Game_CameraPosition.y;
    ps << " : z = " << g_Game_CameraPosition.z << std::endl;

    std::stringstream fs;
    fs << "Game_Camera Front : x = " << g_Game_CameraFront.x;
    fs << " : y = " << g_Game_CameraFront.y;
    fs << " : z = " << g_Game_CameraFront.z << std::endl;

    std::stringstream us;
    us << "Game_Camera Up : x = " << g_Game_CameraUp.x;
    us << " : y = " << g_Game_CameraUp.y;
    us << " : z = " << g_Game_CameraUp.z << std::endl;

    std::stringstream rs;
    rs << "Game_Camera Right : x = " << g_Game_CameraRight.x;
    rs << " : y = " << g_Game_CameraRight.y;
    rs << " : z = " << g_Game_CameraRight.z << std::endl;

    g_pDT->SetText(ps.str().c_str());
    g_pDT->SetText(fs.str().c_str());
    g_pDT->SetText(us.str().c_str());
    g_pDT->SetText(rs.str().c_str());
    g_pDT->Draw();
    g_pDT->Clear();
#endif
}

const DirectX::XMFLOAT3& Game_Camera_GetPosition()
{
    return g_Game_CameraPosition;
}
