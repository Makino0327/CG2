#include "TitleScene.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include "../../engine/audio/SoundManager.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/3d/obj3d/Object3dCommon.h"
#include "../../engine/3d/model/ModelManager.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/input/Input.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../game/camera/Camera.h"
#include "../../game/enemy/Enemy.h"
#include "../../game/player/PlayerBullet.h"
#include "../SceneManager.h"
#include "../gameplay/GamePlayScene.h"

namespace {
    // プレイヤーの位置と、武器を切り替える時間（60FPSで6秒）
    constexpr Vector3 kPlayerPosition{ 0.0f, 1.0f, 0.0f };
    constexpr int kWeaponDuration = 360;
    constexpr float kPi = 3.14159265f;
    // 横長のロゴの下へ、余白を取って文字だけのメニューを並べる
    constexpr float kButtonX = 500.0f;
    constexpr float kButtonWidth = 280.0f;
    constexpr float kButtonHeight = 44.0f;
    constexpr float kButtonY[] = { 470.0f, 524.0f };

    // 演出の始まりと終わりを滑らかにする補間曲線
    float SmoothStep01(float value) {
        const float t = std::clamp(value,0.0f,1.0f);
        return t*t*(3.0f-2.0f*t);
    }

    // 敵の数は少し増やしつつ、1体ずつの撃破演出は見せる
    constexpr int kMaxEnemies = 9;
    constexpr int kInitialEnemies = 5;
    constexpr int kSpawnInterval = 65;
    // 出現位置は画面に映る床の範囲より外側にして、歩いて入ってくるようにする
    constexpr float kEnemySpawnRadiusMin = 34.0f;
    constexpr float kEnemySpawnRadiusMax = 42.0f;
    // 本編の床の上面(0.0) + 敵コライダー半径(1.0)
    constexpr float kEnemyHeight = 1.0f;
    // プレイヤーが敵の方へ向き直る速さ(1フレームで残り角度の何割回るか)と最大回転量
    constexpr float kTurnRate = 0.18f;
    constexpr float kMaxTurnSpeed = 0.20f;
    // この角度まで向き直ったら撃つ
    constexpr float kFireAngleTolerance = 0.15f;

    // 角度を -PI ～ PI に収める
    float WrapAngle(float angle) {
        while (angle > 3.14159265f) { angle -= 6.28318530f; }
        while (angle < -3.14159265f) { angle += 6.28318530f; }
        return angle;
    }
    constexpr float kEnemyStopDistance = 2.3f;

    // 銃声の低い衝撃と、少し遅れて砕けるガラスの高音を合成する。
    // staticで保持し、タイトル破棄後も再生中のPCMデータが消えないようにする。
    const SoundData& GetStartGunshotSound() {
        static const SoundData sound = [] {
            SoundData result;
            result.wfex.wFormatTag = WAVE_FORMAT_PCM;
            result.wfex.nChannels = 1;
            result.wfex.nSamplesPerSec = 44100;
            result.wfex.wBitsPerSample = 16;
            result.wfex.nBlockAlign = 2;
            result.wfex.nAvgBytesPerSec = 88200;
            constexpr uint32_t kSamples = 44100;
            result.buffer.resize(kSamples*sizeof(int16_t));
            std::mt19937 random(2417);
            std::uniform_real_distribution<float> noise(-1.0f,1.0f);
            for (uint32_t i = 0; i < kSamples; ++i) {
                const float t = static_cast<float>(i)/44100.0f;
                const float attack = std::min(t/0.0008f,1.0f);
                // 鋭い破裂音と低い衝撃を重ね、ガラス音より先に一発の銃声を聞かせる。
                float value = attack*(0.58f*noise(random)*std::exp(-t*65.0f)
                    + 0.34f*std::sin(2.0f*kPi*(105.0f*t-95.0f*t*t))*std::exp(-t*29.0f));
                // 弾が届いて溜めを見せた後、画面が割れる瞬間にガラス音を鳴らす。
                constexpr float kGlassStart = ScreenShatter::kBulletFlightTime+ScreenShatter::kBreakDelay;
                if (t >= kGlassStart) {
                    const float glassTime = t-kGlassStart;
                    value += 0.18f*noise(random)*std::exp(-glassTime*16.0f);
                    // 時間差のある短い高音で、複数の小さな破片を表現する。
                    for (int piece = 0; piece < 9; ++piece) {
                        const float age = glassTime-static_cast<float>(piece)*0.038f;
                        if (age >= 0.0f) {
                            const float frequency = 2100.0f+static_cast<float>(piece)*379.0f;
                            value += 0.038f*std::sin(2.0f*kPi*frequency*age)*std::exp(-age*32.0f);
                        }
                    }
                }
                const int16_t sample = static_cast<int16_t>(std::clamp(value,-0.95f,0.95f)*32767.0f);
                std::memcpy(result.buffer.data()+i*sizeof(sample), &sample, sizeof(sample));
            }
            return result;
        }();
        return sound;
    }

