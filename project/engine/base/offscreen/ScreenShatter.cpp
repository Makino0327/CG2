#include "ScreenShatter.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <random>

namespace {
    // 光るヒビの溜めを見せてから割り、破片がスローモーションで漂う時間を取る。
    constexpr float kDuration = ScreenShatter::kBulletFlightTime + ScreenShatter::kBreakDelay + 1.5f;
    constexpr float kPi = 3.14159265f;
    // 大きな破片に加え、着弾時の火花と小片の頂点も確保する。
    constexpr size_t kMaxVertices = 2048;

    // コピーと描画の切り替えでは、実際のリソース状態を必ず元へ戻す。
    void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &barrier);
    }
}

void ScreenShatter::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    snapshot_ = std::make_unique<RenderTexture>();
    snapshot_->Initialize(dxCommon_, srvManager_, WinApp::kClientWidth, WinApp::kClientHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, { 0, 0, 0, 1 });
    vertices_ = dxCommon_->CreateBufferResource(sizeof(Vertex) * kMaxVertices);
    vertices_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    vertexView_.BufferLocation = vertices_->GetGPUVirtualAddress();
    vertexView_.SizeInBytes = static_cast<UINT>(sizeof(Vertex) * kMaxVertices);
    vertexView_.StrideInBytes = sizeof(Vertex);
    // 定数バッファは256バイト単位で確保する。
    parameters_ = dxCommon_->CreateBufferResource((sizeof(Parameters)+255) & ~static_cast<size_t>(255));
    parameters_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
    CreatePipeline();
}

