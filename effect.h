/*==============================================================================

   2D スプライトエフェクト [effect.h]
                                                         Author : 51106
                                                         Date   : 2026/01/20
--------------------------------------------------------------------------------
   スプライトアニメーションを使った 2D エフェクト管理モジュールのパブリック API。
   画面座標 XMFLOAT2 でエフェクトを生成し、再生終了まで自動管理する。

==============================================================================*/
#ifndef EFFECT_H
#define EFFECT_H

#include <DirectXMath.h>

void Effect_Initialize();
void Effect_Finalize();

// アセット解放なしで全エフェクトをクリア（ルーム遷移時用）
void Effect_ClearAll();

void Effect_Update(double elapsed_time);
void Effect_Draw();
void Effect_Create(const DirectX::XMFLOAT2& position);

#endif // !EFFECT_H