    // 本編Playerと同じ射撃パラメータ
    constexpr float kBulletSpeed = 1.4f;
    constexpr float kBulletSpawnHeight = 0.7f;
    constexpr float kBulletMuzzleDistance = 1.6f;
    constexpr int kAssaultFireInterval = 6;
    constexpr float kAssaultSpreadIncrease = 0.008f;
    constexpr float kAssaultMaxSpreadAngle = 0.08f;
    constexpr int kShotgunPelletCount = 5;
    constexpr float kShotgunSpreadAngle = 0.34f;
    constexpr float kShotgunRandomSpreadAngle = 0.08f;

    // 本編と同じ、ワールド座標から衝撃波用の画面UVへの変換
    bool TryConvertWorldToScreenUV(const Vector3& p, const Matrix4x4& m, Vector2& outUV) {
        const float clipX = p.x*m.m[0][0]+p.y*m.m[1][0]+p.z*m.m[2][0]+m.m[3][0];
        const float clipY = p.x*m.m[0][1]+p.y*m.m[1][1]+p.z*m.m[2][1]+m.m[3][1];
        const float clipW = p.x*m.m[0][3]+p.y*m.m[1][3]+p.z*m.m[2][3]+m.m[3][3];
        if (clipW <= 0.0f) { return false; }
        outUV.x = (clipX/clipW+1.0f)*0.5f;
        outUV.y = (1.0f-clipY/clipW)*0.5f;
        return outUV.x >= 0.0f && outUV.x <= 1.0f && outUV.y >= 0.0f && outUV.y <= 1.0f;
    }
}

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

std::unique_ptr<Object3d> TitleScene::CreateObject(const char* model, const Vector3& position,
    const Vector3& scale, const Vector4& color) {
    // タイトル用オブジェクトの共通設定をまとめる
    auto object = std::make_unique<Object3d>();
    object->Initialize(GetContext().object3dCommon);
    object->SetCamera(GetContext().camera);
    object->SetModel(model);
    object->SetTranslate(position);
    object->SetScale(scale);
    object->SetColor(color);
    object->SetLightingType(LightingType::None);
    object->Update();
    return object;
}

Sprite* TitleScene::CreateUiRect(const Vector2& position, const Vector2& size, const Vector4& color) {
    // 白いテクスチャへ色を付け、背景と枠線を作る
    Sprite* sprite = CreateUiTexture("Resources/white1x1.png",position,size);
    sprite->SetColor(color);
    return sprite;
}

Sprite* TitleScene::CreateUiTexture(const char* path, const Vector2& position, const Vector2& size) {
    // 実行時のフォント読み込みを避け、生成済みの画像をそのまま配置する
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), path);
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->Update();
    Sprite* result = sprite.get();
    uiSprites_.push_back(std::move(sprite));
    return result;
}

