#pragma once
#include <chrono>
#include <deque>
#include <functional>
#include <utility>

// 初期化を小さな仕事へ分け、各フレームの読み込み後に必ず描画へ戻る。
class FrameLoadQueue {
public:
    void Add(std::function<void()> work) {
        jobs_.push_back([work = std::move(work)] { work(); return true; });
    }
    // 非同期の解析が終わるまでは、待機せず次のフレームで確認する。
    void AddPoll(std::function<bool()> work) { jobs_.push_back(std::move(work)); }
    void Clear() { jobs_.clear(); }
    bool Empty() const { return jobs_.empty(); }
    bool Tick(std::chrono::steady_clock::duration budget = std::chrono::milliseconds(2)) {
        const auto begin = std::chrono::steady_clock::now();
        while (!jobs_.empty()) {
            auto work = std::move(jobs_.front());
            jobs_.pop_front();
            if (!work()) {
                jobs_.push_front(std::move(work));
                return false;
            }
            // 1つの仕事が予算を超えても、次の仕事を続けず描画を優先する。
            if (std::chrono::steady_clock::now()-begin >= budget) { break; }
        }
        return jobs_.empty();
    }
private:
    std::deque<std::function<bool()>> jobs_;
};
