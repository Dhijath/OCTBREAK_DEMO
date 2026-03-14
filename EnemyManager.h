/*==============================================================================

   エネミー管理 [EnemyManager.h]
                                                         Author : 51106
                                                         Date   : 2026/01/13
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
};

class EnemyManager
{
public:
    void Initialize();                                    // 全削除
    void Finalize();                                      // 全解放

    void Update(double elapsed_time);                     // 全体更新
    void Draw();                                          // 全体描画
    void DrawShadow();                                    // シャドウパス用深度描画

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
    std::vector<std::unique_ptr<Enemy>> m_Enemies;        // 全エネミー配列
};

#endif // ENEMY_MANAGER_H