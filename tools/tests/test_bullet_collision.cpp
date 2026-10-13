#include "../../project/game/collision/Collision.h"
#include "../../project/game/player/BulletAim.h"
#include "../../project/game/player/BulletFlight.h"
#include <cassert>
#include <iostream>

// ゲーム本体と同じ弾の移動・高さ補正・移動する敵の判定で、追跡中の連射を再現する。
static bool RunPursuitShot(const Vector3& muzzle, const Vector3& target, float spread)
{
    const Vector3 direction = CalculateDirectBulletAimDirection(muzzle, target);
    const float c = std::cos(spread), s = std::sin(spread);
    Vector3 velocity{ (direction.x*c-direction.z*s)*1.4f, direction.y*1.4f,
        (direction.x*s+direction.z*c)*1.4f };
    Vector3 position = muzzle;
    BoxCollider enemy{ { 0, 1, 8 }, { 1, 1, 1 }, 0 };
    for (int frame = 0; frame < 30; ++frame) {
        // 後退するプレイヤーを、敵が毎フレーム0.1の速さで追いかける。
        const Vector3 previousCenter = enemy.center;
        enemy.center.z -= 0.1f;
        const Vector3 previousPosition = position;
        AdvanceBulletFlight(position, velocity, target.y);
        float time = 0.0f;
        if (Collision::SweepMovingSphereBox(previousPosition, position, 0.4f,
            enemy, previousCenter, time)) { return true; }
    }
    return false;
}

