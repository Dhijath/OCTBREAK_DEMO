/*==============================================================================

   シールドシステム [shield.h]
                                                         Author : 51106
                                                         Date   : 2026/03/21
--------------------------------------------------------------------------------

   ■概要
   ・LT 長押しでプレイヤーを包むD10多面体シールドを展開する
   ・ガード中はダメージを 50% 軽減
   ・被弾時にフラッシュ演出

   ■使い方（player.cpp 側）
     Shield_Initialize();
     Shield_Update(dt, ltPressed);          // 毎フレーム
     Shield_Draw(playerCenterPos);          // 不透明描画後に呼ぶ
     Shield_Finalize();

==============================================================================*/
#pragma once
#include <DirectXMath.h>

// 初期化（D3D リソース生成）
void  Shield_Initialize();

// 終了処理（D3D リソース解放）
void  Shield_Finalize();

// 毎フレーム更新
// ・guarding : LT が 0.5f 以上押されているか
void  Shield_Update(double dt, bool guarding);

// 描画（不透明オブジェクト描画後・エッジ描画後に呼ぶ）
// ・center : シールドの中心座標（プレイヤー腰辺り）
void  Shield_Draw(const DirectX::XMFLOAT3& center);

// ガード中か
bool  Shield_IsActive();

// ダメージを受けたときに呼ぶ（フラッシュ演出）
void  Shield_NotifyHit();

// ダメージ軽減率（0.5 = 50% 軽減）
float Shield_GetDamageReduction();