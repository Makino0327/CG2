#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <memory>
#include <cmath>
#include <random>

#include "../engine/base/directX/DirectXCommon.h"
#include "../engine/base/winapp/WinApp.h"
#include "../engine/2d/sprite/SpriteCommon.h"
#include "../engine/2d/sprite/Sprite.h"

#include "../engine/3d/obj3d/Object3dCommon.h"
#include "../engine/3d/obj3d/Object3d.h"
#include "../game/camera/Camera.h"
#include "../../game/player/ShootingBloom.h"

#include "../engine/particle/Particle.h"

#include "../engine/3d/skybox/SkyboxCommon.h"
#include "../engine/3d/skybox/Skybox.h"

#include "../engine/base/offscreen/OffscreenRenderer.h"

#include "../engine/input/Input.h"

#include "../result/ResultScene.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

// ワールド座標を画面のUVに変換し、画面内に映っているかを返す。
bool GamePlayScene::TryConvertWorldToScreenUV(
    const Vector3& worldPosition,
    const Matrix4x4& viewProjectionMatrix,
    Vector2& outUV)
{
    // ワールド座標をクリップ座標へ変換する
    const float clipX =
        worldPosition.x * viewProjectionMatrix.m[0][0] +
        worldPosition.y * viewProjectionMatrix.m[1][0] +
        worldPosition.z * viewProjectionMatrix.m[2][0] +
        viewProjectionMatrix.m[3][0];
    const float clipY =
        worldPosition.x * viewProjectionMatrix.m[0][1] +
        worldPosition.y * viewProjectionMatrix.m[1][1] +
        worldPosition.z * viewProjectionMatrix.m[2][1] +
        viewProjectionMatrix.m[3][1];
    const float clipW =
        worldPosition.x * viewProjectionMatrix.m[0][3] +
        worldPosition.y * viewProjectionMatrix.m[1][3] +
        worldPosition.z * viewProjectionMatrix.m[2][3] +
        viewProjectionMatrix.m[3][3];

    // カメラの後ろにある位置は画面UVにできない
    if (clipW <= 0.0f) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;

    // NDC座標をポストエフェクト用の0.0fから1.0fのUVへ変換する
    outUV.x = (ndcX + 1.0f) * 0.5f;
    outUV.y = (1.0f - ndcY) * 0.5f;

    return outUV.x >= 0.0f && outUV.x <= 1.0f && outUV.y >= 0.0f && outUV.y <= 1.0f;
}

// 各進行状態が次の場面を決め、シーン本体は現在の状態へ更新を委譲する。
class GamePlayScene::FlowState {
public:
    virtual ~FlowState() = default;
    virtual void Update(GamePlayScene& owner) const = 0;
    virtual void OnActivated(GamePlayScene& owner) const {
        owner.ChangeFlow(owner.briefingBeforeMission_ ? Flow::Briefing : Flow::Intro);
    }
    virtual void OnDemoResult(GamePlayScene& owner) const { owner.ChangeFlow(Flow::DemoResult); }
    virtual void OnPlayerDied(GamePlayScene& owner, bool updateImmediately) const {
        owner.ChangeFlow(owner.tutorialMode_ ? Flow::TutorialDeath : Flow::Death);
        if (updateImmediately) { owner.flowState_->Update(owner); }
    }
    // 外部のダメージ通知でも、次のフレームの最初に死亡状態へ移す。
    bool UpdateDeathIfNeeded(GamePlayScene& owner) const {
        if (!owner.player_ || !owner.player_->IsDead()) { return false; }
        OnPlayerDied(owner, true);
        return true;
    }
    static void FreezeGeneratorClock(GamePlayScene& owner) {
        owner.generatorPreviousUpdate_ = std::chrono::steady_clock::now();
    }
};

class GamePlayScene::InteractiveFlow : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        if (UpdateDeathIfNeeded(owner)) { return; }
        owner.playerDeadFrames_ = 0;
        if (!owner.IsDemo() && owner.UpdatePauseMenu(1.0f / 60.0f)) {
            owner.resumeFlow_ = this;
            owner.ChangeFlow(Flow::Paused);
            FreezeGeneratorClock(owner);
            return;
        }
        Continue(owner);
    }
    // ポーズ再開のフレームでは入力を二重処理せず、元の状態の続きを進める。
    virtual void Continue(GamePlayScene& owner) const = 0;
};

class GamePlayScene::BriefingFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        if (UpdateDeathIfNeeded(owner)) { return; }
        owner.playerDeadFrames_ = 0;
        owner.introScanFrameOffset_ += 1.0f;
        owner.introScan_.Update(owner.introScanFrameOffset_, 1.0f);
        if (owner.UpdateControlsBriefing(true)) {
            owner.briefingBeforeMission_ = false;
            owner.player_->SuppressFireUntilRelease();
            owner.ChangeFlow(Flow::Intro);
        }
        FreezeGeneratorClock(owner);
    }
};

class GamePlayScene::IntroFlow final : public GamePlayScene::InteractiveFlow {
public:
    void Continue(GamePlayScene& owner) const override {
        owner.UpdateIntroTransition();
        owner.UpdateMissionStart();
        FreezeGeneratorClock(owner);
        if (!owner.missionStartActive_) {
            owner.ChangeFlow(Flow::Playing);
            // 開始演出が終わったフレームから通常更新へ進む。
            owner.UpdatePlaying();
            if (owner.flowState_ != &GetFlowState(Flow::Playing)) { return; }
            if (owner.player_ && owner.player_->IsDead()) { OnPlayerDied(owner, false); }
            else if (owner.completedRun_) { owner.ChangeFlow(Flow::BossDefeat); }
        }
    }
};

