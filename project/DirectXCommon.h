#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class DirectXCommon
{
public:
	// 初期化
	void Initialize();
	// デバイス初期化
	void InitializeDevice();

private:
	// DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	// DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
};

