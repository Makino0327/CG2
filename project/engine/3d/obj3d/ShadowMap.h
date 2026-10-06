#pragma once
#include <cmath>
#include "../../base/DirectX/DirectXCommon.h"
#include "../../base/srv/SrvManager.h"
#include "../../math/Math.h"

// 平行光源から見た深度を保存し、人や壁が光を遮る位置を調べる。
class ShadowMap {
public:
    // HLSLのb5と同じ順序・大きさにそろえる。
    struct Parameters {
        Matrix4x4 lightViewProjection;
        Vector3 lightDirection;
        float strength;
        Vector2 texelSize;
        float depthBias;
        float enabled;
    };
    static_assert(sizeof(Parameters) == 96);

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
        dxCommon_ = dxCommon;
        srvManager_ = srvManager;

        // DSVでは深度として書き、SRVでは深度値の画像として読む。
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = kResolution;
        description.Height = kResolution;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_R32_TYPELESS;
        description.SampleDesc.Count = 1;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_D32_FLOAT;
        clear.DepthStencil.Depth = 1.0f;
        HRESULT result = dxCommon_->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
            IID_PPV_ARGS(depthTexture_.GetAddressOf()));
        assert(SUCCEEDED(result));

        // 画面用の深度バッファと分け、影を描いても画面の深度を消さない。
        D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
        heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDescription.NumDescriptors = 1;
        result = dxCommon_->GetDevice()->CreateDescriptorHeap(
            &heapDescription, IID_PPV_ARGS(dsvHeap_.GetAddressOf()));
        assert(SUCCEEDED(result));
        D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
        depthView.Format = DXGI_FORMAT_D32_FLOAT;
        depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dxCommon_->GetDevice()->CreateDepthStencilView(
            depthTexture_.Get(), &depthView, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
        assert(srvManager_->CanAllocate());
        srvIndex_ = srvManager_->Allocate();
        srvManager_->CreateSRVForDepthTexture(srvIndex_, depthTexture_.Get());

        // 行列や影の濃さは、このフレームの影描画と通常描画で共有する。
        parameterResource_ = dxCommon_->CreateBufferResource(sizeof(Parameters));
        result = parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameters_));
        assert(SUCCEEDED(result));
        *parameters_ = { MakeIdentity4x4(), GetLightDirection(), 0.72f,
            { 1.0f / kResolution, 1.0f / kResolution }, 0.0004f, 1.0f };
    }

    void Begin(const Vector3& focus) {
        const Vector3 forward = GetLightDirection();
        // 光が真下を向いても、右方向を計算できる基準軸を選ぶ。
        const Vector3 referenceUp = std::abs(forward.y) > 0.98f
            ? Vector3{ 0.0f, 0.0f, 1.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
        const Vector3 right = Normalize(Cross(referenceUp, forward));
        const Vector3 up = Cross(forward, right);

        // カメラが少し動いた時に影がちらつかないよう、影の画素幅へ中心をそろえる。
        constexpr float texelWorldSize = 2.0f * kHalfExtent / kResolution;
        const float rightDistance = Dot(focus, right);
        const float upDistance = Dot(focus, up);
        const float rightOffset = std::round(rightDistance / texelWorldSize) * texelWorldSize - rightDistance;
        const float upOffset = std::round(upDistance / texelWorldSize) * texelWorldSize - upDistance;
        const Vector3 center{
            focus.x + right.x * rightOffset + up.x * upOffset,
            focus.y + right.y * rightOffset + up.y * upOffset,
            focus.z + right.z * rightOffset + up.z * upOffset
        };
        const Vector3 position{
            center.x - forward.x * 100.0f,
            center.y - forward.y * 100.0f,
            center.z - forward.z * 100.0f
        };

        // 行ベクトルを使う既存の行列計算に合わせ、光の視点のワールド行列を作る。
        Matrix4x4 world = MakeIdentity4x4();
        world.m[0][0] = right.x; world.m[0][1] = right.y; world.m[0][2] = right.z;
        world.m[1][0] = up.x; world.m[1][1] = up.y; world.m[1][2] = up.z;
        world.m[2][0] = forward.x; world.m[2][1] = forward.y; world.m[2][2] = forward.z;
        world.m[3][0] = position.x; world.m[3][1] = position.y; world.m[3][2] = position.z;
        const Matrix4x4 projection = MakeOrthographicMatrix(
            -kHalfExtent, kHalfExtent, kHalfExtent, -kHalfExtent, 1.0f, 220.0f);
        parameters_->lightViewProjection = Multiply(Inverse(world), projection);
        parameters_->lightDirection = forward;

        // 読み取り用だった画像を深度の書き込み用にして、前のフレームの影を消す。
        Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        auto* list = dxCommon_->GetCommandList();
        const auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
        list->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
        list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        const D3D12_VIEWPORT viewport{ 0, 0, static_cast<float>(kResolution),
            static_cast<float>(kResolution), 0, 1 };
        const D3D12_RECT scissor{ 0, 0, kResolution, kResolution };
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissor);
    }

    void End() {
        // 通常画面のピクセルシェーダーから、完成した影の深度を読めるようにする。
        auto* list = dxCommon_->GetCommandList();
        list->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
        Transition(D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void Bind(ID3D12GraphicsCommandList* list, bool depthPass) const {
        // 影描画中は同じ画像を読み書きしない。通常描画時だけt3へ渡す。
        list->SetGraphicsRootConstantBufferView(8, parameterResource_->GetGPUVirtualAddress());
        if (!depthPass) { srvManager_->SetGraphicsRootDescriptorTable(9, srvIndex_); }
    }

    Vector3 GetLightDirection() const { return Normalize(lightDirection_); }

private:
    // 光の進む方向。横成分を持たせ、人や壁の影を斜めに落とす。
    Vector3 lightDirection_{ 0.35f, -1.0f, 0.25f };
    static constexpr LONG kResolution = 2048;
    static constexpr float kHalfExtent = 60.0f;

    static float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 Cross(const Vector3& a, const Vector3& b) {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    void Transition(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = depthTexture_.Get();
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    }

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
    Parameters* parameters_ = nullptr;
    uint32_t srvIndex_ = 0;
};
