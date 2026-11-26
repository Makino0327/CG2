#pragma once
#include <array>
#include <wrl.h>
#include <d3d12.h>
#include "../srv/SrvManager.h"
#include "Bloom.h"

// 一回の描画に必要な入力だけを渡し、描画管理クラス全体への依存を避ける。
struct PostEffectDrawContext {
    ID3D12GraphicsCommandList* commandList;
    SrvManager* srvManager;
    uint32_t inputSrvIndex;
    uint32_t emissionSrvIndex;
    uint32_t depthSrvIndex;
    uint32_t maskSrvIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE output;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor;
};

class IPostEffect {
public:
    virtual ~IPostEffect() = default;
    virtual void Draw(const PostEffectDrawContext& context) = 0;
    virtual bool RequiresDepthTexture() const = 0;
};

// シェーダーと定数バッファだけが違う処理は、一つの実装と設定で表現する。
class SinglePassPostEffect final : public IPostEffect {
public:
    SinglePassPostEffect(ID3D12RootSignature* root, ID3D12PipelineState* pipeline,
        const std::array<D3D12_GPU_VIRTUAL_ADDRESS,5>& constants, bool requiresDepth)
        : root_(root), pipeline_(pipeline), constants_(constants), requiresDepth_(requiresDepth) {}
    bool RequiresDepthTexture() const override { return requiresDepth_; }
    void Draw(const PostEffectDrawContext& context) override {
        auto* list = context.commandList;
        list->OMSetRenderTargets(1, &context.output, FALSE, nullptr);
        const float clearColor[4] = {0,0,0,1};
        list->ClearRenderTargetView(context.output, clearColor, 0, nullptr);
        list->RSSetViewports(1, &context.viewport);
        list->RSSetScissorRects(1, &context.scissor);
        list->SetGraphicsRootSignature(root_.Get());
        list->SetPipelineState(pipeline_.Get());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D12DescriptorHeap* heaps[] = {context.srvManager->GetDescriptorHeap()};
        list->SetDescriptorHeaps(1, heaps);
        context.srvManager->SetGraphicsRootDescriptorTable(0,context.inputSrvIndex);
        context.srvManager->SetGraphicsRootDescriptorTable(1,context.depthSrvIndex);
        context.srvManager->SetGraphicsRootDescriptorTable(2,context.maskSrvIndex);
        for (uint32_t index = 0; index < constants_.size(); ++index) {
            list->SetGraphicsRootConstantBufferView(index+3,constants_[index]);
        }
        list->DrawInstanced(3,1,0,0);
    }
private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    std::array<D3D12_GPU_VIRTUAL_ADDRESS,5> constants_;
    bool requiresDepth_;
};

// 複数パスを必要とするBloomは、同じ入口を持つ専用の実装へ委譲する。
class BloomPostEffect final : public IPostEffect {
public:
    explicit BloomPostEffect(Bloom& bloom) : bloom_(bloom) {}
    bool RequiresDepthTexture() const override { return false; }
    void Draw(const PostEffectDrawContext& context) override {
        bloom_.Draw(context.inputSrvIndex,context.emissionSrvIndex,context.output);
    }
private:
    Bloom& bloom_;
};
