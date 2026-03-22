/*==============================================================================

   グリッド描画 [grid.cpp]
                                                         Author : 51106
                                                         Date   : 2025/11/12
--------------------------------------------------------------------------------
   デバッグ用のグリッド線を XZ 平面に描画するモジュール。
   10×10 マスのグリッドをラインリストで描画する。
   リリースビルドでは描画を省略してよい。

==============================================================================*/
#include "grid.h"
#include "direct3d.h"
#include <DirectXMath.h>
#include "shader3d.h"
using namespace DirectX;

static constexpr int GRID_H_COUNT = 10;
static constexpr int GRID_V_COUNT = 10;
static constexpr int GRID_H_LINE_COUNT = GRID_H_COUNT + 1;
static constexpr int GRID_V_LINE_COUNT = GRID_V_COUNT + 1;
static constexpr int NUM_VERTEX = GRID_H_LINE_COUNT * 2 + GRID_V_LINE_COUNT * 2;


static ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

// 3D頂点構造体
struct Vertex3d
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
};

static Vertex3d g_GridVertex[NUM_VERTEX]{};


void Grid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3d) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_GridVertex;

	float x = -5.0f;
	for (int i = 0; i < GRID_H_LINE_COUNT * 2; i += 2)
	{ 
		g_GridVertex[i]     = { { x, 0.0f,  5.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } };
		g_GridVertex[i + 1] = { { x, 0.0f, -5.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } };
		x += 1.0f;
	}

	float z = -5.0f;
	for (int i = GRID_V_LINE_COUNT * 2; i < NUM_VERTEX; i += 2)
	{
		g_GridVertex[i    ] = { {  5.0f, 0.0f, z },{ 0.0f, 1.0f, 0.0f, 1.0f } };
		g_GridVertex[i + 1] = { { -5.0f, 0.0f, z },{ 0.0f, 1.0f, 0.0f, 1.0f } };
		z += 1.0f;
	}

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Grid_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Grid_Draw(void)
{
	Shader3d_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3d);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// ワールド変換行列
	XMMATRIX mtxWorld = XMMatrixIdentity(); // 単位行列
	Shader3d_SetWorldMatrix(mtxWorld);

	// ビュー座標変換行列
	XMMATRIX mtxView = XMMatrixLookAtLH({ 5.0f, 5.0f, -5.0f ,1.0f}, { 0.0f, 0.0f, 0.0f ,1.0f}, { 0.0f, 1.0f, 0.0f, 0.0f });
	// Shader3d_SetViewMatrix(mtxView);

	// パースペクティブ行列
	constexpr float fovAngleY = XMConvertToRadians(60.0f);
	float aspectRatio = (float)Direct3D_GetBackBufferWidth() / (float)Direct3D_GetBackBufferHeight();
	float nearz = 0.1f;
	float farz = 100.0f;
	XMMATRIX  mtxProj = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearz, farz);
	// Shader3d_SetProjectMatrix(mtxProj);

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}
