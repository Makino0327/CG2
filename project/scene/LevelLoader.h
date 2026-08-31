#pragma once
#include <string>
#include <vector>
#include "../engine/math/Math.h"

// 当たり判定用のボックスコライダー情報
struct LevelColliderData {
    std::string type;      // コライダーの種類。今は BOX 想定
    Vector3 center;        // コライダー中心のローカル座標
    Vector3 size;          // コライダーのサイズ
    bool hasCollider;      // コライダーを持っているかどうか
};

// Blender から読み込んだ 1 個分のオブジェクト情報
struct LevelObjectData {
    std::string type; // Blender の object.type
    std::string name; // Blender の object.name
    std::string fileName; // 表示に使うモデル名
    std::string objectKind; // Blender側で設定したゲーム用の種類
    std::string targetLevel; // テレポーターで移動する先のレベルJSON
    Vector3 translation; // 平行移動
    Vector3 rotation; // 回転
    Vector3 scaling; // 拡大縮小
    std::vector<LevelObjectData> children; // 子オブジェクト
    LevelColliderData collider; // Blender から読み込んだコライダー情報
};

// NavMesh の三角形 1 枚分の情報
struct LevelNavMeshTriangle {
    int index0 = 0;
    int index1 = 0;
    int index2 = 0;
};

// NavMesh 全体の情報
struct LevelNavMeshData {
    std::vector<Vector3> vertices; // NavMesh の頂点一覧
    std::vector<LevelNavMeshTriangle> triangles; // NavMesh の三角形一覧
};

// レベル全体の情報
struct LevelData {
    std::string name; // ルート名
    std::vector<LevelObjectData> objects; // ルート直下のオブジェクト一覧
    LevelNavMeshData navMesh; // 敵AI用のNavMeshデータ
};

// JSON ファイルから LevelData を作るローダー
class LevelLoader {
public:
    static LevelData LoadFile(const std::string& filePath); // JSON を読み込む
};
