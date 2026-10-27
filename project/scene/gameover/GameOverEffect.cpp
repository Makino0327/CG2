#include "GameOverEffect.h"
#include "../../engine/base/directX/DirectXCommon.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace {
    // 画面の外周を通る曲線を軸にし、中央の見出しとメニューを避ける。
    constexpr Vector2 kRoots[5][4] = {
        { {-100,100}, {110,300}, {-40,650}, {740,810} },
        { {-100,350}, {180,470}, {160,740}, {960,790} },
        { {820,-90}, {1030,70}, {1230,30}, {1380,360} },
        { {1160,-90}, {1080,100}, {1310,400}, {1370,800} },
        { {930,820}, {1220,730}, {1210,520}, {1380,360} }
    };
    constexpr float kPi = 3.14159265f;
}

float GameOverEffect::Random(float minimum, float maximum) {
    return std::uniform_real_distribution<float>(minimum, maximum)(randomEngine_);
}

void GameOverEffect::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    vertices_.reserve(kMaxVertices);
    tendrils_.reserve(120);
    // 太い筋は画面外へつなぎ、異なる位相で動く束を作る。
    for (int i = 0; i < 25; ++i) {
        Tendril tendril;
        const Vector2 offset{ Random(-42.0f,42.0f), Random(-38.0f,38.0f) };
        for (int control = 0; control < 4; ++control) {
            tendril.control[control] = {
                kRoots[i % 5][control].x + offset.x,
                kRoots[i % 5][control].y + offset.y
            };
        }
        tendril.width = Random(3.0f,10.0f);
        tendril.phase = Random(0.0f,2.0f*kPi);
        tendril.amplitude = Random(9.0f,27.0f);
        tendrils_.push_back(tendril);
    }
    // 親の筋から細い枝を伸ばし、単純な平行線ではない血管状の形にする。
    for (int parent = 0; parent < 25; ++parent) {
        for (int branch = 0; branch < 3; ++branch) {
            Tendril tendril;
            tendril.parent = parent;
            tendril.attachment = Random(0.18f,0.86f);
            const Vector2 origin = SampleTendril(static_cast<size_t>(parent), tendril.attachment);
            const Vector2 ahead = SampleTendril(static_cast<size_t>(parent), tendril.attachment+0.02f);
            const float angle = std::atan2(ahead.y-origin.y,ahead.x-origin.x)
                + (branch % 2 == 0 ? 1.0f : -1.0f) * Random(0.45f,1.30f);
            const float length = Random(90.0f,240.0f);
            const Vector2 end{ origin.x+std::cos(angle)*length, origin.y+std::sin(angle)*length };
            tendril.control[0] = origin;
            tendril.control[1] = { origin.x+(ahead.x-origin.x)*12.0f, origin.y+(ahead.y-origin.y)*12.0f };
            tendril.control[2] = { end.x-std::cos(angle+0.35f)*length*0.35f,
                end.y-std::sin(angle+0.35f)*length*0.35f };
            tendril.control[3] = end;
            tendril.width = Random(0.8f,2.8f);
            tendril.phase = Random(0.0f,2.0f*kPi);
            tendril.amplitude = Random(4.0f,13.0f);
            tendrils_.push_back(tendril);
        }
    }
    mist_.resize(100);
    for (auto& particle : mist_) { ResetParticle(particle, true); }

    // テクスチャを参照しない専用の描画設定。時刻と画面サイズだけを渡す。
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants.Num32BitValues = 4;
    parameter.Constants.ShaderRegister = 0;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root.NumParameters = 1;
    root.pParameters = &parameter;
    Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
    HRESULT hr = D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&error);
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0,signature->GetBufferPointer(),
        signature->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
    const auto vs = dxCommon_->CompileShader(L"Resources/shaders/GameOverEffect.VS.hlsl",L"vs_6_0");
    const auto ps = dxCommon_->CompileShader(L"Resources/shaders/GameOverEffect.PS.hlsl",L"ps_6_0");
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,32,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = rootSignature_.Get();
    pipeline.InputLayout = { layout,static_cast<UINT>(std::size(layout)) };
    pipeline.VS = { vs->GetBufferPointer(),vs->GetBufferSize() };
    pipeline.PS = { ps->GetBufferPointer(),ps->GetBufferSize() };
    pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    // 発光しすぎない通常アルファ合成で、煙と濡れた赤い筋を重ねる。
    auto& blend = pipeline.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pipeline.DepthStencilState.DepthEnable = FALSE;
    pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.SampleDesc.Count = 1;
    pipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipeline,IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(Vertex)*kMaxVertices);
    vertexResource_->Map(0,nullptr,reinterpret_cast<void**>(&mappedVertices_));
    vertexView_ = { vertexResource_->GetGPUVirtualAddress(),
        static_cast<UINT>(sizeof(Vertex)*kMaxVertices),static_cast<UINT>(sizeof(Vertex)) };
    BuildVertices();
}

Vector2 GameOverEffect::SampleTendril(size_t index, float progress) const {
    const auto& tendril = tendrils_[index];
    const float t = std::clamp(progress,0.0f,1.0f);
    const float s = 1.0f-t;
    Vector2 point{
        s*s*s*tendril.control[0].x+3.0f*s*s*t*tendril.control[1].x+
            3.0f*s*t*t*tendril.control[2].x+t*t*t*tendril.control[3].x,
        s*s*s*tendril.control[0].y+3.0f*s*s*t*tendril.control[1].y+
            3.0f*s*t*t*tendril.control[2].y+t*t*t*tendril.control[3].y
    };
    if (tendril.parent >= 0) {
        // 枝の始点は親の現在位置へ固定し、根元が離れないようにする。
        const Vector2 root = SampleTendril(static_cast<size_t>(tendril.parent),tendril.attachment);
        point.x += root.x-tendril.control[0].x;
        point.y += root.y-tendril.control[0].y;
    }
    const float envelope = std::sin(t*kPi);
    point.x += envelope*tendril.amplitude*std::sin(t*8.0f+time_*0.51f+tendril.phase);
    point.y += envelope*tendril.amplitude*0.65f*std::sin(t*11.0f-time_*0.37f+tendril.phase);
    return point;
}

