/*==============================================================================

   スカイボックス [Skybox.h]

   ■使い方
   1. Skybox_Initialize() をゲーム初期化時に呼ぶ
   2. 毎フレーム、不透明オブジェクトを描く前に Skybox_Draw() を呼ぶ
      ・Draw の前に view / proj 行列を Shader3d でセット済みであること
   3. Skybox_SetTexture() でテクスチャを差し替え可能

==============================================================================*/

#pragma once
#include <DirectXMath.h>

void Skybox_Initialize();
void Skybox_Finalize();

// view : カメラのビュー行列（平行移動は内部で自動除去）
// proj : プロジェクション行列
void Skybox_Draw(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

// texId : Texture_Load() で取得したID（-1 で非表示）
void Skybox_SetTexture(int texId);
