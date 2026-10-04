/*==============================================================================

   エネミー共通の補助関数 [EnemyUtil.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------
   追加エネミー（EnemyEx）・追加ボス（EnemyBossEx）で共通に使う計算。
     ・プレイヤーまでの距離 / 向き
     ・予測射撃（プレイヤーの移動速度から着弾点を先読みする）
     ・向きと角度から弾を撃つ
==============================================================================*/
#pragma once
#include <DirectXMath.h>
#include <cmath>
#include "Player.h"
#include "EnemyBullet.h"

namespace EnemyUtil
{
    // プレイヤーの胴体（狙う位置）
    inline DirectX::XMFLOAT3 PlayerTarget()
    {
        const DirectX::XMFLOAT3 p = Player_GetPosition();
        return { p.x, p.y + 0.3f, p.z };
    }

    inline float DistXZ(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        const float dx = a.x - b.x, dz = a.z - b.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    // 近接攻撃・地面の衝撃波がプレイヤーに届くか判定する
    // ・from   : 攻撃の基準位置（エネミーの足元）
    // ・radius : 水平方向に届く距離（m）
    // ・height : 足元から届く高さ（m）。プレイヤーの足元がこれより上ならジャンプで回避したとみなす
    inline bool PlayerInReach(const DirectX::XMFLOAT3& from, float radius, float height)
    {
        const DirectX::XMFLOAT3 p = Player_GetPosition();
        const float dy = p.y - from.y;
        if (dy >= height || dy < -1.0f) return false;
        return DistXZ(from, p) < radius;
    }

    // from から見たプレイヤーの水平方向（正規化）。重なっていれば (0,0,1)
    inline DirectX::XMFLOAT3 DirToPlayerXZ(const DirectX::XMFLOAT3& from)
    {
        const DirectX::XMFLOAT3 p = Player_GetPosition();
        const float dx = p.x - from.x, dz = p.z - from.z;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len < 1e-4f) return { 0.0f, 0.0f, 1.0f };
        return { dx / len, 0.0f, dz / len };
    }

    // 予測射撃の向き。speed の弾がプレイヤーに当たる位置を、移動速度から1次近似で先読みする。
    // lead=0 で現在位置を狙う、1 で完全に先読みする（中間で外れやすさを調整できる）
    inline DirectX::XMFLOAT3 LeadDirection(const DirectX::XMFLOAT3& from, float speed, float lead = 1.0f)
    {
        DirectX::XMFLOAT3 target = PlayerTarget();
        const DirectX::XMFLOAT3* vel = Player_GetVelocityPtr();
        if (vel && speed > 0.1f)
        {
            const float dx = target.x - from.x, dy = target.y - from.y, dz = target.z - from.z;
            const float t = std::sqrt(dx * dx + dy * dy + dz * dz) / speed;   // おおよその到達時間
            target.x += vel->x * t * lead;
            target.z += vel->z * t * lead;
        }
        const float dx = target.x - from.x, dy = target.y - from.y, dz = target.z - from.z;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1e-4f) return { 0.0f, 0.0f, 1.0f };
        return { dx / len, dy / len, dz / len };
    }

    // dir を水平に yawOffset（ラジアン）だけ回した向き
    inline DirectX::XMFLOAT3 RotateYaw(const DirectX::XMFLOAT3& dir, float yawOffset)
    {
        const float c = std::cos(yawOffset), s = std::sin(yawOffset);
        return { dir.x * c + dir.z * s, dir.y, -dir.x * s + dir.z * c };
    }

    // 水平の角度（ラジアン。+Z が 0）から向きを作る
    inline DirectX::XMFLOAT3 DirFromYaw(float yaw, float pitch = 0.0f)
    {
        return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
    }

    inline void Fire(const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& dir, int damage, float speed)
    {
        EnemyBullet_Create(from, dir, damage, speed);
    }

    // プレイヤーを近くから吹き飛ばす（爆発・斬撃のノックバック）
    inline void KnockbackPlayer(const DirectX::XMFLOAT3& from, float strength, float up)
    {
        DirectX::XMFLOAT3* vel = Player_GetVelocityPtr();
        if (!vel) return;
        const DirectX::XMFLOAT3 d = DirToPlayerXZ(from);
        vel->x += d.x * strength;
        vel->z += d.z * strength;
        vel->y += up;
    }
}
