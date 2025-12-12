#pragma once
#include "Math.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Input.h"
#include "MapChipField.h"

class Player {
public:
    void Initialize(Object3dCommon* object3dCommon, Input* input);
    void Update();
    void Draw();

    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }

    Vector3 GetPosition() const;

private:
    // ========= 参照系 =========
    Object3d* object_ = nullptr;
    Input* input_ = nullptr;
    MapChipField* mapField_ = nullptr;
    float        tileSize_ = 1.0f;

    // ========= 移動系 =========
    Vector3 prevPos_{};

    float moveSpeed_ = 0.2f;   // 横移動速度
    float jumpPower_ = 0.7f;   // ジャンプ初速度
    float gravity_ = -0.03f; // 重力（下向きなのでマイナス）

    float velocityY_ = 0.0f;
    bool  onGround_ = false;

    // ========= ジャンプ/壁ジャンプ =========
    int   jumpCount_ = 0;
    int   maxJumpCount_ = 2;     // 2段ジャンプ

    bool  touchingLeftWall_ = false;
    bool  touchingRightWall_ = false;

    float extraVelX_ = 0.0f;  // 壁ジャンプによる横速度
    float wallJumpPushX_ = 1.5f;  // 壁ジャンプ時の横方向初速度
    float wallJumpDamping_ = 0.85f; // 減衰（0〜1）

    // ========= 当たり判定 =========
    void ResolveBottomCollisionWithMap(Vector3& pos);
    void ResolveLeftCollisionWithMap(Vector3& pos);
    void ResolveTopCollisionWithMap(Vector3& pos);
    void ResolveRightCollisionWithMap(Vector3& pos);
};
