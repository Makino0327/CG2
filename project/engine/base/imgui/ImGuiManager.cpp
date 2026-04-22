#include "ImGuiManager.h"

#ifdef USE_IMGUI
namespace {

void AllocateImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    // Allocate SRV descriptors for ImGui font and user textures from the engine SRV manager.
    SrvManager* srvManager = static_cast<SrvManager*>(info->UserData);
    const uint32_t srvIndex = srvManager->Allocate();
    *outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
    *outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
}

void FreeImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo*,
    D3D12_CPU_DESCRIPTOR_HANDLE,
    D3D12_GPU_DESCRIPTOR_HANDLE)
{
    // SrvManager currently has no free-list, so ImGui descriptor release is a no-op.
}

} // namespace
#endif

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

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "../generated/imgui_layout.ini"; // Save ImGui layout outside project/imgui.ini.
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking for clean ImGui window layout.

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(winApp->GetHwnd());

    // Use Dear ImGui 1.92 DX12 InitInfo API.
    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon->GetDevice();
    initInfo.CommandQueue = dxCommon->GetCommandQueue();
    initInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetSwapChainResourcesNum());
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();
    initInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
    initInfo.UserData = srvManager;
    ImGui_ImplDX12_Init(&initInfo);
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
    // Start a new ImGui frame.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create a transparent central dockspace so the game screen remains visible.
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
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

    // Bind the SRV heap used by ImGui before issuing ImGui draw commands.
    ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_ };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Submit ImGui draw commands.
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}
