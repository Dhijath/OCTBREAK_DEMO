/*==============================================================================

  モデル制御 [model.cpp] - 警告修正版
                                                         Author : 51106
                                                         Date   : 2026/04/01
  - 型変換を明示的に記述
  - テクスチャ読み込みパスの探索を改善
  - 複数のパスパターンに対応
==============================================================================*/

#include <DirectXMath.h>
#include "texture.h"
#include "model.h"
#include <cstring>
#include <string>

#include <assert.h>
#include "WICTextureLoader11.h"
using namespace DirectX;
#include "direct3d.h"
#include "DirectXTex.h"
#include "shader3d.h"
#include <iostream>

struct Vertex3D
{
    XMFLOAT3 position; // 頂点座標
    XMFLOAT3 normal;   // 法線ベクトル
    XMFLOAT4 color;    // 頂点カラー
    XMFLOAT2 uv;       // UV座標
};

namespace {
    int g_TextureWhite = -1;
}

//----------------------------------------------
// 共通：メッシュから VB / IB を生成するヘルパ
//----------------------------------------------
static void CreateMeshBuffers(MODEL* model, unsigned int m, aiMesh* mesh, float size)
{
    // 頂点 or 面が 0 のメッシュはスキップ
    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
    {
        model->VertexBuffer[m] = nullptr;
        model->IndexBuffer[m] = nullptr;
        return;
    }

    // =========================
    // 頂点バッファ作成
    // =========================
    Vertex3D* vertex = new Vertex3D[mesh->mNumVertices];

    for (unsigned int v = 0; v < mesh->mNumVertices; v++)
    {
        // 位置
        vertex[v].position = XMFLOAT3(
            mesh->mVertices[v].x * size,
            mesh->mVertices[v].y * size,
            mesh->mVertices[v].z * size);

        // UV（ある場合だけ読む）
        if (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0])
        {
            vertex[v].uv = XMFLOAT2(
                mesh->mTextureCoords[0][v].x,
                mesh->mTextureCoords[0][v].y);
        }
        else
        {
            vertex[v].uv = XMFLOAT2(0.0f, 0.0f);
        }

        // カラー
        vertex[v].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        // 法線（ない場合は上向き）
        if (mesh->HasNormals())
        {
            vertex[v].normal = XMFLOAT3(
                mesh->mNormals[v].x * size,
                mesh->mNormals[v].y * size,
                mesh->mNormals[v].z * size);
        }
        else
        {
            vertex[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        }
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Vertex3D) * mesh->mNumVertices;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    HRESULT hr = Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->VertexBuffer[m]);
    delete[] vertex;

    if (FAILED(hr))
    {
        // 失敗したら null にしておく
        model->VertexBuffer[m] = nullptr;
        model->IndexBuffer[m] = nullptr;
        return;
    }

    // =========================
    // インデックスバッファ作成
    // =========================
    // 最大確保（三角形以外をスキップするので実際の数はこれ以下になる場合がある）
    unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

    unsigned int actualIndexCount = 0;
    for (unsigned int f = 0; f < mesh->mNumFaces; f++)
    {
        const aiFace* face = &mesh->mFaces[f];

        // 三角形以外（線分・点・クアッドなど）はスキップ
        if (face->mNumIndices != 3) continue;

        index[actualIndexCount++] = face->mIndices[0];
        index[actualIndexCount++] = face->mIndices[1];
        index[actualIndexCount++] = face->mIndices[2];
    }

    if (actualIndexCount == 0)
    {
        delete[] index;
        model->IndexBuffer[m] = nullptr;
        return;
    }

    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(unsigned int) * actualIndexCount;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;

    ZeroMemory(&sd, sizeof(sd));
    sd.pSysMem = index;

    hr = Direct3D_GetDevice()->CreateBuffer(&bd, &sd, &model->IndexBuffer[m]);
    delete[] index;

    if (FAILED(hr))
    {
        model->IndexBuffer[m] = nullptr;
    }
}

