/*==============================================================================

   キューブ描画 [cube.cpp]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   1×1×1 の単位キューブを描画するモジュール。
   迷路の壁・床タイルの描画ベースとして使用する。

   ■機能
     - Cube_Initialize        : D3D11 頂点バッファ・インデックスバッファを生成
     - Cube_Finalize          : バッファ解放
     - Cube_Draw              : テクスチャとワールド行列を指定してキューブを1つ描画
     - Cube_DrawWall          : 壁専用シェーダーでキューブを描画
     - Cube_CreateAABB        : ローカル基準の AABB を生成（当たり判定用）
     - Cube_CreateAABBFromWorld : ワールド行列込みの AABB を生成（床レジストリ登録用）

==============================================================================*/
#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "Shader3d.h"
#include "key_logger.h"
#include "mouse.h"
#include "texture.h"
#include "WallShader.h"

using namespace DirectX;


// 3D用頂点データ
struct Vertex3D
{
    XMFLOAT3 position; // 頂点座標
    XMFLOAT3 normal;   // 法線ベクトル
    XMFLOAT4 color;    // 頂点カラー
    XMFLOAT2 uv;       // UV 座標
};

namespace {
    int g_CubeTexId = -1; // キューブに使うテクスチャID

    constexpr int NUM_VERTEX = 24; // キューブ1個分の頂点数
    constexpr int NUM_INDEX = 36; // インデックス数（三角形 12枚）

    ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
    ID3D11Buffer* g_pIndexBuffer = nullptr; // インデックスバッファ

    // 注意：初期化時に外部から渡されるので Release は不要
    ID3D11Device* g_pDevice = nullptr;
    ID3D11DeviceContext* g_pContext = nullptr;

    float g_RotationX = 0.0f;
    float g_RotationY = 0.0f;
    float g_RotationZ = 0.0f;

    XMFLOAT3 g_TranslationPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 g_Scaling = { 1.0f, 1.0f, 1.0f };

    constexpr float SCALE_SPEED = 0.5f;

    // キューブの頂点データ（位置・法線・色・UV）
    Vertex3D g_CubeVertex[]
    {
        // 前面 (z = -0.5f)
        {{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.5f}},
        {{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, 0.5f,-0.5f},{0.0f,0.0f,-1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.0f}},
        {{ 0.5f,-0.5f,-0.5f},{0.0f,0.0f,-1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.5f}},

        // 後面 (z = 0.5f)
        {{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.0f}}, //11
        {{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.5f}}, //10
        {{ 0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.5f}},//00
        {{ 0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f},{1.0f,1.0f,1.0f,1.0f}, {0.25f, 0.0f}},//01

        // 左面 (x = -0.5f)
        {{-0.5f, 0.5f, 0.5f},{-1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f}, {0.75f, 0.0f}},//11
        {{-0.5f, 0.5f,-0.5f},{-1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f}, {0.75f, 0.5f}},//10
        {{-0.5f,-0.5f,-0.5f},{-1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.5f}}, //00
        {{-0.5f,-0.5f, 0.5f},{-1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f}, {0.5f, 0.0f}}, //01

        // 右面 (x = 0.5f)
        {{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.25f, 0.5f}}, //11
        {{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.25f, 1.0f}}, //10
        {{ 0.5f,-0.5f, 0.5f},{1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.0f, 1.0f}},  //00
        {{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.0f, 0.5f}},  //01

        // 上面 (y = 0.5f)
        {{-0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.5f,  0.5f}}, //11
        {{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.5f,  1.0f}}, //10
        {{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.25f, 1.0f}}, //00
        {{ 0.5f, 0.5f,-0.5f},{0.0f,1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.25f, 0.5f}}, //01

        // 下面 (y = -0.5f)
        {{-0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.75f, 0.5f}},
        {{-0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{0.75f, 0.0f}},
        {{ 0.5f,-0.5f,-0.5f},{0.0f,-1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{1.0f,  0.0f}},
        {{ 0.5f,-0.5f, 0.5f},{0.0f,-1.0f,0.0f},{1.0f,1.0f,1.0f,1.0f},{1.0f,  0.5f}},
    };

    // キューブのインデックス（各面ごとに三角形2枚）
    unsigned short g_CubeVertexIndex[36]
    {
        0,  1,  2,  0,  2,  3,      // 前面
        4,  5,  6,  4,  6,  7,      // 後面
        8,  9, 10,  8, 10, 11,      // 左面
        12,13,14, 12,14,15,         // 右面
        16,17,18, 16,18,19,         // 上面
        20,21,22, 20,22,23          // 下面
    };
}

// キューブ描画用の各種リソースを作成
void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    // デバイスとデバイスコンテキストを保存
    g_pDevice = pDevice;
    g_pContext = pContext;

    // 頂点バッファ生成
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;              // GPU 専用（CPU からは書き換えない）
    bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = g_CubeVertex;
    g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

    // インデックスバッファ生成
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;

    sd.pSysMem = g_CubeVertexIndex;
    g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);
}

