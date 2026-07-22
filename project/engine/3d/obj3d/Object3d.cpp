#include "Object3d.h"
#include "Object3dCommon.h"
#include "../model/Model.h"
#include "../model/ModelManager.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

namespace {
float Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float WrapAnimationTime(float time, float duration)
{
    if (duration <= 0.0f) {
        return 0.0f;
    }

    float wrappedTime = std::fmod(time, duration);
    if (wrappedTime < 0.0f) {
        wrappedTime += duration;
    }
    return wrappedTime;
}

QuaternionTransform SampleJointTransform(const Joint& joint, const Animation& animation, float animationTime)
{
    QuaternionTransform transform = joint.transform;

    auto it = animation.nodeAnimations.find(joint.name);
    if (it == animation.nodeAnimations.end()) {
        return transform;
    }

    const NodeAnimation& nodeAnimation = it->second;

    if (!nodeAnimation.translate.keyframes.empty()) {
        transform.translate = CalculateValue(nodeAnimation.translate.keyframes, animationTime);
    }
    if (!nodeAnimation.rotate.keyframes.empty()) {
        transform.rotate = CalculateValue(nodeAnimation.rotate.keyframes, animationTime);
    }
    if (!nodeAnimation.scale.keyframes.empty()) {
        transform.scale = CalculateValue(nodeAnimation.scale.keyframes, animationTime);
    }

    return transform;
}
}
// Shared light intensity for all Object3d instances.
float Object3d::lightIntensity_ = 0.5f;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    object3dCommon_ = object3dCommon;

    camera_ = object3dCommon_->GetDefaultCamera();

    transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 3.0f, -10.0f} };

    InitializeTransformationMatrix();
    InitializeDirectionalLight();
    InitializeCameraForGPU();
    InitializeMaterial();
    SetEnvironmentTexture(environmentTextureFilePath_);
    InitializeSkinningInformation();
}