Sprite* TitleScene::CreateUiLabel(int row, const Vector2& position, const Vector2& size) {
    // 文字画像の指定行を切り出す。日本語フォントの実行時読み込みは不要
    auto sprite = std::make_unique<Sprite>();
    sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/title/menu_labels.png");
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
    assert(GetContext().camera && GetContext().input && GetContext().dxCommon);
    frame_ = spawnTimer_ = fireTimer_ = weaponTimer_ = 0;
    selectedButton_ = 0;
    buttonSelection_.fill(0.0f);
    isStarting_ = false;
    // スタート時に音の生成待ちが入らないよう、タイトル初期化時に用意する。
    (void)GetStartGunshotSound();
    weapon_ = DemoWeapon::AssaultRifle;
    playerYaw_ = 0.0f;
    assaultContinuousShotCount_ = 0;

    // カメラを中央へ寄せ、プレイヤーと敵の戦闘を大きく見せる
    // 高さに合わせて奥行きも調整し、中央を見下ろす構図を保つ
    GetContext().camera->SetTranslate({ 0.0f, 50.0f, -4.6f });
    GetContext().camera->SetRotate({ 1.48f, 0.0f, 0.0f });
    GetContext().camera->Update();
    if (GetContext().isDebugMode) { *GetContext().isDebugMode = false; }
    if (GetContext().offscreenRenderer) {
        // 前のシーンの死亡・構え演出をタイトルへ持ち越さない
        GetContext().offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        for (int effect = 0; effect <= static_cast<int>(PostEffectType::DepthOutline); ++effect) {
            GetContext().offscreenRenderer->SetPostEffectEnabled(static_cast<PostEffectType>(effect), false);
        }
        // 本編と同じ銃の衝撃波サイズにする
        GetContext().offscreenRenderer->SetShockwaveMaxRadius(0.10f);
    }

    // 本編と同じモデルを使い、見た目の雰囲気をつなげる
    auto models = ModelManager::GetInstance();
    // plane.obj はパーティクルの板ポリに使うので必ず読み込む(無いとエフェクトが描画されない)
    for (const char* model : { "cube.obj", "plane.obj", "player/player.obj", "enemy/enemy.obj", "bullet/bullet.obj" }) {
        models->LoadModel(model);
    }
    originalCubeTextureIndex_ = models->FindModel("cube.obj")->GetModelData().material.textureIndex;
    directionalLightResource_ = GetContext().dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    DirectionalLight* light = nullptr;
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&light));
    light->color = { 1, 1, 1, 1 };
    light->direction = { 0, -1, 0 };
    light->intensity = 1.0f;

    // 本編(testScene.json の Floor)と同じ床: cube.obj を (0,-1,0) に 50x1x50 で置く。上面は y=0
    scenery_.push_back(CreateObject("cube.obj", { 0.0f,-1.0f,0.0f }, { 50.0f,1.0f,50.0f }, { 1,1,1,1 }));
    scenery_.back()->SetLightingType(LightingType::HalfLambert);
    // プレイヤーも本編と同じ色・ライティングにする
    playerObject_ = CreateObject("player/player.obj", kPlayerPosition, { 1,1,1 }, { 1,1,1,1 });
    playerObject_->SetLightingType(LightingType::HalfLambert);

    // 本編の Floor と同じ床コライダー。敵の接地と破片の着地に使う
    floorColliders_.clear();
    floorColliders_.push_back({ "BOX", { 0.0f,-1.0f,0.0f }, { 100.0f,2.0f,100.0f }, true });

    // 本編と同じ構成のパーティクル。弾の軌跡・発射炎は加算、血しぶきは通常アルファ
    particleSystem_ = std::make_unique<ParticleSystem>();
    particleSystem_->Initialize(GetContext().dxCommon, GetContext().particleCommon, GetContext().camera, GetContext().srvManager, ParticleType::CircleBurst);
    bloodParticleSystem_ = std::make_unique<ParticleSystem>();
    bloodParticleSystem_->Initialize(GetContext().dxCommon, GetContext().particleCommon, GetContext().camera, GetContext().srvManager, ParticleType::CircleBurst);
    bloodParticleSystem_->SetBlendMode(ParticleBlendMode::Alpha);
    enemies_.clear();
    bullets_.clear();
    for (int i = 0; i < kInitialEnemies; ++i) { SpawnEnemy(kEnemySpawnRadiusMin + static_cast<float>(i) * 2.0f); }
    spawnTimer_ = kSpawnInterval;
    CreateMenu();
    UpdateMenu();
}

void TitleScene::CreateMenu() {
    // 背景の四辺を暗く落とし、赤白のロゴと中央の戦闘へ視線を集める
    titleShade_ = CreateUiTexture("Resources/title/title_shade.png", { 0,0 }, { 1280,720 });
    titleMist_[0] = CreateUiTexture("Resources/title/title_haze.png", { -120,350 }, { 1520,300 });
    titleMist_[1] = CreateUiTexture("Resources/title/title_haze.png", { -180,80 }, { 1520,250 });
    titleMist_[0]->SetColor({ 0.70f,0.78f,0.80f,0.22f });
    titleMist_[1]->SetColor({ 0.60f,0.67f,0.70f,0.10f });
    titleLogo_ = CreateUiTexture("Resources/title/title_logo.png", { 190,170 }, { 900,132 });

    // 箱や金属枠は描かず、文字・細い下線・小さな赤い印だけで選択を表す
    for (int i = 0; i < 2; ++i) {
        const float y = kButtonY[i];
        buttonHitAreas_[i] = CreateUiRect({ kButtonX,y }, { kButtonWidth,kButtonHeight }, { 0,0,0,0 });
        buttonLabels_[i] = CreateUiLabel(i, { 520,y+2 }, { 240,36 });
        buttonIndicators_[i] = CreateUiRect({ 576,y+17 }, { 3,3 }, { 0.61f,0.14f,0.14f,0 });
        buttonUnderlines_[i] = CreateUiRect({ 602,y+40 }, { 76,1 }, { 0.70f,0.63f,0.61f,0 });
    }

    // 元の色と座標を保存し、フェードを毎フレーム重ね掛けしないようにする
    menuVisuals_.clear();
    for (const auto& sprite : uiSprites_) {
        float delay = sprite.get() == titleLogo_ ? 8.0f : 0.0f;
        if (sprite->GetPosition().y >= kButtonY[0]) { delay = 22.0f; }
        menuVisuals_.push_back({ sprite.get(),sprite->GetPosition(),sprite->GetColor(),delay });
    }
}

