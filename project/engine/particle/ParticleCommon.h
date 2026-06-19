#pragma once
#include <d3d12.h>
#include "../base/DirectX/DirectXCommon.h"
#include "../base/srv/SrvManager.h"

// パーティクル描画で使用する合成方式
enum class ParticleBlendMode {
	Additive,
	Alpha,
};

class ParticleCommon
{
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void CommonDrawSetting(ParticleBlendMode blendMode = ParticleBlendMode::Additive);

	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	// Particle 初期化用 ComputeShader の設定を commandList に入れる
	void InitializeParticleComputeSetting();

	// 毎フレームのParticle発生用ComputeShaderを設定する
	void InitializeEmitParticleComputeSetting();

	// 毎フレームのParticle更新用ComputeShaderを設定する
	void InitializeUpdateParticleComputeSetting();

private:
	void CreateRootSignature();
	void CreateGraphicsPipelineState();
	// Particle 初期化用 ComputeShader の RootSignature を作る
	void CreateInitializeParticleComputeRootSignature();

	// Particle 初期化用 ComputeShader の PipelineState を作る
	void CreateInitializeParticleComputePipelineState();

	// Particle発生用ComputeShaderのRootSignatureを作る
	void CreateEmitParticleComputeRootSignature();

	// Particle発生用ComputeShaderのPipelineStateを作る
	void CreateEmitParticleComputePipelineState();

	// Particle更新用ComputeShaderのRootSignatureを作る
	void CreateUpdateParticleComputeRootSignature();

	// Particle更新用ComputeShaderのPipelineStateを作る
	void CreateUpdateParticleComputePipelineState();

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> alphaPipelineState_;
	
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeParticleComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initializeParticleComputePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> emitParticleComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticleComputePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> updateParticleComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticleComputePipelineState_;

};

