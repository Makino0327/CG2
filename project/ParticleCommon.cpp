#include "ParticleCommon.h"

void ParticleCommon::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreateGraphicsPipelineState();
}

void ParticleCommon::CommonDrawSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_);
    commandList->SetPipelineState(pipelineState_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ★ 共有SRVヒープをセット
    ID3D12DescriptorHeap* heaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
}


void ParticleCommon::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_RANGE ranges[2]{};

    // t0 = Instancing StructuredBuffer
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].NumDescriptors = 1;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t1 = Texture2D
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[1].BaseShaderRegister = 1;
    ranges[1].NumDescriptors = 1;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[2]{};

    // param0 → t0 StructuredBuffer
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
    params[0].DescriptorTable.NumDescriptorRanges = 1;

    // param1 → t1 Texture2D
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
    params[1].DescriptorTable.NumDescriptorRanges = 1;

    // Sampler（s0）
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = params;
    desc.NumParameters = _countof(params);
    desc.pStaticSamplers = &sampler;
    desc.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);

    if (FAILED(hr)) {
        OutputDebugStringA((char*)err->GetBufferPointer());
        assert(false);
    }

    hr = device->CreateRootSignature(
        0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}



void ParticleCommon::CreateGraphicsPipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // --- インプットレイアウト（3Dモデル用） ---
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        // POSITION
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

          // TEXCOORD
          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

            // NORMAL
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    D3D12_INPUT_LAYOUT_DESC inputLayout{};
    inputLayout.pInputElementDescs = inputElementDescs;
    inputLayout.NumElements = _countof(inputElementDescs);

    // --- ブレンド ---
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    // --- ラスタライザ ---
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // ←3D は裏面カリング ON
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // --- シェーダをロード ---
    auto vs = dxCommon_->CompileShader(L"Particle.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"Particle.PS.hlsl", L"ps_6_0");

    // --- PSO 設定 ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_;
    desc.InputLayout = inputLayout;
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

    // --- Depth ---
    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = true;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DepthStencilState = depthDesc;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;  // = 0xFFFFFFFF

    desc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}
