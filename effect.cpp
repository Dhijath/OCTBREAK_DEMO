/*==============================================================================

   2D スプライトエフェクト [effect.cpp]
                                                         Author : 51106
                                                         Date   : 2026/01/20
--------------------------------------------------------------------------------
   スプライトアニメーションを使った 2D エフェクトを管理するモジュール。
   画面座標で指定した位置にアニメを再生し、終了したら自動で非アクティブにする。

   ■機能
     - Effect_Initialize : テクスチャ・アニメパターンをロード
     - Effect_Finalize   : リソース解放
     - Effect_ClearAll   : アセット解放なしで全エフェクトをリセット（ルーム遷移時）
     - Effect_Update     : 全エフェクトの再生状態を更新
     - Effect_Draw       : アクティブなエフェクトを描画
     - Effect_Create     : 指定 2D 座標にエフェクトを 1 つ生成

   ■上限
     同時表示可能なエフェクト数は EFFECT_MAX（256）まで。

==============================================================================*/
#include "effect.h"
#include <DirectXMath.h>
using namespace DirectX;
#include "sprite_anim.h"
#include "texture.h"
#include "Audio.h"


#pragma comment(lib, "winmm.lib")

struct Effect
{
	XMFLOAT2 position;
	// XMFLOAT2 velocity;
	// double lifeTime;
	bool isEnable;
	int sprite_anim_id;
};

static constexpr int EFFECT_MAX = 256;
static Effect g_Effect[EFFECT_MAX]{};
static int g_EffectTexID = -1;
static int g_AnimPatternID = -1;
static int g_EffectSoundID = -1;

void Effect_Initialize()
{
	for (Effect& e : g_Effect)
	{
		e.isEnable = false;
	}

	g_EffectTexID = Texture_Load(L"explosion_96.png");
	g_EffectSoundID = LoadAudio(".wav");
	g_AnimPatternID = SpriteAnim_PatternRegister(g_EffectTexID, 8, 8,  { 96, 96 }, { 0,0 }, false);
}

void Effect_Finalize()
{
	UnloadAudio(g_EffectSoundID);
}

// アセット解放なしで全エフェクトをクリア（ルーム遷移時用）
void Effect_ClearAll()
{
	for (Effect& e : g_Effect)
	{
		if (!e.isEnable) continue;
		SpriteAnim_DestroyPlayer(e.sprite_anim_id);
		e.isEnable = false;
	}
}

void Effect_Update(double elapsed_time)
{
	for (Effect& e : g_Effect)
	{
		if (!e.isEnable) continue;

		if (SpriteAnim_IsStopped(e.sprite_anim_id))
		{
			SpriteAnim_DestroyPlayer(e.sprite_anim_id);
			e.isEnable = false;
		}
	}
}

void Effect_Draw()
{
	for (Effect& e : g_Effect)
	{
		if (!e.isEnable) continue;

		SpriteAnim_Draw(e.sprite_anim_id, e.position.x, e.position.y, 148, 148);
	}
}

void Effect_Create(const DirectX::XMFLOAT2& position)
{
	for (Effect& e : g_Effect)
	{
		if (e.isEnable) continue;

		//空き領域発見
		e.isEnable = true;
		e.position = position;
		e.sprite_anim_id = SpriteAnim_CreatePlayer(g_AnimPatternID);
		PlayAudio(g_EffectSoundID);
		break;
	}
}
