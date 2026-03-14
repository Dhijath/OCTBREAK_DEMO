/*==============================================================================

   OBB当たり判定 [collision_obb.cpp]
                                                         Author : 51106
                                                         Date   : 2026/01/17
--------------------------------------------------------------------------------
==============================================================================*/

#include "collision_obb.h"
#include "direct3d.h"
#include "shader.h"
#include "texture.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cfloat>

using namespace DirectX;

// デバッグ描画用（collision.cpp で定義されているものを参照）
extern ID3D11Buffer* g_pVertexBuffer;
extern ID3D11Device* g_pDevice;
extern ID3D11DeviceContext* g_pContext;
extern int g_WhiteId;

namespace
{
    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
        XMFLOAT2 uv;
    };
}

//==============================================================================
// 位置・サイズ・向き（front）から生成
//==============================================================================
OBB OBB::CreateFromFront(
    const DirectX::XMFLOAT3& position,
    const DirectX::XMFLOAT3& halfExtents,
    const DirectX::XMFLOAT3& front)
{
    OBB obb;
    obb.center = position;
    obb.halfExtents = halfExtents;

    // Y軸は常に上向き
    obb.axisY = { 0.0f, 1.0f, 0.0f };

    // front を正規化して Z軸にする
    XMVECTOR vFront = XMLoadFloat3(&front);
    vFront = XMVector3Normalize(vFront);
    XMStoreFloat3(&obb.axisZ, vFront);

    // X軸 = Y × Z（右手系）
    XMVECTOR vY = XMLoadFloat3(&obb.axisY);
    XMVECTOR vZ = XMLoadFloat3(&obb.axisZ);
    XMVECTOR vX = XMVector3Cross(vY, vZ);
    vX = XMVector3Normalize(vX);
    XMStoreFloat3(&obb.axisX, vX);

    return obb;
}

//==============================================================================
// AABBから生成（回転なし）
//==============================================================================
OBB OBB::CreateFromAABB(const AABB& aabb)
{
    OBB obb;

    // 中心座標
    obb.center.x = (aabb.min.x + aabb.max.x) * 0.5f;
    obb.center.y = (aabb.min.y + aabb.max.y) * 0.5f;
    obb.center.z = (aabb.min.z + aabb.max.z) * 0.5f;

    // 半サイズ
    obb.halfExtents.x = (aabb.max.x - aabb.min.x) * 0.5f;
    obb.halfExtents.y = (aabb.max.y - aabb.min.y) * 0.5f;
    obb.halfExtents.z = (aabb.max.z - aabb.min.z) * 0.5f;

    // 軸は標準XYZ
    obb.axisX = { 1.0f, 0.0f, 0.0f };
    obb.axisY = { 0.0f, 1.0f, 0.0f };
    obb.axisZ = { 0.0f, 0.0f, 1.0f };

    return obb;
}

//==============================================================================
// 任意の軸への投影（分離軸定理）
//
// ■概要
// ・OBBの任意の軸に投影したときの「半径」を返す
//
// ■引数
// ・obb  : 投影対象のOBB
// ・axis : 投影軸（正規化済み）
//
// ■戻り値
// ・投影半径（常に正の値）
//==============================================================================
static float ProjectOBBOntoAxis(const OBB& obb, const XMVECTOR& axis)
{
    XMVECTOR vX = XMLoadFloat3(&obb.axisX);
    XMVECTOR vY = XMLoadFloat3(&obb.axisY);
    XMVECTOR vZ = XMLoadFloat3(&obb.axisZ);

    float projX = fabsf(XMVectorGetX(XMVector3Dot(vX, axis))) * obb.halfExtents.x;
    float projY = fabsf(XMVectorGetX(XMVector3Dot(vY, axis))) * obb.halfExtents.y;
    float projZ = fabsf(XMVectorGetX(XMVector3Dot(vZ, axis))) * obb.halfExtents.z;

    return projX + projY + projZ;
}

