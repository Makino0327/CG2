#include "TitleScene.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../game/camera/Camera.h"
#include "../SceneManager.h"
#include "../gameplay/GamePlayScene.h"

namespace {
    // プレイヤーの位置と、武器を切り替える時間（60FPSで6秒）
    constexpr Vector3 kPlayerPosition{ 0.0f, 1.0f, 0.0f };
    constexpr int kWeaponDuration = 360;
    constexpr float kPi = 3.14159265f;
    constexpr float kButtonX = 480.0f;
    constexpr float kButtonWidth = 320.0f;
    constexpr float kButtonHeight = 54.0f;
    constexpr float kButtonY[] = { 548.0f, 614.0f };

    // 弾の移動区間との最短距離を求め、高速な弾のすり抜けを防ぐ
    float SegmentDistanceSq(const Vector3& start, const Vector3& end, const Vector3& point) {
        const float dx = end.x - start.x;
        const float dz = end.z - start.z;
        const float lengthSq = dx * dx + dz * dz;
        const float t = lengthSq > 0.0001f
            ? std::clamp(((point.x - start.x) * dx + (point.z - start.z) * dz) / lengthSq, 0.0f, 1.0f)
            : 0.0f;
        const float x = point.x - (start.x + dx * t);
        const float z = point.z - (start.z + dz * t);
        return x * x + z * z;
    }
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

std::unique_ptr<Object3d> TitleScene::CreateObject(const char* model, const Vector3& position,
    const Vector3& scale, const Vector4& color) {
    // タイトル用オブジェクトの共通設定をまとめる
    auto object = std::make_unique<Object3d>();
    object->Initialize(context_.object3dCommon);
    object->SetCamera(context_.camera);
    object->SetModel(model);
    object->SetTranslate(position);
    object->SetScale(scale);
    object->SetColor(color);
    object->GetMaterial()->lightingType = static_cast<int>(LightingType::None);
    object->Update();
    return object;
}

Sprite* TitleScene::CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color) {
    // 白いテクスチャへ色を付け、背景と枠線を作る
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), "Resources/white1x1.png");
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    Sprite* result = sprite.get();
    uiSprites_.push_back(std::move(sprite));
    return result;
}

Sprite* TitleScene::CreateUiLabel(int row, const Vector2& position, const Vector2& size) {
    // 文字画像の指定行を切り出す。日本語フォントの実行時読み込みは不要
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(context_.spriteCommon, directionalLightResource_.Get(), "Resources/title/menu_labels.png");
    sprite->SetTextureLeftTop({ row == 2 ? 0.0f : 256.0f, static_cast<float>(row * 64) });
    sprite->SetTextureSize({ row == 2 ? 1024.0f : 512.0f, 64.0f });
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->Update();
    Sprite* result = sprite.get();
    uiSprites_.push_back(std::move(sprite));
    return result;
}