void TitleScene::UpdateMenu() {
    // マウスを動かしたときだけ選択を変え、キーボード操作と競合させない
    const Vector2 mouse = GetContext().input->GetMousePosition();
    const Vector2 delta = GetContext().input->GetMouseDelta();
    int hovered = -1;
    for (int i = 0; i < 2; ++i) {
        // 文字の周囲に透明な入力範囲を設け、クリックしやすくする
        const Vector2 position = buttonHitAreas_[i]->GetPosition();
        if (mouse.x >= position.x && mouse.x <= position.x+kButtonWidth &&
            mouse.y >= position.y && mouse.y <= position.y+kButtonHeight) { hovered = i; }
    }
    // 見えない文字への誤入力を防ぐため、最初の短いフェードだけ待つ
    if (!isStarting_ && frame_ >= 40) {
        if (hovered >= 0 && (delta.x != 0.0f || delta.y != 0.0f)) { selectedButton_ = hovered; }
        if (GetContext().input->TriggerKey(DIK_UP) || GetContext().input->TriggerKey(DIK_DOWN) ||
            GetContext().input->TriggerKey(DIK_W) || GetContext().input->TriggerKey(DIK_S)) {
            selectedButton_ = 1-selectedButton_;
        }
        const bool clicked = hovered >= 0 && GetContext().input->TriggerMouseLeft();
        if (clicked) { selectedButton_ = hovered; }
        if (clicked || GetContext().input->TriggerKey(DIK_RETURN) || GetContext().input->TriggerKey(DIK_SPACE)) {
            if (selectedButton_ == 0) {
                isStarting_ = true;
                // 背景の銃の歪みを止め、画面中央への一発でタイトルを割る。
                if (GetContext().offscreenRenderer) {
                    GetContext().offscreenRenderer->StopShockwave();
                    GetContext().offscreenRenderer->StartScreenShatter({ 0.5f, 0.5f });
                }
                if (GetContext().sound) { GetContext().sound->SoundPlayWave(GetStartGunshotSound()); }
            } else {
                // 強制終了せず、通常の終了メッセージで後片付けを行う
                PostQuitMessage(0);
            }
        }
    }
    UpdateMenuAnimation();
}

void TitleScene::UpdateMenuAnimation() {
    // 暗がりからゆっくり浮かび上がる。メニューはロゴの少し後に表示する
    const float age = static_cast<float>(frame_);
    for (const auto& visual : menuVisuals_) {
        const float fade = visual.sprite == titleShade_ ? 1.0f : SmoothStep01((age-visual.delay)/48.0f);
        visual.sprite->SetPosition(visual.position);
        Vector4 color = visual.color;
        color.w *= fade;
        visual.sprite->SetColor(color);
    }

    // ロゴをわずかに縮めて静止させ、待機中は電圧低下のような弱い明滅を入れる
    const float settle = 1.0f-SmoothStep01(age/72.0f);
    const float scale = 1.0f+settle*0.018f;
    titleLogo_->SetSize({ 900.0f*scale,132.0f*scale });
    titleLogo_->SetPosition({ 640.0f-450.0f*scale,236.0f-66.0f*scale });
    const int powerCycle = frame_%480;
    const bool voltageDip = powerCycle == 401 || powerCycle == 402 || powerCycle == 409;
    Vector4 logoColor = titleLogo_->GetColor();
    logoColor.w *= voltageDip ? 0.84f : 0.97f+0.03f*std::sin(age*0.018f);
    titleLogo_->SetColor(logoColor);

    // 薄い霧を異なる周期で流し、背景の戦闘が見える濃さを保つ
    titleMist_[0]->SetPosition({ -120.0f+std::sin(age*0.003f)*70.0f,350.0f+std::sin(age*0.002f)*12.0f });
    titleMist_[1]->SetPosition({ -180.0f-std::sin(age*0.0022f)*90.0f,80.0f+std::sin(age*0.0018f)*10.0f });
    for (auto* mist : titleMist_) {
        Vector4 color = mist->GetColor();
        color.w *= 0.85f+0.15f*std::sin(age*0.008f);
        mist->SetColor(color);
    }

    // 選択文字を滑らかに明るくし、下線だけを中央から短く伸ばす
    const float menuFade = SmoothStep01((age-22.0f)/48.0f);
    for (int i = 0; i < 2; ++i) {
        const float target = i == selectedButton_ ? 1.0f : 0.0f;
        buttonSelection_[i] += (target-buttonSelection_[i])*0.16f;
        const float selected = buttonSelection_[i];
        buttonLabels_[i]->SetColor({ 1,1,1,menuFade*(0.42f+0.58f*selected) });
        buttonIndicators_[i]->SetColor({ 0.61f,0.14f,0.14f,menuFade*selected });
        const float width = 26.0f+50.0f*selected;
        buttonUnderlines_[i]->SetPosition({ 640.0f-width*0.5f,kButtonY[i]+40 });
        buttonUnderlines_[i]->SetSize({ width,1 });
        buttonUnderlines_[i]->SetColor({ 0.70f,0.63f,0.61f,menuFade*selected*0.65f });
    }
    for (auto& sprite : uiSprites_) { sprite->Update(); }
}