//----------------------------------------------
// テクスチャ読み込みヘルパー（複数パス試行）
//----------------------------------------------
static ID3D11ShaderResourceView* TryLoadTexture(const std::string& directory, const std::string& textureFilename)
{
    ID3D11ShaderResourceView* texture = nullptr;
    ID3D11Resource* resource = nullptr;

    // バックスラッシュをスラッシュに統一
    std::string normalizedFilename = textureFilename;
    for (size_t i = 0; i < normalizedFilename.size(); i++)
    {
        if (normalizedFilename[i] == '\\')
            normalizedFilename[i] = '/';
    }

    // ファイル名のみを抽出
    std::string filenameOnly = normalizedFilename;
    size_t slashPos = filenameOnly.find_last_of("/");
    if (slashPos != std::string::npos)
    {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    // 試すパスのリスト
    std::vector<std::string> pathsToTry;

    // 1. FBXに書かれているパスそのまま（相対パス含む）
    pathsToTry.push_back(directory + "/" + normalizedFilename);

    // 2. モデルと同じフォルダ
    pathsToTry.push_back(directory + "/" + filenameOnly);

    // 3. texturesサブフォルダ
    pathsToTry.push_back(directory + "/textures/" + filenameOnly);

    // 4. Texturesサブフォルダ（大文字）
    pathsToTry.push_back(directory + "/Textures/" + filenameOnly);

    // 5. 一つ上のtexturesフォルダ
    pathsToTry.push_back(directory + "/../textures/" + filenameOnly);

    // 各パスを試す
    for (const auto& texPath : pathsToTry)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(),
            static_cast<int>(texPath.size() + 1), nullptr, 0);

        if (len > 0)
        {
            wchar_t* pWideFilename = new wchar_t[len];
            MultiByteToWideChar(CP_UTF8, 0, texPath.c_str(),
                static_cast<int>(texPath.size() + 1), pWideFilename, len);

            HRESULT hr = CreateWICTextureFromFile(
                Direct3D_GetDevice(),
                Direct3D_GetContext(),
                pWideFilename,
                &resource,
                &texture);

            delete[] pWideFilename;

            if (SUCCEEDED(hr) && texture)
            {
#ifdef _DEBUG
                // デバッグビルド時のみメッセージ表示
                std::string msg = "Texture loaded:\n" + texPath;
                MessageBoxA(nullptr, msg.c_str(), "Model Debug", MB_OK | MB_ICONINFORMATION);
#endif
                resource->Release();
                return texture;
            }
        }
    }

    // どのパスでも読み込めなかった
#ifdef _DEBUG
    std::string errorMsg = "Failed to load texture:\n" + textureFilename + "\n\nTried paths:\n";
    for (const auto& path : pathsToTry)
    {
        errorMsg += "  " + path + "\n";
    }
    MessageBoxA(nullptr, errorMsg.c_str(), "Model Error", MB_OK | MB_ICONWARNING);
#endif

    return nullptr;
}

