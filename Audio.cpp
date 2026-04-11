/*==============================================================================

   オーディオ管理 [Audio.cpp]
                                                         Author : 51106
                                                         Date   : 2026/02/08
--------------------------------------------------------------------------------
   XAudio2 を使ったサウンド再生モジュール。

   ■機能
     - InitAudio / UninitAudio : XAudio2 本体とマスターボイスの生成・解放
     - LoadAudio               : WAV ファイルをメモリに読み込み、インデックスを返す
     - UnloadAudio             : 指定インデックスのサウンドを解放
     - PlayAudio               : 指定インデックスを再生（ループ指定可）
     - SetMasterVolume         : マスター音量を 0.0〜1.0 で設定

   ■注意
     - LoadAudio が返す int インデックスを保持して PlayAudio / UnloadAudio に渡す
     - 使い終わった音は UnloadAudio で解放すること（メモリリーク防止）

==============================================================================*/
#include <xaudio2.h>
#include <assert.h>
#include "audio.h"

// グローバル：XAudio2本体とマスターボイス
static IXAudio2* g_Xaudio{};
static IXAudio2MasteringVoice* g_MasteringVoice{};

// XAudio2を使うためのリンク指定
#pragma comment(lib, "xaudio2.lib")   // 　XAudio2 本体
#pragma comment(lib, "winmm.lib")

// -----------------------------------------------------------------------------
// 初期化：XAudio2の生成とマスターボイス作成
// -----------------------------------------------------------------------------
void InitAudio()
{
    // XAudio2本体を生成
    XAudio2Create(&g_Xaudio, 0);

    // 出力先（スピーカー）用のマスターボイスを作成
    g_Xaudio->CreateMasteringVoice(&g_MasteringVoice);

    // 初期音量を 1.0 に（任意）
    if (g_MasteringVoice) {
        g_MasteringVoice->SetVolume(1.0f);
    }
}

// -----------------------------------------------------------------------------
// 終了処理：マスターボイスとXAudio2本体を解放
// -----------------------------------------------------------------------------
void UninitAudio()
{
    if (g_MasteringVoice) {
        g_MasteringVoice->DestroyVoice();
        g_MasteringVoice = nullptr;
    }
    if (g_Xaudio) {
        g_Xaudio->Release();
        g_Xaudio = nullptr;
    }
}

// -----------------------------------------------------------------------------
// 1つのサウンドデータを保持する構造体
// -----------------------------------------------------------------------------
struct AUDIO
{
    IXAudio2SourceVoice* SourceVoice{}; // 個別再生用ボイス
    BYTE* SoundData{};                  // PCMデータ
    int Length{};                       // バイト数
    int PlayLength{};                   // 再生可能なサンプル数
    bool AttenuationEnabled = false;    // 距離減衰を使うか
    float BaseVolume = 1.0f;            // 減衰計算前の基準音量
};

#define AUDIO_MAX 100
static AUDIO g_Audio[AUDIO_MAX]{}; // 最大100音源を管理

// -----------------------------------------------------------------------------
// WAVファイルを読み込んでSourceVoiceを作成する
// -----------------------------------------------------------------------------
int LoadAudio(const char* FileName)
{
    int index = -1;

    // 空いているスロットを探す
    for (int i = 0; i < AUDIO_MAX; i++)
    {
        if (g_Audio[i].SourceVoice == nullptr)
        {
            index = i;
            break;
        }
    }
    if (index == -1) return -1; // 空きが無ければ失敗

    // WAVフォーマット解析用の変数
    WAVEFORMATEX wfx = { 0 };

    {
        HMMIO hmmio = NULL;
        MMIOINFO mmioinfo = { 0 };
        MMCKINFO riffchunkinfo = { 0 };
        MMCKINFO datachunkinfo = { 0 };
        MMCKINFO mmckinfo = { 0 };
        UINT32 buflen;
        LONG readlen;

        // ファイルを開く
        hmmio = mmioOpen((LPSTR)FileName, &mmioinfo, MMIO_READ);
        assert(hmmio);

        // "RIFF WAVE" チャンクを探す
        riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
        mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

        // "fmt " チャンクを探す（フォーマット情報）
        mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
        mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

        if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
        {
            // WAVEFORMATEXで読める場合
            mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
        }
        else
        {
            // 古いPCMフォーマットの場合はこちら
            PCMWAVEFORMAT pcmwf = { 0 };
            mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
            memset(&wfx, 0x00, sizeof(wfx));
            memcpy(&wfx, &pcmwf, sizeof(pcmwf));
            wfx.cbSize = 0;
        }
        mmioAscend(hmmio, &mmckinfo, 0);

        // "data" チャンクを探す（実データ）
        datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
        mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);

        // バッファ確保してデータを読み込む
        buflen = datachunkinfo.cksize;
        g_Audio[index].SoundData = new unsigned char[buflen];
        readlen = mmioRead(hmmio, (HPSTR)g_Audio[index].SoundData, buflen);

        g_Audio[index].Length = readlen;
        g_Audio[index].PlayLength = (wfx.nBlockAlign > 0) ? (readlen / wfx.nBlockAlign) : 0;

        // ファイルを閉じる
        mmioClose(hmmio, 0);
    }

    // 読み込んだフォーマットでSourceVoiceを作成
    g_Xaudio->CreateSourceVoice(&g_Audio[index].SourceVoice, &wfx);
    assert(g_Audio[index].SourceVoice);

    return index;
}

