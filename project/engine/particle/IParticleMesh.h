#pragma once
#include <cstdint>

// 形状によらず、現在設定されているパイプラインで粒子を描画する。
class IParticleMesh {
public:
    virtual ~IParticleMesh() = default;
    virtual void DrawInstanced(uint32_t instanceCount) = 0;
};
