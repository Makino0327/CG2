#include "Line3DCommon.h"
#include <cassert>

void Line3DCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    assert(dxCommon);
    assert(srvManager);

    // 参照を保存する
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // ルートシグネチャを作る
    CreateRootSignature();

    // パイプラインを作る
    CreateGraphicsPipelineState();
}

void Line3DCommon::CommonDrawSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // ルートシグネチャを設定する
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    // パイプラインステートを設定する
    commandList->SetPipelineState(pipelineState_.Get());

    // ラインリストとして描画する
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

void Line3DCommon::CreateRootSignature()
{
    HRESULT hr;

    D3D12_ROOT_PARAMETER rootParameters[1] = {};

    // WVP用の定数バッファ
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumStaticSamplers = 0;
    descriptionRootSignature.pStaticSamplers = nullptr;
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

    hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    assert(SUCCEEDED(hr));

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void Line3DCommon::CreateGraphicsPipelineState()
{
    HRESULT hr;

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;

    vertexShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Line3D.VS.hlsl",
        L"vs_6_0");

    pixelShaderBlob = dxCommon_->CompileShader(
        L"Resources/shaders/Line3D.PS.hlsl",
        L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};

    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "COLOR";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};

    // Skeleton デバッグ線は常に見えるように深度テストを無効にする
    depthStencilDesc.DepthEnable = false;

    // 深度を書き込まない
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    // 比較関数は使われないが無難な値を入れておく
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;


    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    graphicsPipelineStateDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // // DepthBufferの形式とPSOのDSV形式を合わせる
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &graphicsPipelineStateDesc,
        IID_PPV_ARGS(pipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}