void Object3d::SetWorldMatrix(const Matrix4x4& worldMatrix)
{
    // 豁ｦ蝎ｨ縺ｪ縺ｩ繧谷oint縺ｫ隕ｪ蟄蝉ｻ倥￠縺吶ｋ縺ｨ縺阪∬ｨ育ｮ玲ｸ医∩縺ｮWorld陦悟・繧剃ｽｿ縺・
    customWorldMatrix_ = worldMatrix;
    useCustomWorldMatrix_ = true;
}
void Object3d::ApplyAnimationPose(float animationTime)
{
    if (animation_.duration <= 0.0f || animation_.nodeAnimations.empty()) {
        return;
    }

    // ImGui縺ｮ陬憺俣遒ｺ隱咲畑縺ｫ縲∵欠螳壹＠縺滓凾蛻ｻ縺ｮ繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蟋ｿ蜍｢繧貞叉蠎ｧ縺ｫ蜿肴丐縺吶ｋ
    if (animationTime < 0.0f) {
        animationTime = 0.0f;
    }
    animationTime_ = std::fmod(animationTime, animation_.duration);

    if (hasSkeleton_) {
        ApplyAnimation(skeleton_, animation_, animationTime_);
        UpdateSkeleton(skeleton_);
    }
}
void Object3d::ApplyAnimationBlendPose(const Animation& fromAnimation, float fromTime, const Animation& toAnimation, float toTime, float blendRate)
{
    if (!hasSkeleton_) {
        return;
    }
    if (fromAnimation.nodeAnimations.empty() || toAnimation.nodeAnimations.empty()) {
        return;
    }

    const float t = Clamp01(blendRate);
    const float fromWrappedTime = WrapAnimationTime(fromTime, fromAnimation.duration);
    const float toWrappedTime = WrapAnimationTime(toTime, toAnimation.duration);

    // 2つのアニメーション姿勢をJointごとに混ぜて、切り替え時の急なジャンプを防ぐ
    for (Joint& joint : skeleton_.joints) {
        QuaternionTransform fromTransform = SampleJointTransform(joint, fromAnimation, fromWrappedTime);
        QuaternionTransform toTransform = SampleJointTransform(joint, toAnimation, toWrappedTime);

        joint.transform.translate = Lerp(fromTransform.translate, toTransform.translate, t);
        joint.transform.rotate = Slerp(fromTransform.rotate, toTransform.rotate, t);
        joint.transform.scale = Lerp(fromTransform.scale, toTransform.scale, t);
    }

    animationTime_ = toWrappedTime;
    UpdateSkeleton(skeleton_);
}
void Object3d::Update()
{
    assert(transformationMatrixData_);

    Quaternion animationRotate = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasAnimationRotate = false;

    if (isAnimationPlaying_ && animation_.duration > 0.0f) {
        // 縺ｲ縺ｨ縺ｾ縺・0fps蜑肴署縺ｧ譎る俣繧帝ｲ繧√ｋ
        animationTime_ += 1.0f / 60.0f;

        // 譛蠕後∪縺ｧ陦後▲縺溘ｉ蜈磯ｭ縺ｫ謌ｻ縺励※繝ｫ繝ｼ繝怜・逕溘☆繧・
        animationTime_ = std::fmod(animationTime_, animation_.duration);
    }

    if (hasSkeleton_ && isAnimationPlaying_ && !animation_.nodeAnimations.empty()) {
        // Skeleton縺ｫanimation繧帝←逕ｨ縺吶ｋ
        ApplyAnimation(skeleton_, animation_, animationTime_);
    }

    if (!hasSkeleton_ && isAnimationPlaying_ && !animation_.nodeAnimations.empty()) {
        auto it = animation_.nodeAnimations.find(animationNodeName_);

        // 謖・ｮ壹＠縺殤ode蜷阪・Animation縺後≠繧九→縺阪□縺大・逕溘☆繧・
        if (it != animation_.nodeAnimations.end()) {
            const NodeAnimation& nodeAnimation = it->second;

            // translate縺ｮ繧ｭ繝ｼ縺後≠繧後・迴ｾ蝨ｨ譎ょ綾縺ｮ蛟､繧貞渚譏縺吶ｋ
            if (!nodeAnimation.translate.keyframes.empty()) {
                transform.translate =
                    CalculateValue(nodeAnimation.translate.keyframes, animationTime_);
            }

            // scale縺ｮ繧ｭ繝ｼ縺後≠繧後・迴ｾ蝨ｨ譎ょ綾縺ｮ蛟､繧貞渚譏縺吶ｋ
            if (!nodeAnimation.scale.keyframes.empty()) {
                transform.scale =
                    CalculateValue(nodeAnimation.scale.keyframes, animationTime_);
            }

            // rotate縺ｮ繧ｭ繝ｼ縺後≠繧後・迴ｾ蝨ｨ譎ょ綾縺ｮ蛟､繧貞叙蠕励☆繧・
            if (!nodeAnimation.rotate.keyframes.empty()) {
                animationRotate =
                    CalculateValue(nodeAnimation.rotate.keyframes, animationTime_);
                hasAnimationRotate = true;
            }
        }
    }

    if (hasSkeleton_) {
        // animation驕ｩ逕ｨ蠕後・transform縺九ｉSkeleton陦悟・繧呈峩譁ｰ縺吶ｋ
        UpdateSkeleton(skeleton_);
    }

    if (hasSkinCluster_) {
        // 迴ｾ蝨ｨ縺ｮSkeleton迥ｶ諷九°繧唄kinCluster繧呈峩譁ｰ縺吶ｋ
        UpdateSkinCluster(skinCluster_, skeleton_);
        ApplySkinningCompute();
    }

    Matrix4x4 worldMatrix;

    if (hasAnimationRotate) {
        // 繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ蝗櫁ｻ｢縺後≠繧九→縺阪・Quaternion迚医ｒ菴ｿ縺・
        worldMatrix =
            MakeAffineMatrix(transform.scale, animationRotate, transform.translate);
    } else {
        // 騾壼ｸｸ縺ｯEuler隗偵〒World陦悟・繧剃ｽ懊ｋ
        worldMatrix =
            MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    }


    if (useCustomWorldMatrix_) {
        // World陦悟・縺檎峩謗･謖・ｮ壹＆繧後※縺・ｋ蝣ｴ蜷医・縲・壼ｸｸ縺ｮTRS繧医ｊ蜆ｪ蜈医☆繧・
        worldMatrix = customWorldMatrix_;
    }

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& vp = camera_->GetViewProjectionMatrix();
        viewProjectionMatrix_ = vp;
        worldViewProjectionMatrix = Multiply(worldMatrix, vp);
    } else {
        worldViewProjectionMatrix = worldMatrix;
        viewProjectionMatrix_ = MakeIdentity4x4();
    }

    transformationMatrixData_->WVP = worldViewProjectionMatrix;
    transformationMatrixData_->World = worldMatrix;

    if (cameraData_) {
        cameraData_->worldPosition = camera_ ? camera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
        cameraData_->padding = 0.0f;
    }
}
void Object3d::Draw()
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    object3dCommon_->CommonDrawSetting();

    // Apply the shared light intensity before drawing.
    directionalLightData_->intensity = lightIntensity_;

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());
    // 縺薙・繧ｪ繝悶ず繧ｧ繧ｯ繝亥ｰら畑縺ｮ繝・ぅ繧ｾ繝ｫ繝冶ｨｭ螳壹ｒb4縺ｸ貂｡縺・
    commandList->SetGraphicsRootConstantBufferView(
        7, dissolveResource_->GetGPUVirtualAddress());

    if (model_) {
        // 繝｢繝・Ν縺ｫ險ｭ螳壹＆繧後※縺・ｋ繝・け繧ｹ繝√Ε繧・t1 縺ｫ險ｭ螳壹☆繧・
        commandList->SetGraphicsRootDescriptorTable(
            5,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 迺ｰ蠅・・繝・・繧・t2 縺ｫ險ｭ螳壹☆繧・
    commandList->SetGraphicsRootDescriptorTable(
        6,
        TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        if (hasSkinCluster_) {
            // Skinning 逕ｨ縺ｮ influence VBV 繧よｸ｡縺励※謠冗判縺吶ｋ
            model_->DrawWithSkinnedVertexBuffer(skinCluster_.skinnedVertexBufferView, materialData_->color);

        } else {
            // 騾壼ｸｸ謠冗判繧定｡後≧
            model_->Draw(materialData_->color);
        }
    }
}




MaterialData Object3d::LoadMaterialTemplateFile(
    const std::string& directoryPath,
    const std::string& mtlFileName,
    MaterialData& material)
{
    std::ifstream file(directoryPath + "/" + mtlFileName);
    if (!file.is_open()) {
        return material;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string id;
        s >> id;

        if (id == "map_Kd") {
            std::string texName;
            s >> texName;
            material.textureFilePath = directoryPath + "/" + texName;
        }
    }

    return material;
}

void Object3d::InitializeTransformationMatrix()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
}

