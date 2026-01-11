#include "ImGuiManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"


void ImGuiManager::Initialize(
    WinApp* winApp,
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    ID3D12DescriptorHeap* srvHeap)
{
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

}

void ImGuiManager::Finalize()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::Begin()
{
    // ImGuiフレーム開始
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

}

void ImGuiManager::End()
{
	ImGui::Render();
}

void ImGuiManager::Draw()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // デスクリプタヒープの配列をセットするコマンド
    ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // 描画コマンドを発行
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

}