//==============================================================================
// OBB同士の重なり判定（分離軸定理）
//==============================================================================
bool Collision_IsOverlapOBB(const OBB& a, const OBB& b)
{
    // 中心間ベクトル
    XMVECTOR vCenterA = XMLoadFloat3(&a.center);
    XMVECTOR vCenterB = XMLoadFloat3(&b.center);
    XMVECTOR vDist = vCenterB - vCenterA;

    // Y軸回転のみなので、検査軸4本（各OBBのX/Z軸）
    XMVECTOR axes[5] =
    {
        XMLoadFloat3(&a.axisX),
        XMLoadFloat3(&a.axisY),  // Y-axis check (needed for height separation)
        XMLoadFloat3(&a.axisZ),
        XMLoadFloat3(&b.axisX),
        XMLoadFloat3(&b.axisZ)
    };

    for (int i = 0; i < 5; ++i)
    {
        XMVECTOR axis = XMVector3Normalize(axes[i]);

        // 中心間距離の投影
        float distProj = fabsf(XMVectorGetX(XMVector3Dot(vDist, axis)));

        // 各OBBの投影半径
        float radiusA = ProjectOBBOntoAxis(a, axis);
        float radiusB = ProjectOBBOntoAxis(b, axis);

        // この軸で分離 → 非衝突
        if (distProj > radiusA + radiusB)
            return false;
    }

    // すべての軸で重なっている → 衝突
    return true;
}

//==============================================================================
// OBBとAABBの重なり判定
//==============================================================================
bool Collision_IsOverlapOBB_AABB(const OBB& obb, const AABB& aabb)
{
    // AABBをOBBに変換して判定
    OBB obbFromAABB = OBB::CreateFromAABB(aabb);
    return Collision_IsOverlapOBB(obb, obbFromAABB);
}

//==============================================================================
// OBB同士の衝突判定（押し戻しベクトル付き）
//==============================================================================
Hit Collision_IsHitOBB(const OBB& a, const OBB& b)
{
    Hit hit{};
    hit.isHit = false;

    XMVECTOR vCenterA = XMLoadFloat3(&a.center);
    XMVECTOR vCenterB = XMLoadFloat3(&b.center);
    XMVECTOR vDist = vCenterB - vCenterA;

    XMVECTOR axes[5] =
    {
        XMLoadFloat3(&a.axisX),
        XMLoadFloat3(&a.axisY),  // Y-axis check (needed for height separation)
        XMLoadFloat3(&a.axisZ),
        XMLoadFloat3(&b.axisX),
        XMLoadFloat3(&b.axisZ)
    };

    float minPenetration = FLT_MAX;
    XMVECTOR bestAxis = XMVectorZero();

    for (int i = 0; i < 5; ++i)
    {
        XMVECTOR axis = XMVector3Normalize(axes[i]);

        float distProj = XMVectorGetX(XMVector3Dot(vDist, axis));
        float radiusA = ProjectOBBOntoAxis(a, axis);
        float radiusB = ProjectOBBOntoAxis(b, axis);

        float penetration = (radiusA + radiusB) - fabsf(distProj);

        // この軸で分離 → 非衝突
        if (penetration < 0.0f)
            return hit;

        // 最小めり込み軸を記録
        if (penetration < minPenetration)
        {
            minPenetration = penetration;
            bestAxis = axis;

            // bからaへの押し出し方向に修正
            if (distProj < 0.0f)
                bestAxis = -bestAxis;
        }
    }

    hit.isHit = true;
    hit.penetration = minPenetration;
    XMStoreFloat3(&hit.normal, XMVector3Normalize(bestAxis));

    return hit;
}

//==============================================================================
// OBBとAABBの衝突判定（押し戻しベクトル付き）
//==============================================================================
Hit Collision_IsHitOBB_AABB(const OBB& obb, const AABB& aabb)
{
    // AABBをOBBに変換して判定
    OBB obbFromAABB = OBB::CreateFromAABB(aabb);
    return Collision_IsHitOBB(obb, obbFromAABB);
}

