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