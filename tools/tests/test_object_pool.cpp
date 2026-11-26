#include "../../project/engine/utils/ObjectPool.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>

// 描画を使わず、実際に生成・破棄された個数から再利用の有無を確認する。
struct TrackedObject {
    inline static int created = 0;
    inline static int destroyed = 0;
    int value = 0;
    TrackedObject() { ++created; }
    ~TrackedObject() { ++destroyed; }
};

static_assert(!std::is_copy_constructible_v<ObjectPool<TrackedObject>>);
static_assert(std::is_nothrow_move_constructible_v<ObjectPool<TrackedObject>>);
static_assert(!std::is_copy_constructible_v<ObjectPool<TrackedObject>::Handle>);

static void CheckReuseAndCapacity() {
    ObjectPool<TrackedObject> pool(2);
    int preparations = 0;
    pool.Prewarm(1, [&](TrackedObject& object) { object.value = 7; ++preparations; });
    auto first = pool.Acquire();
    auto second = pool.Acquire([&](TrackedObject& object) { object.value = 9; ++preparations; });
    assert(first && second && first->value == 7 && second->value == 9);
    assert(pool.GetCreatedCount() == 2 && pool.GetActiveCount() == 2 && pool.GetAvailableCount() == 0);
    assert(!pool.Acquire() && !pool.Reset());
    TrackedObject* address = first.get();
    const int destroyed = TrackedObject::destroyed;
    first.reset();
    assert(TrackedObject::destroyed == destroyed && pool.GetIdleCount() == 1);
    auto reused = pool.Acquire([&](TrackedObject&) { ++preparations; });
    assert(reused.get() == address && preparations == 2);
    assert(second->value == 9); // 使用中の別オブジェクトを上書きしていない。
    reused.reset();
    second.reset();
    assert(pool.Reset() && pool.GetCreatedCount() == 0 && pool.GetActiveCount() == 0);
    assert(TrackedObject::destroyed == destroyed + 2);
}

static void CheckRepeatedBursts() {
    ObjectPool<TrackedObject> pool(32);
    pool.Prewarm(32);
    const int created = TrackedObject::created;
    std::vector<ObjectPool<TrackedObject>::Handle> active;
    active.reserve(32);
    for (int frame = 0; frame < 1000; ++frame) {
        for (int index = 0; index < 32; ++index) {
            auto object = pool.Acquire();
            assert(object);
            object->value = frame;
            active.push_back(std::move(object));
        }
        assert(!pool.Acquire());
        // 弾の削除やシーンの初期化と同じく、配列から外すだけで全件が返る。
        active.erase(active.begin());
        assert(pool.GetActiveCount() == 31 && pool.GetIdleCount() == 1);
        active.clear();
        assert(pool.GetActiveCount() == 0 && pool.GetIdleCount() == 32);
    }
    assert(TrackedObject::created == created);
}

static void CheckOwnershipLifetime() {
    const int created = TrackedObject::created;
    const int destroyed = TrackedObject::destroyed;
    {
        ObjectPool<TrackedObject>::Handle survivor;
        {
            ObjectPool<TrackedObject> original(1);
            survivor = original.Acquire();
            ObjectPool<TrackedObject> moved(std::move(original));
            assert(moved.GetActiveCount() == 1 && !moved.Acquire());
        }
        // 所有元が先に消えても、使用中のハンドルはオブジェクトを保持する。
        assert(TrackedObject::created == created + 1 && TrackedObject::destroyed == destroyed);
        survivor->value = 42;
    }
    assert(TrackedObject::destroyed == destroyed + 1);
}

static void CheckFailedPreparation() {
    ObjectPool<TrackedObject> pool(2);
    auto active = pool.Acquire();
    bool threw = false;
    try {
        pool.Acquire([](TrackedObject&) { throw std::runtime_error("test"); });
    } catch (const std::runtime_error&) { threw = true; }
    // 初期化に失敗しても、使用中の枠数と空き枠の数を壊さない。
    assert(threw && pool.GetCreatedCount() == 1 && pool.GetActiveCount() == 1);
    assert(pool.Acquire());
    active.reset();
    assert(pool.Reset());
    ObjectPool<TrackedObject> empty(0);
    empty.Prewarm(100);
    assert(!empty.Acquire() && empty.GetCreatedCount() == 0 && empty.Reset());
}

int main() {
    CheckReuseAndCapacity();
    CheckRepeatedBursts();
    CheckOwnershipLifetime();
    CheckFailedPreparation();
    assert(TrackedObject::created == TrackedObject::destroyed);
    std::cout << "PASS: pool reuse, 32000 acquisitions without creation, capacity, erase/clear return, ownership and exception safety\n";
}