void TitleScene::Initialize() {
    if (initialized_) { return; }
    initialized_ = true;
    assert(context_.camera && context_.input && context_.dxCommon);
    frame_ = spawnTimer_ = fireTimer_ = weaponTimer_ = transitionTimer_ = 0;
    selectedButton_ = 0;
    isStarting_ = false;
    weapon_ = DemoWeapon::Handgun;
    playerYaw_ = recoil_ = 0.0f;

    // 真上に近い固定カメラで、中央のプレイヤーと周囲の群れを見せる
    context_.camera->SetTranslate({ 0.0f, 68.0f, -6.2f });
    context_.camera->SetRotate({ 1.48f, 0.0f, 0.0f });
    context_.camera->Update();
    if (context_.isDebugMode) { *context_.isDebugMode = false; }
    if (context_.offscreenRenderer) {
        // 前のシーンの死亡・構え演出をタイトルへ持ち越さない
        context_.offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        for (int effect = 0; effect <= static_cast<int>(PostEffectType::DepthOutline); ++effect) {
            context_.offscreenRenderer->SetPostEffectEnabled(static_cast<PostEffectType>(effect), false);
        }
    }

    // 本編と同じモデルを使い、見た目の雰囲気をつなげる
    auto models = ModelManager::GetInstance();
    for (const char* model : { "cube.obj", "player/player.obj", "enemy/enemy.obj", "bullet/bullet.obj" }) {
        models->LoadModel(model);
    }
    originalCubeTextureIndex_ = models->FindModel("cube.obj")->GetModelData().material.textureIndex;
    directionalLightResource_ = context_.dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = { 1, 1, 1, 1 };
    light->direction = { 0, -1, 0 };
    light->intensity = 1.0f;

    // 暗い床と継ぎ目を作り、弾・血・プレイヤーが埋もれない背景にする
    scenery_.push_back(CreateObject("cube.obj", { 0,-0.3f,0 }, { 45,0.25f,45 }, { 0.10f,0.13f,0.15f,1 }));
    scenery_.back()->SetTexture("Resources/white1x1.png");
    for (int i = -6; i <= 6; ++i) {
        const float offset = static_cast<float>(i) * 5.0f;
        scenery_.push_back(CreateObject("cube.obj", { offset,0,0 }, { 0.025f,0.015f,35 }, { 0.18f,0.22f,0.23f,1 }));
        scenery_.back()->SetTexture("Resources/white1x1.png");
        scenery_.push_back(CreateObject("cube.obj", { 0,0,offset }, { 35,0.015f,0.025f }, { 0.18f,0.22f,0.23f,1 }));
        scenery_.back()->SetTexture("Resources/white1x1.png");
    }
    playerObject_ = CreateObject("player/player.obj", kPlayerPosition, { 1,1,1 }, { 0.65f,0.95f,1,1 });
    gunObject_ = CreateObject("cube.obj", { 0,1.4f,1.3f }, { 0.18f,0.16f,0.7f }, { 0.9f,0.8f,0.45f,1 });
    gunObject_->SetTexture("Resources/white1x1.png");

    // 敵と弾の描画リソースは最初に確保し、デモ中は再利用する
    for (auto& zombie : zombies_) {
        zombie.object = CreateObject("enemy/enemy.obj", {}, { 0.8f,1,0.8f }, { 0.6f,0.8f,0.4f,1 });
        zombie.active = false;
    }
    for (auto& bullet : bullets_) {
        bullet.object = CreateObject("bullet/bullet.obj", {}, { 0.12f,0.12f,0.55f }, { 1,0.8f,0.3f,1 });
        bullet.life = 0;
    }
    sparks_ = std::make_unique<ParticleSystem>();
    sparks_->Initialize(context_.dxCommon, context_.particleCommon, context_.camera, context_.srvManager, ParticleType::CircleBurst);
    blood_ = std::make_unique<ParticleSystem>();
    blood_->Initialize(context_.dxCommon, context_.particleCommon, context_.camera, context_.srvManager, ParticleType::CircleBurst);
    blood_->SetBlendMode(ParticleBlendMode::Alpha);
    for (int i = 0; i < 12; ++i) { SpawnZombie(10.0f + static_cast<float>(i % 4) * 2.0f); }
    CreateMenu();
    UpdateMenu();
}

void TitleScene::CreateMenu() {
    // タイトル名は未定なので、上部には文字を入れず空の枠だけを置く
    CreateUiRect({ 300,42 }, { 680,126 }, { 0.55f,0.72f,0.73f,0.8f });
    CreateUiRect({ 302,44 }, { 676,122 }, { 0.018f,0.03f,0.04f,0.93f });
    CreateUiRect({ 300,42 }, { 64,4 }, { 1,0.72f,0.25f,1 });
    CreateUiRect({ 916,164 }, { 64,4 }, { 1,0.72f,0.25f,1 });

    // 下部の2ボタンは背景に埋もれない濃色パネルにする
    for (int i = 0; i < 2; ++i) {
        buttonBorders_[i] = CreateUiRect({ kButtonX,kButtonY[i] }, { kButtonWidth,kButtonHeight }, { 1,1,1,1 });
        buttonBackgrounds_[i] = CreateUiRect({ kButtonX+2,kButtonY[i]+2 }, { kButtonWidth-4,kButtonHeight-4 }, { 0.03f,0.05f,0.06f,0.96f });
        CreateUiLabel(i, { 480,kButtonY[i]+3 }, { 320,48 });
    }
    CreateUiLabel(2, { 280,680 }, { 720,30 });
    CreateUiRect({ 1010,631 }, { 230,48 }, { 0.015f,0.03f,0.04f,0.92f });
    weaponLabel_ = CreateUiLabel(3, { 1015,637 }, { 220,30 });
    weaponProgress_ = CreateUiRect({ 1010,677 }, { 230,3 }, { 1,0.72f,0.25f,1 });
    // フェードは一番最後に描画して、開始時に画面全体を覆う
    fadeSprite_ = CreateUiRect({ 0,0 }, { 1280,720 }, { 0,0,0,0 });
}

