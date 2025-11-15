#pragma once  
#include <d3d12.h>  
#include <dxgi1_6.h>  
#include <wrl.h>  
#include <array> // Ensure <array> is included for std::array  
#include "WinApp.h"  
#include <dxcapi.h>

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

   // 初期化  
   void Initialize(WinApp* winApp);  
   // デバイス初期化  
   void InitializeDevice();  
   // コマンド初期化  
   void InitializeCommand();  
   // スワップチェーン初期化  
   void InitializeSwapChain();  
   // 深度バッファの初期化  
   void InitializeDepthBuffer();  
   // ディスクリプターヒープ  
   void InitializeDescriptorHeaps();  

   void InitializeRenderTargetView();  

   void InitializeDepthStencilView();

   void InitializeFence();

   void InitializeScissorRect();

   void InitializeDXC();

   void InitializeImGui();
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
   //  
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;  
   //  
   std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_; 
   // 
   Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
   UINT descriptorSizeDSV_ = 0;

   // --- フェンス関連（main のグローバルをそのままメンバ化） ---
   ID3D12Fence* fence_ = nullptr;
   UINT64 fenceValue_ = 0;
   HANDLE fenceEvent_ = nullptr;

   D3D12_RECT scissorRect_{};

   Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
   Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
   Microsoft::WRL::ComPtr<IDxcIncludeHandler> dxcIncludeHandler_;

private:  
   // WindowsAPI  
   WinApp* winApp = nullptr;  

   // ==== ここから今回のスライド部分 ====  

   // 各種ディスクリプタサイズ  
   UINT descriptorSizeRTV_ = 0;  
   UINT descriptorSizeSRV_ = 0;  


   // 各種ディスクリプタヒープ  
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr; // SRV用  

   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(  
       D3D12_DESCRIPTOR_HEAP_TYPE heapType,  
       UINT numDescriptors,  
       bool shaderVisible);  
};