//----------------------------------------------
// 通常ロード
//----------------------------------------------
MODEL* ModelLoad(const char* FileName, float size)
{
    MODEL* model = new MODEL;

    // スケールを保存
    model->Scale = size;

    model->AiScene = aiImportFile(
        FileName,
        aiProcessPreset_TargetRealtime_MaxQuality |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenBoundingBoxes |
        aiProcess_PreTransformVertices);

    if (!model->AiScene)
    {
#ifdef _DEBUG
        std::string msg = "Failed to load model:\n";
        msg += FileName;
        MessageBoxA(nullptr, msg.c_str(), "Model Error", MB_OK | MB_ICONERROR);
#endif
        delete model;
        return nullptr;
    }

    model->VertexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];
    model->IndexBuffer = new ID3D11Buffer * [model->AiScene->mNumMeshes];

    //  必ず nullptr で初期化
    for (unsigned int i = 0; i < model->AiScene->mNumMeshes; ++i)
    {
        model->VertexBuffer[i] = nullptr;
        model->IndexBuffer[i] = nullptr;
    }

    // 各メッシュの VB / IB を生成
    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = model->AiScene->mMeshes[m];
        CreateMeshBuffers(model, m, mesh, size);
    }

    // 白テクスチャ
    g_TextureWhite = Texture_Load(L"resource/texture/white.png");

    // 埋め込みテクスチャ読み込み
    for (unsigned int i = 0; i < model->AiScene->mNumTextures; i++)
    {
        aiTexture* aitexture = model->AiScene->mTextures[i];

        ID3D11ShaderResourceView* texture = nullptr;
        ID3D11Resource* resource = nullptr;

        CreateWICTextureFromMemory(
            Direct3D_GetDevice(),
            Direct3D_GetContext(),
            (const uint8_t*)aitexture->pcData,
            (size_t)aitexture->mWidth,
            &resource,
            &texture);

        if (texture)
        {
            assert(texture);
            resource->Release();
            model->Texture[aitexture->mFilename.C_Str()] = texture;
            std::cout << "[Model] Embedded texture loaded: " << aitexture->mFilename.C_Str() << std::endl;
        }
    }

    // モデルパスからディレクトリ抽出
    const std::string modelPath(FileName);
    size_t pos = modelPath.find_last_of("/\\");
    std::string directory = (pos != std::string::npos)
        ? modelPath.substr(0, pos)
        : ".";

    std::cout << "[Model] Loading from directory: " << directory << std::endl;

    // 外部テクスチャの読み込み（改善版）
    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
    {
        aiString filename;
        aiMaterial* aiMaterial =
            model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex];
        aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &filename);

        if (filename.length == 0)
            continue;

        // 既に読み込み済みならスキップ
        if (model->Texture.count(filename.C_Str()))
            continue;

        // 複数パスを試して読み込み
        ID3D11ShaderResourceView* texture = TryLoadTexture(directory, filename.C_Str());

        if (texture)
        {
            model->Texture[filename.C_Str()] = texture;
        }
    }

    return model;
}

//----------------------------------------------
// S版ロード（今は座標変換なしで共通化）
//----------------------------------------------
MODEL* ModelLoadS(const char* FileName, float size)
{
    // 今回は ModelLoad と同じ処理にしておく
    return ModelLoad(FileName, size);
}

//----------------------------------------------
// 解放
//----------------------------------------------
void ModelRelease(MODEL* model)
{
    if (model == nullptr)return;//ヌルポガード

    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
    {

        if (model->VertexBuffer[m])
            model->VertexBuffer[m]->Release();
        if (model->IndexBuffer[m])
            model->IndexBuffer[m]->Release();

    }

    delete[] model->VertexBuffer;
    delete[] model->IndexBuffer;

    for (auto& pair : model->Texture)
    {
        if (pair.second)
            pair.second->Release();
    }

    aiReleaseImport(model->AiScene);
    delete model;
}

