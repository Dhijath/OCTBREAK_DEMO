/*==============================================================================

   ブロックステージ [BlockStage.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   手続き生成のメッシュ（箱・円柱・岩・八面体など）を並べて作る立体ステージ。
   テクスチャは使わず、暗い地の色＋ネオンの発光＋グリッドの地面＋
   手続き生成の空（星雲・星）＋距離フォグで描画する。

   ■既存システムとの互換性
     ・当たり判定は map.cpp の MapObject（床 / 壁 / 天井）として登録する
       → プレイヤー・敵・弾・カメラ・ボス演出は既存コードのまま動く
     ・スポーン位置 / ゴール / ボス位置 / 敵スポーン / 巡回点も
       map.cpp の内部API（Map_Internal_*）経由で設定する
     ・描画だけ map.cpp から委譲される（Map_Draw / Map_DrawGoal /
       Map_DrawForMinimap が BlockStage_IsActive() のときこちらを呼ぶ）

   ■使い方（Game_Manager.cpp 側）
     Game_Initialize();                       // 先に通常初期化（屋外マップと同じ手順）
     BlockStage_Build(id, seed, enemyCount);  // マップを差し替え
     Map_RegisterFloors();

==============================================================================*/
#pragma once
#include <cstdint>
#include <vector>
#include <DirectXMath.h>

enum class BlockStageID
{
    City,      // 廃棄市街：街区とビル群。屋上を使った立体戦
    Terminal,  // 輸送ターミナル：コンテナヤードとガントリークレーン
    Fortress,  // 要塞：外壁と門、中央の司令棟
    Trench,    // 渓谷施設：左右を崖に挟まれた縦長の進攻路
    Arena,     // 制御区画：大型兵器と戦う八角形アリーナ
    Plant,     // 二層プラント：地上階と高さ5mの上階（デッキ）に分かれた工場。階段でつながる

    // ── 第二作戦区域 ──
    Spaceport,    // 宇宙港：発着パッドと格納庫、管制塔。北端の打ち上げ台がゴール
    DataVault,    // 電子金庫：天井のある屋内。サーバーラックの迷路と中央のデータコア
    CryoMine,     // 氷結採掘場：氷の岩と採掘機。中央に高さ5mの掘削リグ
    CarrierDeck,  // 空母甲板：縦長の飛行甲板。艦橋と駐機中の機体、北端の艦首がゴール
    Count
};

// ステージを構築して有効化する（MapObject / スポーン / ゴール / 敵配置を設定）
//   seed        : 敵の配置に使う乱数シード（地形は毎回同じ）
//   enemyCount  : 部隊とは別に散らす哨戒兵の数
//   placeSquads : false でステージ設計の部隊を置かない（同じステージでのボス戦フェーズ用）
void BlockStage_Build(BlockStageID id, std::uint32_t seed, int enemyCount, bool placeSquads = true);

// ブロックステージが有効か（map.cpp が描画の委譲先を決めるために使う）
bool BlockStage_IsActive();

// 無効化（ダンジョン / ボス部屋 / 屋外マップの生成時に map 側から呼ぶ）
void BlockStage_Deactivate();

// 描画リソースの解放（アプリ終了時）
void BlockStage_Finalize();

// index 番目の敵スポーン（Map_GetEnemySpawnPositions と同じ並び）に出す敵の種別。
// ステージが部隊として指定した種別（EnemyType の値）、指定なしは -1
int BlockStage_GetEnemyType(int index);

// 増援の出現位置を選ぶ。プレイヤーから離れた降下地点のまわりの空き地を返す。
//   random : 降下地点の選択に使う乱数（1回の増援の中では同じ値を渡す）
//   slot   : 増援内の何体目か（同じ降下地点のまわりに散らすのに使う）
//   戻り値 : 降下地点が設定されていないステージでは false
bool BlockStage_PickReinforcePoint(const DirectX::XMFLOAT3& playerPos, std::uint32_t random, int slot,
                                   DirectX::XMFLOAT3* outPos);

//==============================================================================
// 作戦エリアの見取り図（ミッション選択画面の戦術マップ用）
// ※取得すると内部の配置データを上書きし、ステージは無効状態になる。
//   出撃時に BlockStage_Build で必ず作り直すので、メニュー画面でだけ呼ぶこと
//==============================================================================
struct BlockStagePreview
{
    struct Rect  { float cx, cz, sx, sz, top; };
    struct Squad { float x, z; int count; int type; };

    float halfX = 0.0f, halfZ = 0.0f;
    std::vector<Rect>               structures;   // 当たり判定のある構造物（top = 高さ）
    std::vector<Squad>              squads;       // 配置部隊（type は BlockStage_GetEnemyType と同じ）
    std::vector<DirectX::XMFLOAT2>  dropZones;    // 増援の降下地点
    DirectX::XMFLOAT2 spawn = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 goal  = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 boss  = { 0.0f, 0.0f };
    bool hasGoal = false;
    bool hasBoss = false;
    DirectX::XMFLOAT3 accent = { 1.0f, 1.0f, 1.0f };  // ステージのテーマ色
};
void BlockStage_GetPreview(BlockStageID id, BlockStagePreview* out);

// 描画（map.cpp から委譲される）
void BlockStage_Draw();         // ステージ本体
void BlockStage_DrawGoal();     // ゴールビーコン（半透明。モデル描画の後に呼ぶ）
void BlockStage_DrawMinimap();  // ミニマップ用（真上からの平行投影で呼ぶ）