// -----------------------------------------------------------------------------
// 音声データを解放する
// -----------------------------------------------------------------------------
void UnloadAudio(int Index)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;

    if (g_Audio[Index].SourceVoice)
    {
        // DestroyVoiceすると再生も止まる
        g_Audio[Index].SourceVoice->DestroyVoice();
        g_Audio[Index].SourceVoice = nullptr;
    }

    if (g_Audio[Index].SoundData)
    {
        delete[] g_Audio[Index].SoundData;
        g_Audio[Index].SoundData = nullptr;
    }

    g_Audio[Index].Length = 0;
    g_Audio[Index].PlayLength = 0;
}

// -----------------------------------------------------------------------------
// 音声を再生する（Loop = true で無限ループ再生）
// -----------------------------------------------------------------------------
void PlayAudio(int Index, bool Loop)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;
    if (!g_Audio[Index].SourceVoice || !g_Audio[Index].SoundData) return;

    // 常に停止 → フラッシュしてからバッファを積み直す
    // ※ BuffersQueued の条件チェックを外すことで、Stop() の非同期完了タイミングに
    //   起因する "音が消えるバグ" を回避する（Stop 済みボイスへの Stop/Flush は無害）
    g_Audio[Index].SourceVoice->Stop();
    g_Audio[Index].SourceVoice->FlushSourceBuffers();

    // 再生バッファの設定
    XAUDIO2_BUFFER bufinfo = {};
    bufinfo.AudioBytes = g_Audio[Index].Length;
    bufinfo.pAudioData = g_Audio[Index].SoundData;
    bufinfo.PlayLength = g_Audio[Index].PlayLength; // サンプル数

    // ループ設定
    if (Loop)
    {
        bufinfo.LoopBegin = 0;
        bufinfo.LoopLength = g_Audio[Index].PlayLength;
        bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    // バッファを登録
    HRESULT hr = g_Audio[Index].SourceVoice->SubmitSourceBuffer(&bufinfo, NULL);
    if (FAILED(hr)) {
        assert(false && "SubmitSourceBuffer failed!");
        return;
    }

    // 再生開始
    g_Audio[Index].SourceVoice->Start();
}

// -----------------------------------------------------------------------------
// 　マスター音量を設定する（0.0f ～ 1.0f）
// -----------------------------------------------------------------------------
void SetMasterVolume(float volume)
{
    if (!g_MasteringVoice) return;

    // 範囲をクランプ
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    g_MasteringVoice->SetVolume(volume);
}

float GetMasterVolume()
{
    if (!g_MasteringVoice) return 0.5f;
    float vol = 0.5f;
    g_MasteringVoice->GetVolume(&vol);
    return vol;
}

// -----------------------------------------------------------------------------
// WAVファイルを読み込み、初期音量を指定してSourceVoiceを作成する
// -----------------------------------------------------------------------------
int LoadAudioWithVolume(const char* FileName, float volume)
{
    int index = LoadAudio(FileName);
    if (index < 0) return -1;

    // 音量をクランプ
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    // SourceVoice 単位で音量設定
    if (g_Audio[index].SourceVoice)
    {
        g_Audio[index].SourceVoice->SetVolume(volume);
    }

    return index;
}

// -----------------------------------------------------------------------------
// 再生中かどうかを返す（バッファが残っていれば true）
// -----------------------------------------------------------------------------
bool IsAudioPlaying(int Index)
{
    if (Index < 0 || Index >= AUDIO_MAX) return false;
    if (!g_Audio[Index].SourceVoice) return false;
    XAUDIO2_VOICE_STATE state{};
    g_Audio[Index].SourceVoice->GetState(&state);
    return state.BuffersQueued > 0;
}

// -----------------------------------------------------------------------------
// 再生を停止してバッファをクリアする
// -----------------------------------------------------------------------------
void StopAudio(int Index)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;
    if (!g_Audio[Index].SourceVoice) return;
    g_Audio[Index].SourceVoice->Stop();
    g_Audio[Index].SourceVoice->FlushSourceBuffers();
}

// -----------------------------------------------------------------------------
// SourceVoice 単位の音量を変更する（0.0f ～ 1.0f）
// -----------------------------------------------------------------------------
void SetAudioVolume(int Index, float volume)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;
    if (!g_Audio[Index].SourceVoice) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_Audio[Index].BaseVolume = volume;
    g_Audio[Index].SourceVoice->SetVolume(volume);
}

// -----------------------------------------------------------------------------
// 距離減衰の有効/無効を切り替える
// -----------------------------------------------------------------------------
void SetAudioAttenuationEnabled(int Index, bool enabled)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;
    g_Audio[Index].AttenuationEnabled = enabled;
}

// -----------------------------------------------------------------------------
// 距離に応じて音量を更新する（毎フレーム呼ぶ）
// AttenuationEnabled が false のサウンドは無視される
// maxDist : この距離以上で無音（0.0f）
// -----------------------------------------------------------------------------
void UpdateAudioAttenuation(int Index, float distance, float maxDist)
{
    if (Index < 0 || Index >= AUDIO_MAX) return;
    if (!g_Audio[Index].SourceVoice) return;
    if (!g_Audio[Index].AttenuationEnabled) return;
    if (maxDist <= 0.0f) return;

    float t = 1.0f - (distance / maxDist);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    g_Audio[Index].SourceVoice->SetVolume(g_Audio[Index].BaseVolume * t);
}

