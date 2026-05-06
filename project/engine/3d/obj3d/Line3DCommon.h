#pragma once
#include "../../base/DirectX/DirectXCommon.h"
#include "../../base/srv/SrvManager.h"

class Line3DCommon {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 線描画用の共通設定を行う
    void CommonDrawSetting();

    // DirectX共通を返す
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    // DirectX共通
    DirectXCommon* dxCommon_ = nullptr;

    // SRV管理
    SrvManager* srvManager_ = nullptr;

    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    // パイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
