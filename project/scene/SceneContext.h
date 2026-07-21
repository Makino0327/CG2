// SceneContext.h
#pragma once

// 前方宣言（インクルードを減らしてコンパイルを早くするため）
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class Line3DCommon;
class ModelCommon;
class ParticleCommon;
class Camera;
class Input;
class SoundManager;
class OffscreenRenderer;

struct SceneContext {
    DirectXCommon* dxCommon = nullptr;
    SrvManager* srvManager = nullptr;
    SpriteCommon* spriteCommon = nullptr;
    Object3dCommon* object3dCommon = nullptr;
    Line3DCommon* line3dCommon = nullptr;
    ModelCommon* modelCommon = nullptr;
    ParticleCommon* particleCommon = nullptr;
    Camera* camera = nullptr;
    Input* input = nullptr;
    SoundManager* sound = nullptr;
    OffscreenRenderer* offscreenRenderer = nullptr;

    bool* isDebugMode = nullptr;

};