// キューブ用リソースの解放
void Cube_Finalize(void)
{
    SAFE_RELEASE(g_pIndexBuffer);
    SAFE_RELEASE(g_pVertexBuffer);
}

// キューブ1個分を描画（ワールド行列とテクスチャIDを受け取る）
void Cube_Draw(int texID, const XMMATRIX mtxW)
{
    // 3D シェーダーをセット
    Shader3d_Begin();


    // キューブの基本色（テクスチャに乗算される）
    Shader3d_SetColor({ 1.0f,1.0f,1.0f,1.0f });

    // 使用するテクスチャを指定
    Set_Texture(texID);

    // 頂点バッファをパイプラインに設定
    UINT stride = sizeof(Vertex3D);
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // インデックスバッファを設定
    g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // ワールド行列をシェーダーに送る
    Shader3d_SetWorldMatrix(mtxW);

    // 三角形リストで描画
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画命令発行
    g_pContext->DrawIndexed(36, 0, 0);
}

// 中心 position、サイズ 1x1x1 の AABB を作成
AABB Cube_CreateAABB(const DirectX::XMFLOAT3& position)
{
    return {
        {
            position.x - 0.5f,
            position.y - 0.5f,
            position.z - 0.5f,
        },
        {
            position.x + 0.5f,
            position.y + 0.5f,
            position.z + 0.5f
        }
    };
}

AABB Cube_CreateAABBFromWorld(const DirectX::XMMATRIX& world)
{
    using namespace DirectX;

    // ローカル空間の8頂点（-0.5～+0.5）
    const XMVECTOR corners[8] =
    {
        XMVectorSet(-0.5f,-0.5f,-0.5f, 1.0f),
        XMVectorSet(+0.5f,-0.5f,-0.5f, 1.0f),
        XMVectorSet(-0.5f,+0.5f,-0.5f, 1.0f),
        XMVectorSet(+0.5f,+0.5f,-0.5f, 1.0f),
        XMVectorSet(-0.5f,-0.5f,+0.5f, 1.0f),
        XMVectorSet(+0.5f,-0.5f,+0.5f, 1.0f),
        XMVectorSet(-0.5f,+0.5f,+0.5f, 1.0f),
        XMVectorSet(+0.5f,+0.5f,+0.5f, 1.0f),
    };

    XMFLOAT3 mn = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
    XMFLOAT3 mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR pW = XMVector3TransformCoord(corners[i], world);

        XMFLOAT3 p{};
        XMStoreFloat3(&p, pW);

        mn.x = (p.x < mn.x) ? p.x : mn.x;
        mn.y = (p.y < mn.y) ? p.y : mn.y;
        mn.z = (p.z < mn.z) ? p.z : mn.z;

        mx.x = (p.x > mx.x) ? p.x : mx.x;
        mx.y = (p.y > mx.y) ? p.y : mx.y;
        mx.z = (p.z > mx.z) ? p.z : mx.z;
    }

    AABB aabb{};
    aabb.min = mn;
    aabb.max = mx;
    return aabb;
}

//==============================================================================
// 壁専用描画（WallShader 経由）
//==============================================================================
void Cube_DrawWall(
    int texID,
    const DirectX::XMMATRIX& mtxW,
    const DirectX::XMFLOAT2& uvRepeat)
{
    // 壁専用シェーダ開始（中身は Shader3d）
    WallShader_Begin();

    // 将来用（今は未使用）
    WallShader_SetUVRepeat(uvRepeat);

    // 行列・色・テクスチャ設定
    WallShader_SetWorldMatrix(mtxW);
    WallShader_SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    WallShader_SetTexture(texID);

    // 既存の VB / IB を流用
    UINT stride = sizeof(Vertex3D);
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pContext->DrawIndexed(36, 0, 0);
}

