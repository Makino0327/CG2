#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <string>                               // ★ 追加：std::string, std::wstring
#include "WinApp.h"
#include <dxcapi.h>
#include <chrono>

#include "externals/DirectXTex/DirectXTex.h"    // ★ 追加：DirectX::TexMetadata 等

class DirectXCommon
{
public:
    /// CPU ディスクリプタハンドルを取得
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    /// GPU ディスクリプタハンドルを取得
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);

    // SRV用ディスクリプタヒープのCPUハンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    // SRV用ディスクリプタヒープのGPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

    // シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    // バッファリソースの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    // テクスチャリソースの生成
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        const DirectX::TexMetadata& metadata);

    // テクスチャデータのアップロード
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(
        const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages);

    // テクスチャの読み込み（← cpp 側と合わせて非 static にしてある）
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    // 初期化
    void Initialize(WinApp* winApp);
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDepthBuffer();
    void InitializeDescriptorHeaps();
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeScissorRect();
    void InitializeDXC();
    void InitializeImGui();

    // 描画開始前処理 / 描画後処理
    void PreDraw();
    void PostDraw();

	// FPS制御関連
    void InitializeFixFPS();
	// FPS制御の更新
	void UpdateFixFPS();

    // Getter
    ID3D12Device* GetDevice() const { return device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

    ID3D12DescriptorHeap* GetSrvDescriptorHeap() const {
        return srvDescriptorHeap_.Get();
    }

    uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV_; }

    // 最大SRV数
    static const uint32_t kMaxSRVCount;

private:
    // DirectX12デバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    // DXGIファクトリ
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
    // コマンドキュー
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    // コマンドアロケータ
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    // コマンドリスト
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    // スワップチェーン
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
    // DSV用ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
    // RTV用ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    // スワップチェーンのバックバッファ
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_;
    // 深度ステンシル
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    UINT descriptorSizeDSV_ = 0;

    // フェンス関連
    ID3D12Fence* fence_ = nullptr;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // シザー矩形
    D3D12_RECT scissorRect_{};

    // DXC
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;

    // WindowsAPI
    WinApp* winApp = nullptr;

    // ディスクリプタサイズ
    UINT descriptorSizeRTV_ = 0;
    UINT descriptorSizeSRV_ = 0;

    // 各種ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr; // SRV用

    // ディスクリプタヒープ生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);

	// --- FPS制御関連 ---
	std::chrono::steady_clock::time_point reference_;
};