//----------------------------------------------
// 描画
//----------------------------------------------
void ModelDraw(MODEL* model, const DirectX::XMMATRIX& mtxWorld)
{

    // モデル未読込時は描画しない（呼び出し側の初期化失敗に耐える）
    if (model == nullptr || model->AiScene == nullptr || model->AiScene->mNumMeshes == 0)
    {
        return;
    }

    // シェーダーを描画パイプラインに設定
    Shader3d_Begin();


    // プリミティブトポロジ
    Direct3D_GetContext()->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ワールド行列
    Shader3d_SetWorldMatrix(mtxWorld);

    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = model->AiScene->mMeshes[m];

        // フェイス数 0 or バッファなし → スキップ
        if (mesh->mNumFaces == 0)
            continue;
        if (!model->VertexBuffer[m] || !model->IndexBuffer[m])
            continue;

        // -------- テクスチャ設定 --------
        aiString texName;
        aiMaterial* aimaterial =
            model->AiScene->mMaterials[mesh->mMaterialIndex];

        aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texName);

        bool textureSet = false;

        if (texName.length != 0)
        {
            // 事前に読み込んで model->Texture に入れてある SRV を探す
            auto it = model->Texture.find(texName.C_Str());
            if (it != model->Texture.end() && it->second)
            {
                ID3D11ShaderResourceView* srv = it->second;
                Direct3D_GetContext()->PSSetShaderResources(
                    0, 1, &srv);
                textureSet = true;
            }
        }

        // テクスチャが設定できなかったら白テクスチャ
        if (!textureSet)
        {
            Set_Texture(g_TextureWhite);
        }

        // -------- マテリアルカラー設定 --------
        {
            aiMaterial* aimaterial =
                model->AiScene->mMaterials[mesh->mMaterialIndex];
            aiColor3D diffuse;
            aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);

            Shader3d_SetColor(
                XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));
        }

        // -------- VB / IB 設定＆描画 --------
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;

        Direct3D_GetContext()->IASetVertexBuffers(
            0, 1, &model->VertexBuffer[m], &stride, &offset);
        Direct3D_GetContext()->IASetIndexBuffer(
            model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

        UINT indexCount = mesh->mNumFaces * 3;
        Direct3D_GetContext()->DrawIndexed(indexCount, 0, 0);
    }
}


//----------------------------------------------
// AABB（　スケール対応版）
//----------------------------------------------
AABB ModelGetAABB(MODEL* model, const DirectX::XMFLOAT3& position)
{
    AABB aabb;

    // モデルが無効な場合は位置点AABBを返す（クラッシュ回避）
    if (model == nullptr || model->AiScene == nullptr || model->AiScene->mNumMeshes == 0)
    {
        aabb.min = position;
        aabb.max = position;
        return aabb;
    }


    aiVector3D min = model->AiScene->mMeshes[0]->mAABB.mMin;
    aiVector3D max = model->AiScene->mMeshes[0]->mAABB.mMax;

    for (unsigned int m = 1; m < model->AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = model->AiScene->mMeshes[m];
        if (min.x > mesh->mAABB.mMin.x) min.x = mesh->mAABB.mMin.x;
        if (min.y > mesh->mAABB.mMin.y) min.y = mesh->mAABB.mMin.y;
        if (min.z > mesh->mAABB.mMin.z) min.z = mesh->mAABB.mMin.z;
        if (max.x < mesh->mAABB.mMax.x) max.x = mesh->mAABB.mMax.x;
        if (max.y < mesh->mAABB.mMax.y) max.y = mesh->mAABB.mMax.y;
        if (max.z < mesh->mAABB.mMax.z) max.z = mesh->mAABB.mMax.z;
    }

    // 
    // スケールを適用
    aabb.min.x = min.x * model->Scale + position.x;
    aabb.min.y = min.y * model->Scale + position.y;
    aabb.min.z = min.z * model->Scale + position.z;
    aabb.max.x = max.x * model->Scale + position.x;
    aabb.max.y = max.y * model->Scale + position.y;
    aabb.max.z = max.z * model->Scale + position.z;

    return aabb;
}

