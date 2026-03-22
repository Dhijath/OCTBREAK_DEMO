/*==============================================================================

   弾ヒットエフェクト [bullet_hit_effect.h]
                                                         Author : 51106
                                                         Date   : 2026/02/20
--------------------------------------------------------------------------------
   弾ヒット時に 3D ワールド座標でビルボード再生するエフェクトのパブリック API。
   ヒット位置を XMFLOAT3 で渡すと自動でアニメを再生・終了まで管理する。

==============================================================================*/
#pragma once
#include <DirectXMath.h>

// 弾ヒットエフェクト全体の初期化（テクスチャ・アニメ登録など）
void BulletHitEffect_Initialize();

// 弾ヒットエフェクト全体の終了処理（残りエフェクト解放）
void BulletHitEffect_Finalize();

// アセット解放なしで全エフェクトをクリア（ルーム遷移時用）
void BulletHitEffect_ClearAll();

// 全ヒットエフェクトの更新（再生状態のチェックなど）
void BulletHitEffect_Update();

// 指定位置にヒットエフェクトを1つ生成
void BulletHitEffect_Create(const DirectX::XMFLOAT3& position);

// 全ヒットエフェクトを描画
void BulletHitEffect_Draw();