class GamePlayScene::PlayingFlow final : public GamePlayScene::InteractiveFlow {
public:
    void Continue(GamePlayScene& owner) const override {
        owner.UpdatePlaying();
        // 更新中に別の状態へ移った場合は、その遷移を上書きしない。
        if (owner.flowState_ != this) { return; }
        if (owner.player_ && owner.player_->IsDead()) { OnPlayerDied(owner, false); }
        else if (owner.completedRun_) { owner.ChangeFlow(Flow::BossDefeat); }
    }
};

class GamePlayScene::PausedFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        if (owner.UpdatePauseMenu(1.0f / 60.0f)) { FreezeGeneratorClock(owner); return; }
        owner.flowState_ = owner.resumeFlow_;
        owner.resumeFlow_->Continue(owner);
    }
};

class GamePlayScene::DeathFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override { owner.UpdateDeathSequence(); }
};

class GamePlayScene::TutorialDeathFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        owner.UpdateTutorialDeath();
        if (!owner.player_->IsDead()) { owner.ChangeFlow(Flow::Playing); }
    }
};

class GamePlayScene::BossDefeatFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        owner.UpdateBossDefeatSequence();
        if (std::all_of(owner.bosses_.begin(), owner.bosses_.end(),
            [](const auto& boss) { return boss->IsReadyToRemove(); })) {
            // ボスの破片が消えたら、この状態がクリア演出へ進める。
            owner.ChangeFlow(Flow::Clear);
            owner.flowState_->Update(owner);
        }
    }
};

class GamePlayScene::ClearFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override { owner.UpdateClearTransition(); }
};

class GamePlayScene::DemoResultFlow final : public GamePlayScene::FlowState {
public:
    void Update(GamePlayScene& owner) const override {
        if (owner.demoResult_->IsDemoRestartReady()) { owner.RestartDemoRound(); }
        else { owner.demoResult_->Update(); }
    }
};

const GamePlayScene::FlowState& GamePlayScene::GetFlowState(Flow flow) {
    // 状態の対応表は遷移時にだけ使い、毎フレームの選択分岐には使わない。
    static const BriefingFlow briefing;
    static const IntroFlow intro;
    static const PlayingFlow playing;
    static const PausedFlow paused;
    static const DeathFlow death;
    static const TutorialDeathFlow tutorialDeath;
    static const BossDefeatFlow bossDefeat;
    static const ClearFlow clear;
    static const DemoResultFlow demoResult;
    const std::array<const FlowState*, 9> states{
        &briefing, &intro, &playing, &paused, &death, &tutorialDeath, &bossDefeat, &clear, &demoResult };
    return *states.at(static_cast<size_t>(flow));
}

GamePlayScene::GamePlayScene() : flowState_(&GetFlowState(Flow::Playing)) {}
void GamePlayScene::ChangeFlow(Flow flow) { flowState_ = &GetFlowState(flow); }
void GamePlayScene::BeginMissionFlow() { flowState_->OnActivated(*this); }
void GamePlayScene::BeginDemoResultFlow() { flowState_->OnDemoResult(*this); }
void GamePlayScene::BeginPlayerDeath(bool updateImmediately) { flowState_->OnPlayerDied(*this, updateImmediately); }

void GamePlayScene::Update() {
    // 複数のフラグから場面を選び直さず、保持している状態を仮想関数で更新する。
    flowState_->Update(*this);
}

