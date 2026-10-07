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

    // 銃口の発射炎から周囲へ当たる、一瞬の点光源を設定する。
    void SetMuzzleLight(const Vector3& position, const Vector3& color, float radius, float intensity);
    void ClearMuzzleLight(); // 発射していないフレームや別シーンへ光を残さない。

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

    // HLSLのb6と同じ32バイト配置で、全3Dモデルへ銃口の光を渡す。
    struct MuzzleLightParameters {
        Vector3 position{};
        float radius = 4.5f;
        Vector3 color{ 1.0f, 0.65f, 0.30f };
        float intensity = 0.0f;
    };
    static_assert(sizeof(MuzzleLightParameters) == 32);
    Microsoft::WRL::ComPtr<ID3D12Resource> muzzleLightResource_;
    MuzzleLightParameters* muzzleLightData_ = nullptr;

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
