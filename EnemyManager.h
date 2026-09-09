/*==============================================================================

   エネミー管理 [EnemyManager.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
--------------------------------------------------------------------------------
   ・Enemyを複数管理するマネージャ
   ・初期化 / 更新 / 描画 / 参照を一元化
   ・Enemy本体の実装には一切手を出さない
   ・unique_ptrでEnemyTank・EnemySpeed・EnemySniperを一括管理する
==============================================================================*/

#ifndef ENEMY_MANAGER_H
#define ENEMY_MANAGER_H

#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "Enemy.h"
#include "collision.h"

//==============================================================================
// エネミー種別
//
// ■役割
// ・Spawnで生成するエネミーの種類を指定する
//==============================================================================
enum class EnemyType
{
    Normal,  // 基本エネミー（Enemy）
    Tank,    // タンク型（EnemyTank）
    Speed,   // スピード型（EnemySpeed）
    Sniper,  // 射撃型（EnemySniper）
    Boss,    // ボス（EnemyBoss）HP8000・巨大モデル

    // ── 追加エネミー（EnemyEx）──
    Bomber,     // 自爆型：高速で接近し、近くで爆発する
    Gunner,     // 突撃型：中距離を保って横移動しながら3連射
    Artillery,  // 砲撃型：遠距離から溜めて重い砲弾を3発
    Turret,     // 固定砲台：動かず、全方位弾と狙い撃ち
    Phantom,    // 幻影型：プレイヤーの近くへ瞬間移動して斬りかかる

    // ── 追加エネミー（EnemyBall：既存と同じ球体型・複数パーツのアニメーション付き）──
    Wing,       // 白い球＋翼：旋回しながら撃ち、溜めてから急降下突撃
    Gatling,    // 灰色の球＋回転砲：砲身を回して加速してから長い連射
    Orbiter,    // 青い球＋子機3つ：子機が周回して順番に撃つ
    Walker,     // 赤い球＋4本脚：跳び上がって着地の踏みつけと衝撃波
    Halo,       // 黄色い球＋リング：リングを高速回転させてから螺旋弾幕

    // ── 追加ボス（EnemyBossEx）──
    BossArgus,    // 浮遊する眼：全方位弾・扇状弾・突撃型の召喚
    BossGoliath,  // 重装歩行機：衝撃波・重砲・突進
    BossOmega,    // 動力炉：螺旋弾幕・連射・砲台の召喚
    BossHydra,    // 三つ首：扇状の一斉射・機雷・自爆型の召喚
    BossSpectre,  // 刃の騎士：瞬間移動・斬撃・幻影型の召喚
};

class EnemyManager
{
public:
    void Initialize();                                    // 全削除
    void Finalize();                                      // 全解放

    void Update(double elapsed_time);                     // 全体更新
    void Draw();                                          // 全体描画
    void DrawShadow();                                    // シャドウパス用深度描画
    void DrawMarkers();                                   // ミニマップ用エネミーマーカー描画

    //==========================================================================
    // エネミー生成
    //
    // ■引数
    // ・position : 生成位置
    // ・type     : 生成するエネミー種別（デフォルトはNormal）
    //
    // ■戻り値
    // ・生成したエネミーのインデックス
    //==========================================================================
    int Spawn(const DirectX::XMFLOAT3& position,
        EnemyType type = EnemyType::Normal);

    //==========================================================================
    // 死亡エネミーの削除
    //
    // ■役割
    // ・IsAlive()がfalseのエネミーをvectorから取り除く
    //==========================================================================
    void RemoveDead();

    int GetCount() const;                                 // 生存数取得

    Enemy& GetEnemy(int index);                     // Enemy参照
    const Enemy& GetEnemy(int index) const;               // const参照

    AABB                      GetAABBAt(int index) const; // AABB取得
    const DirectX::XMFLOAT3& GetPositionAt(int index) const; // 位置取得

private:
    void ResolveSeparation();                             // エネミー同士の押し合い

    std::vector<std::unique_ptr<Enemy>> m_Enemies;        // 全エネミー配列
};

#endif // ENEMY_MANAGER_H