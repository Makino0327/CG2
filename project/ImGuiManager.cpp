#include "ImGuiManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"


void ImGuiManager::Initialize(
    WinApp* winApp,
    DirectXCommon* dxCommon,
    SrvManager* srvManager)
{
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
