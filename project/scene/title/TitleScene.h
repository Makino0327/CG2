#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <array>
#include <memory>
#include <random>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "../BaseScene.h"
#include "../../engine/math/Math.h"

class Object3d;
class Sprite;
class ParticleSystem;

class TitleScene : public BaseScene {
public:
    // 描画クラスの解放は、型の定義を読み込んだcpp側で行う
    TitleScene();
    ~TitleScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;

private:
    // タイトル専用の敵。通常ゲームのAIやHPには影響させない
    struct DemoZombie {
        std::unique_ptr<Object3d> object;
        Vector3 position{};
        Vector3 knockback{};
        float speed = 0.05f;
        float yaw = 0.0f;
        int hp = 0;
        int deathTimer = 0;
        bool active = false;
    };
    // 弾は使い回し、タイトルを放置しても個数が増え続けないようにする
    struct DemoBullet {
        std::unique_ptr<Object3d> object;
        Vector3 position{};
        Vector3 velocity{};
        Vector4 color{};
        int life = 0;
        int damage = 1;
    };
    // 発射間隔と散らばり方で、3種類の銃を表現する
    enum class DemoWeapon { Handgun, AssaultRifle, Shotgun };

    std::unique_ptr<Object3d> CreateObject(const char* model, const Vector3& position,
        const Vector3& scale, const Vector4& color);
    Sprite* CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color);
    Sprite* CreateUiLabel(int row, const Vector2& position, const Vector2& size);
    void CreateMenu();
    void UpdateMenu();
    void UpdateDemo();
    void SpawnZombie(float radius);
    void FireWeapon(const Vector3& direction);
    void HitZombie(DemoZombie& zombie, const DemoBullet& bullet);

    bool initialized_ = false;
    bool isStarting_ = false;
    int transitionTimer_ = 0;
    int selectedButton_ = 0;
    int frame_ = 0;
    int spawnTimer_ = 0;
    int fireTimer_ = 0;
    int weaponTimer_ = 0;
    float playerYaw_ = 0.0f;
    float recoil_ = 0.0f;
    // 共有モデルのテクスチャを、本編へ移る前に元へ戻すため保存する
    uint32_t originalCubeTextureIndex_ = 0;
    DemoWeapon weapon_ = DemoWeapon::Handgun;
    std::mt19937 randomEngine_{ std::random_device{}() };

    // プレイヤーは位置固定の描画専用。ダメージ処理を持たないため絶対に死亡しない
    std::unique_ptr<Object3d> playerObject_;
    std::unique_ptr<Object3d> gunObject_;
    std::vector<std::unique_ptr<Object3d>> scenery_;
    std::array<DemoZombie, 24> zombies_;
    std::array<DemoBullet, 72> bullets_;
    std::unique_ptr<ParticleSystem> sparks_;
    std::unique_ptr<ParticleSystem> blood_;

    // 枠と文字は通常のSpriteで描き、Releaseビルドでもメニューを表示する
    std::vector<std::unique_ptr<Sprite>> uiSprites_;
    std::array<Sprite*, 2> buttonBorders_{};
    std::array<Sprite*, 2> buttonBackgrounds_{};
    Sprite* weaponLabel_ = nullptr;
    Sprite* weaponProgress_ = nullptr;
    Sprite* fadeSprite_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