void TitleScene::UpdateMenu() {
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = context_.input->GetMousePosition();
    const Vector2 delta = context_.input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < 2; ++i) {
        if (mouse.x >= kButtonX && mouse.x <= kButtonX+kButtonWidth &&
            mouse.y >= kButtonY[i] && mouse.y <= kButtonY[i]+kButtonHeight) { hovered = i; }
    }
    if (!isStarting_) {
        if (hovered >= 0 && (delta.x != 0.0f || delta.y != 0.0f)) { selectedButton_ = hovered; }
        if (context_.input->TriggerKey(DIK_UP) || context_.input->TriggerKey(DIK_DOWN) ||
            context_.input->TriggerKey(DIK_W) || context_.input->TriggerKey(DIK_S)) {
            selectedButton_ = 1-selectedButton_;
        }
        const bool clicked = hovered >= 0 && context_.input->TriggerMouseLeft();
        if (clicked) { selectedButton_ = hovered; }
        if (clicked || context_.input->TriggerKey(DIK_RETURN) || context_.input->TriggerKey(DIK_SPACE)) {
            if (selectedButton_ == 0) {
                isStarting_ = true;
                transitionTimer_ = 0;
            } else {
                // 強制終了せず、通常の終了メッセージで後片付けを行う
                PostQuitMessage(0);
            }
        }
    }
    const float pulse = 0.8f + 0.2f * std::sin(static_cast<float>(frame_) * 0.05f);
    for (int i = 0; i < 2; ++i) {
        const bool selected = i == selectedButton_;
        buttonBorders_[i]->SetColor(selected ? Vector4{ 1,0.72f,0.25f,pulse } : Vector4{ 0.4f,0.52f,0.55f,0.7f });
        buttonBackgrounds_[i]->SetColor(selected ? Vector4{ 0.12f,0.14f,0.13f,0.96f } : Vector4{ 0.025f,0.04f,0.05f,0.96f });
    }
    weaponLabel_->SetTextureLeftTop({ 256,static_cast<float>((3+static_cast<int>(weapon_))*64) });
    weaponProgress_->SetSize({ 230.0f * (1.0f-static_cast<float>(weaponTimer_)/kWeaponDuration),3 });
    fadeSprite_->SetColor({ 0,0,0,isStarting_ ? std::clamp(static_cast<float>(transitionTimer_)/30.0f,0.0f,1.0f) : 0.0f });
    for (auto& sprite : uiSprites_) { sprite->Update(); }
}

void TitleScene::SpawnZombie(float radius) {
    // 画面の周囲から補充し、必ず中央へ向かわせる
    for (auto& zombie : zombies_) {
        if (zombie.active) { continue; }
        const float angle = std::uniform_real_distribution<float>(-kPi,kPi)(randomEngine_);
        zombie.position = { std::sin(angle)*radius,1,std::cos(angle)*radius*0.75f };
        zombie.speed = std::uniform_real_distribution<float>(0.045f,0.075f)(randomEngine_);
        zombie.yaw = std::atan2(-zombie.position.x,-zombie.position.z);
        zombie.hp = 3;
        zombie.deathTimer = 0;
        zombie.active = true;
        zombie.object->SetTranslate(zombie.position);
        zombie.object->SetRotate({ 0,zombie.yaw,0 });
        zombie.object->SetScale({ 0.8f,1,0.8f });
        zombie.object->SetColor({ 0.6f,0.8f,0.4f,1 });
        zombie.object->Update();
        return;
    }
}

