#include "GamePlayScene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/base/winapp/WinApp.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/utils/Logger.h"
#include "../../game/camera/Camera.h"
#include "../../game/collision/BodyContact.h"
#include "../SceneManager.h"
#include "../clear/ClearScene.h"
#include "../gameover/GameOverScene.h"
#include "../result/ResultScene.h"

namespace {
    // 合わせ目の緑の線が左右へ伸びる時間と、扉が開き切るまでの時間（60FPS）
    constexpr int kIntroSeamFrames = 8;
    constexpr int kIntroOpenFrames = 28;
    constexpr int kShutterFrames = kIntroSeamFrames + kIntroOpenFrames;
    // シャッターが開き始めてから少し接写を見せ、その後90フレームで引く。
    constexpr int kMissionPullbackStart = 24;
    constexpr int kMissionPullbackFrames = 90;
    constexpr int kMissionEndFrame = kMissionPullbackStart + kMissionPullbackFrames;
}

// プレイヤーが倒れた後のぼかしと画面キャプチャを進め、ゲームオーバー画面を開く。
void GamePlayScene::UpdateDeathSequence()
{
    ++playerDeadFrames_;
    // 確認時に、死亡演出へ一度だけ入ったことが分かるようにする。
    if (playerDeadFrames_ == 1) { Logger::Log("[DeathSequence] Collapse started."); }
    // 敵とカメラはその場所に止め、プレイヤーが倒れる動作だけを進める。
    player_->Update(GetContext().camera);
    constexpr float dt = 1.0f/60.0f;
    if (particleSystem_) { particleSystem_->Update(dt); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(dt); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(dt); }
    generatorPreviousUpdate_ = std::chrono::steady_clock::now();
    auto* renderer = GetContext().offscreenRenderer;
    if (renderer) {
        // 倒れる動作へ入ったら赤い縁を徐々に消し、視界のぼかしへつなぐ。
        renderer->SetLowHealthWarning(0.0f);
        const float progress = std::clamp((static_cast<float>(playerDeadFrames_)-18.0f)/108.0f,0.0f,1.0f);
        renderer->SetDeathBlur(progress*progress*(3.0f-2.0f*progress));
        renderer->SetPostEffectType(PostEffectType::DeathBlur);
        renderer->SetVignetteIntensity(0.0f);
        renderer->SetPostEffectEnabled(PostEffectType::Vignette,false);
        renderer->StopShockwave();
        // 倒れ終わった画面を描画してから保存し、保存完了の次フレームで遷移する。
        if (playerDeadFrames_ >= 149 && !renderer->HasDeathSceneCapture()) {
            renderer->RequestDeathSceneCapture();
        }
    }
    if (playerDeadFrames_ >= 150 && (!renderer || renderer->HasDeathSceneCapture())) {
        if (IsDemo()) {
            ShowDemoResult(std::make_unique<GameOverScene>());
            Logger::Log("[GameOverDemo] Game over displayed.");
        } else {
            GetSceneManager()->SetNextScene(std::make_unique<GameOverScene>());
        }
    }
}