void TitleScene::SpawnEnemy(float radius) {
    // 画面に出す敵の数を絞り、1体ずつの撃破演出を見やすくする
    if (static_cast<int>(enemies_.size()) >= kMaxEnemies) { return; }
    const float angle = std::uniform_real_distribution<float>(-kPi,kPi)(randomEngine_);
    const Vector3 position{ std::sin(angle)*radius,kEnemyHeight,std::cos(angle)*radius*0.75f };

    // 本編と同じEnemyを使う。中央への巡回点を与え、視界に入ったら本編AIで追跡させる
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(GetContext().object3dCommon, GetContext().camera, position);
    enemy->SetFloorColliders(&floorColliders_);
    enemy->SetBloodParticleSystem(bloodParticleSystem_.get());
    enemy->SetTargetPosition(kPlayerPosition);
    enemy->SetWaypoints({ Vector3{ 0.0f,kEnemyHeight,0.0f } });
    enemies_.push_back(std::move(enemy));
}

void TitleScene::FireWeapon(const Vector3& direction) {
    // 本編と同じ発射処理を武器ごとに呼び分ける
    if (weapon_ == DemoWeapon::Shotgun) {
        FireShotgun(direction);
        fireTimer_ = 46;
    } else {
        // 本編と同じく、連射するほど少しずつ弾をばらけさせる
        const float spread = std::min(kAssaultMaxSpreadAngle,
            static_cast<float>(assaultContinuousShotCount_)*kAssaultSpreadIncrease);
        FireBullet(direction, spread);
        ++assaultContinuousShotCount_;
        fireTimer_ = kAssaultFireInterval;
    }
}

void TitleScene::FireBullet(const Vector3& baseDirection, float spreadAngle) {
    // Player::FireBulletと同じ位置・弾速・ばらけ方で1発撃つ
    Vector3 direction = baseDirection;
    if (spreadAngle > 0.0f) {
        const float angle = std::uniform_real_distribution<float>(-spreadAngle,spreadAngle)(randomEngine_);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        direction = Normalize(Vector3{ direction.x*c-direction.z*s,direction.y,direction.x*s+direction.z*c });
    }
    Vector3 firePosition{ kPlayerPosition.x,kPlayerPosition.y+kBulletSpawnHeight,kPlayerPosition.z };
    firePosition.x += direction.x*kBulletMuzzleDistance;
    firePosition.z += direction.z*kBulletMuzzleDistance;

    auto bullet = std::make_unique<PlayerBullet>();
    bullet->Initialize(GetContext().object3dCommon, firePosition,
        { direction.x*kBulletSpeed,direction.y*kBulletSpeed,direction.z*kBulletSpeed },
        nullptr, particleSystem_.get());
    bullets_.push_back(std::move(bullet));

    StartShockwave(firePosition);
    EmitMuzzleFlash(firePosition, direction, false);
}