void TitleScene::FireWeapon(const Vector3& direction) {
    // 単発・連射・散弾の差を、発射間隔、弾数、色で見せる
    const int pellets = weapon_ == DemoWeapon::Shotgun ? 7 : 1;
    const float baseYaw = std::atan2(direction.x,direction.z);
    for (int pellet = 0; pellet < pellets; ++pellet) {
        for (auto& bullet : bullets_) {
            if (bullet.life > 0) { continue; }
            const float spread = weapon_ == DemoWeapon::Shotgun ? static_cast<float>(pellet-3)*0.10f : 0.0f;
            const float yaw = baseYaw+spread;
            bullet.velocity = { std::sin(yaw)*0.85f,0,std::cos(yaw)*0.85f };
            bullet.position = { direction.x*1.6f,1.3f,direction.z*1.6f };
            bullet.life = weapon_ == DemoWeapon::Shotgun ? 17 : 35;
            bullet.damage = weapon_ == DemoWeapon::AssaultRifle ? 1 : 3;
            bullet.color = weapon_ == DemoWeapon::Shotgun ? Vector4{ 1,0.45f,0.13f,1 } : Vector4{ 1,0.9f,0.4f,1 };
            bullet.object->SetTranslate(bullet.position);
            bullet.object->SetRotate({ 0,yaw,0 });
            bullet.object->SetColor(bullet.color);
            bullet.object->Update();
            break;
        }
    }
    const Vector3 muzzle{ direction.x*1.9f,1.4f,direction.z*1.9f };
    sparks_->Emit(muzzle, { 0.65f,0.65f,0.65f }, { 0,0,0 }, { 1,0.8f,0.3f,1 },0.09f);
    fireTimer_ = weapon_ == DemoWeapon::Handgun ? 24 : (weapon_ == DemoWeapon::AssaultRifle ? 6 : 46);
    recoil_ = weapon_ == DemoWeapon::Shotgun ? 0.45f : 0.22f;
}

void TitleScene::HitZombie(DemoZombie& zombie, const DemoBullet& bullet) {
    // 命中の血しぶきと、倒れながら縮む撃破演出を出す
    zombie.hp -= bullet.damage;
    for (int i = 0; i < 7; ++i) {
        const float angle = std::uniform_real_distribution<float>(-kPi,kPi)(randomEngine_);
        blood_->Emit(zombie.position,{ 0.2f,0.2f,0.2f },
            { std::sin(angle)*2.0f,0.4f,std::cos(angle)*2.0f },{ 0.6f,0.025f,0.015f,0.9f },0.45f);
    }
    zombie.object->SetColor({ 1,0.22f,0.12f,1 });
    if (zombie.hp <= 0) {
        zombie.deathTimer = 32;
        zombie.knockback = { bullet.velocity.x*0.20f,0,bullet.velocity.z*0.20f };
    }
}