// 展示デモの種類に合わせて、開始状態と敵の配置を準備する。
void GamePlayScene::PrepareDemoRound()
{
    demoFrames_ = 0;
    ++demoRound_;
    demoEnemy_ = nullptr;
    if (GetContext().isDebugMode) { *GetContext().isDebugMode = false; }
    if (demoMode_ == DemoMode::Clear) {
        // ボス戦の終盤から始め、実際に当たった最後の数発で撃破する。
        constexpr int kDemoBossHp = 3;
        runStats_ = {};
        player_->ResetDemoShooting();
        for (auto& boss : bosses_) {
            for (int hp = boss->GetHp(); hp > kDemoBossHp; --hp) { boss->OnHit(); }
        }
        Logger::Log("[ClearDemo] Boss round started: " + std::to_string(demoRound_));
        return;
    }
    const Vector3 origin = player_->GetWorldPosition();
    Vector3 spawn = origin;
    float bestDistance = 0.0f;
    // 周囲の8方向を壁と床に沿って調べ、展示用の敵を離れた見える場所へ置く。
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * 0.78539816f;
        const Vector3 movement{ std::sin(angle) * 5.0f, 0.0f, std::cos(angle) * 5.0f };
        const Vector3 candidate = BodyContact::Move(origin, movement, 1.0f, &wallColliders_, &floorColliders_);
        // 壁で横滑りした候補は使わず、接近できる直線上の床だけを選ぶ。
        if (std::abs(candidate.x - origin.x - movement.x) > 0.1f ||
            std::abs(candidate.z - origin.z - movement.z) > 0.1f) { continue; }
        bool onFloor = false;
        for (const auto& floor : floorColliders_) {
            if (floor.hasCollider && floor.type == "BOX" &&
                std::abs(candidate.x-floor.center.x) < floor.size.x*0.5f &&
                std::abs(candidate.z-floor.center.z) < floor.size.z*0.5f) { onFloor = true; break; }
        }
        if (!onFloor) { continue; }
        const float distance = movement.x * movement.x + movement.z * movement.z;
        if (distance > bestDistance) { spawn = candidate; bestDistance = distance; }
    }
    if (bestDistance > 0.0f) {
        // ダメージは演出で減らさず、通常の敵AIと接触判定に任せる。
        auto enemy = std::make_unique<Enemy>();
        enemy->Initialize(GetContext().object3dCommon, GetContext().camera, spawn);
        ConfigureEnemyForLevel(*enemy);
        enemy->SetWaypoints({ spawn });
        demoEnemy_ = enemy.get();
        enemies_.push_back(std::move(enemy));
    } else {
        // 狭い開始地点では、元の配置の最寄りの敵へ近づく。
        float nearest = 1.0e20f;
        for (const auto& enemy : enemies_) {
            const Vector3 position = enemy->GetWorldPosition();
            const float distance = (position.x-origin.x)*(position.x-origin.x) +
                (position.z-origin.z)*(position.z-origin.z);
            if (distance < nearest) { nearest = distance; demoEnemy_ = enemy.get(); }
        }
    }
    Logger::Log("[GameOverDemo] Round started: " + std::to_string(demoRound_));
}

// ゲームオーバーデモで、プレイヤーを敵へ近づける移動量を求める。
Vector3 GamePlayScene::UpdateDemoMovement()
{
    ++demoFrames_;
    if (!demoEnemy_) { return {}; }
    // 最初は少し横へ歩き、その後に敵へ近づいて通常の攻撃を受ける。
    if (demoFrames_ <= 90) {
        return { demoFrames_ <= 45 ? 0.035f : -0.035f, 0.0f, 0.012f };
    }
    if (demoFrames_ == 91) { demoEnemy_->OnHearSound(player_->GetWorldPosition()); }
    const Vector3 target = demoEnemy_->GetWorldPosition();
    const Vector3 position = player_->GetWorldPosition();
    const Vector3 direction{ target.x-position.x, 0.0f, target.z-position.z };
    const float distance = std::sqrt(direction.x*direction.x + direction.z*direction.z);
    if (distance <= 0.8f) { return {}; }
    return { direction.x/distance*0.065f, 0.0f, direction.z/distance*0.065f };
}

// クリアデモで、プレイヤーの照準とボスへの射撃を制御する。
void GamePlayScene::UpdateClearDemoPlayer()
{
    ++demoFrames_;
    const auto activeBoss = std::find_if(bosses_.begin(), bosses_.end(),
        [](const auto& boss) { return !boss->IsDead(); });
    if (activeBoss == bosses_.end()) {
        player_->UpdateDemo(GetContext().camera, {});
        return;
    }

    // 構えが終わってから、同じボスを狙ってハンドガンを一定間隔で撃つ。
    constexpr int kFirstShotFrame = 42;
    constexpr int kShotInterval = 18;
    const bool shoot = demoFrames_ >= kFirstShotFrame &&
        (demoFrames_ - kFirstShotFrame) % kShotInterval == 0;
    const Vector3 target = (*activeBoss)->GetCollider().center;
    player_->UpdateDemo(GetContext().camera, {}, &target, shoot);
}

// 展示デモの結果画面を初期化し、プレイ画面に重ねるために保持する。
void GamePlayScene::ShowDemoResult(std::unique_ptr<ResultScene> result)
{
    // シーンを破棄せずにリザルトを再生し、暗転後に同じボス部屋を読み直す。
    result->SetContext(GetContext());
    result->SetSceneManager(GetSceneManager());
    result->SetDemoMode(true);
    result->Initialize();
    demoResult_ = std::move(result);
    BeginDemoResultFlow();
}

// 結果画面を閉じ、ステージとカメラを初期状態に戻してデモを再開する。
void GamePlayScene::RestartDemoRound()
{
    // 共有のモデルやプレイヤーは保ち、配置と体力だけを毎回戻す。
    demoResult_->Finalize();
    demoResult_.reset();
    demoEnemy_ = nullptr;
    playerDeadFrames_ = 0;
    ReloadLevel(false);
    // 前周の被弾によるカメラの揺れと追従途中の位置をリセットする。
    followCamera_->Initialize(GetContext().camera);
    aimingVignetteIntensity_ = 0.0f;
    if (GetContext().offscreenRenderer) {
        GetContext().offscreenRenderer->ResetDamageFeedback();
        GetContext().offscreenRenderer->SetDeathBlur(0.0f);
    }
    OnActivated(0.0f);
    Logger::Log(demoMode_ == DemoMode::Clear ? "[ClearDemo] Restart completed." : "[GameOverDemo] Restart completed.");
}

