/*==============================================================================

  2Dスプライト描画 実装 [Sprite.cpp]
														 Author : 51106
														 Date   : 2026/02/15
--------------------------------------------------------------------------------

  ・動的頂点バッファ(4頂点のTRIANGLESTRIP)でスプライト描画
  ・正射影(左上原点)のプロジェクションを Sprite_Begin でセット
  ・描画関数は用途別にオーバーロード：
	  - 全表示
	  - 全表示(サイズ変更)
	  - UVカット(部分表示)
	  - UVカット(サイズ変更)
	  - UVカット(サイズ変更+回転) … スプライト中心回転

==============================================================================*/

#include <d3d11.h>
#include <DirectXMath.h>
#include "DirectXTex.h"
using namespace DirectX;
#include "direct3d.h"
#include "shader.h"
#include "debug_ostream.h"
#include "sprite.h"
#include "texture.h"

//------------------------------------
// 定数
//------------------------------------
static constexpr int NUM_VERTEX = 4; // 4頂点(四角形)を TRIANGLESTRIP で描く

//------------------------------------
// このモジュール内で使うリソース
//------------------------------------
static ID3D11Buffer* g_pVertexBuffer = nullptr;              // 頂点バッファ(動的)
static ID3D11ShaderResourceView* g_pTexture = nullptr;       // 使っていない(予約)
// ※ 実際は Set_Texture(texid) を使用

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;

//------------------------------------
// 頂点構造体
//------------------------------------
struct Vertex
{
	XMFLOAT3 position; // 頂点座標 (画面座標系)
	XMFLOAT4 color;    // 乗算カラー
	XMFLOAT2 uv;       // テクスチャ座標
};

//==============================================================================
// 初期化／終了
//==============================================================================

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// 引数チェック
	if (!pDevice || !pContext) {
		hal::dout << "Sprite_Initialize() : 与えられたデバイスかコンテキストが不正です" << std::endl;
		return;
	}

	// 保持
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成（動的：毎回 Map/Unmap で書き換える）
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	g_pDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);
}

void Sprite_Finalize(void)
{
	SAFE_RELEASE(g_pTexture);      // 予約(未使用だが解放は安全)
	SAFE_RELEASE(g_pVertexBuffer); // 頂点バッファ解放
}

//==============================================================================
// 描画開始（正射影セット）
// ・左上(0,0) 右下(幅,高さ) の LH オフセンター射影を設定
//==============================================================================
void Sprite_Begin()
{
	// 仮想解像度（スプライト座標系）を固定にすることで、
	// バックバッファがモニター解像度に変わっても画面全体に引き伸ばされる
	// 3Dはビューポートがネイティブ解像度なのでそのままシャープに描画される
	static constexpr float VIRTUAL_W = 1600.0f;
	static constexpr float VIRTUAL_H = 900.0f;

	// 2D用：左上原点の正射影を VS 定数へ
	Shader_SetProjectionMatrix(
		XMMatrixOrthographicOffCenterLH(
			0.0f, VIRTUAL_W,     // left, right
			VIRTUAL_H, 0.0f,    // bottom, top（LH版はここで上下を入れ替える）
			0.0f, 1.0f));           // nearZ, farZ
}

