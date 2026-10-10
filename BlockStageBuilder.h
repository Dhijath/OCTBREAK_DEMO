/*==============================================================================

   ブロックステージ構築API [BlockStageBuilder.h]
                                                         Author : 51106
                                                         Date   : 2026/10/03
--------------------------------------------------------------------------------

   BlockStage.cpp（エンジン）と BlockStageLayouts.cpp（各ステージの配置）の
   間だけで使う内部ヘッダ。

   ■座標系
     ・原点はステージ中央。X=東西、Z=南北（+Z が北）。プレイヤーは南から北へ進む
     ・高さ（h / base）はすべて「地面からの高さ」で指定する

   ■配置物の種類
     Solid     : 箱。当たり判定あり（側面は壁、上面は床＝屋上に乗れる）
     SolidMesh : 箱以外の見た目（岩・円柱など）。当たり判定は見た目より少し小さい箱
     Deco      : 見た目だけの箱（路面・庇など）
     Glow      : 見た目だけの発光する箱（まわりに光のにじみが付く）
     Prop      : 見た目だけの任意メッシュ。回転させられる（浮遊物・遠景用）

   ■色
     色はリニア値。暗い地の色（0.05 前後）に、1.0 を超える発光を重ねると
     ネオン風に出る（描画側でトーンマップとガンマ補正を掛けている）。

   ■敵AIとの約束
     敵は壁を高さ無視（XZ）で判定するため、Solid は base を浮かせても
     敵にとっては地面から立つ柱と同じ扱いになる。
     また敵の視線判定は地面から 0.5m の高さで行うので、
     遮蔽物にする Solid は高さ 0.8m 以上にすること。

==============================================================================*/
#pragma once
#include <DirectXMath.h>
#include <random>
#include "BlockStageRender.h"

namespace BlockStageBuilder
{
    using Col = DirectX::XMFLOAT3;

    struct Theme
    {
        // 空とフォグ
        Col               skyTop;
        Col               skyBottom;
        DirectX::XMFLOAT4 nebula;     // 星雲の色（a = 濃さ）
        Col               fog;
        float             fogStart;
        float             fogEnd;

        // 地面
        Col   grid;                   // グリッド線の色
        float gridCell;               // 1マス（m）
        Col   ground;                 // 地の色
        bool  ceiling;                // 上空にもグリッド面を張る（屋内風）

        Col wall;                     // 外周壁の色
        Col accent;                   // 外周の発光ライン・ゴールの色
        Col minimap;                  // ミニマップ上の構造物の色（高さで濃淡を付ける）
    };

    // 地面・外周壁・飛行上限を作る。最初に必ず呼ぶ
    //   halfX, halfZ : ステージの半分の広さ（m）
    //   wallH        : 外周壁の見た目の高さ（当たり判定は飛行上限まで伸びる）
    void Begin(float halfX, float halfZ, float wallH, const Theme& theme);

    void Solid(float cx, float cz, float sx, float sz, float h, const Col& col, float base = 0.0f);

    // collider  : 当たり判定の箱を見た目の何倍にするか（XZ）
    // colliderH : 同じく高さ方向
    void SolidMesh(StageMesh mesh, float cx, float cz, float sx, float sz, float h, const Col& col,
                   float base = 0.0f, float collider = 0.78f, float colliderH = 1.0f);

    void Deco(float cx, float cz, float sx, float sz, float h, const Col& col, float base = 0.0f);
    void Glow(float cx, float cz, float sx, float sz, float h, const Col& col, float base = 0.0f,
              float emissive = 2.0f);

    // center.y は地面からの高さ。rot / spin はラジアン（spin は毎秒の回転量）
    void Prop(StageMesh mesh, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size,
              const Col& col, float emissive = 0.0f,
              const DirectX::XMFLOAT3& rot = { 0.0f, 0.0f, 0.0f },
              const DirectX::XMFLOAT3& spin = { 0.0f, 0.0f, 0.0f });

    // ビル：暗い本体（Solid）＋屋上の縁のネオン＋中ほどを一周するネオン
    void Building(float cx, float cz, float sx, float sz, float h, const Col& body, const Col& neon);

    // 遠景の高層ビル群（外周壁の外側。当たり判定なし）
    void Skyline(std::mt19937& rng, int count, const Col& body, const Col& neonA, const Col& neonB);

    // 敵の部隊を配置する。(x,z) のまわり radius 以内の空き地に count 体。
    // level は地面からの高さ（上の階に置くときに指定。0 = 地面）
    // type は EnemyType の値（下の EnemyKind）
    enum EnemyKind
    {
        NORMAL = 0, TANK = 1, SPEED = 2, SNIPER = 3,
        BOMBER = 5, GUNNER = 6, ARTILLERY = 7, TURRET = 8, PHANTOM = 9,
        WING = 10, GATLING = 11, ORBITER = 12, WALKER = 13, HALO = 14,
    };
    void Squad(float x, float z, int count, int type, float radius = 4.0f, float level = 0.0f);

    // 上の階の床（デッキ）：上面に乗れる床板。下を通れる（巡回点・敵は上下それぞれに置ける）
    //   top : 地面から床板の上面までの高さ
    void Platform(float cx, float cz, float sx, float sz, float top, const Col& col);

    // 階段：(x0,z0) から dir 方向（0=+Z 1=+X 2=-Z 3=-X）へ登る。
    //   width : 幅 / run : 水平の長さ / rise : 登る高さ / base : 下端の高さ
    //   段の上面は床、段の下の中身は壁（1つ下の段の踏み面より低い）なので、
    //   プレイヤーも敵も段を上れて、横や下からは通り抜けられない
    void Stairs(float x0, float z0, int dir, float width, float run, float rise, const Col& col, float base = 0.0f);

    // 増援の降下地点（ミッションの増援はここから出現する。プレイヤーから遠い地点が選ばれる）
    void DropZone(float x, float z);

    void SetSpawn(float x, float z);                    // プレイヤー開始位置
    void SetGoal (float x, float z, float base = 0.0f); // ゴール（base = 足場の高さ）
    void SetBoss (float x, float z);                    // ボス出現位置
    void SetEnemyMinDistance(float meters);             // 敵をプレイヤー開始位置から離す距離
}

// 各ステージの配置（BlockStageLayouts.cpp）。rng は地形のばらつき用（固定シード）
void BlockStageLayout_City    (std::mt19937& rng);
void BlockStageLayout_Terminal(std::mt19937& rng);
void BlockStageLayout_Fortress(std::mt19937& rng);
void BlockStageLayout_Trench  (std::mt19937& rng);
void BlockStageLayout_Arena   (std::mt19937& rng);
void BlockStageLayout_Plant   (std::mt19937& rng);
void BlockStageLayout_Spaceport  (std::mt19937& rng);
void BlockStageLayout_DataVault  (std::mt19937& rng);
void BlockStageLayout_CryoMine   (std::mt19937& rng);
void BlockStageLayout_CarrierDeck(std::mt19937& rng);
