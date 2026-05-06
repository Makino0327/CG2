#pragma once
#include "../../animation/Skeleton.h"
#include "DebugLine3D.h"

class DebugSkeletonRenderer {
public:
    void Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon);


    // Skeleton からデバッグ描画用の線分を組み立てる
    void Build(const Skeleton& skeleton, const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix);


    // 組み立て済みの線分を描画する
    void Draw();

    // Joint のワイヤー球の半径を設定する
    void SetJointRadius(float radius);
private:
    // 行列から平行移動成分を取り出す
    Vector3 ExtractTranslation(const Matrix4x4& matrix) const;

    // Joint 用のワイヤー球を追加する
    void AddWireSphere(const Vector3& center, float radius, const Vector4& color);

private:
    // 線分描画本体
    DebugLine3D debugLine_;

    // Joint のワイヤー球の半径
    float jointRadius_ = 0.15f;

};