void GamePlayScene::UpdatePlaying()
{
    const float dt = 1.0f / 60.0f; // 通常プレイ中の更新間隔。
#ifdef USE_IMGUI
    // F7で通常の被弾を確認できる。無敵時間中はダメージも演出も再発生しない。
    if (!IsDemo() && GetContext().input && GetContext().input->TriggerKey(DIK_F7) && player_) {
        Vector3 attackerPosition = player_->GetWorldPosition();
        attackerPosition.x += 2.0f;
        ApplyPlayerDamage(attackerPosition);
    }
#endif
    // 動作確認用の自動再生とF9からも、通常と同じ倒れる処理へ入る。
    bool previewDeath = deathPreviewFrames_ > 0 && --deathPreviewFrames_ == 0;
#ifdef USE_IMGUI
    previewDeath = previewDeath || (!IsDemo() && GetContext().input && GetContext().input->TriggerKey(DIK_F9));
#endif
    if (previewDeath && player_) {
        player_->Die();
        BeginPlayerDeath(true);
        return;
    }

    // 発電機の15秒は描画フレーム数に依存させず、実際の経過時間で測る
    const auto generatorNow = std::chrono::steady_clock::now();
    const double generatorDeltaSeconds =
        std::chrono::duration<double>(generatorNow - generatorPreviousUpdate_).count();
    generatorPreviousUpdate_ = generatorNow;
    generatorUiIndex_ = -1;
    UpdateIntroTransition();

    // Count down the reload notice display time each frame.
    if (reloadNoticeFrameCount_ > 0) {
        reloadNoticeFrameCount_--;
    }

    // Reload the level JSON manually when F5 is pressed.
    if (!IsDemo() && GetContext().input && GetContext().input->TriggerKey(DIK_F5)) {
        ReloadLevel(true);
    }

    // Mキーを押した瞬間にミニマップの大きさを切り替える
    if (!IsDemo() && GetContext().input &&
        GetContext().input->TriggerKey(DIK_M)) {
        if (minimap_) {
            minimap_->ToggleExpanded();
        }
    }

    // Reload the level automatically after the file update becomes stable.
    if (!IsDemo() && levelHotReload_.Update()) {
        ReloadLevel(false);
    }

    if (skybox_) { skybox_->Update(); }
    if (particleSystem_) { particleSystem_->Update(dt); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(dt); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(dt); }

    // デモ中は移動キーやマウスでデバッグカメラを操作できないようにする。
    if (!IsDemo() && debugCamera_ && GetContext().isDebugMode) {
        debugCamera_->Update(
            GetContext().camera,
            GetContext().input,
            GetContext().offscreenRenderer,
            *GetContext().isDebugMode);
    }

    if (GetContext().isDebugMode && *GetContext().isDebugMode) {
        if (GetContext().camera) {
            GetContext().camera->Update();
        }

        if (player_) {
            player_->UpdateRenderOnly();
        }

        for (auto& enemy : enemies_) {
            enemy->UpdateRenderOnly();
        }

        for (auto& boss : bosses_) {
            // デバッグ中に攻撃の溜めや撃破演出を進めない
            boss->UpdateRenderOnly();
        }

        // デバッグカメラに弾薬の描画だけを追従させる。
        for (auto& drop : ammoDrops_) { drop.object->UpdateTransform(); }
        for (auto& crate : ammoCrates_) { if (crate.hp > 0) { crate.object->UpdateTransform(); } }

        for (auto& floorObject : floorObjects_) {
            floorObject->Update();
        }

        for (auto& wallObject : wallObjects_) {
            wallObject->Update();
        }

        if (skybox_) {
            skybox_->Update();
        }

        return;
    }

    // 説明・開始演出・ポーズ・デバッグカメラを抜けた通常更新で実時間を加算する。
    if (player_ && !tutorialMode_ && demoMode_ != DemoMode::GameOver && !completedRun_) {
        runStats_.clearSeconds += generatorDeltaSeconds;
        if (!bosses_.empty()) { runStats_.bossSeconds += generatorDeltaSeconds; }
    }

    if (player_) {
        // 発射する瞬間に描画されている敵の位置で、マウスの狙い先を調べる。
        std::vector<BoxCollider> aimBoxes;
        std::vector<SphereCollider> aimSpheres;
        aimBoxes.reserve(enemies_.size());
        aimSpheres.reserve(bosses_.size());
        for (const auto& enemy : enemies_) {
            if (!enemy->IsDead()) { aimBoxes.push_back(enemy->GetBulletCollider()); }
        }
        // 木箱の側面を狙ったときも、実際の箱の高さへ銃口を向ける。
        for (const auto& crate : ammoCrates_) {
            if (crate.hp > 0) { aimBoxes.push_back(crate.collider); }
        }
        for (const auto& boss : bosses_) {
            if (!boss->IsDead()) { aimSpheres.push_back(boss->GetCollider()); }
        }
        player_->SetBulletAimTargets(aimBoxes, aimSpheres);
        // 展示中は、モードに合わせた自動操作だけでプレイヤーを動かす。
        if (demoMode_ == DemoMode::Clear) { UpdateClearDemoPlayer(); }
        else if (demoMode_ == DemoMode::GameOver) { player_->UpdateDemo(GetContext().camera, UpdateDemoMovement()); }
        else { player_->Update(GetContext().camera); }

        // Player更新中に発生したグレネード爆発を敵へ反映する
        CheckGrenadeExplosions();
    }
    if (GetContext().camera) { GetContext().camera->Update(); }

    if (player_ && followCamera_) {
        // グレネード爆発通知を受け取ったフレームからカメラを揺らす
        if (player_->ConsumeGrenadeShakeRequest()) {
            followCamera_->StartShake();
        }

        followCamera_->SetTarget(player_->GetWorldPosition());
        followCamera_->Update();
    }

    // カメラ更新後の行列で弾の発射位置に画面歪みを出す
    StartBulletShockwaves();

    for (auto& floorObject : floorObjects_) {
        floorObject->Update();
    }

    for (auto& wallObject : wallObjects_) {
        wallObject->Update();
    }

    // 木箱はワールド座標を保ち、追従カメラ更新後の行列を毎フレーム反映する。
    for (auto& crate : ammoCrates_) {
        if (crate.hp > 0) { crate.object->UpdateTransform(); }
    }

    if (skybox_) {
        skybox_->Update();
    }

    // 発砲したら銃声をゲーム内の「音」として発生させ、範囲内の敵へ伝える
    if (player_ && player_->HasFiredThisFrame()) {
        const Vector3 soundPosition = player_->GetWorldPosition();
        const float soundRange = player_->GetGunshotSoundRange();

        for (const auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            // XZ平面の距離で音が届くかを判定する
            const Vector3 enemyPosition = enemy->GetWorldPosition();
            const float deltaX = enemyPosition.x - soundPosition.x;
            const float deltaZ = enemyPosition.z - soundPosition.z;
            if (deltaX * deltaX + deltaZ * deltaZ <=
                soundRange * soundRange) {
                enemy->OnHearSound(soundPosition);
            }
        }

        // ミニマップの音範囲円を光らせる
        if (minimap_) {
            minimap_->NotifyGunshot();
        }
    }

    for (auto& enemy : enemies_) {
        // 敵の追跡目標を現在のプレイヤー位置に更新する
        enemy->SetTargetPosition(player_->GetWorldPosition());

        // 敵の移動と状態を更新する
        enemy->Update();
    }
    // 全敵の移動が終わってから、一度だけ敵同士の重なりを解消する。
    ResolveEnemyOverlap();

    UpdateBosses();

    UpdateMinimap();

    // ボスのHPに合わせて画面上部のHPバーを更新する
    UpdateBossHpUiSprites();

    // 全敵の視界判定後に近接攻撃を処理する
    if (!IsDemo()) { UpdateMeleeAttack(); }

    CheckCollisions();

    // とどめの弾や、このフレームの撃破・被弾を含めてから戦績を確定する。
    if (!completedRun_ && !bosses_.empty() && player_ && !player_->IsDead() &&
        std::all_of(bosses_.begin(), bosses_.end(), [](const auto& boss) { return boss->IsDead(); })) {
        runStats_.shotsFired = player_->GetShotCount();
        runStats_.shotsHit = player_->GetHitShotCount();
        completedRun_ = runStats_;
        // 撃破と被弾が重なっても、撃破演出には白黒のノイズを持ち越さない。
        if (auto* renderer = GetContext().offscreenRenderer) {
            renderer->ResetDamageFeedback();
            renderer->SetVignetteIntensity(0.0f);
            renderer->SetPostEffectEnabled(PostEffectType::Vignette, false);
            renderer->StopShockwave();
            renderer->SetPostEffectEnabled(PostEffectType::RandomNoise, false);
            if (renderer->GetPostEffectType() == PostEffectType::RandomNoise) {
                renderer->SetPostEffectType(PostEffectType::Copy);
            }
        }
        // 撃破時点で入力を止め、次のフレームからカメラと撃破演出だけを進める。
        return;
    }

    // 回収した予備弾も、そのフレームの左下の表示に反映する。
    UpdateAmmoUiSprites();

    // 移動とダメージ判定の後に、近くの発電機へのE入力を処理する
    if (!IsDemo()) { UpdateGeneratorInteraction(generatorDeltaSeconds); }
    UpdateGeneratorWorld(generatorDeltaSeconds);
    UpdateTeleporterEffects();
    if (tutorialMode_) {
        UpdateTutorial();
        if (tutorialLeaving_) { return; }
    }

    UpdatePlayerScreenEffects();
}

