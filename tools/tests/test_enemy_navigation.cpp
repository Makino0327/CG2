#include <array>
#include <memory>
#include <vector>
#include <cassert>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cfloat>
#include <random>
#include <queue>
#include "../../project/engine/3d/obj3d/DebugLine3D.h"
#include "../../project/engine/3d/obj3d/Object3d.h"
#include "../../project/game/collision/Collision.h"
#include "../../project/game/common/WaypointMover.h"
#include "../../project/game/map/MapChipField.h"
#include "../../project/scene/LevelLoader.h"

// 描画を初期化せずに、実際の経路判断と視認判定を直接検証する
#define private public
#include "../../project/game/enemy/Enemy.h"
#include "../../generated/tests/enemy_navigation_methods.h"
#undef private

static LevelNavMeshData MakeCoverNavMesh()
{
    // 中央のカバーを避け、上下どちらにも回り込める通路を作る
    LevelNavMeshData mesh;
    const float xs[] = { -8.0f, -4.0f, -2.5f, 2.5f, 4.0f, 8.0f };
    const float zs[] = { -8.0f, -4.5f, 4.5f, 8.0f };
    for (float z : zs) {
        for (float x : xs) {
            mesh.vertices.push_back({ x, 0.0f, z });
        }
    }
    for (int z = 0; z < 3; ++z) {
        for (int x = 0; x < 5; ++x) {
            if (x == 2 && z == 1) { continue; }
            const int a = z * 6 + x;
            const int b = a + 1;
            const int c = a + 6;
            const int d = c + 1;
            mesh.triangles.push_back({ a, b, d });
            mesh.triangles.push_back({ a, d, c });
        }
    }
    mesh.neighbors.resize(mesh.triangles.size());
    for (int a = 0; a < static_cast<int>(mesh.triangles.size()); ++a) {
        const auto& first = mesh.triangles[a];
        for (int b = a + 1; b < static_cast<int>(mesh.triangles.size()); ++b) {
            const auto& second = mesh.triangles[b];
            int shared = 0;
            for (int vertex : { first.index0, first.index1, first.index2 }) {
                if (vertex == second.index0 || vertex == second.index1 || vertex == second.index2) {
                    ++shared;
                }
            }
            if (shared == 2) {
                mesh.neighbors[a].push_back(b);
                mesh.neighbors[b].push_back(a);
            }
        }
    }
    return mesh;
}

int main()
{
    // 正反対の目標へ切り替えても、以前の方向へ進み続けない
    Enemy turning;
    turning.position_ = { 0.0f, 0.5f, 0.0f };
    turning.navMeshSmoothedDirection_ = { 0.0f, 0.0f, 1.0f };
    Vector3 previous = turning.navMeshSmoothedDirection_;
    for (int frame = 0; frame < 30; ++frame) {
        const Vector3 direction = turning.CalculateNavMeshChaseDirection({ 0.0f, 0.5f, -10.0f });
        assert(std::isfinite(direction.x) && std::isfinite(direction.z));
        assert(previous.x * direction.x + previous.z * direction.z > std::cos(0.141f));
        previous = direction;
    }
    assert(previous.z < -0.99f);

    // NavMeshの有無の両方で、壁へ入り込まず反対側まで回り込める
    LevelNavMeshData mesh = MakeCoverNavMesh();
    std::vector<LevelColliderData> walls{
        { "BOX", { 0.0f, 2.0f, 0.0f }, { 2.0f, 4.0f, 6.0f }, true }
    };
    for (bool useNavMesh : { false, true }) {
        Enemy chasing;
        chasing.position_ = { -5.0f, 0.5f, 0.0f };
        chasing.SetWallColliders(&walls);
        if (useNavMesh) { chasing.SetNavMesh(&mesh); }
        const Vector3 target = { 5.0f, 0.5f, 0.0f };
        bool arrived = false;
        for (int frame = 0; frame < 600; ++frame) {
            const Vector3 direction = chasing.CalculateNavMeshChaseDirection(target);
            chasing.position_.x += direction.x * chasing.moveSpeed_;
            chasing.position_.z += direction.z * chasing.moveSpeed_;
            assert(!chasing.IsPositionBlockedByWall(chasing.position_));
            if (std::hypot(chasing.position_.x - target.x, chasing.position_.z - target.z) < 0.4f) {
                arrived = true;
                break;
            }
        }
        assert(arrived);
    }

    // 古い経路に現在位置が含まれない場合は、更新待ち時間を残していても作り直す
    Enemy displaced;
    displaced.position_ = { -5.0f, 0.5f, 0.0f };
    displaced.SetWallColliders(&walls);
    displaced.SetNavMesh(&mesh);
    const Vector3 blockedTarget = { 5.0f, 0.5f, 0.0f };
    displaced.navMeshPath_ = { displaced.FindNavMeshTriangle(blockedTarget) };
    displaced.navMeshPathRefreshTimer_ = 9;
    displaced.CalculateNavMeshChaseDirection(blockedTarget);
    const int currentTriangle = displaced.FindNavMeshTriangle(displaced.position_);
    assert(std::find(displaced.navMeshPath_.begin(), displaced.navMeshPath_.end(),
        currentTriangle) != displaced.navMeshPath_.end());

    // 回り込み途中に対象が反対方向へ現れたら、古い経路へ進み続けない
    const Vector3 newTarget = { -5.0f, 0.5f, -7.0f };
    Vector3 redirected{};
    for (int frame = 0; frame < 30; ++frame) {
        redirected = displaced.CalculateNavMeshChaseDirection(newTarget);
    }
    assert(redirected.z < -0.99f);

    // 到着後は以前の進行方向を残さず、停止する
    const Vector3 stopped = displaced.CalculateNavMeshChaseDirection(displaced.position_);
    assert(stopped.x == 0.0f && stopped.z == 0.0f);

    // 追跡中は横へ回り込んだ対象を再発見し、巡回中の視野は維持する
    Enemy vision;
    vision.position_ = { 0.0f, 0.5f, 0.0f };
    vision.rotation_ = { 0.0f, 0.0f, 0.0f };
    vision.SetTargetPosition({ 5.0f, 1.5f, 1.0f });
    assert(!vision.CheckTargetInSight());
    vision.isChasing_ = true;
    assert(vision.CheckTargetInSight());

    // 視野を広げても、壁の向こう側にいる対象は見えない
    std::vector<LevelColliderData> sightWalls{
        { "BOX", { 2.5f, 2.0f, 0.5f }, { 1.0f, 4.0f, 6.0f }, true }
    };
    vision.SetWallColliders(&sightWalls);
    assert(!vision.CheckTargetInSight());
    std::cout << "PASS: smooth reversal, cover navigation, route refresh, moving target, stopping, reacquisition, sight occlusion\n";
}
