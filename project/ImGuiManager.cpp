#include "ImGuiManager.h"

void ImGuiManager::Initialize(
    [[maybe_unused]] WinApp* winApp,
    [[maybe_unused]] DirectXCommon* dxCommon,
    [[maybe_unused]] SrvManager* srvManager,
    [[maybe_unused]] ID3D12DescriptorHeap* srvHeap)
{
#ifdef USE_IMGUI
    dxCommon_ = dxCommon;
    srvHeap_ = srvHeap;
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(winApp->GetHwnd());

    // ① SRVマネージャからインデックスを確保
    imguiSrvIndex_ = srvManager->Allocate();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        srvManager->GetCPUDescriptorHandle(imguiSrvIndex_);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        srvManager->GetGPUDescriptorHandle(imguiSrvIndex_);


    // DirectX12用初期化
    ImGui_ImplDX12_Init(
        dxCommon->GetDevice(),
        static_cast<int>(dxCommon->GetSwapChainResourcesNum()),
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvManager->GetDescriptorHeap(),
        cpuHandle,
        gpuHandle
    );
#endif
}

void ImGuiManager::Finalize()
{
#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif
}

void ImGuiManager::Begin()
{
#ifdef USE_IMGUI
    // ImGuiフレーム開始
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiManager::End()
{
#ifdef USE_IMGUI
	ImGui::Render();
#endif
}

void ImGuiManager::Draw()
{
#ifdef USE_IMGUI
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // デスクリプタヒープの配列をセットするコマンド
    ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_ };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // 描画コマンドを発行
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