void ScreenShatter::CreatePipeline() {
    // 保存したタイトル画像と、着弾位置・経過時間だけをシェーダーへ渡す。
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER roots[2]{};
    roots[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    roots[0].DescriptorTable = { 1, &range };
    roots[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    roots[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    roots[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    signature.NumParameters = 2;
    signature.pParameters = roots;
    signature.NumStaticSamplers = 1;
    signature.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> blob, error;
    HRESULT hr = D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, barycentric), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, appearance), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    auto vs = dxCommon_->CompileShader(L"Resources/shaders/ScreenShatter.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"Resources/shaders/ScreenShatter.PS.hlsl", L"ps_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = { layout, _countof(layout) };
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    // 両面を描画し、回転した破片の裏側も消えないようにする。
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.SampleDesc.Count = 1;
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pipeline_.GetAddressOf()));
    assert(SUCCEEDED(hr));
}

void ScreenShatter::Start(const Vector2& impactUV) {
    if (playing_) { return; }
    // 端に寄りすぎると細長い破片ばかりになるため、着弾点を画面内へ収める。
    impactUV_ = { std::clamp(impactUV.x, 0.15f, 0.85f), std::clamp(impactUV.y, 0.15f, 0.85f) };
    time_ = 0.0f;
    playing_ = capturePending_ = true;
    CreateShards();
    UpdateVertices();
}

void ScreenShatter::CreateShards() {
    shards_.clear();
    std::mt19937 random(7319);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float aspect = static_cast<float>(WinApp::kClientWidth) / WinApp::kClientHeight;
    // 放射状の線に四隅へ向かう線も加え、画面全体を隙間なく分割する。
    // 四隅の線は外周まで必ず残し、破片が画面の角を覆えるようにする。
    std::vector<std::pair<float, bool>> angles;
    for (int i = 0; i < 24; ++i) {
        angles.push_back({ -kPi + (static_cast<float>(i) + unit(random) * 0.5f) * 2.0f * kPi / 24.0f, false });
    }
    for (Vector2 corner : { Vector2{0,0}, Vector2{1,0}, Vector2{1,1}, Vector2{0,1} }) {
        angles.push_back({ std::atan2(corner.y - impactUV_.y, (corner.x - impactUV_.x) * aspect), true });
    }
    std::sort(angles.begin(), angles.end());
    // 着弾点の近くほどリングを詰め、中心は細かく砕け、外側は大きな破片になるようにする。
    constexpr float radii[] = { 0.02f, 0.05f, 0.10f, 0.18f, 0.30f, 0.48f, 0.72f, 1.0f };
    constexpr size_t kRings = std::size(radii);
    const size_t count = angles.size();
    std::vector<std::array<Vector2, kRings>> rings(count);
    // 外側のリングでは放射状のヒビを途中で止め、隣の破片とつなげて大きな破片にする。
    std::vector<std::array<bool, kRings>> active(count);
    for (size_t i = 0; i < count; ++i) {
        const Vector2 ray{ std::cos(angles[i].first) / aspect, std::sin(angles[i].first) };
        const float tx = std::abs(ray.x) < 0.00001f ? 10000.0f :
            (ray.x > 0 ? 1.0f - impactUV_.x : -impactUV_.x) / ray.x;
        const float ty = std::abs(ray.y) < 0.00001f ? 10000.0f :
            (ray.y > 0 ? 1.0f - impactUV_.y : -impactUV_.y) / ray.y;
        const float boundary = std::min(tx, ty);
        for (size_t ring = 0; ring < kRings; ++ring) {
            const float radius = radii[ring] * (ring == kRings-1 ? 1.0f : 0.85f + unit(random) * 0.30f);
            rings[i][ring] = { impactUV_.x + ray.x * boundary * radius, impactUV_.y + ray.y * boundary * radius };
            const bool inner = ring == 0 || active[i][ring-1];
            const float dropChance = ring < 4 ? 0.0f : 0.25f + 0.12f * static_cast<float>(ring - 4);
            active[i][ring] = angles[i].second || (inner && unit(random) >= dropChance);
        }
    }
    auto AddShard = [&](std::vector<Vector2> points) {
        Shard shard{};
        // 分割の向きをランダムにし、同じ向きの対角線が並んで格子に見えるのを防ぐ。
        if (points.size() > 3 && unit(random) < 0.5f) {
            std::rotate(points.begin(), points.end()-1, points.end());
        }
        Vector2 center{};
        for (const Vector2& point : points) { center.x += point.x; center.y += point.y; }
        center.x /= static_cast<float>(points.size());
        center.y /= static_cast<float>(points.size());
        float size = 0.0f;
        for (const Vector2& point : points) {
            size = std::max(size, std::hypot((point.x-center.x)*aspect, point.y-center.y)*2.0f);
        }
        shard.uv = std::move(points);
        shard.center = center;
        const float dx = (center.x-impactUV_.x)*aspect;
        const float dy = impactUV_.y-center.y;
        const float distance = std::hypot(dx, dy);
        // 弾け飛ばさず、割れ目が開くように着弾点から少しずつ離れていく。近いほどわずかに大きく動く。
        const float closeness = std::exp(-distance*6.0f);
        const float direction = std::atan2(dy, dx)+(unit(random)-0.5f)*0.5f;
        // 外側の破片ほど画面の外へ向かって大きく広がるよう、距離に応じて速さを足す。
        const float speed = (0.4f+0.55f*closeness+0.7f*distance)*(0.6f+unit(random)*0.8f);
        // 重力はかけず、スローモーションのようにゆっくり奥へ沈みながら止まっていく。
        shard.velocity = { std::cos(direction)*speed, std::sin(direction)*speed,
            0.4f+1.0f*closeness+unit(random)*0.3f };
        // 回転も控えめにし、破片が少し傾く程度にとどめる。
        const float spinScale = std::clamp(0.35f/std::max(size, 0.001f), 0.3f, 1.0f)*(1.0f+2.0f*closeness);
        shard.spin = { (unit(random)-0.5f)*spinScale, (unit(random)-0.5f)*spinScale, (unit(random)-0.5f)*spinScale*0.8f };
        // 中心から外へ一気に弾けるよう、割れ始めの時間差は短めにする。
        shard.delay = kBulletFlightTime + kBreakDelay + distance*0.12f + (distance > 0.1f ? unit(random)*0.04f : 0.0f);
        shards_.push_back(std::move(shard));
    };
    size_t triangles = 0;
    for (size_t i = 0; i < count; ++i) {
        AddShard({ impactUV_, rings[i][0], rings[(i+1)%count][0] });
        ++triangles;
    }
    for (size_t ring = 1; ring < kRings; ++ring) {
        for (size_t a = 0; a < count; ++a) {
            if (!active[a][ring]) { continue; }
            size_t b = (a+1)%count;
            while (!active[b][ring]) { b = (b+1)%count; }
            // 外側の2点と、その間にある内側リングの点を順に結んだ多角形を1枚の破片にする。
            std::vector<Vector2> points{ rings[a][ring] };
            for (size_t k = a;; k = (k+1)%count) {
                if (active[k][ring-1]) { points.push_back(rings[k][ring-1]); }
                if (k == b) { break; }
            }
            points.push_back(rings[b][ring]);
            triangles += points.size()-2;
            AddShard(std::move(points));
        }
    }
    // 火花は短く鋭く、小片は少し遅く大きく飛ばして、着弾を二段階で見せる。
    impactParticles_.clear();
    for (int i = 0; i < 36; ++i) {
        const bool spark = i < 22;
        const float angle = unit(random)*2.0f*kPi;
        const float speed = spark ? 1.6f+unit(random)*3.4f : 0.6f+unit(random)*1.7f;
        impactParticles_.push_back({
            { std::cos(angle)*speed, std::sin(angle)*speed },
            spark ? 0.10f+unit(random)*0.12f : 0.22f+unit(random)*0.16f,
            spark ? 0.002f+unit(random)*0.003f : 0.008f+unit(random)*0.012f,
            unit(random)*2.0f*kPi, spark });
    }
    // 最後の6頂点は、着弾点へ飛んでいく弾の曳光に使う。
    assert(triangles*3+impactParticles_.size()*6+6 <= kMaxVertices);
    (void)triangles;
}

void ScreenShatter::Update(float deltaTime) {
    if (!playing_ || capturePending_) { return; }
    time_ += std::max(deltaTime, 0.0f);
    if (time_ >= kDuration) { playing_ = false; return; }
    UpdateVertices();
}

void ScreenShatter::UpdateVertices() {
    const float aspect = static_cast<float>(WinApp::kClientWidth) / WinApp::kClientHeight;
    // ヒビや閃光は弾が届いた時刻から数える。届く前は負の値になり、シェーダーは元の画面をそのまま出す。
    const float impactTime = time_-kBulletFlightTime;
    *parameterData_ = { impactUV_, impactTime, aspect, kBreakDelay, {} };
    // 遠い破片から描いて、近い破片が正しく手前に重なるようにする。
    // 速度は時間とともに減り、破片は空中で静止していく。経過時間を「進んだ量」に変換する。
    // 減速をゆるめにして、演出の最後まで少しずつ動き続けるスローモーションに見せる。
    constexpr float kDrag = 1.2f;
    auto Travel = [&](const Shard& shard) {
        const float t = std::max(0.0f, time_-shard.delay);
        return (1.0f-std::exp(-kDrag*t))/kDrag;
    };
    std::sort(shards_.begin(), shards_.end(), [&](const Shard& a, const Shard& b) {
        return a.velocity.z*Travel(a) > b.velocity.z*Travel(b);
    });
    // 撃った反動でカメラが跳ね上がるため、画面は一瞬下へ沈んで小さく戻る。
    const float kickAge = time_/0.025f;
    const float kick = kickAge*std::exp(1.0f-kickAge);
    const float reboundAge = (time_-0.085f)/0.024f;
    const float rebound = std::exp(-reboundAge*reboundAge);
    const float shakeX = 0.012f*kick-0.004f*rebound;
    const float shakeY = -0.052f*kick+0.014f*rebound;
    // 着弾直後の拡大で揺れの隙間を隠し、溜めの間はじわじわ寄って緊張感を出す。
    // 割れる瞬間には画面を前へ打ち出すように一度だけ大きく拡大する。
    // 弾が当たった瞬間にも小さく打ち込む。
    auto Punch = [](float age) { return age > 0.0f ? age*std::exp(1.0f-age) : 0.0f; };
    const float punch = Punch((impactTime-kBreakDelay)/0.045f);
    const float hitPunch = Punch(impactTime/0.03f);
    const float zoom = 1.0f+0.07f*std::exp(-time_*13.0f)+0.03f*hitPunch
        +0.03f*std::clamp(impactTime/kBreakDelay,0.0f,1.0f)+0.08f*punch;
    // 演出の終わりに残った破片を消し、本編の読み込み前の黒画面へつなぐ。
    const float fadeOut = std::clamp((kDuration-time_)/0.3f,0.0f,1.0f);
    size_t index = 0;
    for (const Shard& shard : shards_) {
        // 移動も回転も同じ減速で止まり、飛び散った形のまま宙に浮いて見える。
        const float t = Travel(shard);
        const float cx = std::cos(shard.spin.x*t), sx = std::sin(shard.spin.x*t);
        const float cy = std::cos(shard.spin.y*t), sy = std::sin(shard.spin.y*t);
        const float cz = std::cos(shard.spin.z*t), sz = std::sin(shard.spin.z*t);
        // 弾の抜ける奥へ遠ざかりながら、外へ広がって止まる。
        const float distance = 1.0f+shard.velocity.z*t;
        const float alpha = fadeOut*std::clamp(4.0f-distance,0.0f,1.0f);
        const float appearance = 0.58f+0.42f*std::abs(cx*cy);
        auto Project = [&](const Vector2& uv) {
            const float x = (uv.x-shard.center.x)*2.0f*aspect;
            const float y = (shard.center.y-uv.y)*2.0f;
            const float rx = x*cy+y*sx*sy;
            const float ry = y*cx;
            const float rz = -x*sy+y*sx*cy;
            const float worldX = (shard.center.x-0.5f)*2.0f*aspect+shard.velocity.x*t+rx*cz-ry*sz;
            const float worldY = (0.5f-shard.center.y)*2.0f+shard.velocity.y*t+rx*sz+ry*cz;
            const float w = std::max(0.5f,distance+rz);
            return Vector4{ worldX/aspect*zoom+shakeX*w, worldY*zoom+shakeY*w, 0.5f*w, w };
        };
        // 多角形を先頭の頂点から扇状に分割し、内部の分割線にはヒビを描かない。
        // 内部の辺に向かい合う成分を1で固定すると、その辺では0にならず線が出ない。
        const size_t last = shard.uv.size()-1;
        for (size_t k = 1; k < last; ++k) {
            const size_t corners[] = { 0, k, k+1 };
            const float innerK = k > 1 ? 1.0f : 0.0f;
            const float innerNext = k+1 < last ? 1.0f : 0.0f;
            const Vector3 barycentric[] = {
                { 1, innerNext, innerK },
                { 0, 1, innerK },
                { 0, innerNext, 1 },
            };
            for (size_t v = 0; v < 3; ++v) {
                Vertex& vertex = vertexData_[index++];
                vertex.position = Project(shard.uv[corners[v]]);
                vertex.uv = shard.uv[corners[v]];
                vertex.barycentric = barycentric[v];
                vertex.appearance = { appearance, alpha };
            }
        }
    }
    // 火花と小片はガラスより後に描き、割れる前から着弾点の手前へ飛ばす。
    const Vector2 corners[] = { {-1,-1}, {-1,1}, {1,-1}, {1,-1}, {-1,1}, {1,1} };
    for (const ImpactParticle& particle : impactParticles_) {
        const float age = std::max(0.0f, impactTime-0.012f);
        if (impactTime < 0.012f || age >= particle.lifetime) { continue; }
        const float progress = age/particle.lifetime;
        const float alpha = (1.0f-progress)*(1.0f-progress);
        const float angle = particle.spark ? std::atan2(particle.velocity.y,particle.velocity.x)
            : particle.rotation+age*13.0f;
        const float c = std::cos(angle), s = std::sin(angle);
        const float halfLength = particle.spark ? 0.018f+0.045f*(1.0f-progress)
            : particle.size*(1.0f+age*3.0f);
        const float halfWidth = particle.size*(particle.spark ? 1.0f : 1.0f+age*3.0f);
        const float centerX = (impactUV_.x-0.5f)*2.0f*aspect+particle.velocity.x*age;
        const float centerY = (0.5f-impactUV_.y)*2.0f+particle.velocity.y*age-age*age*1.4f;
        for (const Vector2& corner : corners) {
            const float x = corner.x*halfLength, y = corner.y*halfWidth;
            Vertex& vertex = vertexData_[index++];
            vertex.position = { (centerX+x*c-y*s)/aspect*zoom+shakeX,
                (centerY+x*s+y*c)*zoom+shakeY, 0.5f, 1.0f };
            vertex.uv = { (corner.x+1.0f)*0.5f, (corner.y+1.0f)*0.5f };
            vertex.barycentric = { 0, 0, 0 };
            // 負の値は着弾粒子の印。通常の破片の陰影とは区別する。
            vertex.appearance = { particle.spark ? -1.0f : -2.0f, alpha };
        }
    }
    // 撃った弾を、画面右下の手前から着弾点へ遠ざかりながら飛ぶ曳光として描く。
    // 先端が着弾点に届いた後は、尾が追いついて着弾点へ吸い込まれるように消える。
    constexpr float kBulletTailDelay = 0.05f;
    const float headProgress = std::clamp(time_/kBulletFlightTime, 0.0f, 1.0f);
    const float tailProgress = std::clamp((time_-kBulletTailDelay)/kBulletFlightTime, 0.0f, 1.0f);
    if (tailProgress < 0.999f) {
        // 手前の開始位置は、画面の右下の角のすぐ外に見える位置にする。
        const Vector2 startWorld{ 0.30f*aspect, -0.32f };
        const Vector2 impactWorld{ (impactUV_.x-0.5f)*2.0f*aspect, (0.5f-impactUV_.y)*2.0f };
        constexpr float kStartDepth = 0.3f;
        // 奥行きごと補間して透視投影し、遠ざかるほど小さく速度も落ちて見えるようにする。
        auto BulletScreen = [&](float progress, float& depth) {
            depth = kStartDepth+(1.0f-kStartDepth)*progress;
            return Vector2{ (startWorld.x+(impactWorld.x-startWorld.x)*progress)/depth*zoom,
                (startWorld.y+(impactWorld.y-startWorld.y)*progress)/depth*zoom };
        };
        float headDepth = 0.0f, tailDepth = 0.0f;
        const Vector2 head = BulletScreen(headProgress, headDepth);
        const Vector2 tail = BulletScreen(tailProgress, tailDepth);
        const float length = std::max(std::hypot(head.x-tail.x, head.y-tail.y), 0.0001f);
        const Vector2 direction{ (head.x-tail.x)/length, (head.y-tail.y)/length };
        const Vector2 normal{ -direction.y, direction.x };
        // 先端を平らな太い辺にすると、尾の方へ向かう矢印に見えて弾が逆向きに見える。
        // 先端を尖らせ、そのすぐ後ろを一番太くして、尾へ細く伸ばす。
        const float bodyWidth = 0.035f/headDepth;
        const float tipLength = std::min(bodyWidth*2.5f, length*0.4f);
        const Vector2 body{ head.x-direction.x*tipLength, head.y-direction.y*tipLength };
        const float bodyAlong = 1.0f-tipLength/length;
        const Vector2 points[] = {
            tail,
            { body.x+normal.x*bodyWidth, body.y+normal.y*bodyWidth },
            { body.x-normal.x*bodyWidth, body.y-normal.y*bodyWidth },
            head,
        };
        const Vector2 uvs[] = { {0,0.5f}, {bodyAlong,1}, {bodyAlong,0}, {1,0.5f} };
        for (size_t corner : { 0, 1, 2, 1, 3, 2 }) {
            Vertex& vertex = vertexData_[index++];
            vertex.position = { points[corner].x/aspect+shakeX, points[corner].y+shakeY, 0.5f, 1.0f };
            vertex.uv = uvs[corner];
            vertex.barycentric = { 0, 0, 0 };
            // -4は弾の曳光の印。
            vertex.appearance = { -4.0f, 1.0f };
        }
    }
    vertexCount_ = static_cast<UINT>(index);
}

void ScreenShatter::Capture(ID3D12Resource* source) {
    if (!capturePending_) { return; }
    // スタートを押したフレームのタイトルを一度だけ保存する。
    auto* list = dxCommon_->GetCommandList();
    Transition(list, source, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(list, snapshot_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(snapshot_->GetResource(), source);
    Transition(list, source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    Transition(list, snapshot_->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    capturePending_ = false;
}

void ScreenShatter::Draw() {
    if (!playing_ || capturePending_) { return; }
    auto* list = dxCommon_->GetCommandList();
    Transition(list, snapshot_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // 破片の後ろは黒にする。本編の読み込みは、この演出が終わってから行う。
    const auto rtv = dxCommon_->GetCurrentBackBufferRTVHandle();
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    list->ClearRenderTargetView(rtv, black, 0, nullptr);
    list->SetGraphicsRootSignature(rootSignature_.Get());
    list->SetPipelineState(pipeline_.Get());
    srvManager_->SetGraphicsRootDescriptorTable(0, snapshot_->GetSRVIndex());
    list->SetGraphicsRootConstantBufferView(1, parameters_->GetGPUVirtualAddress());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->IASetVertexBuffers(0, 1, &vertexView_);
    list->DrawInstanced(vertexCount_, 1, 0, 0);
    Transition(list, snapshot_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
}
