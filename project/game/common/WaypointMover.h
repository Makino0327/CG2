#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../engine/math/Math.h"

class Object3d;
class Object3dCommon;
class Camera;

// Shared helper that moves one Object3d along waypoint positions.
class WaypointMover {
public:
    // Create the internal Object3d and bind the camera.
    void Initialize(Object3dCommon* object3dCommon, Camera* camera);

    // Set the render model file name.
    void SetModel(const std::string& fileName);

    // Set the initial transform.
    void SetPosition(const Vector3& position);
    void SetRotation(const Vector3& rotation);
    void SetScale(const Vector3& scale);

    // Set route points used for movement.
    void SetWaypoints(const std::vector<Vector3>& waypoints);

    // Set move settings.
    void SetMoveSpeed(float moveSpeed);
    void SetLoop(bool isLoop);

    // Update movement and internal Object3d.
    void Update();

    // Draw the internal Object3d.
    void Draw();

    // Return true when at least one waypoint exists.
    bool HasWaypoints() const;

    // Debug getters.
    uint32_t GetCurrentWaypointIndex() const { return currentWaypointIndex_; }
    uint32_t GetWaypointCount() const { return static_cast<uint32_t>(waypoints_.size()); }
    Object3d* GetObject3d() const { return object3d_.get(); }

private:
    // Move toward the current waypoint.
    void MoveToCurrentWaypoint();

private:
    std::unique_ptr<Object3d> object3d_;

    std::vector<Vector3> waypoints_;
    uint32_t currentWaypointIndex_ = 0;

    float moveSpeed_ = 0.05f;
    float reachDistance_ = 0.1f;
    bool isLoop_ = true;
};
