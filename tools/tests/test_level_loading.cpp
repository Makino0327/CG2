#include "../../project/scene/LevelLoader.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <unordered_map>

// 変更前に敵ごとに実行していた処理を残し、実ステージの経路情報が一致するか比較する。
static std::vector<std::vector<int>> BuildPreviousLinks(const LevelNavMeshData& mesh) {
    std::vector<std::vector<int>> links(mesh.triangles.size());
    std::unordered_map<long long, int> owners;
    for (int i = 0; i < static_cast<int>(mesh.triangles.size()); ++i) {
        const auto& triangle = mesh.triangles[i];
        const int indices[] = { triangle.index0, triangle.index1, triangle.index2 };
        for (int edge = 0; edge < 3; ++edge) {
            const int a = indices[edge], b = indices[(edge+1)%3];
            const long long key = (static_cast<long long>(a < b ? a : b) << 32)
                | static_cast<unsigned int>(a < b ? b : a);
            auto found = owners.find(key);
            if (found == owners.end()) { owners[key] = i; continue; }
            links[i].push_back(found->second);
            links[found->second].push_back(i);
        }
    }
    return links;
}

template<class Function>
static double MeasureMilliseconds(Function function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count();
}

int main(int argc, char** argv) {
    assert(argc == 3);
    // 向きが逆の共有辺は接続し、離れた島とは接続しない。
    LevelNavMeshData small;
    small.vertices.resize(7);
    small.triangles = { {0,1,2}, {2,1,3}, {4,5,6} };
    small.BuildLinks();
    assert((small.neighbors == std::vector<std::vector<int>>{{1},{0},{}}));
    small.BuildLinks();
    assert((small.neighbors == std::vector<std::vector<int>>{{1},{0},{}}));
    // 空のステージへ更新したときは、前の接続情報をすべて消す。
    small.triangles.clear();
    small.BuildLinks();
    assert(small.neighbors.empty());

    // 通常ステージ→ボスステージ→通常ステージの順で読み直して、接続を比較する。
    LevelNavMeshData shared;
    for (const char* file : { argv[1], argv[2], argv[1] }) {
        LevelData level = LevelLoader::LoadFile(file);
        const auto expected = BuildPreviousLinks(level.navMesh);
        shared = std::move(level.navMesh);
        shared.BuildLinks();
        assert(shared.neighbors == expected);
    }

    // ゲーム全体ではなく、今回変更した読み込みと地図構築だけを同じ条件で比較する。
    size_t checksum = 0;
    double previousMs = 0.0, currentMs = 0.0;
    for (int run = 0; run < 3; ++run) {
        previousMs += MeasureMilliseconds([&] {
            LevelData last;
            for (int read = 0; read < 5; ++read) {
                last = LevelLoader::LoadFile(argv[1]);
                checksum += last.objects.size();
            }
            for (int enemy = 0; enemy < 10; ++enemy) {
                checksum += BuildPreviousLinks(last.navMesh).size();
            }
        });
        currentMs += MeasureMilliseconds([&] {
            LevelData level = LevelLoader::LoadFile(argv[1]);
            level.navMesh.BuildLinks();
            checksum += level.objects.size()+level.navMesh.neighbors.size();
        });
    }
    std::cout << "PASS: connected edges, disconnected island, rebuild, empty map, main/boss/main equivalence\n";
    std::cout << "Mean data preparation only (3 runs): before=" << previousMs/3.0
        << " ms, after=" << currentMs/3.0 << " ms, checksum=" << checksum << '\n';
}
