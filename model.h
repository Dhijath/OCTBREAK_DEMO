/*==============================================================================

   3D モデル管理 [model.h]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   Assimp を使って FBX / OBJ などの 3D モデルをロード・描画するモジュール。

   ■機能
     - Model_Load  : ファイルパスからモデルをロードし、モデルポインタを返す
     - Model_Draw  : ワールド行列を指定してモデルを描画
     - Model_Unload: ロード済みモデルを解放

   ■使用ライブラリ
     Assimp（Open Asset Import Library）を使用。
     リンクは resource/texture/assimp-vc143-mt.lib。

   ■カテゴリ ID
     CategoryID（Grass / Brick 等）でマテリアルの種類を判別できる。

==============================================================================*/
#pragma once

#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "resource/texture/assimp-vc143-mt.lib")

#include <d3d11.h>
#include <DirectXMath.h>

#include <unordered_map>
#include <DirectXMath.h>
#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "resource/texture/assimp-vc143-mt.lib")
#include "collision.h"

enum CategoryID
{
	Grass = 0,
	Brick = 1,
};

struct MODEL
{
    const aiScene* AiScene = nullptr;
    ID3D11Buffer** VertexBuffer = nullptr;
    ID3D11Buffer** IndexBuffer = nullptr;
    std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture;

    float Scale = 1.0f; 
};


MODEL* ModelLoad(const char* FileName, float size);
MODEL* ModelLoadS(const char* FileName, float size);
void ModelRelease(MODEL* model);

void ModelDraw(MODEL* model, const DirectX::XMMATRIX& mtxWorld);

AABB ModelGetAABB(MODEL* model, const DirectX::XMFLOAT3& position);

void ModelDrawWithoutBegin(MODEL* model, const DirectX::XMMATRIX& mtxWorld);
