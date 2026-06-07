#include "LevelLoader.h"
#include <cassert>
#include <fstream>
#include "../externals/nlohmann/json.hpp" // JSON 読み込み用ライブラリ

using json = nlohmann::json;

// 度をラジアンへ変換する
static float ToRadian(float degree) {
    return degree * 3.1415926535f / 180.0f;
}

// JSON 配列から Vector3 を作る
static Vector3 ReadVector3(const json& array) {
    assert(array.is_array()); // 配列であることを確認
    assert(array.size() == 3); // xyz の 3 要素であることを確認

    return Vector3{
        static_cast<float>(array[0]),
        static_cast<float>(array[1]),
        static_cast<float>(array[2])
    };
}

// JSON の 1 オブジェクト分を再帰的に読む
static LevelObjectData ParseObject(const json& objectJson) {
    assert(objectJson.contains("type")); // type は必須
    assert(objectJson.contains("name")); // name は必須
    assert(objectJson.contains("transform")); // transform は必須

    LevelObjectData objectData{};

    objectData.type = objectJson["type"].get<std::string>(); // オブジェクト種別を読む
    objectData.name = objectJson["name"].get<std::string>(); // オブジェクト名を読む

    if (objectJson.contains("file_name")) {
        objectData.fileName = objectJson["file_name"].get<std::string>(); // モデル名を読む
    }

    const json& transform = objectJson["transform"];

    // Blender座標系からゲーム座標系へ変換して位置を入れる
    // Blender: X右, Y奥, Z上
    // Game   : X右, Y上, Z奥
    objectData.translation.x = static_cast<float>(transform["translation"][0]); // Xはそのまま
    objectData.translation.y = static_cast<float>(transform["translation"][2]); // Blender Z -> Game Y
    objectData.translation.z = static_cast<float>(transform["translation"][1]); // Blender Y -> Game Z

    // Blender座標系からゲーム座標系へ変換して回転を入れる
    objectData.rotation.x = -static_cast<float>(transform["rotation"][0]); // 軸の向き差を吸収するため符号反転
    objectData.rotation.y = static_cast<float>(transform["rotation"][2]);  // Blender Z -> Game Y
    objectData.rotation.z = static_cast<float>(transform["rotation"][1]);  // Blender Y -> Game Z

    // Blender座標系からゲーム座標系へ変換してスケールを入れる
    objectData.scaling.x = static_cast<float>(transform["scaling"][0]); // Xはそのまま
    objectData.scaling.y = static_cast<float>(transform["scaling"][2]); // Blender Z -> Game Y
    objectData.scaling.z = static_cast<float>(transform["scaling"][1]); // Blender Y -> Game Z

    // Blenderは度数法で出力しているので、ゲーム用にラジアンへ変換する
    objectData.rotation.x = ToRadian(objectData.rotation.x);
    objectData.rotation.y = ToRadian(objectData.rotation.y);
    objectData.rotation.z = ToRadian(objectData.rotation.z);

    // コライダー情報があるなら読む
    objectData.collider.hasCollider = false;
    if (objectJson.contains("collider")) {
        const json& colliderJson = objectJson["collider"];

        objectData.collider.hasCollider = true;
        objectData.collider.type = colliderJson["type"].get<std::string>();

        // Blender座標系からゲーム座標系へ変換してコライダー中心を入れる
        objectData.collider.center.x = static_cast<float>(colliderJson["center"][0]); // Xはそのまま
        objectData.collider.center.y = static_cast<float>(colliderJson["center"][2]); // Blender Z -> Game Y
        objectData.collider.center.z = static_cast<float>(colliderJson["center"][1]); // Blender Y -> Game Z

        // Blender座標系からゲーム座標系へ変換してコライダーサイズを入れる
        objectData.collider.size.x = static_cast<float>(colliderJson["size"][0]); // Xはそのまま
        objectData.collider.size.y = static_cast<float>(colliderJson["size"][2]); // Blender Z -> Game Y
        objectData.collider.size.z = static_cast<float>(colliderJson["size"][1]); // Blender Y -> Game Z
    }

    if (objectJson.contains("children")) {
        for (const json& childJson : objectJson["children"]) {
            objectData.children.push_back(ParseObject(childJson)); // 子オブジェクトを再帰的に追加
        }
    }

    return objectData;
}

LevelData LevelLoader::LoadFile(const std::string& filePath) {
    std::ifstream file(filePath); // JSON ファイルを開く
    assert(file.is_open()); // 開けなければ停止

    json rootJson;
    file >> rootJson; // JSON 全体を読み込む

    assert(rootJson.contains("name")); // ルート名があることを確認
    assert(rootJson.contains("objects")); // objects 配列があることを確認

    LevelData levelData{};
    levelData.name = rootJson["name"].get<std::string>(); // レベル名を読む

    for (const json& objectJson : rootJson["objects"]) {
        levelData.objects.push_back(ParseObject(objectJson)); // 直下のオブジェクトを順番に読む
    }

    return levelData;
}