void ModelDrawWithoutBegin(MODEL* model, const DirectX::XMMATRIX& mtxWorld)
{
    if (model == nullptr || model->AiScene == nullptr || model->AiScene->mNumMeshes == 0)
        return;

    // Shader3d_Begin() を呼ばない（呼び出し元が既にシェーダーをセット済み）
    Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ワールド行列（呼び出し元のシェーダーのCBに渡す）
    // ※ ShaderEdge_BeginNormalPass の場合は ShaderEdge_SetWorldMatrix を先に呼ぶこと


    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; m++)
    {
        aiMesh* mesh = model->AiScene->mMeshes[m];

        if (mesh->mNumFaces == 0) continue;
        if (!model->VertexBuffer[m] || !model->IndexBuffer[m]) continue;

        // テクスチャ設定
        aiString texName;
        aiMaterial* aimaterial =
            model->AiScene->mMaterials[mesh->mMaterialIndex];
        aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texName);

        bool textureSet = false;
        if (texName.length != 0)
        {
            auto it = model->Texture.find(texName.C_Str());
            if (it != model->Texture.end() && it->second)
            {
                ID3D11ShaderResourceView* srv = it->second;
                Direct3D_GetContext()->PSSetShaderResources(0, 1, &srv);
                textureSet = true;
            }
        }
        if (!textureSet)
        {
            Set_Texture(g_TextureWhite);
        }

        // マテリアルカラー設定
        {
            aiColor3D diffuse;
            aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            Shader3d_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));
        }

        // VB / IB 設定＆描画
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;

        Direct3D_GetContext()->IASetVertexBuffers(
            0, 1, &model->VertexBuffer[m], &stride, &offset);
        Direct3D_GetContext()->IASetIndexBuffer(
            model->IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

        UINT indexCount = mesh->mNumFaces * 3;
        Direct3D_GetContext()->DrawIndexed(indexCount, 0, 0);
    }
}

//----------------------------------------------
// メッシュ数（マテリアルごとにまとめられた後の数）
//----------------------------------------------
int ModelGetMeshCount(MODEL* model)
{
    if (model == nullptr || model->AiScene == nullptr) return 0;
    return static_cast<int>(model->AiScene->mNumMeshes);
}

//----------------------------------------------
// 1つのメッシュだけを描画する（パーツごとに動かすエネミー用。目のまばたきなど）
//   meshIndex : 0 〜 ModelGetMeshCount()-1。範囲外なら何もしない
//----------------------------------------------
void ModelDrawMesh(MODEL* model, int meshIndex, const DirectX::XMMATRIX& mtxWorld)
{
    if (model == nullptr || model->AiScene == nullptr) return;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(model->AiScene->mNumMeshes)) return;

    aiMesh* mesh = model->AiScene->mMeshes[meshIndex];
    if (mesh->mNumFaces == 0) return;
    if (!model->VertexBuffer[meshIndex] || !model->IndexBuffer[meshIndex]) return;

    Shader3d_Begin();
    Direct3D_GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Shader3d_SetWorldMatrix(mtxWorld);

    // テクスチャ（無ければ白）
    aiMaterial* aimaterial = model->AiScene->mMaterials[mesh->mMaterialIndex];
    aiString texName;
    aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texName);
    bool textureSet = false;
    if (texName.length != 0)
    {
        auto it = model->Texture.find(texName.C_Str());
        if (it != model->Texture.end() && it->second)
        {
            ID3D11ShaderResourceView* srv = it->second;
            Direct3D_GetContext()->PSSetShaderResources(0, 1, &srv);
            textureSet = true;
        }
    }
    if (!textureSet) Set_Texture(g_TextureWhite);

    aiColor3D diffuse;
    aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
    Shader3d_SetColor(XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f));

    UINT stride = sizeof(Vertex3D);
    UINT offset = 0;
    Direct3D_GetContext()->IASetVertexBuffers(0, 1, &model->VertexBuffer[meshIndex], &stride, &offset);
    Direct3D_GetContext()->IASetIndexBuffer(model->IndexBuffer[meshIndex], DXGI_FORMAT_R32_UINT, 0);
    Direct3D_GetContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
}

//----------------------------------------------
// マテリアル名からメッシュ番号を探す（見つからなければ -1）
//----------------------------------------------
int ModelFindMesh(MODEL* model, const char* materialName)
{
    if (model == nullptr || model->AiScene == nullptr || materialName == nullptr) return -1;
    for (unsigned int m = 0; m < model->AiScene->mNumMeshes; ++m)
    {
        aiString name;
        model->AiScene->mMaterials[model->AiScene->mMeshes[m]->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
        if (strcmp(name.C_Str(), materialName) == 0) return static_cast<int>(m);
    }
    return -1;
}
