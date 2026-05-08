#include "Object3dCommon.h"
#include "../../2d/texture/TextureManager.h"

void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    // DirectXCommon を保存する
    dxCommon_ = dxCommon;

    // SrvManager を保存する
    srvManager_ = srvManager;

    CreateRootSignature();
    CreateGraphicsPipelineState();

    CreateSkinningComputeRootSignature();
    CreateSkinningComputePipelineState();

}


void Object3dCommon::CommonDrawSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ★ 共有SRVヒープをセット
    ID3D12DescriptorHeap* heaps[] = {
        TextureManager::GetInstance()->GetSrvManager()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, heaps);
}


void Object3dCommon::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // --- SRVテーブル（t0） ---
        // t0 : MatrixPalette 用
    D3D12_DESCRIPTOR_RANGE paletteDescriptorRange{};
    paletteDescriptorRange.BaseShaderRegister = 0;
    paletteDescriptorRange.NumDescriptors = 1;
    paletteDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    paletteDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t1 : モデルテクスチャ用
    D3D12_DESCRIPTOR_RANGE textureDescriptorRange{};
    textureDescriptorRange.BaseShaderRegister = 1;
    textureDescriptorRange.NumDescriptors = 1;
    textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t2 : 環境マップ用
    D3D12_DESCRIPTOR_RANGE environmentDescriptorRange{};
    environmentDescriptorRange.BaseShaderRegister = 2;
    environmentDescriptorRange.NumDescriptors = 1;
    environmentDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


    // --- RootParameter ---
    D3D12_ROOT_PARAMETER rootParameters[7]{};


    // b0 : Material
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // b1 : Light
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    // b2 : Transform (VS)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[2].Descriptor.ShaderRegister = 2;

    // b3 : Camera
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 3;

    // t0 : Texture
        // t0 : MatrixPalette (VS)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &paletteDescriptorRange;

    // t1 : Texture (PS)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;

    // t2 : EnvironmentTexture (PS)
    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[6].DescriptorTable.pDescriptorRanges = &environmentDescriptorRange;


    // --- Sampler ---
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // --- RootSignature 記述 ---
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}


void Object3dCommon::CreateGraphicsPipelineState()
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
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    // --- ラスタライザ ---
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // ←3D は裏面カリング ON
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // --- シェーダをロード ---
        // 通常描画用 shader
    auto vs = dxCommon_->CompileShader(L"Resources/shaders/Object3D.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"Resources/shaders/Object3D.PS.hlsl", L"ps_6_0");

    // --- PSO 設定 ---
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
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
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.DepthStencilState = depthDesc;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;  // = 0xFFFFFFFF

    desc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));

}

// ComputeShader 用の設定を commandList に入れる
void Object3dCommon::SkinningComputeSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    commandList->SetComputeRootSignature(skinningComputeRootSignature_.Get());
    commandList->SetPipelineState(skinningComputePipelineState_.Get());

    // SRV / UAV を使うので DescriptorHeap を設定する
    ID3D12DescriptorHeap* heaps[] = {
        TextureManager::GetInstance()->GetSrvManager()->GetDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(1, heaps);
}

// ComputeShader 用の RootSignature を作る
void Object3dCommon::CreateSkinningComputeRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // t0 : MatrixPalette
    D3D12_DESCRIPTOR_RANGE matrixPaletteRange{};
    matrixPaletteRange.BaseShaderRegister = 0;
    matrixPaletteRange.NumDescriptors = 1;
    matrixPaletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    matrixPaletteRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t1 : InputVertices
    D3D12_DESCRIPTOR_RANGE inputVertexRange{};
    inputVertexRange.BaseShaderRegister = 1;
    inputVertexRange.NumDescriptors = 1;
    inputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    inputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // t2 : Influences
    D3D12_DESCRIPTOR_RANGE influenceRange{};
    influenceRange.BaseShaderRegister = 2;
    influenceRange.NumDescriptors = 1;
    influenceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    influenceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // u0 : OutputVertices
    D3D12_DESCRIPTOR_RANGE outputVertexRange{};
    outputVertexRange.BaseShaderRegister = 0;
    outputVertexRange.NumDescriptors = 1;
    outputVertexRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputVertexRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[5]{};

    // t0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &matrixPaletteRange;

    // t1
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &inputVertexRange;

    // t2
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &influenceRange;

    // u0
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &outputVertexRange;

    // b0
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].Descriptor.ShaderRegister = 0;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = nullptr;
    desc.NumStaticSamplers = 0;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(skinningComputeRootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

// ComputeShader 用の PipelineState を作る
void Object3dCommon::CreateSkinningComputePipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // ComputeShader をコンパイルする
    auto cs = dxCommon_->CompileShader(
        L"Resources/shaders/Skinning.CS.hlsl",
        L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = skinningComputeRootSignature_.Get();
    desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    HRESULT hr = device->CreateComputePipelineState(
        &desc,
        IID_PPV_ARGS(skinningComputePipelineState_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}