// 全ボスの行動と攻撃演出を更新し、死亡演出が終わったボスを片付ける。
void GamePlayScene::UpdateBosses()
{
    for (auto& boss : bosses_) {
        // 巨大ゾンビが生存中のプレイヤーを追跡して攻撃する
        boss->SetBloodParticleSystem(bloodParticleSystem_.get());
        boss->Update(player_ ? player_->GetWorldPosition() : Vector3{},
            player_ && !player_->IsDead());
        if (boss->IsDeathBurstFrame()) { EmitBossDeathBurst(*boss); }

        // 溜め中は赤い予告円、着地時は外へ広がる土煙色の衝撃を出す
        if (particleSystem_ && (boss->IsWindingUp() || boss->IsImpactFrame())) {
            constexpr int kCirclePoints = 40;
            constexpr float kTwoPi = 6.2831853f;
            const bool impact = boss->IsImpactFrame();
            const Vector3& center = boss->GetAttackCenter();
            for (int i = 0; i < kCirclePoints; ++i) {
                const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kCirclePoints);
                const float x = std::cos(angle);
                const float z = std::sin(angle);
                const Vector3 position{
                    center.x + x * boss->GetAttackRadius(), center.y,
                    center.z + z * boss->GetAttackRadius()
                };
                const Vector3 velocity = impact ? Vector3{ x * 6.0f, 1.5f, z * 6.0f } : Vector3{};
                const Vector4 color = impact ? Vector4{ 1.0f, 0.65f, 0.25f, 0.9f }
                    : Vector4{ 1.0f, 0.08f, 0.03f, 0.8f };
                const float size = impact ? 0.7f : 0.25f;
                particleSystem_->Emit(position, { size, size, size }, velocity, color,
                    impact ? 0.45f : 0.04f);
            }
        }
        if (boss->IsImpactFrame() && followCamera_) {
            // 攻撃が外れても、重い着地の振動をカメラで伝える
            followCamera_->StartShake(0.45f, 18);
        }
    }

    // ボスが複数いる場合、先に倒したボスは破片が消えたあとに片付ける。
    std::erase_if(bosses_, [](const auto& boss) { return boss->IsReadyToRemove(); });
}

// プレイヤーと生存中の敵の位置、銃声が届く範囲をミニマップに反映する。
void GamePlayScene::UpdateMinimap()
{
    // ミニマップへ渡す生存中の敵位置を集める
    std::vector<Vector3> minimapEnemyPositions;

    for (const auto& enemy : enemies_) {
        if (enemy->IsDead()) {
            continue;
        }

        minimapEnemyPositions.push_back(
            enemy->GetWorldPosition());
    }

    // プレイヤーと敵の現在位置をミニマップへ反映する
    if (minimap_ && player_) {
        // 銃声の届く範囲をミニマップの円表示へ渡す
        minimap_->SetSoundRange(player_->GetGunshotSoundRange());
        minimap_->Update(
            player_->GetWorldPosition(),
            minimapEnemyPositions);
    }
}

// プレイヤーのHPや照準状態に合わせて画面の縁と死亡時のぼかしを設定する。
void GamePlayScene::UpdatePlayerScreenEffects()
{
    if (player_ && GetContext().offscreenRenderer) {
        // HPが減るほど赤い縁を濃くする。現在の最大HP3では、残り2で半分、残り1で最大。
        const float missingHp = static_cast<float>(player_->GetMaxHp()-player_->GetHp());
        const float warningRange = static_cast<float>(std::max(1,player_->GetMaxHp()-1));
        GetContext().offscreenRenderer->SetLowHealthWarning(
            player_->IsDead() ? 0.0f : missingHp/warningRange);
        const bool isAimingGun =
            GetContext().input &&
            !IsDemo() &&
            GetContext().input->PushMouseRight() &&
            !player_->IsDead();

        const float targetVignetteIntensity = isAimingGun ? 1.0f : 0.0f;
        const float vignetteFadeSpeed = isAimingGun
            ? aimingVignetteFadeInSpeed_
            : aimingVignetteFadeOutSpeed_;

        // 右クリック状態に合わせてビネットを急に切り替えず、少しずつ近づける
        if (aimingVignetteIntensity_ < targetVignetteIntensity) {
            aimingVignetteIntensity_ = std::min(
                aimingVignetteIntensity_ + vignetteFadeSpeed,
                targetVignetteIntensity);
        } else if (aimingVignetteIntensity_ > targetVignetteIntensity) {
            aimingVignetteIntensity_ = std::max(
                aimingVignetteIntensity_ - vignetteFadeSpeed,
                targetVignetteIntensity);
        }

        GetContext().offscreenRenderer->SetVignetteIntensity(aimingVignetteIntensity_);
        GetContext().offscreenRenderer->SetPostEffectEnabled(
            PostEffectType::Vignette,
            aimingVignetteIntensity_ > 0.0f);

        if (player_->IsDead()) {
            aimingVignetteIntensity_ = 0.0f;
            GetContext().offscreenRenderer->SetVignetteIntensity(0.0f);
            GetContext().offscreenRenderer->SetPostEffectEnabled(PostEffectType::Vignette, false);
            // 死亡した瞬間は通常の色を保ち、次の更新から徐々にぼかす。
            GetContext().offscreenRenderer->SetDeathBlur(0.0f);
            GetContext().offscreenRenderer->SetPostEffectType(PostEffectType::DeathBlur);
        } else if (GetContext().offscreenRenderer->GetPostEffectType() != PostEffectType::Shockwave) {
            // 衝撃波の再生中はCopyへ戻さず、OffscreenRenderer側の終了処理に任せる
            GetContext().offscreenRenderer->SetPostEffectType(PostEffectType::Copy);
        }
    }
}