void GameOverEffect::ResetParticle(MistParticle& particle, bool prewarm) {
    // 粒子ごとに寿命・流れ・大きさを持たせ、外周でゆっくり生まれ変わらせる。
    const int side = static_cast<int>(Random(0.0f,3.999f));
    particle.position = { Random(0.0f,1280.0f),Random(0.0f,720.0f) };
    if (side == 0) { particle.position.x = Random(-70.0f,150.0f); }
    if (side == 1) { particle.position.x = Random(1130.0f,1350.0f); }
    if (side == 2) { particle.position.y = Random(-80.0f,100.0f); }
    if (side == 3) { particle.position.y = Random(620.0f,800.0f); }
    particle.velocity = { Random(-9.0f,9.0f),Random(-11.0f,5.0f) };
    particle.life = Random(8.0f,17.0f);
    particle.age = prewarm ? Random(0.0f,particle.life) : 0.0f;
    particle.radius = Random(65.0f,170.0f);
    particle.phase = Random(0.0f,2.0f*kPi);
}

void GameOverEffect::Update(float seconds) {
    seconds = std::clamp(seconds,0.0f,0.1f);
    time_ += seconds;
    for (auto& particle : mist_) {
        particle.age += seconds;
        if (particle.age >= particle.life) { ResetParticle(particle, false); }
        particle.position.x += (particle.velocity.x+std::sin(time_*0.6f+particle.phase)*4.0f)*seconds;
        particle.position.y += (particle.velocity.y+std::cos(time_*0.4f+particle.phase)*3.0f)*seconds;
    }
    BuildVertices();
}

void GameOverEffect::AddQuad(const Vector2& center, float radius, const Vector4& color, float kind) {
    const Vertex a{ {center.x-radius,center.y-radius},{-1,-1},color,kind };
    const Vertex b{ {center.x+radius,center.y-radius},{1,-1},color,kind };
    const Vertex c{ {center.x-radius,center.y+radius},{-1,1},color,kind };
    const Vertex d{ {center.x+radius,center.y+radius},{1,1},color,kind };
    vertices_.insert(vertices_.end(),{a,b,c,b,d,c});
}

void GameOverEffect::BuildVertices() {
    vertices_.clear();
    // 奥の煙から描き、筋の輪郭が煙で隠れないようにする。
    for (const auto& particle : mist_) {
        const float life = particle.age/particle.life;
        const float fade = std::sin(life*kPi);
        AddQuad(particle.position,particle.radius*(0.75f+life*0.4f),
            {0.30f,0.008f,0.018f,fade*0.32f},1.0f);
    }
    // 曲線を帯状の三角形へ組み立て直し、画像ではなく形状そのものを動かす。
    constexpr int kSegments = 48;
    for (size_t index = 0; index < tendrils_.size(); ++index) {
        const auto& tendril = tendrils_[index];
        for (int segment = 0; segment < kSegments; ++segment) {
            const float t0 = static_cast<float>(segment)/kSegments;
            const float t1 = static_cast<float>(segment+1)/kSegments;
            const Vector2 a = SampleTendril(index,t0);
            const Vector2 b = SampleTendril(index,t1);
            const Vector2 tangentA = SampleTendril(index,t0+0.003f);
            const Vector2 tangentB = SampleTendril(index,t1-0.003f);
            const float lengthA = (std::max)(0.001f,std::hypot(tangentA.x-a.x,tangentA.y-a.y));
            const float lengthB = (std::max)(0.001f,std::hypot(b.x-tangentB.x,b.y-tangentB.y));
            const float pulse = 1.0f+0.09f*std::sin(time_*0.83f+tendril.phase);
            const float widthA = tendril.width*(1.0f-t0*0.82f)*pulse;
            const float widthB = tendril.width*(1.0f-t1*0.82f)*pulse;
            const Vector2 normalA{ -(tangentA.y-a.y)/lengthA*widthA,(tangentA.x-a.x)/lengthA*widthA };
            const Vector2 normalB{ -(b.y-tangentB.y)/lengthB*widthB,(b.x-tangentB.x)/lengthB*widthB };
            const float brightness = 0.68f+0.14f*std::sin(time_*0.55f-t0*14.0f+tendril.phase);
            const Vector4 color{ brightness,0.018f,0.025f,0.84f };
            const Vertex v0{ {a.x-normalA.x,a.y-normalA.y},{t0,-1},color,0 };
            const Vertex v1{ {a.x+normalA.x,a.y+normalA.y},{t0,1},color,0 };
            const Vertex v2{ {b.x-normalB.x,b.y-normalB.y},{t1,-1},color,0 };
            const Vertex v3{ {b.x+normalB.x,b.y+normalB.y},{t1,1},color,0 };
            vertices_.insert(vertices_.end(),{v0,v1,v2,v1,v3,v2});
        }
    }
    assert(vertices_.size() <= kMaxVertices);
    std::memcpy(mappedVertices_,vertices_.data(),vertices_.size()*sizeof(Vertex));
}

void GameOverEffect::Draw(float opacity) {
    auto* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0,1,&vertexView_);
    const float parameters[]{ time_,1280.0f,720.0f,std::clamp(opacity,0.0f,1.0f) };
    commandList->SetGraphicsRoot32BitConstants(0,4,parameters,0);
    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()),1,0,0);
}