// シャッターを閉じ、クリア結果を表示する。
void GamePlayScene::UpdateClearTransition()
{
    if (clearShutterFrame_ == kShutterFrames) {
        auto result = std::make_unique<ClearScene>(*completedRun_);
        if (demoMode_ == DemoMode::Clear) {
            ShowDemoResult(std::move(result));
            Logger::Log("[ClearDemo] Clear results displayed.");
        } else {
            GetSceneManager()->SetNextScene(std::move(result));
        }
        return;
    }

    // ミッション開始で使う扉・緑の縁・走査線を、同じフレーム数で逆再生する。
    UpdateShutterFrame(kShutterFrames - ++clearShutterFrame_);
}

// ボス撃破時の黒帯、カメラ、死亡演出を進める。
void GamePlayScene::UpdateBossDefeatSequence()
{
    // 上下の黒帯を短くせり出させ、戦闘を止めたままボスを中央へ寄せる。
    constexpr float kBarHeight = 84.0f;
    constexpr int kBarOpenFrames = 14;
    constexpr float kDeltaTime = 1.0f / 60.0f;
    const float progress = std::min(static_cast<float>(++bossDefeatFrame_) / kBarOpenFrames, 1.0f);
    const float height = kBarHeight * progress * progress * (3.0f - 2.0f * progress);
    bossDefeatBars_[0]->SetPosition({ 0.0f, 0.0f });
    bossDefeatBars_[1]->SetPosition({ 0.0f, WinApp::kClientHeight - height });
    for (auto& bar : bossDefeatBars_) {
        bar->SetSize({ static_cast<float>(WinApp::kClientWidth), height });
        bar->Update();
    }

    if (particleSystem_) { particleSystem_->Update(kDeltaTime); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(kDeltaTime); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(kDeltaTime); }
    for (auto& boss : bosses_) {
        boss->SetBloodParticleSystem(bloodParticleSystem_.get());
        boss->Update(bossDefeatFocus_, false);
        if (boss->IsDeathBurstFrame()) { EmitBossDeathBurst(*boss); }
    }
    if (followCamera_) {
        followCamera_->SetTarget(bossDefeatFocus_);
        followCamera_->Update();
    }
    UpdateWorldRenderOnly();

    // クリア演出へ進むかどうかは、ボス撃破の状態クラスが判断する。
}

