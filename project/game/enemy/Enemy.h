#pragma once

#include <memory>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/math/Math.h"

class Object3dCommon;

class Enemy
{
public:
    // 敵を初期化する
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position);

    // 敵を更新する
    void Update();

    // 敵を描画する
    void Draw();

    // 敵の座標を設定する
    void SetPosition(const Vector3& position);

    // 敵の座標を取得する
    Vector3 GetWorldPosition() const;

private:
    // 敵の3Dオブジェクト
    std::unique_ptr<Object3d> object_;

    // 敵の座標
    Vector3 position_ = { 0.0f, 0.5f, 0.0f };

    // 敵の回転
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    // 敵の大きさ
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
};
