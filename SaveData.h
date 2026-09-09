#pragma once
/*==============================================================================
   設定永続化 [SaveData.h]
   保存先: resource/Savedata/config.ini
   保存項目:
     [Audio]    音量
     [Camera]   感度・Y軸反転
     [Display]  フルスクリーン
     [Graphics] シャドウモード（0=なし/1=シャドウマップ/2=マル影/3=両方）
     [Records]  スコアランキング（最大10件）
     [Mission]  最後に選んだミッション・クリア済みフラグ
==============================================================================*/

void SaveData_Load();         // 起動時に読み込み、各モジュールに反映
void SaveData_Save();         // オプション確定時に書き込み
void SaveData_SaveScores();   // スコアレコードのみ書き込み（ゲーム終了時）
void SaveData_SaveMissions(); // ミッション進行のみ書き込み（出撃時・作戦成功時）
void SaveData_SaveDex();      // エネミー図鑑の撃破数のみ書き込み（作戦終了時）
