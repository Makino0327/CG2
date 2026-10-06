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
#include "../LevelLoader.h"

class Object3d;
class Sprite;
class ParticleSystem;
class Enemy;
class PlayerBullet;

class TitleScene : public BaseScene {
public:
    // 描画クラスの解放は、型の定義を読み込んだcpp側で行う
    TitleScene();
    ~TitleScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawShadow() override; // タイトルの背景モデルにも同じ影を付ける。
    void DrawBloom() override;
    // タイトル文字とメニューを、背景の波動より後に描画する
    void DrawOverlay() override;
    void Finalize() override;

private:
    // 発射間隔と散らばり方で、2種類の銃を表現する
    enum class DemoWeapon { AssaultRifle, Shotgun };

    std::unique_ptr<Object3d> CreateObject(const char* model, const Vector3& position,
        const Vector3& scale, const Vector4& color);
    Sprite* CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color);
    // ロゴや霧の画像も、矩形と同じ所有配列で管理する
    Sprite* CreateUiTexture(const char* path, const Vector2& position, const Vector2& size);
    Sprite* CreateUiLabel(int row, const Vector2& position, const Vector2& size);
    void CreateMenu();
    // 通常表示と画面破壊用の保存で、同じUIを描く
    void DrawTitleUi();
    void UpdateMenu();
    // ロゴの登場、文字の選択、霧の移動と電圧低下による明滅を更新する
    void UpdateMenuAnimation();
    void UpdateDemo();
    // 本編と同じEnemyクラスを画面外周に出現させる
    void SpawnEnemy(float radius);
    // 本編のPlayerと同じ発射処理(弾速・散弾・発射炎)で撃つ
    void FireWeapon(const Vector3& direction);
    void FireBullet(const Vector3& direction, float spreadAngle);
    void FireShotgun(const Vector3& direction);
    // 描画と同じ骨の変形を使い、銃口の少し先の位置を取得する。
    Vector3 GetMuzzlePosition() const;
    void EmitMuzzleFlash(const Vector3& firePosition, const Vector3& direction, bool isShotgun);
    void StartShockwave(const Vector3& firePosition);
    // 本編のCheckCollisionsと同じ命中処理
    void CheckBulletHits();
    void ResolveEnemyOverlap();

    bool initialized_ = false;
    bool isStarting_ = false;
    int selectedButton_ = 0;
    int frame_ = 0;
    int spawnTimer_ = 0;
    int fireTimer_ = 0;
    int assaultContinuousShotCount_ = 0;
    int weaponTimer_ = 0;
    float playerYaw_ = 0.0f;
    // 共有モデルのテクスチャを、本編へ移る前に元へ戻すため保存する
    uint32_t originalCubeTextureIndex_ = 0;
    DemoWeapon weapon_ = DemoWeapon::AssaultRifle;
    std::mt19937 randomEngine_{ std::random_device{}() };

    // プレイヤーは位置固定の描画専用。ダメージ処理を持たないため絶対に死亡しない
    std::unique_ptr<Object3d> playerObject_;
    // 銃口の発射位置を示す赤い印。Mキーで表示を切り替える。
    std::unique_ptr<Object3d> muzzleMarker_;
    bool showMuzzleMarker_ = true;
    std::vector<std::unique_ptr<Object3d>> scenery_;
    // 本編と同じ敵・弾クラスを使い、撃破演出や血しぶきも共通にする
    std::vector<std::unique_ptr<Enemy>> enemies_;
    std::vector<std::unique_ptr<PlayerBullet>> bullets_;
    // 破片が床で跳ねて止まるよう、タイトル床の当たり判定を用意する
    std::vector<LevelColliderData> floorColliders_;
    // 本編と同じ構成のパーティクル(弾の軌跡・発射炎 / 血しぶき)
    std::unique_ptr<ParticleSystem> particleSystem_;
    std::unique_ptr<ParticleSystem> bloodParticleSystem_;

    // 枠と文字は通常のSpriteで描き、Releaseビルドでもメニューを表示する
    std::vector<std::unique_ptr<Sprite>> uiSprites_;
    // 透明な入力範囲と文字の表示を分離し、枠のないメニューにする
    std::array<Sprite*, 2> buttonHitAreas_{};
    std::array<Sprite*, 2> buttonLabels_{};
    std::array<Sprite*, 2> buttonIndicators_{};
    std::array<Sprite*, 2> buttonUnderlines_{};
    std::array<float, 2> buttonSelection_{};
    std::array<Sprite*, 2> titleMist_{};
    Sprite* titleLogo_ = nullptr;
    Sprite* titleShade_ = nullptr;
    // 元の位置と色を保存し、毎フレームの移動や透明度を累積させない
    struct MenuVisual {
        Sprite* sprite = nullptr;
        Vector2 position{};
        Vector4 color{};
        float delay = 0.0f;
    };
    std::vector<MenuVisual> menuVisuals_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
};