int main()
{
    float time = 0.0f;
    const SphereCollider enemy{ { 0.0f, 0.0f, 0.0f }, 1.0f };
    // 両端の球が敵から離れていても、移動途中で敵の端をかすめれば命中する。
    const SphereCollider before{ { -0.7f, 1.3f, 0.0f }, 0.4f };
    const SphereCollider after{ { 0.7f, 1.3f, 0.0f }, 0.4f };
    assert(!Collision::IsHit(before, enemy) && !Collision::IsHit(after, enemy));
    assert(Collision::SweepSphere(before.center, after.center, 0.4f, enemy, time));
    assert(time > 0.1f && time < 0.2f);
    // 接する弾・外れる弾・静止中の弾を区別する。
    assert(Collision::SweepSphere({ -2, 1.4f, 0 }, { 2, 1.4f, 0 }, 0.4f, enemy, time));
    assert(std::abs(time-0.5f) < 0.001f);
    assert(!Collision::SweepSphere({ -2, 1.5f, 0 }, { 2, 1.5f, 0 }, 0.4f, enemy, time));
    assert(Collision::SweepSphere({ 0, 0, 0 }, { 0, 0, 0 }, 0.4f, enemy, time) && time == 0);
    assert(!Collision::SweepSphere({ 3, 0, 0 }, { 3, 0, 0 }, 0.4f, enemy, time));
    assert(!Collision::SweepSphere({ 3, 0, 0 }, { 4, 0, 0 }, 0.4f, enemy, time));

    const Vector3 minimum{ -0.1f, -1.0f, -1.0f };
    const Vector3 maximum{ 0.1f, 1.0f, 1.0f };
    // 一回の移動で薄い壁を完全に飛び越しても、手前の面で止まる。
    assert(Collision::SweepSphereAabb({ -0.7f, 0, 0 }, { 0.7f, 0, 0 }, 0.4f, minimum, maximum, time));
    assert(std::abs(time-1.0f/7.0f) < 0.0001f);
    assert(Collision::SweepSphereAabb({ 0.7f, 0, 0 }, { -0.7f, 0, 0 }, 0.4f, minimum, maximum, time));
    assert(std::abs(time-1.0f/7.0f) < 0.0001f);
    assert(!Collision::SweepSphereAabb({ -2, 2, 0 }, { 2, 2, 0 }, 0.4f, minimum, maximum, time));
    assert(Collision::SweepSphereAabb({ 0, 0, 0 }, { 0, 0, 0 }, 0.4f, minimum, maximum, time) && time == 0);
    assert(!Collision::SweepSphereAabb({ 2, 0, 0 }, { 2, 0, 0 }, 0.4f, minimum, maximum, time));
    // 箱の角を球がかすめる場合を判定し、角から離れた球は誤って止めない。
    assert(!Collision::SweepSphereAabb({ -2, 1.3f, 1.3f }, { 2, 1.3f, 1.3f }, 0.4f, minimum, maximum, time));
    assert(Collision::SweepSphereAabb({ -2, 1.2f, 1.2f }, { 2, 1.2f, 1.2f }, 0.4f, minimum, maximum, time));
    assert(std::abs(time-(1.9f-std::sqrt(0.08f))/4.0f) < 0.0001f);
    // 移動の終点で初めて触れた場合も命中する。
    assert(Collision::SweepSphereAabb({ -1, 0, 0 }, { -0.5f, 0, 0 }, 0.4f, minimum, maximum, time));
    assert(std::abs(time-1.0f) < 0.0001f);

    // 壁までに短縮した区間なら、壁の手前の敵だけに当たり、奥の敵には届かない。
    const Vector3 start{ -5, 0, 0 }, end{ 5, 0, 0 };
    assert(Collision::SweepSphereAabb(start, end, 0.4f, minimum, maximum, time));
    const Vector3 wallStop{ start.x+(end.x-start.x)*time, 0, 0 };
    const SphereCollider front{ { -3, 0, 0 }, 0.5f }, behind{ { 3, 0, 0 }, 0.5f };
    assert(Collision::SweepSphere(start, wallStop, 0.4f, front, time));
    assert(!Collision::SweepSphere(start, wallStop, 0.4f, behind, time));
    // 敵の登録順が逆でも、命中時刻を比べれば手前の敵を選べる。
    float nearTime = 0.0f, farTime = 0.0f;
    assert(Collision::SweepSphere(start, end, 0.4f, behind, farTime));
    assert(Collision::SweepSphere(start, end, 0.4f, front, nearTime));
    assert(nearTime < farTime);
    // 実際の敵モデルは球ではなく、一辺2の立方体。45度回転した角を通る弾を再現する。
    const Vector3 cornerStart{ 1.3f, 0.7f, -3.0f }, cornerEnd{ 1.3f, 0.7f, 3.0f };
    const BoxCollider cube{ { 0, 0, 0 }, { 1, 1, 1 }, 0.785398163f };
    assert(!Collision::SweepSphere(cornerStart, cornerEnd, 0.4f, enemy, time));
    assert(Collision::SweepSphereBox(cornerStart, cornerEnd, 0.4f, cube, time));
    assert(time > 0.0f && time < 0.5f);
    const Vector3 cornerHit{ 1.3f, 0.7f, -3.0f+6.0f*time };
    const Vector3 surface = Collision::ClosestPointOnBox(cornerHit, cube);
    const float dx = cornerHit.x-surface.x, dy = cornerHit.y-surface.y, dz = cornerHit.z-surface.z;
    assert(std::abs(std::sqrt(dx*dx+dy*dy+dz*dz)-0.4f) < 0.0001f);
    // 箱の上側の角は命中し、見た目から十分離れた場所は命中しない。
    const BoxCollider unrotated{ { 0, 0, 0 }, { 1, 1, 1 }, 0 };
    assert(!Collision::SweepSphere({ -3, 1, 1 }, { 3, 1, 1 }, 0.4f, enemy, time));
    assert(Collision::SweepSphereBox({ -3, 1, 1 }, { 3, 1, 1 }, 0.4f, unrotated, time));
    assert(!Collision::SweepSphereBox({ 2, 0.7f, -3 }, { 2, 0.7f, 3 }, 0.4f, cube, time));
    // 長方形の箱で回転の向きも検証し、敵の拡大率を変えても描画と一致させる。
    const BoxCollider rectangle{ { 10, 2, -5 }, { 2, 1, 0.5f }, 0.785398163f };
    const Vector3 local{ 2, 0, 0 };
    const Vector3 world = Collision::FromBoxLocal(local, rectangle);
    assert(std::abs(world.x-(10.0f+std::sqrt(2.0f))) < 0.0001f);
    assert(std::abs(world.z-(-5.0f-std::sqrt(2.0f))) < 0.0001f);
    const Vector3 restored = Collision::ToBoxLocal(world, rectangle);
    assert(std::abs(restored.x-2.0f) < 0.0001f && std::abs(restored.z) < 0.0001f);
    assert(Collision::SweepSphereBox({ 10, 2, -5 }, { 10, 2, -5 }, 0.4f, rectangle, time) && time == 0);
    // 敵が横へ0.1移動した途中では接触するが、移動後の箱だけを見ると外れるケース。
    const BoxCollider movingEnemy{ { 0, 0, 0 }, { 1, 1, 1 }, 0 };
    const Vector3 movingStart{ 1.405f, 0, -2 }, movingEnd{ 1.405f, 0, -0.6f };
    assert(!Collision::SweepSphereBox(movingStart, movingEnd, 0.4f, movingEnemy, time));
    assert(Collision::SweepMovingSphereBox(movingStart, movingEnd, 0.4f,
        movingEnemy, { 0.1f, 0, 0 }, time));
    assert(time > 0.0f && time < 1.0f);
    // 同じ方向に逃げる敵も、弾と敵の相対速度から正しい命中時刻を求める。
    const BoxCollider escapingEnemy{ { 0, 0, 4.1f }, { 1, 1, 1 }, 0 };
    assert(Collision::SweepMovingSphereBox({ 0, 0, 0 }, { 0, 0, 5 }, 0.4f,
        escapingEnemy, { 0, 0, 4 }, time));
    assert(std::abs(time-2.6f/4.9f) < 0.0001f);

    // 実モデルの構え姿勢の銃口を使い、近い敵を追いかけたときの上側への外れを再現する。
    const Vector3 muzzle{ 0.10664f, 3.71067f, 2.01790f };
    const Vector3 player{ 0, 1, 0 }, nearTarget{ 0, 1.7f, 2.1f };
    const BoxCollider nearEnemy{ { 0, 1, 2.1f }, { 1, 1, 1 }, 0 };
    const float oldAimDistance = std::sqrt(muzzle.x*muzzle.x+muzzle.z*muzzle.z)+2.0f;
    assert(!Collision::SweepSphereBox(muzzle, { 0, 1.7f, oldAimDistance }, 0.4f, nearEnemy, time));
    const Vector3 aimDirection = CalculateBulletAimDirection(muzzle, player, nearTarget, 1.7f);
    const Vector3 aimEnd{ muzzle.x+aimDirection.x*4.0f,
        muzzle.y+aimDirection.y*4.0f, muzzle.z+aimDirection.z*4.0f };
    assert(Collision::SweepSphereBox(muzzle, aimEnd, 0.4f, nearEnemy, time));
    // 銃口より近い場所を狙っても、弾を後方へ反転させない。
    const Vector3 closeAim = CalculateBulletAimDirection(muzzle, player, { 0, 1.7f, 1 }, 1.7f);
    assert(closeAim.z > 0.0f && closeAim.y < 0.0f);
    const Vector3 noAim = CalculateBulletAimDirection(muzzle, player, player, 1.7f);
    assert(noAim.x == 0.0f && noAim.y == 0.0f && noAim.z == 0.0f);

    // 敵へ近づきすぎて銃口が敵の奥へ出た場合、前方補正では敵の上を通り過ぎる。
    const BoxCollider pointBlankEnemy{ { 0, 1, 0.4f }, { 1, 1, 1 }, 0 };
    const Vector3 pointBlankTarget{ 0, 1.7f, 0.4f };
    const Vector3 clampedAim = CalculateBulletAimDirection(muzzle, player, pointBlankTarget, 1.7f);
    const Vector3 clampedEnd{ muzzle.x+clampedAim.x*5,
        muzzle.y+clampedAim.y*5, muzzle.z+clampedAim.z*5 };
    assert(!Collision::SweepSphereBox(muzzle, clampedEnd, 0.4f, pointBlankEnemy, time));
    // マウスが敵を指す場合はその表面を選び、銃口より手前へ補正せず撃つ。
    Vector3 pickedTarget{};
    float pickTime = 0.0f;
    const std::vector<BoxCollider> aimBoxes{ pointBlankEnemy };
    assert(FindBulletAimTarget({ 0, 10, -10 }, { 0, 1, 0.4f }, aimBoxes, {}, pickedTarget, pickTime));
    const Vector3 directAim = CalculateDirectBulletAimDirection(muzzle, pickedTarget);
    const Vector3 directEnd{ muzzle.x+directAim.x*5,
        muzzle.y+directAim.y*5, muzzle.z+directAim.z*5 };
    assert(Collision::SweepSphereBox(muzzle, directEnd, 0.4f, pointBlankEnemy, time));
    assert(directAim.y < 0.0f && directAim.z < 0.0f);
    // 敵の登録順と関係なく画面の手前の敵を選び、敵のいない場所では選択しない。
    assert(FindBulletAimTarget({ 0, 1, -10 }, { 0, 1, 10 },
        { BoxCollider{ { 0, 1, 5 }, { 1, 1, 1 }, 0 }, pointBlankEnemy }, {}, pickedTarget, pickTime));
    assert(std::abs(pickedTarget.z+0.6f) < 0.0001f);
    // マウスの線を壁へ当てても、丸め誤差で奥の面へ抜けず、手前の面を返す。
    assert(Collision::SweepSphereAabb({ 0, 1, -10 }, { 0, 1, 10 }, 0.0f,
        { -1, 0, -0.6f }, { 1, 2, 1.4f }, time));
    assert(std::abs(time-0.47f) < 0.0001f);
    assert(!FindBulletAimTarget({ 10, 1, -10 }, { 10, 1, 10 }, aimBoxes, {}, pickedTarget, pickTime));
    // 立方体の敵より手前のボスを指した場合は、球の表面を選ぶ。
    assert(FindBulletAimTarget({ 0, 1, -10 }, { 0, 1, 10 }, aimBoxes,
        { SphereCollider{ { 0, 1, -4 }, 1 } }, pickedTarget, pickTime));
    assert(std::abs(pickedTarget.z+5.0f) < 0.0001f);
    // 後退しながらのAR連射では、上面の角を狙うと最大のばらつきで敵の上側を外れる。
    const BoxCollider pursuitEnemy{ { 0, 1, 8 }, { 1, 1, 1 }, 0 };
    Vector3 bodyCenter{};
    assert(FindBulletAimTarget({ 0.99f, 10, 8.99f }, { 0.99f, -1, 8.99f },
        { pursuitEnemy }, {}, pickedTarget, pickTime, &bodyCenter));
    assert(std::abs(pickedTarget.y-2.0f) < 0.0001f);
    assert(!RunPursuitShot(muzzle, pickedTarget, -0.08f));
    // マウスが重なった敵の胴体へ狙いと水平移動の高さを合わせれば、同じ条件で当たる。
    assert(bodyCenter.x == 0 && bodyCenter.y == 1 && bodyCenter.z == 8);
    for (float spread : { -0.08f, -0.04f, 0.0f, 0.04f, 0.08f }) {
        assert(RunPursuitShot(muzzle, bodyCenter, spread));
    }
    // 録画と同じく、敵より奥にマウスを置くとカメラからの線は敵に直接重ならない。
    const BoxCollider interveningEnemy{ { 0, 1, 5 }, { 1, 1, 1 }, 0 };
    const Vector3 beyondEnemy{ 0, 1.7f, 10 };
    assert(!FindBulletAimTarget({ 0, 51, -5.9f }, beyondEnemy,
        { interveningEnemy }, {}, pickedTarget, pickTime, &bodyCenter));
    // 高い銃口から奥の目標へ撃つ旧軌道は、途中の敵の上を通って当たらない。
    assert(!Collision::SweepSphereBox(muzzle, beyondEnemy, 0.4f, interveningEnemy, time));
    // 胴体の高さで手前の敵を調べてから狙えば、同じマウス位置でも途中の敵に当たる。
    assert(FindBulletAimTarget({ 0, 1.7f, 0 }, beyondEnemy,
        { interveningEnemy }, {}, pickedTarget, pickTime, &bodyCenter, 0.4f));
    assert(Collision::SweepSphereBox(muzzle, bodyCenter, 0.4f, interveningEnemy, time));
    std::cout << "Bullet collision regression checks passed.\n";
}