void TitleScene::FireShotgun(const Vector3& baseDirection) {
    // Player::FireShotgunと同じ5発の扇状散弾
    const Vector3 firePosition{ kPlayerPosition.x,kPlayerPosition.y+kBulletSpawnHeight,kPlayerPosition.z };
    const float centerIndex = static_cast<float>(kShotgunPelletCount-1)*0.5f;
    const float angleStep = kShotgunSpreadAngle/static_cast<float>(kShotgunPelletCount-1);
    std::uniform_real_distribution<float> randomAngle(-kShotgunRandomSpreadAngle,kShotgunRandomSpreadAngle);
    std::uniform_real_distribution<float> randomSide(-0.18f,0.18f);
    std::uniform_real_distribution<float> randomSpeed(0.85f,1.08f);

    for (int index = 0; index < kShotgunPelletCount; ++index) {
        const float angle = (static_cast<float>(index)-centerIndex)*angleStep+randomAngle(randomEngine_);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const Vector3 dir = Normalize(Vector3{
            baseDirection.x*c-baseDirection.z*s,baseDirection.y,baseDirection.x*s+baseDirection.z*c });
        const Vector3 side{ -dir.z,0.0f,dir.x };
        const float sideOffset = randomSide(randomEngine_);
        const Vector3 pelletPosition{
            firePosition.x+dir.x*kBulletMuzzleDistance+side.x*sideOffset,
            firePosition.y,
            firePosition.z+dir.z*kBulletMuzzleDistance+side.z*sideOffset };
        const float speed = kBulletSpeed*randomSpeed(randomEngine_);

        auto bullet = std::make_unique<PlayerBullet>();
        bullet->Initialize(GetContext().object3dCommon, pelletPosition,
            { dir.x*speed,dir.y*speed,dir.z*speed }, nullptr, particleSystem_.get());
        bullets_.push_back(std::move(bullet));
    }
    // 衝撃波と発射炎は他の銃と同じく銃口の位置から出す
    const Vector3 muzzlePosition{
        firePosition.x+baseDirection.x*kBulletMuzzleDistance,
        firePosition.y,
        firePosition.z+baseDirection.z*kBulletMuzzleDistance };
    // ショットガン全体で1つの衝撃波だけ出す
    StartShockwave(muzzlePosition);
    EmitMuzzleFlash(muzzlePosition, baseDirection, true);
}

void TitleScene::EmitMuzzleFlash(const Vector3& firePosition, const Vector3& direction, bool isShotgun) {
    // 本編のPlayerと同じ発射炎
    if (!particleSystem_) { return; }
    if (isShotgun) {
        particleSystem_->Emit(firePosition, { 1.8f,1.8f,1.8f }, { 0,0,0 }, { 1.0f,0.38f,0.06f,0.48f }, 0.14f);
        particleSystem_->Emit(firePosition, { 0.75f,0.75f,0.75f }, { 0,0,0 }, { 1.0f,0.92f,0.58f,0.9f }, 0.08f);
        return;
    }
    particleSystem_->Emit(firePosition, { 1.2f,1.2f,1.2f }, { 0,0,0 }, { 1.0f,0.34f,0.05f,0.38f }, 0.16f);
    particleSystem_->Emit(firePosition, { 0.68f,0.68f,0.68f }, { 0,0,0 }, { 1.0f,0.72f,0.22f,0.9f }, 0.11f);
    particleSystem_->Emit(firePosition, { 0.30f,0.30f,0.30f }, { 0,0,0 }, { 1.0f,0.95f,0.72f,1.0f }, 0.075f);
    const Vector3 side{ -direction.z,0.0f,direction.x };
    particleSystem_->Emit({ firePosition.x+side.x*0.30f,firePosition.y,firePosition.z+side.z*0.30f },
        { 0.18f,0.18f,0.18f }, { side.x*0.6f,0.0f,side.z*0.6f }, { 1.0f,0.46f,0.08f,0.4f }, 0.08f);
    particleSystem_->Emit({ firePosition.x-side.x*0.30f,firePosition.y,firePosition.z-side.z*0.30f },
        { 0.18f,0.18f,0.18f }, { -side.x*0.6f,0.0f,-side.z*0.6f }, { 1.0f,0.46f,0.08f,0.4f }, 0.08f);
}

void TitleScene::StartShockwave(const Vector3& firePosition) {
    // 本編と同じく、発射位置を画面UVへ変換して画面歪みを出す
    // 暗転中は衝撃波を出さない(フェードの上に白い波が残るため)
    if (!GetContext().offscreenRenderer || !GetContext().camera || isStarting_) { return; }
    Vector2 uv{};
    if (!TryConvertWorldToScreenUV(firePosition, GetContext().camera->GetViewProjectionMatrix(), uv)) { return; }
    GetContext().offscreenRenderer->SetShockwaveDuration(0.16f);
    GetContext().offscreenRenderer->StartShockwave(uv);
}

void TitleScene::CheckBulletHits() {
    // GamePlayScene::CheckCollisionsと同じ命中位置・方向でEnemy::OnHitを呼ぶ
    for (const auto& bullet : bullets_) {
        if (bullet->IsDead()) { continue; }
        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) { continue; }
            const SphereCollider enemyCollider = enemy->GetCollider();
            const SphereCollider bulletCollider = bullet->GetCollider();
            const float dx = enemyCollider.center.x-bulletCollider.center.x;
            const float dy = enemyCollider.center.y-bulletCollider.center.y;
            const float dz = enemyCollider.center.z-bulletCollider.center.z;
            const float radius = enemyCollider.radius+bulletCollider.radius;
            if (dx*dx+dy*dy+dz*dz > radius*radius) { continue; }

            const Vector3 hitDirection = Normalize(bullet->GetVelocity());
            const Vector3 hitPosition{
                enemyCollider.center.x-hitDirection.x*enemyCollider.radius,
                bulletCollider.center.y,
                enemyCollider.center.z-hitDirection.z*enemyCollider.radius };
            bullet->OnHit();
            enemy->OnHit(hitPosition, hitDirection);
            break;
        }
    }
    // 死亡直後には消さず、破片演出が終わってから削除する
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
        [](const std::unique_ptr<Enemy>& enemy) { return enemy->IsReadyToRemove(); }), enemies_.end());
    bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(),
        [](const std::unique_ptr<PlayerBullet>& bullet) { return bullet->IsDead(); }), bullets_.end());
}