//==============================================================================
// 正方形専用の正射影（画面中央基準、ピクセル比を保持）
//==============================================================================
void Sprite_BeginSquare()
{
	static constexpr float VIRTUAL_W = 1600.0f;
	static constexpr float VIRTUAL_H = 900.0f;

	Shader_SetProjectionMatrix(
		XMMatrixOrthographicOffCenterLH(
			0.0f, VIRTUAL_W,
			VIRTUAL_H, 0.0f,
			0.0f, 1.0f
		)
	);
}
//==============================================================================
// 描画：テクスチャ全表示（原寸）
// ・(sx,sy) を左上として tex の幅高さで表示
//==============================================================================
void Sprite_Draw(int texid, float sx, float sy, const DirectX::XMFLOAT4& color)
{
	// シェーダをパイプラインへ
	Shader_Begin();

	// 頂点バッファをロック
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	// テクスチャの実ピクセルサイズ
	unsigned int dw = Texture_Width(texid);
	unsigned int dh = Texture_Height(texid);

	// 4頂点（左上→右上→左下→右下）
	v[0].position = { sx,       sy,        0.0f };
	v[1].position = { sx + dw,  sy,        0.0f };
	v[2].position = { sx,       sy + dh,   0.0f };
	v[3].position = { sx + dw,  sy + dh,   0.0f };

	// 乗算カラー
	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV（全域表示）
	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	// アンマップ
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// ワールド=単位行列（位置は頂点座標に直接書いたので不要）
	Shader_SetWorldMatrix(XMMatrixIdentity());

	// 入力アセンブラ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// テクスチャセット
	Set_Texture(texid);

	// 描画
	g_pContext->Draw(NUM_VERTEX, 0);
}

