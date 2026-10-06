#pragma once

// 射撃の発光を個別に指定する。0.0fでOFF、1.0fで通常の強さ。
namespace ShootingBloom {
    inline float bulletTrailStrength = 1.80f; // にじみを抑えつつ、弾の明るい芯へ光を渡す。
    inline float muzzleFlashStrength = 2.20f; // 銃口の中心を明るくし、周囲へ光を広げすぎない。
}