void Object3d::InitializeDirectionalLight()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    directionalLightData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    directionalLightData_->direction = Vector3(0.0f, -1.0f, 0.0f);
    directionalLightData_->intensity = lightIntensity_;
}

void Object3d::InitializeCameraForGPU()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
    cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->padding = 0.0f;
}

void Object3d::SetModel(const std::string& filePath)
{
    model_ = ModelManager::GetInstance()->FindModel(filePath);

    hasSkeleton_ = false;
    hasSkinCluster_ = false;

    if (model_) {
        const ModelData& modelData = model_->GetModelData();

        // rootNode 縺ｫ蜷榊燕繧・ｭ舌′蜈･縺｣縺ｦ縺・ｌ縺ｰ Skeleton 繧剃ｽ懊ｋ
        if (!modelData.rootNode.name.empty() || !modelData.rootNode.children.empty()) {
            skeleton_ = CreateSkeleton(modelData.rootNode);
            UpdateSkeleton(skeleton_);
            hasSkeleton_ = true;
        }

        // Skeleton 縺ｨ skinClusterData 縺ｮ荳｡譁ｹ縺後≠繧九↑繧・SkinCluster 繧剃ｽ懊ｋ
        if (hasSkeleton_ && !modelData.skinClusterData.empty()) {
            skinCluster_ = CreateSkinCluster(
                object3dCommon_->GetDxCommon(),
                object3dCommon_->GetSrvManager(),
                skeleton_,
                modelData);

            hasSkinCluster_ = true;
        }
    }
}

void Object3d::ResetSkeletonPose()
{
    if (!model_) {
        return;
    }

    const ModelData& modelData = model_->GetModelData();
    if (modelData.rootNode.name.empty() && modelData.rootNode.children.empty()) {
        return;
    }

    // Skeleton繧偵Δ繝・Ν隱ｭ縺ｿ霎ｼ縺ｿ譎ゅ・TRS縺ｸ菴懊ｊ逶ｴ縺励※T繝昴・繧ｺ縺ｫ謌ｻ縺・
    skeleton_ = CreateSkeleton(modelData.rootNode);
    UpdateSkeleton(skeleton_);
    hasSkeleton_ = true;
}
void Object3d::SetTexture(const std::string& filePath)
{
    TextureManager* texMan = TextureManager::GetInstance();
    texMan->LoadTexture(filePath);

    if (model_) {
        model_->SetTextureIndex(texMan->GetTextureIndexByFilePath(filePath));
    }
}

