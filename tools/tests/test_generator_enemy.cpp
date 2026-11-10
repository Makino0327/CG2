#include "../../project/game/enemy/Enemy.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // 描画初期化なしで、実際の敵AIの音の受信と停止地点の選択を確認する
    const Vector3 center{ 8.0f, 1.4f, 0.0f };
    const Vector3 size{ 3.2f, 2.8f, 2.0f };
    std::vector<LevelColliderData> floors{ { "BOX", { 0, -1, 0 }, { 100, 2, 100 }, true } };
    std::vector<LevelColliderData> walls{ { "BOX", center, size, true } };
    Enemy first;
    first.SetFloorColliders(&floors);
    first.SetWallColliders(&walls);
    first.OnHearGenerator(center, size, {});
    Vector3 firstGoal;
    assert(first.GetGeneratorDestination(firstGoal));
    assert(firstGoal.x < center.x - size.x * 0.5f - first.GetBodyRadius());

    // 同じ音を毎フレーム通知しても、決めた停止地点を上書きしない
    first.OnHearGenerator({ -20, 1.4f, 0 }, size, {});
    Vector3 retainedGoal;
    assert(first.GetGeneratorDestination(retainedGoal));
    assert(firstGoal.x == retainedGoal.x && firstGoal.z == retainedGoal.z);

    // 2体目は予約済みの停止地点を避け、敵同士が重ならない間隔を保つ
    Enemy second;
    second.SetFloorColliders(&floors);
    second.SetWallColliders(&walls);
    second.OnHearGenerator(center, size, { firstGoal });
    Vector3 secondGoal;
    assert(second.GetGeneratorDestination(secondGoal));
    assert(std::hypot(firstGoal.x - secondGoal.x, firstGoal.z - secondGoal.z) >= 2.5f);

    // 銃声の警戒中は発電機への移動を開始しない
    first.OnHearSound({ 0, 0, 10 });
    assert(!first.GetGeneratorDestination(retainedGoal));
    first.OnHearGenerator(center, size, {});
    assert(!first.GetGeneratorDestination(retainedGoal));

    // 壁で完全に隔てた地点を到着地点として選ばない
    walls.push_back({ "BOX", { 4, 2, 0 }, { 1, 4, 100 }, true });
    Enemy blocked;
    blocked.SetFloorColliders(&floors);
    blocked.SetWallColliders(&walls);
    blocked.OnHearGenerator(center, size, {});
    assert(!blocked.GetGeneratorDestination(retainedGoal));

    // 増援は床とNavMeshが重なり、壁に触れない地点だけに出現する。
    walls.pop_back();
    LevelNavMeshData mesh;
    mesh.vertices = { {-12, 0, -8}, {-12, 0, 8}, {12, 0, -8}, {12, 0, 8},
        {16, 0, -4}, {16, 0, 4}, {24, 0, -4}, {24, 0, 4} };
    mesh.triangles = { {0, 1, 2}, {2, 1, 3}, {4, 5, 6}, {6, 5, 7} };
    mesh.BuildLinks();
    Enemy reinforcement;
    reinforcement.SetFloorColliders(&floors);
    reinforcement.SetWallColliders(&walls);
    Vector3 spawn;
    assert(!reinforcement.TryGetReinforcementPosition({-8, 0, 0}, spawn));
    reinforcement.SetNavMesh(&mesh);
    assert(!reinforcement.TryGetReinforcementPosition({40, 0, 0}, spawn));
    assert(!reinforcement.TryGetReinforcementPosition({-8, 3, 0}, spawn));
    assert(!reinforcement.TryGetReinforcementPosition({8, 0, 0}, spawn));
    assert(reinforcement.TryGetReinforcementPosition({-8, 0, 0}, spawn));
    assert(spawn.x == -8 && spawn.y == reinforcement.GetCollider().radius && spawn.z == 0);
    reinforcement.SetPosition(spawn);
    reinforcement.OnHearGenerator(center, size, {});
    assert(reinforcement.GetGeneratorDestination(retainedGoal));

    // NavMeshの島に出現できても、発電機まで接続されていなければ増援には使わない。
    Enemy isolated;
    isolated.SetFloorColliders(&floors);
    isolated.SetWallColliders(&walls);
    isolated.SetNavMesh(&mesh);
    assert(isolated.TryGetReinforcementPosition({20, 0, 0}, spawn));
    isolated.SetPosition(spawn);
    isolated.OnHearGenerator(center, size, {});
    assert(!isolated.GetGeneratorDestination(retainedGoal));
    std::cout << "PASS: enemy generator targeting, spacing, reinforcement placement and disconnected route checks\n";
}