void TitleScene::ResolveEnemyOverlap() {
    // 本編と同じく、敵同士が重ならないよう押し出す
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (enemies_[i]->IsDead()) { continue; }
        for (size_t j = i+1; j < enemies_.size(); ++j) {
            if (enemies_[j]->IsDead()) { continue; }
            Vector3 a = enemies_[i]->GetWorldPosition();
            Vector3 b = enemies_[j]->GetWorldPosition();
            float dx = b.x-a.x;
            float dz = b.z-a.z;
            float distanceSq = dx*dx+dz*dz;
            if (distanceSq <= 0.0001f) { dx = 1.0f; dz = 0.0f; distanceSq = 1.0f; }
            const float distance = std::sqrt(distanceSq);
            const float radiusSum = enemies_[i]->GetBodyRadius()+enemies_[j]->GetBodyRadius();
            if (distance >= radiusSum) { continue; }
            const float push = (radiusSum-distance)*0.5f;
            a.x -= dx/distance*push; a.z -= dz/distance*push;
            b.x += dx/distance*push; b.z += dz/distance*push;
            enemies_[i]->SetPosition(a);
            enemies_[j]->SetPosition(b);
        }
    }
}

void TitleScene::UpdateDemo() {
    // 武器は6秒ごとに循環し、切り替え後はすぐ撃てるようにする
    if (++weaponTimer_ >= kWeaponDuration) {
        weaponTimer_ = 0;
        weapon_ = static_cast<DemoWeapon>((static_cast<int>(weapon_)+1)%2);
        fireTimer_ = 0;
        assaultContinuousShotCount_ = 0;
    }
    if (--spawnTimer_ <= 0) {
        const float radius = std::uniform_real_distribution<float>(kEnemySpawnRadiusMin, kEnemySpawnRadiusMax)(randomEngine_);
        SpawnEnemy(radius);
        spawnTimer_ = kSpawnInterval;
    }

    // 敵は本編のEnemy::Updateで動かす(巡回→発見→追跡、死亡時は破片演出)
    for (auto& enemy : enemies_) {
        enemy->SetTargetPosition(kPlayerPosition);
        enemy->Update();
        if (enemy->IsDead()) { continue; }
        // プレイヤーは被弾しないデモなので、近づいた敵は手前に留める
        Vector3 position = enemy->GetWorldPosition();
        const float distance = std::sqrt(position.x*position.x+position.z*position.z);
        if (distance < kEnemyStopDistance && distance > 0.0001f) {
            position.x = position.x/distance*kEnemyStopDistance;
            position.z = position.z/distance*kEnemyStopDistance;
            enemy->SetPosition(position);
            enemy->UpdateRenderOnly();
        }
    }
    ResolveEnemyOverlap();
    for (auto& enemy : enemies_) { if (!enemy->IsDead()) { enemy->UpdateRenderOnly(); } }

    // 一番近い生存中の敵を狙う
    const Enemy* nearest = nullptr;
    float nearestDistance = 10000.0f;
    for (const auto& enemy : enemies_) {
        if (enemy->IsDead()) { continue; }
        const Vector3 p = enemy->GetWorldPosition();
        const float distance = std::sqrt(p.x*p.x+p.z*p.z);
        if (distance < nearestDistance) { nearestDistance = distance; nearest = enemy.get(); }
    }
    if (fireTimer_ > 0) { --fireTimer_; }
    if (nearest && nearestDistance > 0.0001f) {
        const Vector3 p = nearest->GetWorldPosition();
        const Vector3 direction{ p.x/nearestDistance,0.0f,p.z/nearestDistance };
        // 敵の方向へ一瞬で向かず、最短回りで少しずつ向き直る
        const float targetYaw = std::atan2(direction.x,direction.z);
        const float diff = WrapAngle(targetYaw-playerYaw_);
        const float step = std::clamp(diff*kTurnRate,-kMaxTurnSpeed,kMaxTurnSpeed);
        playerYaw_ = WrapAngle(playerYaw_+step);
        // 十分に向き直ってから、今向いている方向へ撃つ
        const float fireRange = weapon_ == DemoWeapon::Shotgun ? 10.0f : 14.0f;
        if (fireTimer_ <= 0 && nearestDistance <= fireRange && std::fabs(diff) <= kFireAngleTolerance) {
            FireWeapon({ std::sin(playerYaw_),0.0f,std::cos(playerYaw_) });
        }
    } else {
        assaultContinuousShotCount_ = 0;
    }

    // プレイヤーの位置は毎フレーム固定し、向きだけを変える
    playerObject_->SetTranslate(kPlayerPosition);
    playerObject_->SetRotate({ 0,playerYaw_,0 });
    playerObject_->Update();

    // 弾は本編のPlayerBullet::Updateで進め、軌跡パーティクルも本編と同じにする
    for (auto& bullet : bullets_) { bullet->Update(); }
    CheckBulletHits();

    const float dt = 1.0f/60.0f;
    particleSystem_->Update(dt);
    bloodParticleSystem_->Update(dt);
}

