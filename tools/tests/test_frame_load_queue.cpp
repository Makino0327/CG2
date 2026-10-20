#include "../../project/scene/FrameLoadQueue.h"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    FrameLoadQueue queue;
    std::vector<int> order;
    // 予算をゼロにして、必ず仕事1個ごとに描画へ戻る状態を確認する。
    const auto budget = std::chrono::steady_clock::duration::zero();
    queue.Add([&] { order.push_back(1); });
    queue.Add([&] { order.push_back(2); });
    assert(!queue.Tick(budget));
    assert((order == std::vector<int>{1}));
    assert(queue.Tick(budget));
    assert((order == std::vector<int>{1, 2}));

    // 解析待ちは後続の仕事を開始せず、次のフレームで再確認する。
    bool ready = false;
    int polls = 0;
    queue.AddPoll([&] { ++polls; return ready; });
    queue.Add([&] { order.push_back(3); });
    assert(!queue.Tick());
    assert(polls == 1 && order.size() == 2);
    ready = true;
    assert(!queue.Tick(budget));
    assert(polls == 2 && order.size() == 2);
    assert(queue.Tick(budget));
    assert(order.back() == 3);

    // JSON完成後に設置物の仕事を追加しても、先頭の仕事を壊さない。
    queue.Add([&] {
        order.push_back(4);
        queue.Add([&] { order.push_back(6); });
    });
    queue.Add([&] { order.push_back(5); });
    while (!queue.Tick(budget)) {}
    assert((order == std::vector<int>{1, 2, 3, 4, 5, 6}));

    // 遷移の取り消し後に、古いシーンの仕事を実行しない。
    queue.Add([&] { assert(false); });
    queue.Clear();
    assert(queue.Empty() && queue.Tick());
    std::cout << "Frame load queue tests passed\n";
}
