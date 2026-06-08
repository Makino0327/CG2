#include "WaypointMover.h"

#include <cassert>
#include <cmath>

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
}

void WaypointMover::SetMoveSpeed(float moveSpeed) {
    moveSpeed_ = moveSpeed;
}

void WaypointMover::SetLoop(bool isLoop) {
    isLoop_ = isLoop;
}

bool WaypointMover::HasWaypoints() const {
    return !waypoints_.empty();
}

void WaypointMover::Update() {
    if (!object3d_) {
        return;
    }

    if (HasWaypoints()) {
        MoveToCurrentWaypoint();
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

    // Switch to the next point when close enough.
    if (distance <= reachDistance_) {
        if (currentWaypointIndex_ + 1 < waypoints_.size()) {
            currentWaypointIndex_++;
        }
        else if (isLoop_) {
            currentWaypointIndex_ = 0;
        }

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
