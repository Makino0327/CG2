#pragma once
#include <string>
#include <vector>

#include "../../animation/Skeleton.h"
#include "DebugLine3D.h"

class DebugSkeletonRenderer {
public:
    void Initialize(DirectXCommon* dxCommon, Line3DCommon* line3dCommon);

    // Skeletonから見やすい骨線と関節点を作る
    void Build(const Skeleton& skeleton, const Matrix4x4& worldMatrix, const Matrix4x4& viewProjectionMatrix);

    // 作成済みの骨線を描画する
    void Draw();

    // ImGuiの前面描画で主要Joint名を表示する
    void DrawJointLabels();

    // Jointのワイヤー球の半径を設定する
    void SetJointRadius(float radius);

    // 名前表示に使うGame Viewの画面上の範囲を設定する
    void SetScreenRect(const Vector2& topLeft, const Vector2& size);

private:
    struct JointLabel {
        std::string name;
        Vector3 worldPosition;
    };

    // 行列から平行移動成分を取り出す
    Vector3 ExtractTranslation(const Matrix4x4& matrix) const;

    // Joint用の小さいワイヤー球を追加する
    void AddWireSphere(const Vector3& center, float radius, const Vector4& color);

    // 画面に名前を出すJointだけを選ぶ
    bool ShouldDrawJointLabel(const std::string& jointName) const;

    // 表示用にJoint名を短くする
    std::string MakeDisplayName(const std::string& jointName) const;

    // 3D座標を画面座標へ変換する
    bool ProjectToScreen(const Vector3& worldPosition, Vector2& screenPosition) const;

private:
    // 線描画本体
    DebugLine3D debugLine_;

    // Joint名を描くためにBuild時点の位置を保存する
    std::vector<JointLabel> jointLabels_;

    // ラベル表示用に最後に使ったViewProjectionを保存する
    Matrix4x4 viewProjectionMatrix_ = MakeIdentity4x4();

    // Jointのワイヤー球の半径
    float jointRadius_ = 0.05f;

    // 名前表示を行うGame Viewの左上座標
    Vector2 screenTopLeft_ = { 0.0f, 0.0f };

    // 名前表示を行うGame Viewのサイズ
    Vector2 screenSize_ = { 1280.0f, 720.0f };
};