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

    // Return from chase state to normal waypoint patrol.
    void ResumePatrol();

    // Draw the internal Object3d.
    void Draw();

    // Return true when at least one waypoint exists.
    bool HasWaypoints() const;

    // Debug getters.
    uint32_t GetCurrentWaypointIndex() const { return currentWaypointIndex_; }
    uint32_t GetWaypointCount() const { return static_cast<uint32_t>(waypoints_.size()); }
    Object3d* GetObject3d() const { return object3d_.get(); }

private:
    // Patrol state used to insert waiting and turning behaviors.
    enum class PatrolState {
        Move,
        Wait,
        Turn,
        LookAround,
    };

    // Move toward the current waypoint.
    void MoveToCurrentWaypoint();
    // Update the wait state after reaching a waypoint.
    void UpdateWaitState();
    // Update the in-place turning animation toward the next target.
    void UpdateTurnState();
    // Update the random look-around behavior for a single waypoint.
    void UpdateLookAroundState();
    // Return the next waypoint index.
    uint32_t GetNextWaypointIndex() const;
    // Compute the yaw angle needed to face the target position.
    float CalculateTargetYaw(const Vector3& from, const Vector3& to) const;

private:
    std::unique_ptr<Object3d> object3d_;

    std::vector<Vector3> waypoints_;
    uint32_t currentWaypointIndex_ = 0;
    PatrolState patrolState_ = PatrolState::Move;

    float moveSpeed_ = 0.05f;
    float reachDistance_ = 0.1f;
    bool isLoop_ = true;

    // Wait about two seconds at 60fps before turning or looking around.
    uint32_t waitFrameCount_ = 120;
    uint32_t currentWaitFrame_ = 0;

    // Turn over a short fixed number of frames.
    uint32_t turnFrameCount_ = 30;
    uint32_t currentTurnFrame_ = 0;
    float turnStartYaw_ = 0.0f;
    float turnTargetYaw_ = 0.0f;

    // For a single waypoint, pick a random facing direction and turn to it.
    uint32_t lookAroundFrameCount_ = 45;
    uint32_t currentLookAroundFrame_ = 0;
    float lookAroundStartYaw_ = 0.0f;
    float lookAroundTargetYaw_ = 0.0f;
};
