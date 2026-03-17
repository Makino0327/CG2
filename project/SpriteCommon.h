#pragma once
#include <d3d12.h>
#include "DirectXCommon.h"
#include "SrvManager.h"

class SpriteCommon
{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// 共通描画設定
	void CommonDrawSetting();


	// Getter
	DirectXCommon* GetDxCommon()const { return dxCommon_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィクスパイプラインの作成
	void CreateGraphicsPipelineState();

public:
	DirectXCommon* dxCommon_;

private:
	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// グラフィクスパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	SrvManager* srvManager_ = nullptr;

};