void GamePlayScene::DrawShadow()
{
    if (demoResult_) { return; }
    // 影の描画では、空・UI・煙・弾の軌跡を含めない。
    if (player_) { player_->Draw(); }
    for (auto& enemy : enemies_) { enemy->Draw(); }
    for (auto& boss : bosses_) { boss->Draw(); }
    for (auto& drop : ammoDrops_) { drop.object->Draw(); }
    for (auto& crate : ammoCrates_) { if (crate.hp > 0) { crate.object->Draw(); } }
    for (auto& floor : floorObjects_) { floor->Draw(); }
    for (auto& wall : wallObjects_) { wall->Draw(); }
}

void GamePlayScene::Draw()
{
    if (demoResult_) { demoResult_->Draw(); return; }
    assert(GetContext().dxCommon);
    assert(GetContext().spriteCommon);
    assert(GetContext().object3dCommon);
    assert(GetContext().particleCommon);

    // 描画直前に銃口の光を設定し、床や壁にも一瞬だけ暖かい光を当てる。
    if (player_) {
        GetContext().object3dCommon->SetMuzzleLight(
            player_->GetMuzzleLightPosition(), { 1.0f, 0.65f, 0.30f },
            ShootingBloom::GetMuzzleLightRadius(), player_->GetMuzzleLightIntensity());
    }

    ID3D12GraphicsCommandList* commandList = GetContext().dxCommon->GetCommandList();
    assert(commandList);

    GetContext().spriteCommon->CommonDrawSetting();

    if (skybox_) {
        skybox_->Draw();
    }

    if (player_) {
        player_->Draw();
    }

    for (auto& enemy : enemies_) {
        enemy->Draw();
    }

    for (auto& boss : bosses_) {
        boss->Draw();
    }

    for (auto& floorObject : floorObjects_) {
        floorObject->Draw();
    }

    for (auto& wallObject : wallObjects_) {
        wallObject->Draw();
    }

    // 撃破位置に残った弾薬をワールド内に描画する。
    for (auto& drop : ammoDrops_) { drop.object->Draw(); }
    // 補給用の木箱は壊れたら描画せず、中から出た弾薬だけを残す。
    for (auto& crate : ammoCrates_) { if (crate.hp > 0) { crate.object->Draw(); } }

    // 床を描いた後に、1秒間の突進予告を赤い帯と矢印で重ねる。
    for (auto& boss : bosses_) { boss->DrawAttackWarning(); }

    // 死亡時の血だまりは、縁が床と混ざるよう床の後に描画する。
    if (player_) {
        player_->DrawBloodPool();
    }

    // 透明な加算パーティクルは床や壁に上書きされないよう最後に描画する
    // 血しぶきは発光する弾エフェクトより先に描画する
    // 煙を発光エフェクトより先に通常アルファ合成で描画する
    if (grenadeSmokeParticleSystem_) {
        grenadeSmokeParticleSystem_->Draw();
    }

    if (bloodParticleSystem_) {
        bloodParticleSystem_->Draw();
    }

    if (particleSystem_) {
        particleSystem_->Draw();
    }

    // 撃破演出中はHUDを隠し、黒帯の間にボスと破片を見せる。
    if (completedRun_) { return; }

    // 死亡後の背景には、ミニマップや残弾などのHUDを残さない。
    if (player_ && player_->IsDead()) {
        // チュートリアルでは、やり直しのお知らせだけは表示する
        if (tutorialMode_) { DrawTutorialNotice(); }
        return;
    }

    // 敵の視界範囲をデバッグ線で描画する
    if (GetContext().camera && line3dCommon_) {
        enemyVisionDebug_.Reset();

        for (const auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            enemy->AppendVisionDebugLines(enemyVisionDebug_);
        }

        enemyVisionDebug_.SetWVP(MakeIdentity4x4(), GetContext().camera->GetViewProjectionMatrix());
        enemyVisionDebug_.Upload();
        enemyVisionDebug_.Draw();
    }
    // 作戦開始中はHUDを隠し、プレイヤーとカメラの動きを見せる。
    if (missionStartActive_) {
        DrawIntroTransition();
        return;
    }

    DrawAmmoDropLabel();

    // 3Dオブジェクトより手前へミニマップを描画する
    if (minimap_) {
        minimap_->Draw();
    }

    // 画面上部にボスHPバーを描画する
    DrawBossHpUiSprites();

    // 左下に残弾UIを描画する
    DrawAmmoUiSprites();

    // 武器名より上に、グレネードのモデルと残数を表示する。
    DrawGrenadeUi();

    // 発電機の操作案内を通常のゲーム画面に重ねる
    DrawGeneratorUi();
    // 戦闘の中心や下中央の起動メーターを避け、画面の端へ案内を置く
    DrawGuideUi();
    // 開始演出のシャッターは、HUDも含めた画面全体の一番手前に描く
    DrawIntroTransition();
}

