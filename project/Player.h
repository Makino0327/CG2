#pragma once
#include "Object3d.h"
#include "Input.h"
#include "MapChipField.h"
#include <memory>
class Player
{
public:
    void Initialize(Object3dCommon* object3dCommon, Input* input);

    void Update();
    void Draw();

    // 追加：マップとタイルサイズを渡す
    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }

private:
    // 下方向の当たり判定だけを行う新しい関数
    void ResolveBottomCollisionWithMap(Vector3& pos);

    void ResolveLeftCollisionWithMap(Vector3& pos);

    void ResolveTopCollisionWithMap(Vector3& pos);

    void ResolveRightCollisionWithMap(Vector3& pos);
private:
    std::unique_ptr<Object3d> object_;
    Input* input_ = nullptr;

    // 物理系
    float moveSpeed_ = 0.2f;
    float velocityY_ = 0.0f;
    float jumpPower_ = 0.6f;
    float gravity_ = -0.03f;
    bool  onGround_ = false;

    // マップ情報
    MapChipField* mapField_ = nullptr;
    float tileSize_ = 2.0f; // main の kTileSize と同じにする

    Vector3 prevPos_{};
};