void TitleScene::UpdateDemo() {
    // 武器は6秒ごとに循環し、切り替え後はすぐ撃てるようにする
    if (++weaponTimer_ >= kWeaponDuration) {
        weaponTimer_ = 0;
        weapon_ = static_cast<DemoWeapon>((static_cast<int>(weapon_)+1)%3);
        fireTimer_ = 0;
    }
    if (--spawnTimer_ <= 0) { SpawnZombie(23.0f); spawnTimer_ = 22; }
    DemoZombie* nearest = nullptr;
    float nearestDistance = 10000.0f;
    for (auto& zombie : zombies_) {
        if (!zombie.active) { continue; }
        if (zombie.hp <= 0) {
            // 演出が終わった枠は次のゾンビの出現に再利用する
            if (--zombie.deathTimer <= 0) { zombie.active = false; continue; }
            zombie.position.x += zombie.knockback.x;
            zombie.position.z += zombie.knockback.z;
            const float scale = static_cast<float>(zombie.deathTimer)/32.0f;
            zombie.object->SetScale({ 0.8f*scale,scale,0.8f*scale });
            zombie.object->SetRotate({ (1.0f-scale)*1.5f,zombie.yaw,0 });
        } else {
            float distance = std::sqrt(zombie.position.x*zombie.position.x+zombie.position.z*zombie.position.z);
            // プレイヤーは移動も被弾もしない。近づいた敵は手前に留める
            if (distance > 2.3f) {
                const float step = std::min(zombie.speed,distance-2.3f);
                zombie.position.x -= zombie.position.x/distance*step;
                zombie.position.z -= zombie.position.z/distance*step;
                distance -= step;
            }
            zombie.object->SetColor({ 0.6f,0.8f,0.4f,1 });
            if (distance < nearestDistance) { nearestDistance = distance; nearest = &zombie; }
        }
        zombie.object->SetTranslate(zombie.position);
        zombie.object->Update();
    }
    if (fireTimer_ > 0) { --fireTimer_; }
    if (nearest) {
        const Vector3 direction{ nearest->position.x/nearestDistance,0,nearest->position.z/nearestDistance };
        playerYaw_ = std::atan2(direction.x,direction.z);
        const float fireRange = weapon_ == DemoWeapon::Shotgun ? 10.0f : 14.0f;
        if (fireTimer_ <= 0 && nearestDistance <= fireRange) { FireWeapon(direction); }
    }
    // プレイヤーの位置は毎フレーム固定し、向きと銃の反動だけを変える
    recoil_ *= 0.78f;
    playerObject_->SetTranslate(kPlayerPosition);
    playerObject_->SetRotate({ 0,playerYaw_,0 });
    playerObject_->Update();
    const float barrelLength = weapon_ == DemoWeapon::Handgun ? 0.45f : 0.85f;
    gunObject_->SetScale({ weapon_ == DemoWeapon::Shotgun ? 0.26f : 0.16f,0.16f,barrelLength });
    gunObject_->SetRotate({ 0,playerYaw_,0 });
    gunObject_->SetTranslate({ std::sin(playerYaw_)*(1.2f-recoil_),1.5f,std::cos(playerYaw_)*(1.2f-recoil_) });
    gunObject_->Update();

    for (auto& bullet : bullets_) {
        if (bullet.life <= 0) { continue; }
        const Vector3 previous = bullet.position;
        bullet.position.x += bullet.velocity.x;
        bullet.position.z += bullet.velocity.z;
        --bullet.life;
        // 移動区間で最も手前の敵だけに命中させる
        DemoZombie* hit = nullptr;
        float hitDistance = 10000.0f;
        for (auto& zombie : zombies_) {
            if (!zombie.active || zombie.hp <= 0) { continue; }
            if (SegmentDistanceSq(previous,bullet.position,zombie.position) <= 1.0f) {
                const float x = zombie.position.x-previous.x;
                const float z = zombie.position.z-previous.z;
                const float distanceSq = x*x+z*z;
                if (distanceSq < hitDistance) { hitDistance = distanceSq; hit = &zombie; }
            }
        }
        if (hit) { HitZombie(*hit,bullet); bullet.life = 0; }
        sparks_->Emit(bullet.position,{ 0.12f,0.12f,0.12f },{ 0,0,0 },bullet.color,0.08f);
        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
    sparks_->Update(1.0f/60.0f);
    blood_->Update(1.0f/60.0f);
}

void TitleScene::Update() {
    ++frame_;
    UpdateDemo();
    // 入力は一度だけ受け付け、短い暗転のあと本編へ進む
    if (isStarting_ && ++transitionTimer_ >= 30) {
        sceneManager_->SetNextScene(std::make_unique<GamePlayScene>());
        return;
    }
    UpdateMenu();
}

void TitleScene::Draw() {
    // 背景の戦闘を描いたあとに枠とメニューを重ねる
    context_.object3dCommon->CommonDrawSetting();
    for (auto& object : scenery_) { object->Draw(); }
    for (auto& zombie : zombies_) { if (zombie.active) { zombie.object->Draw(); } }
    playerObject_->Draw();
    gunObject_->Draw();
    for (auto& bullet : bullets_) { if (bullet.life > 0) { bullet.object->Draw(); } }
    blood_->Draw();
    sparks_->Draw();
    context_.spriteCommon->CommonDrawSetting();
    for (auto& sprite : uiSprites_) { sprite->Draw(); }
}

void TitleScene::Finalize() {
    // デモのリソースを解放してから、ゲームプレイ側に引き渡す
    if (initialized_) {
        ModelManager::GetInstance()->FindModel("cube.obj")->SetTextureIndex(originalCubeTextureIndex_);
    }
    uiSprites_.clear();
    buttonBorders_.fill(nullptr);
    buttonBackgrounds_.fill(nullptr);
    weaponLabel_ = weaponProgress_ = fadeSprite_ = nullptr;
    for (auto& bullet : bullets_) { bullet.object.reset(); bullet.life = 0; }
    for (auto& zombie : zombies_) { zombie.object.reset(); zombie.active = false; }
    playerObject_.reset();
    gunObject_.reset();
    scenery_.clear();
    sparks_.reset();
    blood_.reset();
    directionalLightResource_.Reset();
    initialized_ = false;
}