//==============================================================================
// 描画：UVカット（部分表示・原寸）
// ・テクスチャ内の (pixx,pixy)-(pixw,pixh) 矩形を切り出して表示
//==============================================================================
void Sprite_Draw(int texid, float sx, float sy,
	int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color)
{
	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	// 表示矩形（原寸）
	v[0].position = { sx,          sy,           0.0f };
	v[1].position = { sx + pixw,   sy,           0.0f };
	v[2].position = { sx,          sy + pixh,    0.0f };
	v[3].position = { sx + pixw,   sy + pixh,    0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV をテクスチャサイズから正規化
	const float tw = static_cast<float>(Texture_Width(texid));
	const float th = static_cast<float>(Texture_Height(texid));
	const float u0 = pixx / tw;
	const float v0 = pixy / th;
	const float u1 = (pixx + pixw) / tw;
	const float v1 = (pixy + pixh) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Set_Texture(texid);
	g_pContext->Draw(NUM_VERTEX, 0);
}

//==============================================================================
// 描画：UVカット（部分表示）＋サイズ変更
// ・画面上の表示サイズを sw, sh で指定
//==============================================================================
void Sprite_Draw(int texid, float sx, float sy, float sw, float sh,
	int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color)
{
	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	// 画面上の表示矩形（サイズ変更）
	v[0].position = { sx,       sy,        0.0f };
	v[1].position = { sx + sw,  sy,        0.0f };
	v[2].position = { sx,       sy + sh,   0.0f };
	v[3].position = { sx + sw,  sy + sh,   0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV は切り出し矩形を正規化
	const float tw = static_cast<float>(Texture_Width(texid));
	const float th = static_cast<float>(Texture_Height(texid));
	const float u0 = pixx / tw;
	const float v0 = pixy / th;
	const float u1 = (pixx + pixw) / tw;
	const float v1 = (pixy + pixh) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Set_Texture(texid);
	g_pContext->Draw(NUM_VERTEX, 0);
}

//==============================================================================
// 描画：UVカット（部分表示）＋サイズ変更＋回転
// ・スプライト中心を原点(-0.5〜+0.5)で作ってから
//   Scale → RotateZ(angle) → Translate(sx+sw/2, sy+sh/2) の順で変換
//==============================================================================
void Sprite_Draw(int texid, float sx, float sy, float sw, float sh,
	int pixx, int pixy, int pixw, int pixh, float angle,
	const DirectX::XMFLOAT4& color)
{
	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	// 中心原点の正方形（-0.5〜+0.5）
	// → 後段のワールド行列で Scale/Rotate/Translate
	v[0].position = { -0.5f, -0.5f, 0.0f };
	v[1].position = { +0.5f, -0.5f, 0.0f };
	v[2].position = { -0.5f, +0.5f, 0.0f };
	v[3].position = { +0.5f, +0.5f, 0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV（部分表示）
	const float tw = static_cast<float>(Texture_Width(texid));
	const float th = static_cast<float>(Texture_Height(texid));
	const float u0 = pixx / tw;
	const float v0 = pixy / th;
	const float u1 = (pixx + pixw) / tw;
	const float v1 = (pixy + pixh) / th;

	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	g_pContext->Unmap(g_pVertexBuffer, 0);

	// 変換行列：Scale → RotateZ → Translate(中心へ)
	const XMMATRIX scale = XMMatrixScaling(sw, sh, 1.0f);
	const XMMATRIX rotation = XMMatrixRotationZ(angle);
	const XMMATRIX translation = XMMatrixTranslation(sx + sw * 0.5f, sy + sh * 0.5f, 0.0f);
	Shader_SetWorldMatrix(scale * rotation * translation);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Set_Texture(texid);
	g_pContext->Draw(NUM_VERTEX, 0);
}

//==============================================================================
// SRV（テクスチャ）をスクリーン座標で矩形描画する
//==============================================================================
void Sprite_DrawSRV(
	ID3D11ShaderResourceView* srv,
	float sx, float sy, float sw, float sh,
	const DirectX::XMFLOAT4& color)
{
	if (!srv || !g_pContext || !g_pVertexBuffer) return;

	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	if (FAILED(g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
		return;

	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	v[0].position = { sx,      sy,      0.0f };
	v[1].position = { sx + sw, sy,      0.0f };
	v[2].position = { sx,      sy + sh, 0.0f };
	v[3].position = { sx + sw, sy + sh, 0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	g_pContext->Unmap(g_pVertexBuffer, 0);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_pContext->PSSetShaderResources(0, 1, &srv);
	g_pContext->Draw(NUM_VERTEX, 0);
}

// UV座標指定版
void Sprite_DrawSRV_UV(
	ID3D11ShaderResourceView* srv,
	float sx, float sy, float sw, float sh,
	float u0, float v0, float u1, float v1,  // UV座標
	const DirectX::XMFLOAT4& color)
{
	if (!srv || !g_pContext || !g_pVertexBuffer) return;

	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	if (FAILED(g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
		return;

	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	v[0].position = { sx,      sy,      0.0f };
	v[1].position = { sx + sw, sy,      0.0f };
	v[2].position = { sx,      sy + sh, 0.0f };
	v[3].position = { sx + sw, sy + sh, 0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV座標を指定
	v[0].uv = { u0, v0 };
	v[1].uv = { u1, v0 };
	v[2].uv = { u0, v1 };
	v[3].uv = { u1, v1 };

	g_pContext->Unmap(g_pVertexBuffer, 0);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_pContext->PSSetShaderResources(0, 1, &srv);
	g_pContext->Draw(NUM_VERTEX, 0);
}


//==============================================================================
// 描画：全表示（サイズ変更）
// ・(sx,sy)-(sw,sh) でそのまま表示
//==============================================================================
void Sprite_Draw(int texid, float sx, float sy, float sw, float sh,
	const DirectX::XMFLOAT4& color)
{
	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	Vertex* v = reinterpret_cast<Vertex*>(msr.pData);

	// 表示矩形
	v[0].position = { sx,      sy,      0.0f };
	v[1].position = { sx + sw,   sy,      0.0f };
	v[2].position = { sx,      sy + sh,   0.0f };
	v[3].position = { sx + sw,   sy + sh,   0.0f };

	v[0].color = v[1].color = v[2].color = v[3].color = color;

	// UV（全域）
	v[0].uv = { 0.0f, 0.0f };
	v[1].uv = { 1.0f, 0.0f };
	v[2].uv = { 0.0f, 1.0f };
	v[3].uv = { 1.0f, 1.0f };

	g_pContext->Unmap(g_pVertexBuffer, 0);
	Shader_SetWorldMatrix(XMMatrixIdentity());

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	Set_Texture(texid);
	g_pContext->Draw(NUM_VERTEX, 0);
}