// ボスの撃破位置にパーティクルを出し、カメラを揺らす。
void GamePlayScene::EmitBossDeathBurst(const Boss& boss)
{
    // ボスの中心から暗赤色の血しぶきと短い赤橙色の爆発を出す。
    constexpr int kBloodCount = 96;
    constexpr float kTwoPi = 6.2831853f;
    const SphereCollider body = boss.GetCollider();
    Vector3 origin = body.center;
    origin.y += body.radius * 0.34f;
    static std::mt19937 burstRandom{ std::random_device{}() };
    std::uniform_real_distribution<float> variation(0.0f, 1.0f);
    for (int index = 0; index < kBloodCount && bloodParticleSystem_; ++index) {
        const float angle = kTwoPi * (static_cast<float>(index) + variation(burstRandom)) / kBloodCount;
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        const float speed = 4.5f + 8.0f * variation(burstRandom);
        const float size = 0.42f + 0.96f * variation(burstRandom);
        const Vector3 spawn{ origin.x + x * 0.54f, origin.y + variation(burstRandom), origin.z + z * 0.54f };
        // 暗めの赤を混ぜ、透け方を少し抑えて血の色を濃くする。
        const Vector4 color = index % 3 == 0 ? Vector4{ 0.66f, 0.018f, 0.010f, 0.77f }
            : Vector4{ 0.32f, 0.004f, 0.003f, 0.83f };
        bloodParticleSystem_->Emit(spawn, { size, size, size },
            { x * speed, 0.2f + 4.0f * variation(burstRandom), z * speed }, color,
            0.46f + 0.38f * variation(burstRandom));
    }
    if (bloodParticleSystem_) {
        // 上下にも全方向へ飛ぶ血を、細かい飛沫から大きな塊まで大きさを変えて重ねる。
        struct BloodLayer { int count; float minSize; float maxSize; float minSpeed; float maxSpeed; float life; };
        constexpr BloodLayer kLayers[] = {
            { 140, 0.12f, 0.35f, 7.0f, 15.0f, 0.55f }, // 遠くまで飛ぶ細かい飛沫
            { 90, 0.45f, 1.1f, 3.5f, 9.0f, 0.85f },    // 中くらいのしずく
            { 28, 1.4f, 2.6f, 1.0f, 3.5f, 1.2f },      // 中心付近に残る大きな塊
        };
        for (const BloodLayer& layer : kLayers) {
            for (int index = 0; index < layer.count; ++index) {
                // 球面上のランダムな向きを作り、上向きを少し強めて噴き上がって見せる。
                const float theta = kTwoPi * variation(burstRandom);
                const float y = variation(burstRandom) * 1.6f - 0.6f;
                const float ring = std::sqrt((std::max)(0.0f, 1.0f - (std::min)(y * y, 1.0f)));
                const Vector3 direction{ std::cos(theta) * ring, y, std::sin(theta) * ring };
                const float speed = layer.minSpeed + (layer.maxSpeed - layer.minSpeed) * variation(burstRandom);
                const float size = layer.minSize + (layer.maxSize - layer.minSize) * variation(burstRandom);
                const Vector3 spawn{ origin.x + direction.x * body.radius * 0.4f,
                    origin.y + direction.y * body.radius * 0.4f, origin.z + direction.z * body.radius * 0.4f };
                const Vector4 color = index % 3 == 0 ? Vector4{ 0.70f, 0.02f, 0.012f, 0.8f }
                    : Vector4{ 0.30f, 0.004f, 0.003f, 0.86f };
                bloodParticleSystem_->Emit(spawn, { size, size, size },
                    { direction.x * speed, direction.y * speed + 1.0f, direction.z * speed }, color,
                    layer.life * (0.75f + 0.6f * variation(burstRandom)));
            }
        }
        // 敵の撃破と同じく、中央にしばらく残る大きな血煙を置く。
        const float clump = body.radius * 1.1f;
        bloodParticleSystem_->Emit(origin, { clump, clump, clump }, { 0.0f, 0.3f, 0.0f },
            { 0.42f, 0.005f, 0.003f, 0.72f }, 1.5f);
    }
    if (particleSystem_) {
        const float size = body.radius * 1.35f;
        particleSystem_->Emit(origin, { size, size, size }, {}, { 0.95f, 0.29f, 0.08f, 0.68f }, 0.14f, 0.9f);
    }
    if (followCamera_) { followCamera_->StartShake(0.72f, 18); }
}

// ミッション開始時のカメラ移動と開始ラベルを更新する。
void GamePlayScene::UpdateMissionStart()
{
    ++missionStartFrame_;
    const float progress = std::clamp(
        static_cast<float>(missionStartFrame_ - kMissionPullbackStart) / kMissionPullbackFrames, 0.0f, 1.0f);
    // 最初は速く引き、通常のカメラ位置へ近づくほどゆっくり止まる。
    const float ease = 1.0f - std::pow(1.0f - progress, 3.0f);
    GetContext().camera->SetTranslate(Lerp(missionCameraFrom_, missionCameraTo_, ease));
    GetContext().camera->SetRotate(missionCameraRotate_);
    GetContext().camera->Update();

    // 文字はカメラが引き始める頃に現れ、引き終わる前に消える。
    const float fadeIn = std::clamp(static_cast<float>(missionStartFrame_ - kMissionPullbackStart) / 16.0f, 0.0f, 1.0f);
    const float fadeOut = std::clamp(static_cast<float>(kMissionEndFrame - missionStartFrame_) / 24.0f, 0.0f, 1.0f);
    const float alpha = fadeIn * fadeOut;
    const float scale = 1.04f - 0.04f * ease;
    missionStartLabel_->SetPosition({ WinApp::kClientWidth * 0.5f, 225.0f });
    missionStartLabel_->SetSize({ 640.0f * scale, 100.0f * scale });
    missionStartLabel_->SetColor({ 0.96f, 0.96f, 0.94f, alpha });
    missionStartLabel_->Update();
    missionStartLine_->SetPosition({ WinApp::kClientWidth * 0.5f, 267.0f });
    missionStartLine_->SetSize({ 480.0f * (1.0f - std::pow(1.0f - fadeIn, 3.0f)), 2.0f });
    missionStartLine_->SetColor({ 0.70f, 0.12f, 0.12f, alpha * 0.85f });
    missionStartLine_->Update();

    // カメラ行列だけは全モデルへ反映し、位置やAIの時間は動かさない。
    player_->SuppressFireUntilRelease();
    UpdateWorldRenderOnly();
    constexpr float dt = 1.0f / 60.0f;
    if (particleSystem_) { particleSystem_->Update(dt); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(dt); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(dt); }
    // 通常の追従位置へ完全に到着してから、操作と敵の更新を始める。
    if (missionStartFrame_ >= kMissionEndFrame) { missionStartActive_ = false; }
}

