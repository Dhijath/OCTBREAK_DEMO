/*==============================================================================

   ゲームプレイ制御 [game.h]
                                                         Author : 51106
                                                         Date   : 2025/12/19
--------------------------------------------------------------------------------
   プレイ中のシーン全体（迷路・敵・プレイヤー・ミニマップ）を管理する
   モジュールのパブリック API。GameManager から呼ばれる。

   ■主な機能
     - Game_Initialize / Finalize / Update / Draw : シーンのライフサイクル
     - Game_RespawnEnemies    : ダンジョン再生成後に敵を再スポーン
     - MiniMap_Render3D       : オフスクリーンにミニマップ用 3D 描画
     - MiniMap_Draw2D         : ミニマップをスプライトとして画面に表示
     - Game_GetLockOnWorldPos : 画面中央に最も近いエネミーの位置を取得
     - Game_IsBossAlive       : ボス生存判定（ゴール有効化の条件に使用）
     - Game_SetBossRoomMode   : ボス部屋モード設定

==============================================================================*/
#ifndef GAME_H
#define GAME_H

#include <DirectXMath.h>

void Game_Initialize();

void Game_Finalize();

void Game_Update(double elapsed_time);

void Game_Draw();

// ダンジョン再生成時に呼ぶ（EnemyManager を使って敵を再スポーン）
void Game_RespawnEnemies();

void MiniMap_Render3D();

void MiniMap_Draw2D();

// ロックオン：画面中央に最も近いエネミーのワールド位置を返す（見つからない場合 false）
bool Game_GetLockOnWorldPos(DirectX::XMFLOAT3* outPos);

// ボスが生存中か（ゴール無効化判定に使用）
// ・true  : ボスが生存中 → ゴール到達を無効にする
// ・false : ボスが撃破済み → ゴール到達を有効にする
bool Game_IsBossAlive();

// ボス部屋モードを設定（true のときのみ Game_RespawnEnemies でボスをスポーンする）
void Game_SetBossRoomMode(bool isBossRoom);

// ボスの向き（正面ベクトル）をセット（BossIntro_Start から呼ぶ）
void Game_SetBossLookDir(const DirectX::XMFLOAT3& dir);

#endif // !GAME_H

