#pragma once

/*==============================================================================
   オーディオ [Audio.h]
   Author : 51106
   Date   : 2026/02/08
==============================================================================*/

// 初期化・終了
void InitAudio();
void UninitAudio();

// 読み込み・解放
int  LoadAudio(const char* FileName);
void UnloadAudio(int Index);

// 再生（Loop 指定可）
void PlayAudio(int Index, bool Loop = false);

//  マスター音量（0.0f ～ 1.0f）
void  SetMasterVolume(float volume);
float GetMasterVolume();            // 現在のマスター音量を取得

// 音量変更可能ロード
int LoadAudioWithVolume(const char* FileName, float volume);

// 再生中かどうか
bool IsAudioPlaying(int Index);

// 停止（バッファもクリア）
void StopAudio(int Index);

// 個別音量変更（0.0f ～ 1.0f）
void SetAudioVolume(int Index, float volume);

// 距離減衰：有効/無効切り替え（デフォルト無効）
void SetAudioAttenuationEnabled(int Index, bool enabled);

// 距離減衰：毎フレーム呼んで音量を更新する
// distance : 音源とリスナーの距離、maxDist : この距離以上で無音
void UpdateAudioAttenuation(int Index, float distance, float maxDist);