void GamePlayScene::UpdateWorldRenderOnly()
{
    // 演出中にカメラが動いても、停止中のキャラクターと背景の行列は更新する。
    player_->UpdateRenderOnly();
    for (auto& enemy : enemies_) { enemy->UpdateRenderOnly(); }
    for (auto& boss : bosses_) { boss->UpdateRenderOnly(); }
    for (auto& floor : floorObjects_) { floor->Update(); }
    for (auto& wall : wallObjects_) { wall->Update(); }
    for (auto& crate : ammoCrates_) { if (crate.hp > 0) { crate.object->UpdateTransform(); } }
    for (auto& drop : ammoDrops_) { drop.object->UpdateTransform(); }
    if (skybox_) { skybox_->Update(); }
}

void GamePlayScene::DrawBloom()
{
    if (demoResult_) { return; }
    // 通常の画面が明るくても、発光を指定していない粒子や3Dモデルは対象にしない。
    if (particleSystem_) { particleSystem_->DrawBloom(); }
    if (bloodParticleSystem_) { bloodParticleSystem_->DrawBloom(); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->DrawBloom(); }
}

void GamePlayScene::Finalize()
{
    // デモ用のリザルトは、共通描画リソースより先に片付ける。
    if (demoResult_) { demoResult_->Finalize(); demoResult_.reset(); }
    demoEnemy_ = nullptr;
    // ポーズのSpriteも、参照先のライト用リソースより先に解放する。
    loadQueue_.Clear();
    initializing_ = false;
    preparedLevel_ = PreparedLevel{};
    loadingMinimapObjects_.clear();
    pause_.Reset();
    pauseDim_.reset();
    pauseShade_.reset();
    pausePanel_.reset();
    pauseBlackout_.reset();
    briefingDim_.reset();
    briefingPanel_.reset();
    briefingButton_.reset();
    briefingLine_.reset();
    briefingBeforeMission_ = false;
    briefingInputReady_ = false;
    // 開始演出のSpriteも、参照先のライト用リソースより先に解放する。
    for (auto& door : introDoors_) { door.reset(); }
    for (auto& edge : introDoorEdges_) { edge.reset(); }
    for (auto& glow : introDoorGlows_) { glow.reset(); }
    introScan_.Reset();
    introFlash_.reset();
    missionStartLabel_.reset();
    missionStartLine_.reset();
    for (auto& bar : bossDefeatBars_) { bar.reset(); }
    missionStartActive_ = false;
    missionStartFrame_ = 0;
    for (auto& label : pauseLabels_) { label.reset(); }
    for (auto& row : pauseRows_) { row.reset(); }
    for (auto& marker : pauseMarkers_) { marker.reset(); }
    // チュートリアルの案内も、参照しているライト用リソースより先に解放する
    tutorialPanel_.reset();
    tutorialAccent_.reset();
    tutorialBannerBack_.reset();
    tutorialControls_.reset();
    tutorialControlsAccent_.reset();
    tutorialDivider_.reset();
    for (auto& sprite : tutorialBannerLines_) { sprite.reset(); }
    for (auto& sprite : tutorialTextRows_) { sprite.reset(); }
    for (auto& sprite : tutorialCheckLabels_) { sprite.reset(); }
    for (auto& sprite : tutorialCheckBoxes_) { sprite.reset(); }
    for (auto& sprite : tutorialCheckFills_) { sprite.reset(); }
    for (auto& row : tutorialCheckCounts_) { for (auto& sprite : row) { sprite.reset(); } }
    for (auto& sprite : tutorialStepPips_) { sprite.reset(); }
    for (auto& sprite : tutorialBannerLabels_) { sprite.reset(); }
    // 案内用Spriteも、参照しているライト用リソースより先に解放する
    objectivePanel_.reset();
    objectiveLabel_.reset();
    objectiveCountSprites_.clear();
    // 発電機のUIをライト用リソースより先に解放する
    generatorUiPanel_.reset();
    generatorUiLabel_.reset();
    generatorGaugeFrame_.reset();
    generatorGaugeBackground_.reset();
    generatorGaugeFill_.reset();
    generators_.clear();
    generatorReinforcementSpawns_.clear();
    doors_.clear();
    interactingGeneratorIndex_ = -1;
    generatorUiIndex_ = -1;
    sprites_.clear();
    ammoSprites_.clear();
    // UIのモデルと文字を、参照先の描画リソースより先に解放する。
    grenadeUiModel_.reset();
    for (auto& glyph : grenadeCountSprites_) { glyph.reset(); }
    weaponNameSprite_.reset();
    reloadLabelSprite_.reset();
    for (auto& glyph : reserveAmmoSprites_) { glyph.reset(); }
    ammoDropLabel_.reset();
    bossHpBackgroundSprite_.reset();
    bossHpFillSprite_.reset();
    bossHpFrameSprite_.reset();

    object3d_.reset();
    particleSystem_.reset();
    bloodParticleSystem_.reset();
    grenadeSmokeParticleSystem_.reset();
    skybox_.reset();
    skyboxCommon_.reset();
    // 借用した共通設定は解放せず、参照だけを外す。
    line3dCommon_ = nullptr;

    materialResource_.Reset();
    directionalLightResource_.Reset();

    initialized_ = false;

    debugCamera_.reset();
    player_.reset();
    enemies_.clear();
    ammoDrops_.clear();
    bosses_.clear();
    ammoCrates_.clear();

    // ミニマップが所有するSpriteを解放する
    minimap_.reset();
}

void GamePlayScene::SpawnAmmoDrop(const Vector3& groundPosition, size_t styleIndex)
{
    if (styleIndex >= kAmmoDropStyles.size()) { return; }
    auto object = std::make_unique<Object3d>();
    object->Initialize(GetContext().object3dCommon);
    object->SetModel(kAmmoDropStyles[styleIndex].modelPath);
    object->SetCamera(GetContext().camera);
    object->SetTranslate({ groundPosition.x, groundPosition.y + 0.36f, groundPosition.z });
    object->UpdateTransform();
    ammoDrops_.push_back({ std::move(object), groundPosition.y, styleIndex });
}

