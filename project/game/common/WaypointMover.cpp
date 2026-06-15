#include "WaypointMover.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../camera/Camera.h"

// Return a - b.
static Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
    return {
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    };
}

// Return vector length.
static float Length(const Vector3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Keep the angle inside -PI to PI.
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
    patrolState_ = PatrolState::Move;
    currentWaitFrame_ = 0;
    currentTurnFrame_ = 0;
    currentLookAroundFrame_ = 0;
}

void WaypointMover::SetMoveSpeed(float moveSpeed) {
    moveSpeed_ = moveSpeed;
}

void WaypointMover::SetLoop(bool isLoop) {
    isLoop_ = isLoop;
}

void WaypointMover::ResumePatrol() {
    // Resume movement from the current position after chase ends.
    patrolState_ = PatrolState::Move;
    currentWaitFrame_ = 0;
    currentTurnFrame_ = 0;
    currentLookAroundFrame_ = 0;

    // A single waypoint enemy should always head back to its guard point.
    if (waypoints_.size() <= 1) {
        currentWaypointIndex_ = 0;
    }
}

bool WaypointMover::HasWaypoints() const {
    return !waypoints_.empty();
}

void WaypointMover::Update() {
    if (!object3d_) {
        return;
    }

    if (HasWaypoints()) {
        // Change behavior depending on the current patrol state.
        switch (patrolState_) {
        case PatrolState::Move:
            MoveToCurrentWaypoint();
            break;
        case PatrolState::Wait:
            UpdateWaitState();
            break;
        case PatrolState::Turn:
            UpdateTurnState();
            break;
        case PatrolState::LookAround:
            UpdateLookAroundState();
            break;
        }
    }

    object3d_->Update();
}

void WaypointMover::Draw() {
    if (!object3d_) {
        return;
    }

    object3d_->Draw();
}

void WaypointMover::MoveToCurrentWaypoint() {
    if (waypoints_.empty()) {
        return;
    }

    Vector3 currentPosition = object3d_->GetTranslate();
    const Vector3& targetPosition = waypoints_[currentWaypointIndex_];

    // Compute offset and target distance.
    Vector3 diff = SubtractVector3(targetPosition, currentPosition);
    float distance = Length(diff);

    // When we reach the waypoint, stop there and switch state.
    if (distance <= reachDistance_) {
        object3d_->SetTranslate(targetPosition);
        currentWaitFrame_ = 0;
        patrolState_ = PatrolState::Wait;
        return;
    }

    // Move toward the target.
    Vector3 direction = Normalize(diff);
    currentPosition.x += direction.x * moveSpeed_;
    currentPosition.y += direction.y * moveSpeed_;
    currentPosition.z += direction.z * moveSpeed_;
    object3d_->SetTranslate(currentPosition);

    // Turn toward the move direction on Y axis.
    Vector3 rotate = object3d_->GetRotate();
    rotate.y = std::atan2(direction.x, direction.z);
    object3d_->SetRotate(rotate);
}

void WaypointMover::UpdateWaitState() {
    if (!object3d_ || waypoints_.empty()) {
        return;
    }

    ++currentWaitFrame_;

    // Stay stopped for roughly two seconds.
    if (currentWaitFrame_ < waitFrameCount_) {
        return;
    }

    currentWaitFrame_ = 0;

    // With only one waypoint, choose a random direction to watch.
    if (waypoints_.size() <= 1) {
        lookAroundStartYaw_ = object3d_->GetRotate().y;

        const float kPi = 3.1415926535f;
        const float randomRate = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        lookAroundTargetYaw_ = NormalizeAngle(-kPi + randomRate * (kPi * 2.0f));

        currentLookAroundFrame_ = 0;
        patrolState_ = PatrolState::LookAround;
        return;
    }

    // With multiple waypoints, turn in place toward the next point.
    const uint32_t nextWaypointIndex = GetNextWaypointIndex();
    const Vector3 currentPosition = object3d_->GetTranslate();
    turnStartYaw_ = object3d_->GetRotate().y;
    turnTargetYaw_ = CalculateTargetYaw(currentPosition, waypoints_[nextWaypointIndex]);
    currentTurnFrame_ = 0;
    currentWaypointIndex_ = nextWaypointIndex;
    patrolState_ = PatrolState::Turn;
}

void WaypointMover::UpdateTurnState() {
    if (!object3d_) {
        return;
    }

    Vector3 rotate = object3d_->GetRotate();
    ++currentTurnFrame_;

    // Interpolate along the shortest yaw path for a quick turn-around.
    float t = static_cast<float>(currentTurnFrame_) / static_cast<float>(turnFrameCount_);
    if (t > 1.0f) {
        t = 1.0f;
    }

    const float deltaYaw = NormalizeAngle(turnTargetYaw_ - turnStartYaw_);
    rotate.y = NormalizeAngle(turnStartYaw_ + deltaYaw * t);
    object3d_->SetRotate(rotate);

    if (currentTurnFrame_ >= turnFrameCount_) {
        currentTurnFrame_ = 0;
        patrolState_ = PatrolState::Move;
    }
}

void WaypointMover::UpdateLookAroundState() {
    if (!object3d_) {
        return;
    }

    Vector3 rotate = object3d_->GetRotate();
    ++currentLookAroundFrame_;

    // Turn smoothly toward a random watch direction.
    float t = static_cast<float>(currentLookAroundFrame_) / static_cast<float>(lookAroundFrameCount_);
    if (t > 1.0f) {
        t = 1.0f;
    }

    const float deltaYaw = NormalizeAngle(lookAroundTargetYaw_ - lookAroundStartYaw_);
    rotate.y = NormalizeAngle(lookAroundStartYaw_ + deltaYaw * t);
    object3d_->SetRotate(rotate);

    if (currentLookAroundFrame_ >= lookAroundFrameCount_) {
        currentLookAroundFrame_ = 0;
        patrolState_ = PatrolState::Wait;
    }
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

    // If the target is effectively the same point, keep the current yaw.
    if (Length(direction) <= 0.0001f) {
        return object3d_ ? object3d_->GetRotate().y : 0.0f;
    }

    return std::atan2(direction.x, direction.z);
}
