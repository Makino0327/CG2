#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../engine/math/Math.h"

class Object3d;
class Object3dCommon;
class Camera;

// 1つの描画オブジェクトを巡回地点に沿って動かす。
class WaypointMover {
public:
    // 初期状態は巡回地点へ向かう移動状態にする。
    WaypointMover();

    // 内部の描画オブジェクトを生成し、カメラを設定する。
    void Initialize(Object3dCommon* object3dCommon, Camera* camera);

    // 描画に使用するモデル名を設定する。
    void SetModel(const std::string& fileName);

    // 初期位置・回転・拡大率を設定する。
    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);

    // 移動に使用する巡回地点を設定する。
    void SetWaypoints(const std::vector<Vector3>& waypoints);

    // 移動速度と巡回の繰り返しを設定する。
    void SetMoveSpeed(float moveSpeed);
    void SetLoop(bool isLoop);

    // 現在の巡回状態と描画オブジェクトを更新する。
    void Update();

    // 巡回移動を進めず、設定済みの姿勢だけを描画へ反映する。
    void UpdateRenderOnly();
    // 接触による位置補正では、アニメーションも進めない。
    void UpdateTransform();

    // 追跡を終え、現在位置から通常の巡回を再開する。
    void ResumePatrol();

    // 内部の描画オブジェクトを描く。
    void Draw();

    // 巡回地点が1つ以上あるか返す。
    bool HasWaypoints() const;

    // 確認用に現在の巡回地点と地点数を返す。
    uint32_t GetCurrentWaypointIndex() const { return currentWaypointIndex_; }
    uint32_t GetWaypointCount() const { return static_cast<uint32_t>(waypoints_.size()); }
    // 内部オブジェクトは渡さず、必要な状態だけを値で返す。
    bool IsInitialized() const { return object3d_ != nullptr; }
    Vector3 GetPosition() const;
    Vector3 GetRotation() const;

    // 現在向かっているウェイポイント位置を返す
    Vector3 GetCurrentWaypointPosition() const;

    // ウェイポイントへ到着扱いにする距離を返す
    float GetReachDistance() const { return reachDistance_; }

private:
    // 移動・待機・旋回・見回しを、独立した状態クラスとして扱う。
    class State;
    class MoveState;
    class WaitState;
    class TurnState;
    class LookAroundState;
    template<class T> static const State& GetState();
    void ChangeState(const State& nextState);
    // 次に向かう巡回地点の番号を求める。
    uint32_t GetNextWaypointIndex() const;
    // 目的地へ向くための水平角を求める。
    float CalculateTargetYaw(const Vector3& from, const Vector3& to) const;

private:
    std::unique_ptr<Object3d> object3d_;

    std::vector<Vector3> waypoints_;
    uint32_t currentWaypointIndex_ = 0;
    const State* state_ = nullptr; // 現在の巡回状態を保持する。

    float moveSpeed_ = 0.05f;
    float reachDistance_ = 0.1f;
    bool isLoop_ = true;

    // 60FPSで約2秒待ってから、旋回または見回しへ進む。
    uint32_t waitFrameCount_ = 120;
    uint32_t currentWaitFrame_ = 0;

    // 決められたフレーム数で次の巡回地点へ向く。
    uint32_t turnFrameCount_ = 30;
    uint32_t currentTurnFrame_ = 0;
    float turnStartYaw_ = 0.0f;
    float turnTargetYaw_ = 0.0f;

    // 巡回地点が1つなら、無作為に選んだ方向へ見回す。
    uint32_t lookAroundFrameCount_ = 45;
    uint32_t currentLookAroundFrame_ = 0;
    float lookAroundStartYaw_ = 0.0f;
    float lookAroundTargetYaw_ = 0.0f;
};
