/*==============================================================================

   汎用カメラ制御 [camera.cpp]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   ビュー行列・透視投影行列を管理する汎用カメラモジュール。
   プレイヤー追従カメラ（player_camera.cpp）とは独立して動作する。

   ■機能
     - Camera_Initialize : カメラ位置・向き・上ベクトルを初期化
     - Camera_Update     : 入力によるカメラ移動・回転処理
     - Camera_GetMatrix  : ビュー行列を取得
     - Camera_GetPerspectiveMatrix : 透視投影行列を取得

   ■座標系
     右手系 Y-up。Yaw（水平回転）・Pitch（垂直回転）で向きを管理。

==============================================================================*/
#include "camera.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "shader3d.h"
#include "direct3d.h"
#include "key_logger.h"
#include <sstream>
#include "debug_text.h"
#include "player.h"    //  追加：プレイヤー情報を使う

static XMFLOAT3 g_CameraPosition{ 0.0f, 0.0f, 0.0f };
static XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };
static XMFLOAT3 g_CameraUp{ 0.0f, 1.0f, 0.0f };
static XMFLOAT3 g_CameraRight{ 1.0f, 0.0f, 0.0f };
static constexpr float CAMERA_MOVE_SPEED = 6.0f;  // 今は使わないけど残しておいてもOK
static constexpr float CAMERA_ROTATION_SPEED = XMConvertToRadians(45.0f);

//  追従用
static constexpr float CAMERA_DISTANCE = 30.0f; // プレイヤーからの後方距離
static constexpr float CAMERA_HEIGHT = 10.0f; // カメラの高さ

static XMFLOAT4X4 g_CameraMatrix;
static XMFLOAT4X4 g_PerspectiveMatrix;
static float g_Yaw = 0.0f;
static float g_Pitch = 0.0f;
static hal::DebugText* g_pDT = nullptr;

void Camera_Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
    Camera_Initialize();

    g_CameraPosition = position;
    XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&front));
    XMVECTOR r = XMVector3Normalize(XMLoadFloat3(&right) * XMVECTOR {1.0f, 0.0f, 1.0f});
    XMVECTOR u = XMVector3Normalize(XMVector3Cross(f, r));
    XMStoreFloat3(&g_CameraFront,f);
    XMStoreFloat3(&g_CameraRight, r);
    XMStoreFloat3(&g_CameraUp, u);
}

void Camera_Initialize()
{
	g_CameraPosition = { -4.0f, 2.0f, 0.0f };
	g_CameraFront    = { 0.0f, 0.0f, 1.0f };
	g_CameraUp       = { 0.0f, 1.0f, 0.0f };
	g_CameraRight    = { 1.0f, 0.0f, 0.0f };

	XMStoreFloat4x4(&g_CameraMatrix, XMMatrixIdentity());
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

void Camera_Finalize()
{
    delete g_pDT;
}

void Camera_Update(double elapsed_time)
{
    // プレイヤー位置を先に取っておく
    XMVECTOR playerPos = XMLoadFloat3(&Player_GetPosition());

    // --- 入力で角度を更新（矢印キーのみ） ---
    if (KeyLogger_IsPressed(KK_RIGHT)) g_Yaw -= CAMERA_ROTATION_SPEED * (float)elapsed_time;
    if (KeyLogger_IsPressed(KK_LEFT))  g_Yaw += CAMERA_ROTATION_SPEED * (float)elapsed_time;
    if (KeyLogger_IsPressed(KK_UP))    g_Pitch += CAMERA_ROTATION_SPEED * (float)elapsed_time;
    if (KeyLogger_IsPressed(KK_DOWN))  g_Pitch -= CAMERA_ROTATION_SPEED * (float)elapsed_time;

    // ピッチ角を制限（裏返り防止）

    constexpr float limit = DirectX::XMConvertToRadians(89.0f);
    if (g_Pitch > limit)  g_Pitch = limit;
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

    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, front));
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(front, right));

    // --- カメラ位置をプレイヤー基準で固定 ---
    // プレイヤーの後ろに一定距離、少し上から見下ろす位置
    XMVECTOR position =
        playerPos
        - front * CAMERA_DISTANCE   // 後ろに一定距離
        + up * CAMERA_HEIGHT;    // 上に一定高さ

    //  ここが大事：
    // もう WASD / Q / E で position を動かさないこと
    // （元コードの position += front * ... などは全部削除）

    // --- 結果を保存 ---
    XMStoreFloat3(&g_CameraPosition, position);
    XMStoreFloat3(&g_CameraFront, front);
    XMStoreFloat3(&g_CameraRight, right);
    XMStoreFloat3(&g_CameraUp, up);

    // ビュー行列
    XMMATRIX view = XMMatrixLookAtLH(position, playerPos, up);
    XMStoreFloat4x4(&g_CameraMatrix, view);
    Shader3d_SetViewMatrix(view);

    // プロジェクション行列
    float aspect = (float)Direct3D_GetBackBufferWidth() / Direct3D_GetBackBufferHeight();
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);
    XMStoreFloat4x4(&g_PerspectiveMatrix, proj);
    Shader3d_SetProjectMatrix(proj);
}



const DirectX::XMFLOAT4X4& Camera_GetMatrix()
{
	return g_CameraMatrix;
}

const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix()
{
	return g_PerspectiveMatrix;
}

void Camera_DebugDraw()
{
#if defined(DEBUG) || defined(_DEBUG)
    std::stringstream ps;
    ps  << "Camera Position : x = " << g_CameraPosition.x; 
    ps << " : y = " << g_CameraPosition.y;
    ps << " : z = " << g_CameraPosition.z << std::endl;

    std::stringstream fs;
    fs  << "Camera Front : x = " << g_CameraFront.x;
    fs << " : y = " << g_CameraFront.y;
    fs << " : z = " << g_CameraFront.z << std::endl;

    std::stringstream us;
    us  << "Camera Up : x = " << g_CameraUp.x;
    us << " : y = " << g_CameraUp.y;
    us << " : z = " << g_CameraUp.z << std::endl;

    std::stringstream rs;
    rs  << "Camera Right : x = " << g_CameraRight.x;
    rs << " : y = " << g_CameraRight.y;
    rs << " : z = " << g_CameraRight.z << std::endl;

    g_pDT->SetText(ps.str().c_str());
    g_pDT->SetText(fs.str().c_str());
    g_pDT->SetText(us.str().c_str());
    g_pDT->SetText(rs.str().c_str());
    g_pDT->Draw();
    g_pDT->Clear();
#endif
}

const DirectX::XMFLOAT3& Camera_GetPosition()
{
    return g_CameraPosition;
}

const DirectX::XMFLOAT3& Camera_GetFront()
{ 
    return g_CameraFront;
}