/*==============================================================================

   弾ヒットエフェクト [bullet_hit_effect.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/20
--------------------------------------------------------------------------------
   弾がエネミーや壁にヒットしたときに 3D 空間でビルボード再生する
   スプライトアニメーションエフェクトを管理するモジュール。

   ■機能
     - BulletHitEffect_Initialize : テクスチャ・アニメパターンをロード
     - BulletHitEffect_Finalize   : リソース解放
     - BulletHitEffect_ClearAll   : アセット解放なしで全エフェクトをリセット（ルーム遷移時）
     - BulletHitEffect_Update     : 全エフェクトの再生状態を更新
     - BulletHitEffect_Create     : 指定ワールド座標にエフェクトを 1 つ生成
     - BulletHitEffect_Draw       : アクティブなエフェクトをビルボードとして描画

   ■描画方式
     カメラを向くビルボードとして描画するため、
     BulletHitEffect_Draw は player_camera の回転情報を使用する。

==============================================================================*/
#include "sprite_anim.h"
#include <DirectXMath.h>
#include "bullet_hit_effect.h"
using namespace DirectX;
#include "texture.h"
#include "direct3d.h"
#include "billboard.h"
#include "player_camera.h"

namespace {
    int g_TexID = -1;          // エフェクト用テクスチャID
    int g_AnimePatternID = -1; // スプライトアニメのパターンID
    constexpr float HIT_EFFECT_SCALE = 0.25f;//エフェクトサイズ
}

// 弾がヒットしたときの 1インスタンス分のエフェクト
class BulletHitEffect
{
private:
    XMFLOAT3 m_position{};      // エフェクトを表示するワールド座標
    int      m_AnimePlayId{ -1 };  // スプライトアニメの再生ID
    bool     m_is_destroy{ false }; // 再生終了して破棄してよいかどうか

public:
    // コンストラクタ：位置を受け取って再生用プレイヤーを作成
    BulletHitEffect(const XMFLOAT3& position)
        : m_position(position),
        m_AnimePlayId(SpriteAnim_CreatePlayer(g_AnimePatternID))
    {
    }

    // デストラクタ：アニメプレイヤーを破棄
    ~BulletHitEffect() {
        SpriteAnim_DestroyPlayer(m_AnimePlayId);
    }

    // 状態更新（アニメが終わったかどうかチェック）
    void Update();

    // 描画処理
    void Draw() const;

    // 破棄してよいか？
    bool IsDestroy() const {
        return m_is_destroy;
    }
};

void BulletHitEffect::Update()
{
    // 登録しているアニメが停止していたら「終了」とみなす
    if (SpriteAnim_IsStopped(m_AnimePlayId))
    {
        m_is_destroy = true;
    }
}



void BulletHitEffect::Draw() const
{
    // ビルボード＋アニメでエフェクトを描画
    BillboardAnim_Draw(
        m_AnimePlayId,  // 再生ID
        m_position,     // 位置
        { HIT_EFFECT_SCALE,HIT_EFFECT_SCALE }   // 表示スケール
    );
}

// エフェクト同時表示の上限
static constexpr int MAX_BULLET_HIT_EFFECT = 256;

// 現在アクティブなエフェクト一覧
static BulletHitEffect* g_pEffects[MAX_BULLET_HIT_EFFECT]{ nullptr };
static int g_EffectCount = 0;

// エフェクトシステム初期化
void BulletHitEffect_Initialize()
{
    // テクスチャ読み込み
    g_TexID = Texture_Load(L"Resource/Texture/Explode3.png");

    // テクスチャのサイズを取得
    unsigned int texW = Texture_Width(g_TexID);
    unsigned int texH = Texture_Height(g_TexID);

    // 3×3 シート用の 1コマ分のサイズ
    DirectX::XMUINT2 frameSize{
        texW / 3u,
        texH / 3u
    };

    // 開始位置 (0,0)
    DirectX::XMUINT2 startPos{ 0u, 0u };

    // アニメパターン登録
    g_AnimePatternID = SpriteAnim_PatternRegister(
        g_TexID,
        9,          // パターン総数
        0.05,        // 1コマあたり秒数（double）
        frameSize,  // 1パターンのサイズ（幅・高さ）
        startPos,   // 開始位置
        false,      // ループしない
        3           // 1行あたりのパターン数（横方向）
    );

    g_EffectCount = 0;
}

// エフェクトシステム終了処理
void BulletHitEffect_Finalize()
{
    for (int i = 0; i < g_EffectCount; i++)
    {
        delete g_pEffects[i];
        g_pEffects[i] = nullptr;
    }
    g_EffectCount = 0;
}

// アセット解放なしで全エフェクトをクリア（ルーム遷移時用）
void BulletHitEffect_ClearAll()
{
    for (int i = 0; i < g_EffectCount; i++)
    {
        delete g_pEffects[i];
        g_pEffects[i] = nullptr;
    }
    g_EffectCount = 0;
}

// 全エフェクトの更新
void BulletHitEffect_Update()
{
    // まず全エフェクトを更新
    for (int i = 0; i < g_EffectCount; i++)
    {
        g_pEffects[i]->Update();
    }

    // 再生が終わったエフェクトを削除（Swap & Pop で詰める）
    for (int i = 0; i < g_EffectCount; i++)
    {
        if (g_pEffects[i]->IsDestroy())
        {
            delete g_pEffects[i];

            g_pEffects[i] = g_pEffects[g_EffectCount - 1];
            g_pEffects[g_EffectCount - 1] = nullptr;
            g_EffectCount--;
            i--; // 入れ替えた要素をチェックし直す
        }
    }
}

// 指定位置に新しいヒットエフェクトを作成
void BulletHitEffect_Create(const DirectX::XMFLOAT3& position)
{
    if (g_EffectCount >= MAX_BULLET_HIT_EFFECT) return;

    XMFLOAT3 pos = position;

    // カメラ位置から見てちょっと手前にずらす例
    XMFLOAT3 cam = Player_Camera_GetPosition(); // いつも使ってるカメラ取得関数に合わせて
    XMVECTOR vPos = XMLoadFloat3(&pos);
    XMVECTOR vCam = XMLoadFloat3(&cam);

    XMVECTOR dir = XMVector3Normalize(vPos - vCam); // カメラ→ヒット位置の方向
    float offset = 0.005f;                           // 5cm 分くらい手前に
    vPos -= dir * offset;

    XMStoreFloat3(&pos, vPos);

    g_pEffects[g_EffectCount] = new BulletHitEffect(pos);
    g_EffectCount++;
}


// 全ヒットエフェクトの描画
void BulletHitEffect_Draw()
{
    // エフェクト描画中：αブレンド ON、深度テストはするけど深度書き込み OFF にする
    Direct3D_SetBlendState(true);
    Direct3D_SetDepthStencilStateDepthWriteDisable(false); // false → 書き込み禁止ステート

    for (int i = 0; i < g_EffectCount; i++)
    {
        g_pEffects[i]->Draw();
    }

    // 元の設定に戻す（深度書き込み ON、ブレンド OFF）
    Direct3D_SetDepthStencilStateDepthWriteDisable(true);
    Direct3D_SetBlendState(false);
}