// 開始時のシャッターを開き、光の強さを更新する。
void GamePlayScene::UpdateIntroTransition()
{
    if (introFrame_ > kShutterFrames) {
        return;
    }
    // 読み込み直後の長いフレームで飛ばないよう、時間ではなくフレーム数で進める
    UpdateShutterFrame(++introFrame_);
    // 白緑の閃光は、扉を開ける演出で重ねる。
    const float openAge = static_cast<float>(std::max(0, introFrame_-kIntroSeamFrames));
    const float flash = introFrame_ >= kIntroSeamFrames ? 0.24f*std::exp(-openAge*0.32f) : 0.0f;
    introFlash_->SetColor({ 0.65f, 1.0f, 0.76f, flash });
    introFlash_->Update();
}

// 指定されたフレームに合わせて、シャッターと光る縁の位置や見た目を更新する。
void GamePlayScene::UpdateShutterFrame(int frame)
{
    const float seam = std::clamp(static_cast<float>(frame) / kIntroSeamFrames, 0.0f, 1.0f);
    const float open = std::clamp(static_cast<float>(frame - kIntroSeamFrames) / kIntroOpenFrames, 0.0f, 1.0f);
    // 線は勢いよく伸びて止まり、扉は重さを感じるように動き出しと止まりをゆっくりにする
    const float seamEase = 1.0f - (1.0f - seam) * (1.0f - seam) * (1.0f - seam);
    const float openEase = open < 0.5f
        ? 4.0f * open * open * open
        : 1.0f - std::pow(-2.0f * open + 2.0f, 3.0f) * 0.5f;
    // 線の太さぶん余分に開き、最後に画面へ線が残らないようにする
    const float centerY = WinApp::kClientHeight * 0.5f;
    const float gap = openEase * (centerY + 4.0f);
    // 中央の線は扉を開く時点から出し、扉が画面外へ出る頃には消す。
    const float edgeAlpha = frame >= kIntroSeamFrames
        ? 1.0f-std::clamp((open-0.6f)/0.4f, 0.0f, 1.0f) : 0.0f;
    const float edgeWidth = WinApp::kClientWidth * seamEase;
    for (size_t i = 0; i < introDoors_.size(); ++i) {
        const float y = i == 0 ? centerY - gap : centerY + gap;
        introDoors_[i]->SetPosition({ 0.0f, y });
        introDoors_[i]->Update();
        introDoorEdges_[i]->SetPosition({ WinApp::kClientWidth * 0.5f, y });
        introDoorEdges_[i]->SetSize({ edgeWidth, 3.0f });
        // 発電機の起動完了と同じ緑で、合わせ目を光らせる
        introDoorEdges_[i]->SetColor({ 0.3f, 1.0f, 0.45f, edgeAlpha });
        introDoorEdges_[i]->Update();
        // 細い芯の周囲へ薄い緑を重ね、重い扉の縁が発光しているように見せる。
        introDoorGlows_[i]->SetPosition({ WinApp::kClientWidth*0.5f, y });
        introDoorGlows_[i]->SetSize({ edgeWidth, 22.0f });
        introDoorGlows_[i]->SetColor({ 0.12f, 0.8f, 0.3f, edgeAlpha*0.18f });
        introDoorGlows_[i]->Update();
    }
    // 扉が閉じているほど走査線を濃くする。
    introScan_.Update(introScanFrameOffset_ + static_cast<float>(frame), 1.0f - openEase);
}

// シャッター本体と光る縁を描画する。
void GamePlayScene::DrawShutter()
{
    for (auto& door : introDoors_) { door->Draw(); }
    for (auto& glow : introDoorGlows_) { glow->Draw(); }
    for (auto& edge : introDoorEdges_) { edge->Draw(); }
    introScan_.Draw();
}

// 開始演出のシャッターとフラッシュを画面に描画する。
void GamePlayScene::DrawIntroTransition()
{
    if (introFrame_ > kShutterFrames || !introDoors_[0]) {
        return;
    }
    GetContext().spriteCommon->CommonDrawSetting();
    DrawShutter();
    introFlash_->Draw();
}
