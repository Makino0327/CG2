#include "WaypointMover.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <algorithm>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../camera/Camera.h"

// 2つの位置の差を求める。
static Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// ベクトルの長さを求める。
static float Length(const Vector3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// 角度をマイナスπからπの範囲に収める。
static float NormalizeAngle(float angle) {
    const float kPi = 3.1415926535f;
    const float kTwoPi = kPi * 2.0f;

    while (angle > kPi) {
        angle -= kTwoPi;
    }
    while (angle < -kPi) {
        angle += kTwoPi;
    }

    return angle;
}

// 巡回の各状態が自身の更新と遷移を担当し、共通側では現在の状態を呼ぶ。
class WaypointMover::State {
public:
    virtual ~State() = default;
    virtual void Update(WaypointMover& owner) const = 0;
};

class WaypointMover::MoveState final : public WaypointMover::State {
public:
    // 現在の状態に必要な処理だけを実行し、完了時に次の状態へ移る。
    void Update(WaypointMover& owner) const override {
        if (owner.waypoints_.empty()) {
            return;
        }

        Vector3 currentPosition = owner.object3d_->GetTranslate();
        const Vector3& targetPosition = owner.waypoints_[owner.currentWaypointIndex_];

        // 目的地までの差分と距離を求める。
        Vector3 diff = SubtractVector3(targetPosition, currentPosition);
        float distance = Length(diff);

        // 巡回地点へ到着したら、その位置で停止して待機状態へ移る。
        if (distance <= owner.reachDistance_) {
            owner.object3d_->SetTranslate(targetPosition);
            owner.currentWaitFrame_ = 0;
            owner.ChangeState(GetState<WaitState>());
            return;
        }

        // 目的地へ向かって移動する。
        Vector3 direction = Normalize(diff);
        currentPosition.x += direction.x * owner.moveSpeed_;
        currentPosition.y += direction.y * owner.moveSpeed_;
        currentPosition.z += direction.z * owner.moveSpeed_;
        owner.object3d_->SetTranslate(currentPosition);

        // 水平方向の回転を移動方向へ合わせる。
        Vector3 rotate = owner.object3d_->GetRotate();
        rotate.y = std::atan2(direction.x, direction.z);
        owner.object3d_->SetRotate(rotate);
    }
};

class WaypointMover::WaitState final : public WaypointMover::State {
public:
    // 現在の状態に必要な処理だけを実行し、完了時に次の状態へ移る。
    void Update(WaypointMover& owner) const override {
        if (!owner.object3d_ || owner.waypoints_.empty()) {
            return;
        }

        ++owner.currentWaitFrame_;

        // 約2秒の待機時間が終わるまで停止する。
        if (owner.currentWaitFrame_ < owner.waitFrameCount_) {
            return;
        }

        owner.currentWaitFrame_ = 0;

        // ウェイポイントが1つだけの敵は、少し待ってから周りを見渡す
        if (owner.waypoints_.size() <= 1) {
            owner.lookAroundStartYaw_ = owner.object3d_->GetRotate().y;

            const float kPi = 3.1415926535f;
            const float randomRate = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            owner.lookAroundTargetYaw_ = NormalizeAngle(-kPi + randomRate * (kPi * 2.0f));

            owner.currentLookAroundFrame_ = 0;
            owner.ChangeState(GetState<LookAroundState>());
            return;
        }

        // 巡回地点が複数なら、その場で次の地点の方向へ向く。
        const uint32_t nextWaypointIndex = owner.GetNextWaypointIndex();
        const Vector3 currentPosition = owner.object3d_->GetTranslate();
        owner.turnStartYaw_ = owner.object3d_->GetRotate().y;
        owner.turnTargetYaw_ = owner.CalculateTargetYaw(currentPosition, owner.waypoints_[nextWaypointIndex]);
        owner.currentTurnFrame_ = 0;
        owner.currentWaypointIndex_ = nextWaypointIndex;
        owner.ChangeState(GetState<TurnState>());
    }
};

class WaypointMover::TurnState final : public WaypointMover::State {
public:
    // 現在の状態に必要な処理だけを実行し、完了時に次の状態へ移る。
    void Update(WaypointMover& owner) const override {
        if (!owner.object3d_) {
            return;
        }

        Vector3 rotate = owner.object3d_->GetRotate();
        ++owner.currentTurnFrame_;

        // 最短の回転方向へ補間し、滑らかに向きを変える。
        float t = static_cast<float>(owner.currentTurnFrame_) / static_cast<float>(owner.turnFrameCount_);
        if (t > 1.0f) {
            t = 1.0f;
        }

        const float deltaYaw = NormalizeAngle(owner.turnTargetYaw_ - owner.turnStartYaw_);
        rotate.y = NormalizeAngle(owner.turnStartYaw_ + deltaYaw * t);
        owner.object3d_->SetRotate(rotate);

        if (owner.currentTurnFrame_ >= owner.turnFrameCount_) {
            owner.currentTurnFrame_ = 0;
            owner.ChangeState(GetState<MoveState>());
        }
    }
};

class WaypointMover::LookAroundState final : public WaypointMover::State {
public:
    // 現在の状態に必要な処理だけを実行し、完了時に次の状態へ移る。
    void Update(WaypointMover& owner) const override {
        if (!owner.object3d_) {
            return;
        }

        Vector3 rotate = owner.object3d_->GetRotate();
        ++owner.currentLookAroundFrame_;

        // 無作為に選んだ見回し方向へ滑らかに回転する。
        float t = static_cast<float>(owner.currentLookAroundFrame_) / static_cast<float>(owner.lookAroundFrameCount_);
        if (t > 1.0f) {
            t = 1.0f;
        }

        const float deltaYaw = NormalizeAngle(owner.lookAroundTargetYaw_ - owner.lookAroundStartYaw_);
        rotate.y = NormalizeAngle(owner.lookAroundStartYaw_ + deltaYaw * t);
        owner.object3d_->SetRotate(rotate);

        if (owner.currentLookAroundFrame_ >= owner.lookAroundFrameCount_) {
            owner.currentLookAroundFrame_ = 0;
            owner.ChangeState(GetState<WaitState>());
        }
    }
};

template<class T>
const WaypointMover::State& WaypointMover::GetState() {
    // 状態オブジェクトは共有し、経過時間や巡回地点は各個体に保存する。
    static const T state;
    return state;
}

WaypointMover::WaypointMover() : state_(&GetState<MoveState>()) {}

void WaypointMover::ChangeState(const State& nextState) {
    // 状態の選択を毎フレームやり直さず、遷移要求が来たときだけ切り替える。
    state_ = &nextState;
}

void WaypointMover::Initialize(Object3dCommon* object3dCommon, Camera* camera) {
    assert(object3dCommon);
    assert(camera);

    object3d_ = std::make_unique<Object3d>();
    object3d_->Initialize(object3dCommon);
    object3d_->SetCamera(camera);
}

void WaypointMover::SetModel(const std::string& fileName) {
    assert(object3d_);
    object3d_->SetModel(fileName);
}

void WaypointMover::SetPosition(const Vector3& position) {
    assert(object3d_);
    object3d_->SetTranslate(position);
}

void WaypointMover::SetRotation(const Vector3& rotation) {
    assert(object3d_);
    object3d_->SetRotate(rotation);
}

void WaypointMover::SetScale(const Vector3& scale) {
    assert(object3d_);
    object3d_->SetScale(scale);
}

void WaypointMover::SetWaypoints(const std::vector<Vector3>& waypoints) {
    waypoints_ = waypoints;
    currentWaypointIndex_ = 0;
    ChangeState(GetState<MoveState>());
    currentWaitFrame_ = 0;
    currentTurnFrame_ = 0;
    currentLookAroundFrame_ = 0;
}

void WaypointMover::SetMoveSpeed(float moveSpeed) {
    // 負の速度や無効な数値で巡回状態が壊れることを防ぐ。
    if (std::isfinite(moveSpeed)) { moveSpeed_ = (std::max)(0.0f, moveSpeed); }
}

Vector3 WaypointMover::GetPosition() const {
    // 未初期化の確認にも使えるよう、描画オブジェクトがなければ原点を返す。
    return object3d_ ? object3d_->GetTranslate() : Vector3{};
}

Vector3 WaypointMover::GetRotation() const {
    // 外部へはコピーを渡し、内部の姿勢への直接書き込みを防ぐ。
    return object3d_ ? object3d_->GetRotate() : Vector3{};
}

void WaypointMover::UpdateRenderOnly() {
    // 巡回の状態を変えず、描画に必要な更新だけを実行する。
    if (object3d_) { object3d_->Update(); }
}

void WaypointMover::UpdateTransform() {
    // 押し戻しでは位置だけを反映し、通常更新の重複を避ける。
    if (object3d_) { object3d_->UpdateTransform(); }
}

void WaypointMover::SetLoop(bool isLoop) {
    isLoop_ = isLoop;
}

void WaypointMover::ResumePatrol() {
    // 追跡終了後は現在位置から移動状態を再開する。
    ChangeState(GetState<MoveState>());
    currentWaitFrame_ = 0;
    currentTurnFrame_ = 0;
    currentLookAroundFrame_ = 0;

    // 巡回地点が1つの敵は、その警備地点へ戻す。
    if (waypoints_.size() <= 1) {
        currentWaypointIndex_ = 0;
    }
}

bool WaypointMover::HasWaypoints() const {
    return !waypoints_.empty();
}

Vector3 WaypointMover::GetCurrentWaypointPosition() const {
    // ウェイポイントがない場合は現在位置を返す
    if (waypoints_.empty()) {
        return object3d_ ? object3d_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
    }

    return waypoints_[currentWaypointIndex_];
}

void WaypointMover::Update() {
    if (!object3d_) {
        return;
    }

    if (HasWaypoints()) {
        // 基底型の仮想関数を通して、現在の巡回状態を更新する。
        state_->Update(*this);
    }

    object3d_->Update();
}

void WaypointMover::Draw() {
    if (!object3d_) {
        return;
    }

    object3d_->Draw();
}

uint32_t WaypointMover::GetNextWaypointIndex() const {
    if (waypoints_.empty()) {
        return 0;
    }

    if (currentWaypointIndex_ + 1 < waypoints_.size()) {
        return currentWaypointIndex_ + 1;
    }

    if (isLoop_) {
        return 0;
    }

    return currentWaypointIndex_;
}

float WaypointMover::CalculateTargetYaw(const Vector3& from, const Vector3& to) const {
    const Vector3 direction = SubtractVector3(to, from);

    // 目的地がほぼ同じ位置なら、現在の向きを維持する。
    if (Length(direction) <= 0.0001f) {
        return object3d_ ? object3d_->GetRotate().y : 0.0f;
    }

    return std::atan2(direction.x, direction.z);
}