void GamePlayScene::DamageAmmoCrate(size_t index, int damage)
{
    if (index >= ammoCrates_.size() || damage <= 0) { return; }
    auto& crate = ammoCrates_[index];
    if (crate.hp <= 0) { return; }
    crate.hp = std::max(0, crate.hp - damage);
    if (crate.hp > 0) { return; }

    // 配列の番号を変えずに壁判定を消し、壊れた木箱の場所を通れるようにする。
    wallColliders_[crate.wallColliderIndex].hasCollider = false;
    const Vector3& center = crate.collider.center;
    const float groundY = center.y - crate.collider.halfSize.y;
    // 1箱につきライフル15発とショットガン3発を、重ならない位置へ出す。
    SpawnAmmoDrop({ center.x - 0.75f, groundY, center.z }, 1);
    SpawnAmmoDrop({ center.x + 0.75f, groundY, center.z }, 2);
    if (particleSystem_) {
        // 茶色い破片を短く散らして、木箱が壊れたことを伝える。
        for (int piece = 0; piece < 16; ++piece) {
            const float angle = static_cast<float>(piece) * 6.2831853f / 16.0f;
            particleSystem_->Emit(center, { 0.25f, 0.25f, 0.25f },
                { std::cos(angle) * 3.0f, 1.5f, std::sin(angle) * 3.0f },
                { 0.65f, 0.35f, 0.12f, 0.9f }, 0.4f);
        }
    }
}

void GamePlayScene::UpdateAmmoDrops()
{
    constexpr float kPickupRadius = 1.25f;
    constexpr float kPickupHeight = 1.5f;
    constexpr float kPickupOffsetY = 0.35f;
    constexpr float kHoverHeight = 0.36f;
    const Vector3 playerPosition = player_->GetWorldPosition();

    // 天面のイラストが逆さにならないよう、水平のまま軽く上下させる。
    for (auto& drop : ammoDrops_) {
        if (drop.pickupDelay > 0) { --drop.pickupDelay; }
        drop.hoverPhase = std::fmod(drop.hoverPhase + 0.05f, 6.2831853f);
        Vector3 position = drop.object->GetTranslate();
        position.y = drop.groundY + kHoverHeight + 0.055f * std::sin(drop.hoverPhase);
        drop.object->SetTranslate(position);
        drop.object->UpdateTransform();
    }
    if (!player_->IsDead()) {
        // 見た目の上下動に関係なく、床を基準に回収できる距離を判定する。
        std::erase_if(ammoDrops_, [&](const AmmoDrop& drop) {
            const Vector3& position = drop.object->GetTranslate();
            const float dx = position.x - playerPosition.x;
            const float dz = position.z - playerPosition.z;
            const float dy = drop.groundY + kPickupOffsetY - playerPosition.y;
            if (drop.pickupDelay > 0 || std::abs(dy) > kPickupHeight ||
                dx * dx + dz * dz > kPickupRadius * kPickupRadius) {
                return false;
            }
            const auto& style = kAmmoDropStyles[drop.styleIndex];
            player_->AddReserveAmmo(style.weapon, style.rounds);
            return true;
        });
    }

    // 撃破ごとにハンドガン50％、ライフル25％、ショットガン25％で弾種を選ぶ。
    for (auto& enemy : enemies_) {
        if (!enemy->ConsumeDeathDrop()) { continue; }
        // 銃・ナイフ・グレネードの撃破を、ドロップと同じタイミングで1回だけ数える。
        ++runStats_.enemiesDefeated;
        static std::mt19937 randomEngine{ std::random_device{}() };
        static std::discrete_distribution<size_t> dropStyle{ 50, 25, 25 };
        const size_t styleIndex = dropStyle(randomEngine);
        const SphereCollider collider = enemy->GetCollider();
        const float groundY = collider.center.y - collider.radius;
        SpawnAmmoDrop({ collider.center.x, groundY, collider.center.z }, styleIndex);
    }

    if (particleSystem_) {
        // 弾種ごとの色で光らせる。回収と同時に光も消す。
        constexpr float kGlowLife = 1.0f / 60.0f;
        constexpr float kGlowStrength = 0.9f;
        for (const auto& drop : ammoDrops_) {
            Vector3 position = drop.object->GetTranslate();
            position.y = drop.groundY + 0.16f;
            if (!particleSystem_->IsSegmentVisible(position, position)) { continue; }
            const auto& style = kAmmoDropStyles[drop.styleIndex];
            particleSystem_->Emit(position, { style.glowSize, style.glowSize, style.glowSize }, {},
                style.glowColor, kGlowLife, kGlowStrength);
        }
    }
}

void GamePlayScene::DrawAmmoDropLabel()
{
    if (!player_ || !ammoDropLabel_ || !GetContext().camera) { return; }

    // ラベルが重ならないよう、回収範囲の手前にある一番近い弾薬だけに出す。
    constexpr float kLabelRange = 4.0f;
    const Vector3 playerPosition = player_->GetWorldPosition();
    const Matrix4x4& viewProjection = GetContext().camera->GetViewProjectionMatrix();
    float nearestDistanceSq = kLabelRange * kLabelRange;
    const AmmoDrop* nearestDrop = nullptr;
    Vector2 labelUV{};
    for (const auto& drop : ammoDrops_) {
        const Vector3& position = drop.object->GetTranslate();
        const float dx = position.x - playerPosition.x;
        const float dy = position.y - playerPosition.y;
        const float dz = position.z - playerPosition.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        Vector2 uv{};
        if (distanceSq < nearestDistanceSq && TryConvertWorldToScreenUV(position, viewProjection, uv)) {
            nearestDistanceSq = distanceSq;
            nearestDrop = &drop;
            labelUV = uv;
        }
    }
    if (!nearestDrop) { return; }

    // アトラスから弾種に合うラベルを選び、モデルの少し下に表示する。
    const auto& style = kAmmoDropStyles[nearestDrop->styleIndex];
    ammoDropLabel_->SetTextureLeftTop({ 64.0f * static_cast<float>(nearestDrop->styleIndex), 0.0f });
    ammoDropLabel_->SetPosition({ labelUV.x * WinApp::kClientWidth,
        labelUV.y * WinApp::kClientHeight + style.labelOffsetY });
    ammoDropLabel_->Update();
    ammoDropLabel_->Draw();
}