void TitleScene::Update() {
    // 破片が飛び終わるまではタイトルに留まり、重い本編の初期化を始めない。
    if (isStarting_) {
        if (!GetContext().offscreenRenderer || !GetContext().offscreenRenderer->IsScreenShatterPlaying()) {
            // このフレームで黒画面を表示してから、次の更新で本編を読み込む。
            GetSceneManager()->SetNextScene(std::make_unique<GamePlayScene>());
        }
        return;
    }

    ++frame_;
    UpdateDemo();
    UpdateMenu();
}

void TitleScene::Draw() {
    // 演出終了後に黒画面を一度描き、読み込み中も最後の破片が残らないようにする。
    if (isStarting_ && GetContext().offscreenRenderer && !GetContext().offscreenRenderer->IsScreenShatterPlaying()) {
        const float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        GetContext().dxCommon->GetCommandList()->ClearRenderTargetView(
            GetContext().offscreenRenderer->GetRenderTexture()->GetRTVHandle(), black, 0, nullptr);
        return;
    }

    // 通常時は背景の戦闘だけを描き、波動をかける対象を分離する
    GetContext().object3dCommon->CommonDrawSetting();
    for (auto& object : scenery_) { object->Draw(); }
    // 敵は生存中は本体、撃破後は破片を描画する(本編と同じ)
    for (auto& enemy : enemies_) { enemy->Draw(); }
    playerObject_->Draw();
    // 本編と同じく血しぶきを先に、発光する弾の軌跡を後に描く
    bloodParticleSystem_->Draw();
    particleSystem_->Draw();

    // 開始時は波動を停止済みなので、文字も一緒に保存して画面を割る
    // オフスクリーン描画を使わない場合も、ここでUIを表示する
    if (isStarting_ || !GetContext().offscreenRenderer) {
        DrawTitleUi();
    }
}

void TitleScene::DrawOverlay() {
    // 破壊中は保存した画面に文字が含まれるため、上から重ね直さない
    if (isStarting_ || !GetContext().offscreenRenderer) { return; }

    // エフェクト後の画面を消さず、スプライト用の深度バッファだけを設定し直す
    auto* commandList = GetContext().dxCommon->GetCommandList();
    const auto rtv = GetContext().dxCommon->GetCurrentBackBufferRTVHandle();
    const auto dsv = GetContext().dxCommon->GetDSVHandle();
    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    DrawTitleUi();
}

void TitleScene::DrawTitleUi() {
    // ロゴ・メニュー・霧を同じ順序で重ね、文字を波動で歪ませない
    GetContext().spriteCommon->CommonDrawSetting();
    for (auto& sprite : uiSprites_) { sprite->Draw(); }
}

void TitleScene::Finalize() {
    // デモのリソースを解放してから、ゲームプレイ側に引き渡す
    if (initialized_) {
        ModelManager::GetInstance()->FindModel("cube.obj")->SetTextureIndex(originalCubeTextureIndex_);
    }
    uiSprites_.clear();
    menuVisuals_.clear();
    buttonHitAreas_.fill(nullptr);
    buttonLabels_.fill(nullptr);
    // 所有権は共通配列にあるため、解放後は演出部品の参照だけをクリアする
    buttonIndicators_.fill(nullptr);
    buttonUnderlines_.fill(nullptr);
    buttonSelection_.fill(0.0f);
    titleMist_.fill(nullptr);
    titleLogo_ = nullptr;
    titleShade_ = nullptr;
    bullets_.clear();
    enemies_.clear();
    floorColliders_.clear();
    playerObject_.reset();
    scenery_.clear();
    particleSystem_.reset();
    bloodParticleSystem_.reset();
    directionalLightResource_.Reset();
    initialized_ = false;
}
