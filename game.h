/*==============================================================================

   ゲームプレイ制御 [game.h]
                                                         Author : 51106
                                                         Date   : 2026/04/01
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
#include "MissionDef.h"

// D3Dリソース（アプリ起動・終了時に1回だけ呼ぶ）
void Game_InitializeD3D();
void Game_FinalizeD3D();

// ゲームプレイリソース（プレイ開始・終了のたびに呼ぶ）
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

// 敵編成を設定（Game_Initialize / Game_RespawnEnemies のスポーン種別割り振りに使う）
void Game_SetEnemyMix(EnemyMix mix);

// 通常エネミーの耐久倍率を設定（以降に出現する敵に適用。ボスは対象外。既定 1.0）
void Game_SetEnemyHpScale(float scale);
void Game_SetBossHpScale(float scale);   // ボスの耐久倍率（1 = 通常）

// 編成 mix で index 番目に出す敵の種別（Game_SpawnEnemy の type に渡せる値）
int Game_GetEnemyTypeForMix(EnemyMix mix, int index);

// ボス部屋で出すボスの種類（EnemyType の値。ボス以外を渡すと従来のボス）
void Game_SetBossType(int type);

// ボスの体力と表示名（ボス戦中でなければ false）
bool Game_GetBossStatus(int* outHp, int* outMaxHp, const wchar_t** outName);

// 撃破数（Game_Initialize で 0 に戻る）
int  Game_GetKillCount();
void Game_ResetKillCount();

// エネミーの更新中からの出現要求（召喚など）。その回の更新が終わってから出現する
void Game_RequestEnemySpawn(const DirectX::XMFLOAT3& pos, int type);

// ボスの向き（正面ベクトル）をセット（BossIntro_Start から呼ぶ）
void Game_SetBossLookDir(const DirectX::XMFLOAT3& dir);

// ミニマップ用：エネミーマーカー一括描画
void Game_DrawEnemyMarkers();

// サバイバル用：現在の生存エネミー数
int Game_GetAliveEnemyCount();

// index 番目のエネミーの位置を取得（0 〜 Game_GetAliveEnemyCount()-1。範囲外は false）
// ミニマップの範囲外マーカー表示に使う
bool Game_GetEnemyPosition(int index, DirectX::XMFLOAT3* outPos);

// サバイバルモード中か
bool Game_IsSurvivalMode();
void Game_SetSurvivalMode(bool val);

// サバイバル用：指定位置にエネミーをスポーン
void Game_SpawnEnemy(const DirectX::XMFLOAT3& pos, int type);

// サバイバル用：全エネミーをクリア（ウェーブ開始前の初期化）
void Game_ClearEnemies();

#endif // !GAME_H

