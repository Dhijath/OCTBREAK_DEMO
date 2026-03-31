/*==============================================================================

   Item [Item.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/18
--------------------------------------------------------------------------------
==============================================================================*/

#include "Item.h"
#include "billboard.h"
#include "texture.h"
#include "Player.h"
#include "ItemManager.h"
#include <DirectXMath.h>

#include "Audio.h"
#include "HUD.h"

#include "score.h"
using namespace DirectX;

static const float ITEM_RADIUS = 0.6f;  // 取得判定の半径（メートル）
static const float ITEM_SCALE = 0.5f;  // ビルボード表示サイズ
static const int   ITEM_HP_VALUE = 20;    // HP回復量
static const float ITEM_ENERGY_VALUE = 50.0f; // エネルギー回復量
static const float ITEM_ATK_BONUS = 0.15f;  // 攻撃力倍率加算値（+15%）
static const float ITEM_SPEED_BONUS = 0.010f; // 速度倍率加算値
//static int s_TexID = -1;

//アイテム種別ごとのテクスチャ
static int s_TexHp = -1;
static int s_TexEnergy = -1;
static int s_TexAtk = -1;
static int s_TexSpeed = -1;

// 読み込み済み判定
static bool s_TexInited = false;

// アイテム種別ごとのSE
static int s_SeHp = -1;
static int s_SeEnergy = -1;
static int s_SeAtk = -1;
static int s_SeSpeed = -1;

static bool s_SeInited = false;

// SE読み込み（初回のみ）
static void Item_LoadSEOnce()
{
    if (s_SeInited) return;

    s_SeHp = LoadAudioWithVolume("resource/sound/item_hp.wav", 0.5f);
    s_SeEnergy = LoadAudioWithVolume("resource/sound/item_energy.wav", 0.5f);
    s_SeAtk = LoadAudioWithVolume("resource/sound/item_atk.wav", 0.5f);
    s_SeSpeed = LoadAudioWithVolume("resource/sound/item_speed.wav", 0.5f);

    s_SeInited = true;
}

// SE解放
void Item_UnloadSE()
{
    if (!s_SeInited) return;

    UnloadAudio(s_SeHp);     // HP回復SE解放
    UnloadAudio(s_SeEnergy); // エネルギー回復SE解放
    UnloadAudio(s_SeAtk);    // 攻撃力アップSE解放
    UnloadAudio(s_SeSpeed);  // スピードアップSE解放

    s_SeHp = -1;
    s_SeEnergy = -1;
    s_SeAtk = -1;
    s_SeSpeed = -1;

    s_SeInited = false;
}

// テクスチャ読み込み（初回のみ）
static void Item_LoadTexturesOnce()
{
    if (s_TexInited) return;

    // 実ファイルに合わせてパスを調整
    s_TexHp = Texture_Load(L"resource/texture/item_hp.png");
    s_TexEnergy = Texture_Load(L"resource/texture/item_energy.png");
    s_TexAtk = Texture_Load(L"resource/texture/item_atk.png");
    s_TexSpeed = Texture_Load(L"resource/texture/item_speed.png");

    // フォールバック（読み込み失敗時の保険）いちおう
    if (s_TexHp < 0) s_TexHp = Texture_Load(L"resource/texture/white.png");
    if (s_TexEnergy < 0) s_TexEnergy = Texture_Load(L"resource/texture/white.png");
    if (s_TexAtk < 0) s_TexAtk = Texture_Load(L"resource/texture/white.png");
    if (s_TexSpeed < 0) s_TexSpeed = Texture_Load(L"resource/texture/white.png");

    s_TexInited = true;
}


//==============================================================================
// Initialize
//==============================================================================
void Item::Initialize(ItemType type, const XMFLOAT3& position)
{
    m_Type = type;
    m_Position = position;
    m_Position.y += 0.5f;  // 床から浮かせる
    m_IsAlive = true;

    // white.png 1枚だけロードしてた板(ただの板になる)
    // if (s_TexID < 0)
    //     s_TexID = Texture_Load(L"resource/texture/white.png");

    // 種別テクスチャを初回だけロード
    Item_LoadTexturesOnce();

    Item_LoadSEOnce();
}




//==============================================================================
// Update
// プレイヤーとの距離が ITEM_RADIUS 以下なら取得処理を呼ぶ
//==============================================================================
void Item::Update()
{
    if (!m_IsAlive)         return;
    if (!Player_IsEnable()) return;

    const XMFLOAT3& playerPos = Player_GetPosition();

    const float dx = playerPos.x - m_Position.x;
    const float dz = playerPos.z - m_Position.z;
    const float distSq = dx * dx + dz * dz;

    if (distSq <= ITEM_RADIUS * ITEM_RADIUS)
        Pickup();
}

//==============================================================================
// Draw
// HP=赤 / エネルギー=水色 / 攻撃力=橙 / 速度=黄
//==============================================================================
void Item::Draw() const
{
    if (!m_IsAlive)  return;

    int texId = -1;
    switch (m_Type)
    {
    case ItemType::HP_HEAL:     texId = s_TexHp;     break;
    case ItemType::ENERGY_HEAL: texId = s_TexEnergy; break;
    case ItemType::ATK_UP:      texId = s_TexAtk;    break;
    case ItemType::SPEED_UP:    texId = s_TexSpeed;  break;
    default:                    texId = s_TexHp;     break;
    }

    if (texId < 0) return;

    // 絵の色をそのまま出す
    const XMFLOAT4 color = { 2.0f, 2.0f, 2.0f, 2.0f };

    const unsigned int tw = Texture_Width(texId);
    const unsigned int th = Texture_Height(texId);
    Billboard_Draw(
        texId,
        m_Position,
        { ITEM_SCALE, ITEM_SCALE },
        color,
        { 0.0f, 0.0f, static_cast<float>(tw), static_cast<float>(th) },
        { 0.5f, 0.5f }
    );
}

//==============================================================================
// Pickup
// 種類に応じてプレイヤーのパラメータを変化させる
//==============================================================================
void Item::Pickup()
{
    if (!m_IsAlive) return;



    switch (m_Type)
    {
    case ItemType::HP_HEAL:
        Player_Heal(ITEM_HP_VALUE);
        PlayAudio(s_SeHp, false);   // HP回復SE再生
        break;

    case ItemType::ENERGY_HEAL:
        Player_AddBeamEnergy(ITEM_ENERGY_VALUE);
        PlayAudio(s_SeEnergy, false);  // エネルギー回復SE再生
        break;

    case ItemType::ATK_UP:
        Player_SetDamageMultiplier(
            Player_GetDamageMultiplier() + ITEM_ATK_BONUS);
        PlayAudio(s_SeAtk, false);  // 攻撃力アップSE再生
        break;

    case ItemType::SPEED_UP:
        Player_SetSpeedMultiplier(
            Player_GetSpeedMultiplier() + ITEM_SPEED_BONUS);
        PlayAudio(s_SeSpeed, false);  // スピードアップSE再生
        break;
    }

    Score_Addscore(750);
    HUD_AddCollectedItem(m_Type);  // HUDに追加
    m_IsAlive = false;
}

//==============================================================================
// IsAlive
//==============================================================================
bool Item::IsAlive() const { return m_IsAlive; }

//==============================================================================
// GetType
//==============================================================================
ItemType Item::GetType() const { return m_Type; }

//==============================================================================
// GetPosition
//==============================================================================
const XMFLOAT3& Item::GetPosition() const { return m_Position; }