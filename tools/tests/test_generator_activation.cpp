#include "../../project/game/generator/GeneratorActivation.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    // E入力で開始するまでは、時間が経っても未起動のままにする
    GeneratorActivation generator;
    assert(!generator.Update(30.0));
    assert(generator.GetState() == GeneratorActivation::State::Idle);
    assert(generator.GetProgress() == 0.0f);
    assert(generator.TryStart());

    // 半分の時間で50%になり、E連打や不正な時間で進捗を壊さない
    assert(!generator.Update(7.5));
    assert(generator.GetProgress() == 0.5f);
    assert(!generator.TryStart());
    assert(!generator.Update(-1.0));
    assert(!generator.Update(0.0));
    assert(generator.GetProgress() == 0.5f);
    assert(!generator.Update(7.4));
    assert(generator.GetState() == GeneratorActivation::State::Starting);
    assert(generator.Update(0.2));
    assert(generator.GetState() == GeneratorActivation::State::Active);
    assert(generator.GetProgress() == 1.0f);

    // 起動完了通知は一度だけ発生し、起動済みなら再操作できない
    assert(!generator.Update(100.0));
    assert(!generator.TryStart());
    assert(generator.GetProgress() == 1.0f);

    // 異なるフレームレートでも約15秒で完了し、ほかの発電機には影響しない
    for (int fps : { 30, 60, 144 }) {
        GeneratorActivation timed;
        GeneratorActivation other;
        timed.TryStart();
        double seconds = 0.0;
        while (timed.GetState() != GeneratorActivation::State::Active) {
            timed.Update(1.0 / fps);
            seconds += 1.0 / fps;
            assert(seconds < 16.0);
        }
        assert(std::fabs(seconds - 15.0) <= 1.0 / fps + 0.000001);
        assert(other.GetState() == GeneratorActivation::State::Idle);
        assert(other.GetProgress() == 0.0f);
    }
    std::cout << "PASS: start, 15-second progress, repeat input, completion, independent generators, 30/60/144 FPS\n";
}
