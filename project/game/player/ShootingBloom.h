#pragma once

// 射撃の発光を個別に指定する。0.0fでOFF、1.0fで通常の強さ。
namespace ShootingBloom {
    inline float bulletTrailStrength = 1.80f; // 前の弾の発光へ戻す。見た目の大きさは軌跡側で80%にする。
    inline float muzzleFlashStrength = 2.60f; // 発射炎のBloomを2倍にし、銃口の光のにじみを強める。
    inline constexpr float muzzleFlashDurationScale = 1.50f; // 発射炎が残る時間を1.5倍にする。
    inline float muzzleLightIntensity = 20.0f; // 周囲へ当たるライトも2倍にし、銃や床の明るさを強める。
    inline float muzzleLightRadius = 6.0f; // 銃口の高さから床にも光が届くようにする。
    inline constexpr int muzzleLightDurationFrames = 9; // 周囲を照らす発射光を約0.15秒で消す。
    inline bool previewMuzzleLight = false; // デバッグ用。点灯し続けて光の当たり方を確認する。
}