void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI
    // ポーズ中にデバッグUIからカメラやゲームの状態を変更しない。
    if (IsPaused()) { return; }
    if (!GetContext().camera) {
        return;
    }

    // ImGuiではコピーを編集し、変更があった時だけセッターでカメラへ反映する
    Transform cameraTransform = GetContext().camera->GetTransform();

    ImGui::Begin("Camera");
    if (ImGui::DragFloat3("Translate", &cameraTransform.translate.x, 0.1f)) {
        GetContext().camera->SetTranslate(cameraTransform.translate);
    }
    if (ImGui::DragFloat3("Rotate", &cameraTransform.rotate.x, 0.01f)) {
        GetContext().camera->SetRotate(cameraTransform.rotate);
    }
    ImGui::Text("Skybox and objects use this camera.");
    ImGui::End();

    ImGui::Begin("Level Reload");
    ImGui::Text("F5 : Manual Reload");
    ImGui::Text("Watching : %s", levelHotReload_.GetFilePath().c_str());

    if (reloadNoticeFrameCount_ > 0) {
        ImGui::Text("%s", reloadNoticeText_.c_str());
    } else {
        ImGui::Text("Idle");
    }

    ImGui::End();

    if (GetContext().offscreenRenderer) {
        // 弾の発射時に出る衝撃波の見た目をゲームシーン側で調整する
        float shockwaveDuration = GetContext().offscreenRenderer->GetShockwaveDuration();
        float shockwaveSize = GetContext().offscreenRenderer->GetShockwaveMaxRadius();
        bool isWhiteWaveEnabled = GetContext().offscreenRenderer->IsShockwaveWhiteWaveEnabled();

        ImGui::Begin("Bullet Shockwave");
        if (ImGui::DragFloat("Duration", &shockwaveDuration, 0.01f, 0.05f, 1.0f)) {
            GetContext().offscreenRenderer->SetShockwaveDuration(shockwaveDuration);
        }
        if (ImGui::DragFloat("Size", &shockwaveSize, 0.01f, 0.05f, 0.40f)) {
            GetContext().offscreenRenderer->SetShockwaveMaxRadius(shockwaveSize);
        }
        if (ImGui::Checkbox("White Wave", &isWhiteWaveEnabled)) {
            GetContext().offscreenRenderer->SetShockwaveWhiteWaveEnabled(isWhiteWaveEnabled);
        }
        ImGui::End();
    }

    if (player_) {
        const char* equipName = "Unknown";
        if (player_->GetAttackMode() == Player::AttackMode::Gun) {
            equipName = "Gun";
        } else if (player_->GetAttackMode() == Player::AttackMode::AssaultRifle) {
            equipName = "Assault Rifle";
        } else if (player_->GetAttackMode() == Player::AttackMode::Shotgun) {
            equipName = "Shotgun";
        } else if (player_->GetAttackMode() == Player::AttackMode::Knife) {
            equipName = "Knife";
        }

        // ゲーム画面左上に現在装備をHUDとして表示する
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin(
            "Player Equipment",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Weapon : %s", equipName);
        ImGui::End();
    }

    if (object3d_) {
        // Object3d全体で共有しているライト設定をImGuiに表示する
        object3d_->DrawLightImGui();
    }

    if (GetContext().offscreenRenderer) {
        GetContext().offscreenRenderer->DrawDebugGameViewImGui();
        GetContext().offscreenRenderer->DrawImGui();
    }
    // 弾の軌跡と発射炎を別々に変更できるようにする。0にするとその対象だけOFFになる。
    ImGui::Begin("Shooting Bloom");
    // 編集用のコピーを使い、変更時に設定クラスで範囲を確認する。
    float bulletTrailStrength = ShootingBloom::GetBulletTrailStrength();
    if (ImGui::SliderFloat("Bullet Trail", &bulletTrailStrength, 0.0f, 3.0f)) {
        ShootingBloom::SetBulletTrailStrength(bulletTrailStrength);
    }
    float muzzleFlashStrength = ShootingBloom::GetMuzzleFlashStrength();
    if (ImGui::SliderFloat("Muzzle Flash", &muzzleFlashStrength, 0.0f, 3.0f)) {
        ShootingBloom::SetMuzzleFlashStrength(muzzleFlashStrength);
    }
    // 発射炎のBloomとは別に、周囲へ当たる実際のライトを調整する。
    float muzzleLightIntensity = ShootingBloom::GetMuzzleLightIntensity();
    if (ImGui::SliderFloat("Muzzle Light Intensity", &muzzleLightIntensity, 0.0f, 20.0f)) {
        ShootingBloom::SetMuzzleLightIntensity(muzzleLightIntensity);
    }
    float muzzleLightRadius = ShootingBloom::GetMuzzleLightRadius();
    if (ImGui::SliderFloat("Muzzle Light Radius", &muzzleLightRadius, 1.0f, 10.0f)) {
        ShootingBloom::SetMuzzleLightRadius(muzzleLightRadius);
    }
    bool previewMuzzleLight = ShootingBloom::IsMuzzleLightPreviewEnabled();
    if (ImGui::Checkbox("Preview Muzzle Light", &previewMuzzleLight)) {
        ShootingBloom::SetMuzzleLightPreviewEnabled(previewMuzzleLight);
    }
    ImGui::End();
#endif
}
