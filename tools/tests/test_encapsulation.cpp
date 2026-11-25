#include "../../project/game/player/ShootingBloom.h"
#include "../../project/game/player/Player.h"
#include "../../project/game/common/WaypointMover.h"
#include "../../project/engine/base/offscreen/Bloom.h"
#include "../../project/engine/base/offscreen/OffscreenRenderer.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <type_traits>
#include <utility>

// 所有コンテナや内部オブジェクトの直接公開が復活していないことを確認する。
template<class T> concept ExposesBullets = requires(const T& value) { value.GetBullets(); };
template<class T> concept ExposesObject = requires(const T& value) { value.GetObject3d(); };
template<class T> concept ExposesSkinCluster = requires(const T& value) { value.GetSkinCluster(); };
template<class T> concept ExposesRenderTexture = requires(const T& value) { value.GetRenderTexture(); };
template<class T> concept ExposesDeviceInitialization = requires(T& value) { value.InitializeDevice(); };
template<class T> concept ExposesVertexCreation = requires(T& value) { value.CreateVertexData(); };
template<class T> concept ExposesMaterialInitialization = requires(T& value) { value.InitializeMaterial(); };
static_assert(!ExposesBullets<Player> && !ExposesObject<WaypointMover>);
static_assert(!ExposesSkinCluster<Object3d> && !ExposesRenderTexture<OffscreenRenderer>);
static_assert(!ExposesDeviceInitialization<DirectXCommon> && !ExposesVertexCreation<Sprite>);
static_assert(!ExposesMaterialInitialization<Object3d>);

// 参照先を変更できるポインタやビューが取得関数から漏れていないことを確認する。
static_assert(std::is_same_v<decltype(std::declval<const Player&>().GetBullet(0)), const PlayerBullet*>);
static_assert(std::is_same_v<decltype(std::declval<const Object3d&>().GetVertexInfluences()), std::span<const VertexInfluence>>);
static_assert(std::is_same_v<decltype(std::declval<const Object3d&>().GetSkinningPalette()), std::span<const WellForGPU>>);
static_assert(!std::is_reference_v<decltype(std::declval<const Bloom&>().GetSettings())>);

// 設定を拒否した後も、投影行列が変わらず有効な値を保っていることを調べる。
static void AssertSameProjection(const Matrix4x4& before, const Matrix4x4& after) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            assert(std::isfinite(after.m[row][column]));
            assert(before.m[row][column] == after.m[row][column]);
        }
    }
}

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    // 射撃設定は範囲外の値を制限し、無効な数値で描画を壊さない。
    ShootingBloom::SetBulletTrailStrength(-1.0f);
    assert(ShootingBloom::GetBulletTrailStrength() == 0.0f);
    ShootingBloom::SetMuzzleFlashStrength(100.0f);
    assert(ShootingBloom::GetMuzzleFlashStrength() == 3.0f);
    ShootingBloom::SetMuzzleLightIntensity(12.0f);
    ShootingBloom::SetMuzzleLightIntensity(nan);
    assert(ShootingBloom::GetMuzzleLightIntensity() == 12.0f);
    ShootingBloom::SetMuzzleLightRadius(0.0f);
    ShootingBloom::SetMuzzleLightRadius(infinity);
    assert(ShootingBloom::GetMuzzleLightRadius() == 1.0f);
    ShootingBloom::SetMuzzleLightPreviewEnabled(true);
    assert(ShootingBloom::IsMuzzleLightPreviewEnabled());

    // 取得した設定の編集だけでは内部状態が変わらず、反映時に範囲が確認される。
    Bloom bloom;
    auto settings = bloom.GetSettings();
    settings.threshold = -2.0f;
    settings.knee = 4.0f;
    settings.intensity = 100.0f;
    settings.radius = -1.0f;
    assert(bloom.GetSettings().intensity == 1.0f);
    bloom.SetSettings(settings);
    const auto limited = bloom.GetSettings();
    assert(limited.threshold == 0.0f && limited.knee == 0.5f);
    assert(limited.intensity == 6.0f && limited.radius == 0.5f);
    bloom.SetSettings({ nan, infinity, nan, infinity });
    const auto retained = bloom.GetSettings();
    assert(retained.threshold == limited.threshold && retained.knee == limited.knee);
    assert(retained.intensity == limited.intensity && retained.radius == limited.radius);

    // 不正な投影設定を渡しても、カメラの有効な設定を維持する。
    Camera camera;
    camera.Update();
    const Matrix4x4 before = camera.GetProjectionMatrix();
    camera.SetFovY(0.0f);
    camera.SetFovY(3.2f);
    camera.SetFovY(nan);
    camera.SetAspectRatio(0.0f);
    camera.SetAspectRatio(infinity);
    camera.SetNearClip(-1.0f);
    camera.SetNearClip(100.0f);
    camera.SetFarClip(0.1f);
    camera.SetFarClip(nan);
    camera.Update();
    AssertSameProjection(before, camera.GetProjectionMatrix());
    camera.SetFovY(0.65f);
    camera.SetAspectRatio(1.0f);
    camera.SetNearClip(0.2f);
    camera.SetFarClip(200.0f);
    camera.Update();
    assert(camera.GetProjectionMatrix().m[0][0] != before.m[0][0]);

    // 存在しない弾への通知は安全に無視し、命中統計を変更しない。
    Player player;
    assert(player.GetBulletCount() == 0 && player.GetBullet(0) == nullptr);
    player.ResolveBulletHit(0, 0.5f, true);
    player.FinishBulletUpdate(0);
    assert(player.GetShotCount() == 0 && player.GetHitShotCount() == 0);

    std::cout << "PASS: encapsulation boundaries, effect settings and camera invariants\n";
}
