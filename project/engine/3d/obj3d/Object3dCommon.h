#pragma once
#include <d3d12.h>
#include "../../base/DirectX/DirectXCommon.h"
#include "../../../game/camera/Camera.h"
#include "../../base/srv/SrvManager.h"
#include "ShadowMap.h"

class Object3dCommon
{
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void CommonDrawSetting();

    // 通常画面の前に、人や壁の深度を光の視点から描く。
    void BeginShadowPass(const Vector3& focus);
    void EndShadowPass();
    Vector3 GetLightDirection() const { return shadowMap_.GetLightDirection(); }

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
    Camera* GetDefaultCamera() const { return defaultCamera_; }
    SrvManager* GetSrvManager() const { return srvManager_; }

    // ComputeShader 用の設定を commandList に入れる
    void SkinningComputeSetting();

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    // ComputeShader 用の RootSignature を作る
    void CreateSkinningComputeRootSignature();

    // ComputeShader 用の PipelineState を作る
    void CreateSkinningComputePipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    // 通常描画と同じモデルを、色を出さずに深度だけ描く。
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    ShadowMap shadowMap_;
    bool isShadowPass_ = false;
    Camera* defaultCamera_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;

};