void Object3d::InitializeMaterial()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = Vector4(1, 1, 1, 1);
    materialData_->lightingType = static_cast<int>(LightingType::HalfLambert);
    materialData_->environmentCoefficient = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();

    // 繝・ぅ繧ｾ繝ｫ繝悶ｒ菴ｿ繧上↑縺・壼ｸｸ陦ｨ遉ｺ縺ｮ迥ｶ諷九〒蛻晄悄蛹悶☆繧・
    dissolveResource_ = dxCommon->CreateBufferResource(sizeof(DissolveData));
    dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
    dissolveData_->threshold = 0.0f;
    dissolveData_->edgeWidth = 0.04f;
    dissolveData_->isEnabled = 0;
    dissolveData_->padding = 0.0f;
    dissolveData_->edgeColor = Vector4(1.0f, 0.85f, 0.35f, 1.0f);
}

void Object3d::SetColor(const Vector4& color)
{
    if (materialData_) {
        materialData_->color = color;
    }
}

void Object3d::SetEnvironmentTexture(const std::string& filePath)
{
    environmentTextureFilePath_ = filePath;
    TextureManager::GetInstance()->LoadTexture(environmentTextureFilePath_);
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (materialData_) {
        materialData_->environmentCoefficient = coefficient;
    }
}

void Object3d::SetDissolveEnabled(bool enabled)
{
    if (dissolveData_) {
        // HLSL蛛ｴ縺ｧ謇ｱ縺・ｄ縺吶＞繧医≧縺ｫ逵溷⊃蛟､繧・縺・縺ｧ菫晏ｭ倥☆繧・
        dissolveData_->isEnabled = enabled ? 1u : 0u;
    }
}

void Object3d::SetDissolveThreshold(float threshold)
{
    if (dissolveData_) {
        // 諠ｳ螳壼､悶・蛟､縺ｧ蜈ｨ菴薙′豸医∴縺ｪ縺・ｈ縺・.0f縺九ｉ1.0f縺ｸ蛻ｶ髯舌☆繧・
        if (threshold < 0.0f) {
            threshold = 0.0f;
        } else if (threshold > 1.0f) {
            threshold = 1.0f;
        }
        dissolveData_->threshold = threshold;
    }
}

void Object3d::SetDissolveEdgeWidth(float edgeWidth)
{
    if (dissolveData_) {
        // 蠅・阜蟷・′雋縺ｫ縺ｪ繧峨↑縺・ｈ縺・↓縺吶ｋ
        dissolveData_->edgeWidth = edgeWidth < 0.0f ? 0.0f : edgeWidth;
    }
}

void Object3d::SetDissolveEdgeColor(const Vector4& edgeColor)
{
    if (dissolveData_) {
        // 繝・ぅ繧ｾ繝ｫ繝門｢・阜縺ｸ蜉邂励☆繧玖牡繧剃ｿ晏ｭ倥☆繧・
        dissolveData_->edgeColor = edgeColor;
    }
}

void Object3d::DrawInstanced(UINT instanceCount)
{
    assert(object3dCommon_);
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    object3dCommon_->CommonDrawSetting();

    // Apply the shared light intensity before drawing.
    directionalLightData_->intensity = lightIntensity_;

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, directionalLightResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        2, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        3, cameraResource_->GetGPUVirtualAddress());
    // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ謠冗判縺ｧ繧ょ酔縺倥ョ繧｣繧ｾ繝ｫ繝冶ｨｭ螳壹ｒb4縺ｸ貂｡縺・
    commandList->SetGraphicsRootConstantBufferView(
        7, dissolveResource_->GetGPUVirtualAddress());

    if (model_) {
        // 繝｢繝・Ν縺ｫ險ｭ螳壹＆繧後※縺・ｋ繝・け繧ｹ繝√Ε繧・t1 縺ｫ險ｭ螳壹☆繧・
        commandList->SetGraphicsRootDescriptorTable(
            5,
            TextureManager::GetInstance()->GetSrvHandleGPU(
                model_->GetModelData().material.textureFilePath));
    }

    // 迺ｰ蠅・・繝・・繧・t2 縺ｫ險ｭ螳壹☆繧・
    commandList->SetGraphicsRootDescriptorTable(
        6,
        TextureManager::GetInstance()->GetSrvHandleGPU(environmentTextureFilePath_));

    if (model_) {
        if (hasSkinCluster_) {
            // Skinning 逕ｨ縺ｮ influence VBV 繧よｸ｡縺励※謠冗判縺吶ｋ
            model_->DrawInstancedWithSkinnedVertexBuffer(
                instanceCount,
                skinCluster_.skinnedVertexBufferView,
                materialData_->color);

        } else {
            // 騾壼ｸｸ縺ｮ繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ謠冗判繧定｡後≧
            model_->DrawInstanced(instanceCount, materialData_->color);
        }
    }
}




