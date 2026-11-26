#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

// 必要な個数まで生成し、使用を終えたオブジェクトを破棄せず再利用する。
// 同じスレッドで使うプール。描画などの初期化は生成時の関数へ任せる。
template<class T>
class ObjectPool {
private:
    struct Storage {
        explicit Storage(size_t limit) : capacity(limit) {
            // 返却処理でメモリ確保が発生しないよう、空き一覧の領域を先に確保する。
            idle.reserve(limit);
        }
        size_t capacity;
        size_t createdCount = 0;
        std::vector<std::unique_ptr<T>> idle;
    };

public:
    class ReturnToPool {
    public:
        ReturnToPool() = default;
        void operator()(T* object) const noexcept {
            if (!object) { return; }
            assert(storage_ && storage_->idle.size() < storage_->createdCount);
            // ハンドルを消す・配列から外す・初期化する、いずれの場合も自動で返却する。
            storage_->idle.emplace_back(object);
        }
    private:
        friend class ObjectPool<T>;
        // 所有コンテナを外部へ公開せず、プール自身だけが返却先を設定する。
        explicit ReturnToPool(std::shared_ptr<Storage> storage) : storage_(std::move(storage)) {}
        std::shared_ptr<Storage> storage_;
    };
    using Handle = std::unique_ptr<T, ReturnToPool>;

    explicit ObjectPool(size_t capacity) : storage_(std::make_shared<Storage>(capacity)) {}
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    // ハンドルが共有する保管領域は動かさないため、プール本体を移動しても返却できる。
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;

    size_t GetCapacity() const { return storage_->capacity; }
    size_t GetCreatedCount() const { return storage_->createdCount; }
    size_t GetIdleCount() const { return storage_->idle.size(); }
    size_t GetActiveCount() const { return GetCreatedCount() - GetIdleCount(); }
    size_t GetAvailableCount() const { return GetCapacity() - GetActiveCount(); }

    Handle Acquire() { return Acquire([](T&) {}); }
    template<class Prepare>
    Handle Acquire(Prepare&& prepare) {
        if (!storage_->idle.empty()) {
            auto object = std::move(storage_->idle.back());
            storage_->idle.pop_back();
            return Handle(object.release(), ReturnToPool{storage_});
        }
        if (storage_->createdCount >= storage_->capacity) {
            // 枠が埋まっている場合は、使用中のオブジェクトを上書きしない。
            return Handle(nullptr, ReturnToPool{storage_});
        }
        auto object = std::make_unique<T>();
        prepare(*object); // 高価な初期化は、新しく生成したときだけ実行する。
        ++storage_->createdCount;
        return Handle(object.release(), ReturnToPool{storage_});
    }

    void Prewarm(size_t count) { Prewarm(count, [](T&) {}); }
    template<class Prepare>
    void Prewarm(size_t count, Prepare&& prepare) {
        // 読み込み中に少しずつ呼べるよう、指定数まで足りない分だけを準備する。
        const size_t target = (std::min)(count, storage_->capacity);
        while (storage_->createdCount < target) {
            auto object = std::make_unique<T>();
            prepare(*object);
            storage_->idle.push_back(std::move(object));
            ++storage_->createdCount;
        }
    }

    bool Reset() {
        // 使用中のリソースは破棄せず、全ハンドルが返った場合だけ保管領域を空にする。
        if (GetActiveCount() != 0) { return false; }
        storage_->idle.clear();
        storage_->createdCount = 0;
        return true;
    }

private:
    // 借りたハンドルがプール本体より長く残っても、返却先は最後まで保持される。
    std::shared_ptr<Storage> storage_;
};
