/*==============================================================================

   ゲーム用カメラ制御 [game_camera.h]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   ゲームプレイ中に使用するビュー行列・透視投影行列を管理するカメラモジュールの
   パブリック API。

   ■取得できる行列
     - ビュー行列         : Game_Camera_GetMatrix()
     - 透視投影行列       : Game_Camera_GetPerspectiveMatrix()
     - カメラワールド位置 : Game_Camera_GetPosition()

==============================================================================*/

#ifndef Game_Camera_H
#define Game_Camera_H

#include <DirectXMath.h>

void Game_Camera_Initialize();
void Game_Camera_Finalize();
void Game_Camera_Update(double elapsd_time);

const DirectX::XMFLOAT4X4& Game_Camera_GetMatrix();
const DirectX::XMFLOAT4X4& Game_Camera_GetPerspectiveMatrix();

void Game_Camera_DebugDraw();

const DirectX::XMFLOAT3& Game_Camera_GetPosition();

#endif // !Game_Camera_H