//==============================================================================
// デバッグ描画：OBB（ワイヤーフレーム）
//==============================================================================
void Collision_DebugDraw(const OBB& obb, const DirectX::XMFLOAT4& color)
{
    // nullチェック
    if (!g_pContext || !g_pVertexBuffer) return;

    XMVECTOR vCenter = XMLoadFloat3(&obb.center);
    XMVECTOR vX = XMLoadFloat3(&obb.axisX) * obb.halfExtents.x;
    XMVECTOR vY = XMLoadFloat3(&obb.axisY) * obb.halfExtents.y;
    XMVECTOR vZ = XMLoadFloat3(&obb.axisZ) * obb.halfExtents.z;

    // 8頂点を計算
    XMFLOAT3 corners[8];
    XMStoreFloat3(&corners[0], vCenter - vX - vY - vZ); // 0: Bottom-Back-Left
    XMStoreFloat3(&corners[1], vCenter + vX - vY - vZ); // 1: Bottom-Back-Right
    XMStoreFloat3(&corners[2], vCenter + vX + vY - vZ); // 2: Top-Back-Right
    XMStoreFloat3(&corners[3], vCenter - vX + vY - vZ); // 3: Top-Back-Left
    XMStoreFloat3(&corners[4], vCenter - vX - vY + vZ); // 4: Bottom-Front-Left
    XMStoreFloat3(&corners[5], vCenter + vX - vY + vZ); // 5: Bottom-Front-Right
    XMStoreFloat3(&corners[6], vCenter + vX + vY + vZ); // 6: Top-Front-Right
    XMStoreFloat3(&corners[7], vCenter - vX + vY + vZ); // 7: Top-Front-Left

    // 12本の線（24頂点）
    Vertex v[24];

    auto set_line = [&](int v_index, int corner_a, int corner_b)
        {
            v[v_index].position = corners[corner_a];
            v[v_index].color = color;
            v[v_index].uv = { 0.0f, 0.0f };

            v[v_index + 1].position = corners[corner_b];
            v[v_index + 1].color = color;
            v[v_index + 1].uv = { 0.0f, 0.0f };
        };

    // 底面（4本）
    set_line(0, 0, 1);
    set_line(2, 1, 5);
    set_line(4, 5, 4);
    set_line(6, 4, 0);

    // 上面（4本）
    set_line(8, 3, 2);
    set_line(10, 2, 6);
    set_line(12, 6, 7);
    set_line(14, 7, 3);

    // 縦の4本
    set_line(16, 0, 3);
    set_line(18, 1, 2);
    set_line(20, 5, 6);
    set_line(22, 4, 7);

    // D3D描画処理
    Shader_Begin();

    D3D11_MAPPED_SUBRESOURCE msr;
    HRESULT hr = g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
    if (FAILED(hr)) return;

    memcpy(msr.pData, v, sizeof(Vertex) * 24);
    g_pContext->Unmap(g_pVertexBuffer, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    Shader_SetWorldMatrix(XMMatrixIdentity());

    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    Set_Texture(g_WhiteId);

    g_pContext->Draw(24, 0);
}

//==============================================================================
// レイセグメントとOBBの交差判定（スラブ法）
//==============================================================================
bool OBB_RaySegmentIntersect(const OBB& obb, const XMFLOAT3& from, const XMFLOAT3& to)
{
    XMVECTOR vFrom = XMLoadFloat3(&from);
    XMVECTOR vTo   = XMLoadFloat3(&to);
    XMVECTOR vDir  = vTo - vFrom;

    float length = XMVectorGetX(XMVector3Length(vDir));
    if (length < 1e-6f) return false;

    vDir = XMVectorScale(vDir, 1.0f / length); // 正規化

    // OBB中心からレイ始点へのベクトル（OBBローカル空間で判定するため）
    XMVECTOR vCenter = XMLoadFloat3(&obb.center);
    XMVECTOR vOrigin = vFrom - vCenter;

    const XMVECTOR axes[3] = {
        XMLoadFloat3(&obb.axisX),
        XMLoadFloat3(&obb.axisY),
        XMLoadFloat3(&obb.axisZ),
    };
    const float halfExtents[3] = {
        obb.halfExtents.x,
        obb.halfExtents.y,
        obb.halfExtents.z,
    };

    float tMin = 0.0f;
    float tMax = length;

    for (int i = 0; i < 3; ++i)
    {
        float e = XMVectorGetX(XMVector3Dot(axes[i], vOrigin)); // 原点の軸投影
        float f = XMVectorGetX(XMVector3Dot(axes[i], vDir));    // 方向の軸投影

        if (fabsf(f) > 1e-6f)
        {
            float t1 = (-halfExtents[i] - e) / f;
            float t2 = ( halfExtents[i] - e) / f;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
        else
        {
            // レイがスラブと平行：スラブの外側なら交差しない
            if (-halfExtents[i] > e || e > halfExtents[i])
                return false;
        }
    }

    return tMin <= tMax;
}