// ComputeShader 縺ｫ貂｡縺咎らせ謨ｰ諠・ｱ縺ｮ螳壽焚繝舌ャ繝輔ぃ繧貞・譛溷喧縺吶ｋ
void Object3d::InitializeSkinningInformation()
{
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    skinningInformationResource_ =
        dxCommon->CreateBufferResource(sizeof(SkinningInformationForGPU));

    skinningInformationResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&skinningInformationData_));

    skinningInformationData_->numVertices = 0;
    skinningInformationData_->padding[0] = 0;
    skinningInformationData_->padding[1] = 0;
    skinningInformationData_->padding[2] = 0;
}

// ComputeShader 縺ｧ繧ｹ繧ｭ繝九Φ繧ｰ繧貞ｮ溯｡後☆繧・
void Object3d::ApplySkinningCompute()
{
    if (!hasSkinCluster_ || !model_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList =
        object3dCommon_->GetDxCommon()->GetCommandList();

    const ModelData& modelData = model_->GetModelData();
    uint32_t vertexCount = GetSkinClusterVertexCount(modelData);

    // 鬆らせ縺檎┌縺・ｴ蜷医・ ComputeShader 繧定ｵｷ蜍輔＠縺ｪ縺・
    if (vertexCount == 0) {
        return;
    }

    // ComputeShader 縺ｫ貂｡縺咎らせ謨ｰ繧呈峩譁ｰ縺吶ｋ
    skinningInformationData_->numVertices = vertexCount;

    // ComputeShader 逕ｨ縺ｮ RootSignature 縺ｨ PSO 繧定ｨｭ螳壹☆繧・
    object3dCommon_->SkinningComputeSetting();

    // UAV 縺ｸ譖ｸ縺崎ｾｼ繧蜑阪↓ Compute 逕ｨ迥ｶ諷九∈驕ｷ遘ｻ縺吶ｋ
    D3D12_RESOURCE_BARRIER barrierToUav{};
    barrierToUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToUav.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToUav.Transition.pResource = skinCluster_.skinnedVertexResource.Get();
    barrierToUav.Transition.StateBefore = skinCluster_.skinnedVertexCurrentState;
    barrierToUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToUav);

    // 迴ｾ蝨ｨ縺ｮ ResourceState 繧呈峩譁ｰ縺吶ｋ
    skinCluster_.skinnedVertexCurrentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;


    // t0 : MatrixPalette
    commandList->SetComputeRootDescriptorTable(
        0,
        skinCluster_.paletteSrvHandle.second);

    // t1 : InputVertices
    commandList->SetComputeRootDescriptorTable(
        1,
        model_->GetCombinedVertexSrvHandleGPU());


    // t2 : Influences
    commandList->SetComputeRootDescriptorTable(
        2,
        skinCluster_.influenceSrvHandle.second);


    // u0 : OutputVertices
    commandList->SetComputeRootDescriptorTable(
        3,
        skinCluster_.skinnedVertexUavHandle.second);

    // b0 : SkinningInformation
    commandList->SetComputeRootConstantBufferView(
        4,
        skinningInformationResource_->GetGPUVirtualAddress());

    // 1024 thread 縺斐→縺ｫ蛻・ｊ荳翫￡縺ｦ Dispatch 縺吶ｋ
    commandList->Dispatch((vertexCount + 255) / 256, 1, 1);


    // UAV 譖ｸ縺崎ｾｼ縺ｿ螳御ｺ・ｒ菫晁ｨｼ縺吶ｋ
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = skinCluster_.skinnedVertexResource.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    // 謠冗判縺ｧ隱ｭ繧√ｋ迥ｶ諷九∈謌ｻ縺・
    D3D12_RESOURCE_BARRIER barrierToVertexAndConstantBuffer{};
    barrierToVertexAndConstantBuffer.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierToVertexAndConstantBuffer.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrierToVertexAndConstantBuffer.Transition.pResource = skinCluster_.skinnedVertexResource.Get();
    barrierToVertexAndConstantBuffer.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrierToVertexAndConstantBuffer.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrierToVertexAndConstantBuffer.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrierToVertexAndConstantBuffer);

    // 謠冗判逕ｨ縺ｮ ResourceState 縺ｸ謌ｻ縺励◆縺薙→繧定ｨ倬鹸縺吶ｋ
    skinCluster_.skinnedVertexCurrentState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

}

void Object3d::DrawLightImGui()
{
#ifdef USE_IMGUI
    if (!directionalLightData_) {
        return;
    }

    ImGui::Begin("Object Light");

    // Edit the shared light intensity from ImGui.
    ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 3.0f);

    ImGui::End();
#endif
}
