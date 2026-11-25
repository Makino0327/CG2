#pragma once
#include <algorithm>
#include <cmath>

// タイトルと本編で共有する射撃の発光設定。変更は範囲を確認する関数を通す。
class ShootingBloom final {
public:
    // 演出の長さは固定値として公開し、外部からの変更を禁止する。
    static constexpr float muzzleFlashDurationScale = 1.50f;
    static constexpr int muzzleLightDurationFrames = 9;

    // 設定値はコピーで返し、内部変数への参照やポインタを渡さない。
    static float GetBulletTrailStrength() { return bulletTrailStrength_; }
    static float GetMuzzleFlashStrength() { return muzzleFlashStrength_; }
    static float GetMuzzleLightIntensity() { return muzzleLightIntensity_; }
    static float GetMuzzleLightRadius() { return muzzleLightRadius_; }
    static bool IsMuzzleLightPreviewEnabled() { return previewMuzzleLight_; }

    // 無効な数値は採用せず、現在の調整画面で扱う範囲に制限する。
    static void SetBulletTrailStrength(float strength) {
        if (std::isfinite(strength)) { bulletTrailStrength_ = std::clamp(strength, 0.0f, 3.0f); }
    }
    static void SetMuzzleFlashStrength(float strength) {
        if (std::isfinite(strength)) { muzzleFlashStrength_ = std::clamp(strength, 0.0f, 3.0f); }
    }
    static void SetMuzzleLightIntensity(float intensity) {
        if (std::isfinite(intensity)) { muzzleLightIntensity_ = std::clamp(intensity, 0.0f, 20.0f); }
    }
    static void SetMuzzleLightRadius(float radius) {
        if (std::isfinite(radius)) { muzzleLightRadius_ = std::clamp(radius, 1.0f, 10.0f); }
    }
    // 点灯し続けるデバッグ表示だけを切り替える。
    static void SetMuzzleLightPreviewEnabled(bool enabled) { previewMuzzleLight_ = enabled; }

private:
    // 設定専用のクラスなので、インスタンスは生成しない。
    ShootingBloom() = delete;

    inline static float bulletTrailStrength_ = 1.80f;
    inline static float muzzleFlashStrength_ = 2.60f;
    inline static float muzzleLightIntensity_ = 20.0f;
    inline static float muzzleLightRadius_ = 6.0f;
    inline static bool previewMuzzleLight_ = false;
};
