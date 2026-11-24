#include "GamePlayScene.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/base/winapp/WinApp.h"
#include "../../engine/input/Input.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/utils/Logger.h"
#include "../../game/camera/Camera.h"

namespace {

    // プレイヤーの当たり判定から発電機の表面まで届く操作距離
    constexpr float kGeneratorInteractionRange = 2.0f;
    // 起動が完了した後、満タンのメーターを表示しておく秒数
    constexpr double kGeneratorCompleteNoticeSeconds = 3.0;

    // 音声の再生とは独立した、敵の聴覚AIが発電機へ反応する距離。
    constexpr float kGeneratorSoundRadius = 25.0f;
    // 球と箱の当たり判定を調べる
    bool IsSphereHitBox(const SphereCollider& sphere, const Vector3& boxCenter, const Vector3& boxSize)
    {
        const Vector3 halfSize = {
            boxSize.x * 0.5f,
            boxSize.y * 0.5f,
            boxSize.z * 0.5f
        };

        const float closestX = std::clamp(sphere.center.x, boxCenter.x - halfSize.x, boxCenter.x + halfSize.x);
        const float closestY = std::clamp(sphere.center.y, boxCenter.y - halfSize.y, boxCenter.y + halfSize.y);
        const float closestZ = std::clamp(sphere.center.z, boxCenter.z - halfSize.z, boxCenter.z + halfSize.z);

        const float dx = sphere.center.x - closestX;
        const float dy = sphere.center.y - closestY;
        const float dz = sphere.center.z - closestZ;

        return dx * dx + dy * dy + dz * dz <= sphere.radius * sphere.radius;
    }

}

// ワープへの接触を調べ、ステージ移動またはチュートリアルの完了を処理する。
bool GamePlayScene::CheckBossTeleport()
{
    if (!player_) {
        return false;
    }

    const SphereCollider playerCollider = player_->GetCollider();

    for (const BossTeleportData& teleport : bossTeleports_) {
        if (teleport.targetLevel.empty()) {
            continue;
        }

        if (!IsSphereHitBox(playerCollider, teleport.center, teleport.size)) {
            continue;
        }

        // チュートリアルのポータルは別のステージへ移らず、完了の表示を出す
        if (tutorialMode_) {
            if (IsBossTeleportUsable()) { tutorial_.Finish(); }
            return false;
        }

        // 指定されたJSONを監視対象にしてレベルを読み直す
        levelFilePath_ = teleport.targetLevel;
        levelHotReload_.Initialize(levelFilePath_);
        ReloadLevel(false);

        reloadNoticeText_ = "Teleported to " + levelFilePath_;
        reloadNoticeFrameCount_ = 180;
        return true;
    }

    return false;
}

// 発電機の操作案内と進捗ゲージに使うスプライトを用意する。
void GamePlayScene::InitializeGeneratorUi()
{
    // 白い画像に色を付けて、パネルとメーターの各部品を作る
    auto createSprite = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        return sprite;
    };
    generatorUiPanel_ = createSprite("Resources/white2x2.png");
    generatorUiPanel_->SetColor({ 0.02f, 0.025f, 0.03f, 0.88f });
    generatorGaugeFrame_ = createSprite("Resources/white2x2.png");
    generatorGaugeFrame_->SetColor({ 0.75f, 0.75f, 0.65f, 1.0f });
    generatorGaugeBackground_ = createSprite("Resources/white2x2.png");
    generatorGaugeBackground_->SetColor({ 0.08f, 0.09f, 0.1f, 1.0f });
    generatorGaugeFill_ = createSprite("Resources/white2x2.png");

    // 日本語ラベルは画像から切り出し、ImGuiを使わない構成でも表示する
    generatorUiLabel_ = createSprite("Resources/generator/interaction_labels.png");
    generatorUiLabel_->SetTextureSize({ 512.0f, 64.0f });
}

// 操作できる発電機を探し、起動操作と進捗の表示を更新する。
void GamePlayScene::UpdateGeneratorInteraction(double deltaSeconds)
{
    // 死亡中は入力と進捗を停止する。デバッグカメラ中は呼び出し元で更新を止める
    if (!player_ || player_->IsDead()) {
        return;
    }

    // Eを一度押した後は、離れても長押しせずに起動が進む
    if (interactingGeneratorIndex_ >= 0) {
        auto& generator = generators_[interactingGeneratorIndex_];
        if (generator.activation.Update(deltaSeconds)) {
            generatorCompleteNoticeSeconds_ = kGeneratorCompleteNoticeSeconds;
        } else if (generator.activation.GetState() == GeneratorActivation::State::Active) {
            generatorCompleteNoticeSeconds_ = std::max(0.0, generatorCompleteNoticeSeconds_ - deltaSeconds);
        }

        // 増援の確認は起動中だけ行い、完了後はメーターの通知だけを残す。
        const bool isStarting = generator.activation.GetState() == GeneratorActivation::State::Starting;
        if (isStarting) { UpdateGeneratorReinforcements(generator); }
        if (isStarting || generatorCompleteNoticeSeconds_ > 0.0) {
            generatorUiIndex_ = interactingGeneratorIndex_;
            return;
        }
        interactingGeneratorIndex_ = -1;
    }

    // 発電機の大きさが変わっても、表面からの距離で近づいたかを判定する
    const SphereCollider playerCollider = player_->GetCollider();
    const float interactionRadius = playerCollider.radius + kGeneratorInteractionRange;
    float nearestDistanceSq = interactionRadius * interactionRadius;
    int nearestActiveIndex = -1;
    float nearestActiveDistanceSq = nearestDistanceSq;
    for (size_t index = 0; index < generators_.size(); ++index) {
        const auto& generator = generators_[index];
        const Vector3& center = generator.collider.center;
        const Vector3& size = generator.collider.size;
        const float dx = playerCollider.center.x - std::clamp(
            playerCollider.center.x, center.x - size.x * 0.5f, center.x + size.x * 0.5f);
        const float dy = playerCollider.center.y - std::clamp(
            playerCollider.center.y, center.y - size.y * 0.5f, center.y + size.y * 0.5f);
        const float dz = playerCollider.center.z - std::clamp(
            playerCollider.center.z, center.z - size.z * 0.5f, center.z + size.z * 0.5f);
        const float distanceSq = dx * dx + dy * dy + dz * dz;

        // 起動済みの発電機より、操作できる未起動の発電機を優先する
        if (generator.activation.GetState() == GeneratorActivation::State::Idle) {
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                generatorUiIndex_ = static_cast<int>(index);
            }
        } else if (distanceSq <= nearestActiveDistanceSq) {
            nearestActiveDistanceSq = distanceSq;
            nearestActiveIndex = static_cast<int>(index);
        }
    }

    if (generatorUiIndex_ < 0) {
        // 起動済みの発電機に近づいたときは完了表示だけを出す
        generatorUiIndex_ = nearestActiveIndex;
        return;
    }
    if (GetContext().input && GetContext().input->TriggerKey(DIK_E) &&
        generators_[generatorUiIndex_].activation.TryStart()) {
        interactingGeneratorIndex_ = generatorUiIndex_;
        generatorCompleteNoticeSeconds_ = 0.0;
    }
}

// 発電機の起動進捗に合わせて、出現条件を満たした増援を配置する。
void GamePlayScene::UpdateGeneratorReinforcements(GeneratorData& generator)
{
    const auto entry = generatorReinforcementSpawns_.find(generator.name);
    if (entry == generatorReinforcementSpawns_.end()) { return; }
    const float progress = generator.activation.GetProgress();
    if (progress < generator.nextReinforcementCheck) { return; }
    // 見えている地点や塞がれた地点は、毎フレーム調べず少し待ってから確認する。
    constexpr float kCheckInterval = 0.25f / static_cast<float>(GeneratorActivation::kActivationSeconds);
    generator.nextReinforcementCheck = progress + kCheckInterval;

    auto& spawns = entry->second;
    for (size_t index = 0; index < spawns.size(); ++index) {
        auto& spawn = spawns[index];
        // 登録数に応じて起動時間の中に均等に割り振る。各地点は一度だけ使う。
        const float spawnProgress = static_cast<float>(index + 1) / static_cast<float>(spawns.size() + 1);
        if (spawn.processed || progress < spawnProgress) { continue; }

        auto enemy = std::make_unique<Enemy>();
        ConfigureEnemyForLevel(*enemy);
        Vector3 position;
        if (!enemy->TryGetReinforcementPosition(spawn.marker, position)) {
            spawn.processed = true;
            Logger::Log("[GeneratorReinforcement] Invalid spawn position: " + spawn.name);
            continue;
        }

        Vector2 screenUv;
        if (TryConvertWorldToScreenUV(position, GetContext().camera->GetViewProjectionMatrix(), screenUv)) {
            continue;
        }
        const SphereCollider spawnCollider{ position, enemy->GetCollider().radius };
        const bool occupied = std::any_of(enemies_.begin(), enemies_.end(), [&](const auto& other) {
            return !other->IsDead() && Collision::IsHit(spawnCollider, other->GetCollider());
        });
        if (occupied || Collision::IsHit(spawnCollider, player_->GetCollider())) { continue; }

        // 到着済みの敵や、すでに発電機へ向かう敵とは停止位置をずらす。
        std::vector<Vector3> reservedPositions;
        for (const auto& other : enemies_) {
            Vector3 destination;
            if (other->GetGeneratorDestination(destination)) { reservedPositions.push_back(destination); }
        }

        enemy->Initialize(GetContext().object3dCommon, GetContext().camera, position);
        enemy->SetWaypoints({ position });
        enemy->OnHearGenerator(generator.collider.center, generator.collider.size, reservedPositions);
        spawn.processed = true;
        Vector3 destination;
        if (!enemy->GetGeneratorDestination(destination)) {
            Logger::Log("[GeneratorReinforcement] No generator approach: " + spawn.name);
            continue;
        }

        // 経路が見つかった敵を通常の敵として追加し、次の更新から発電機へ向かわせる。
        enemy->SetTargetPosition(player_->GetWorldPosition());
        enemies_.push_back(std::move(enemy));
        return;
    }
}

// 発電機の起動状態に合わせてドアを動かし、近くの敵に発電機の位置を知らせる。
void GamePlayScene::UpdateGeneratorWorld(double deltaSeconds)
{
    // 起動済みの発電機を数え、全台起動したときのドア開放へ渡す。
    size_t activeCount = 0;
    for (const auto& generator : generators_) {
        const auto state = generator.activation.GetState();
        if (state == GeneratorActivation::State::Active) {
            ++activeCount;
        }
    }

    // 全台のメーターが満タンになった時だけ、ステージ内のドアを開く
    generatorDoorLink_.Update(generators_.size(), activeCount, deltaSeconds);
    for (auto& door : doors_) {
        float openProgress = generatorDoorLink_.GetOpenProgress();
        bool isOpen = generatorDoorLink_.IsOpen();
        if (door.tutorialStep > 0) {
            // チュートリアルのドアは、対応する課題を達成した時に1枚ずつ開く
            if (tutorialMode_ && tutorial_.IsDoorOpen(door.tutorialStep) && deltaSeconds > 0.0) {
                door.tutorialOpenSeconds = (std::min)(door.tutorialOpenSeconds + deltaSeconds,
                    GeneratorDoorLink::kOpenSeconds);
            }
            openProgress = static_cast<float>(door.tutorialOpenSeconds / GeneratorDoorLink::kOpenSeconds);
            isOpen = door.tutorialOpenSeconds >= GeneratorDoorLink::kOpenSeconds;
        }
        Vector3 position = door.closedPosition;
        position.y += door.liftDistance * openProgress;
        door.object->SetTranslate(position);
        door.object->Update();
        // 配列要素は消さず、開き切ったら当たり判定を無効にする
        auto& collider = wallColliders_[door.colliderIndex];
        collider.center = door.closedColliderCenter;
        collider.center.y += door.liftDistance * openProgress;
        collider.hasCollider = !isOpen;
    }
    if (minimap_) {
        minimap_->SetDoorsOpen(tutorialMode_ ? tutorial_.IsFinished() : generatorDoorLink_.IsOpen());
    }

    // 聞こえる中で最も近い発電機を知らせ、音声出力の有無によらず敵が反応する
    std::vector<Vector3> reservedGeneratorPositions;
    for (const auto& enemy : enemies_) {
        Vector3 destination;
        if (enemy->GetGeneratorDestination(destination)) {
            reservedGeneratorPositions.push_back(destination);
        }
    }
    for (const auto& enemy : enemies_) {
        Vector3 destination;
        if (enemy->IsDead() || enemy->GetGeneratorDestination(destination)) {
            continue;
        }
        const Vector3 position = enemy->GetWorldPosition();
        const GeneratorData* nearestGenerator = nullptr;
        float nearestDistanceSq = kGeneratorSoundRadius * kGeneratorSoundRadius;
        for (const auto& generator : generators_) {
            if (generator.activation.GetState() == GeneratorActivation::State::Idle) {
                continue;
            }
            const float dx = position.x - generator.collider.center.x;
            const float dz = position.z - generator.collider.center.z;
            const float distanceSq = dx * dx + dz * dz;
            if (distanceSq <= nearestDistanceSq) {
                nearestDistanceSq = distanceSq;
                nearestGenerator = &generator;
            }
        }
        if (nearestGenerator) {
            enemy->OnHearGenerator(nearestGenerator->collider.center, nearestGenerator->collider.size,
                reservedGeneratorPositions);
            if (enemy->GetGeneratorDestination(destination)) {
                reservedGeneratorPositions.push_back(destination);
            }
        }
    }
}

// ドアの開放状況やチュートリアルの進行からワープの利用条件を調べる。
bool GamePlayScene::IsBossTeleportUsable() const
{
    // チュートリアルでは、発電機の課題を達成した後から踏める
    if (tutorialMode_) {
        return tutorial_.GetClearedStep() >= TutorialFlow::kStepCount - 1;
    }
    // ドアも発電機も無いステージでは最初から踏める。ある場合は全ドアが開き切ってから
    if (doors_.empty() && generators_.empty()) {
        return true;
    }
    return generatorDoorLink_.IsOpen();
}

// 利用可能なワープの演出とミニマップの表示を更新する。
void GamePlayScene::UpdateTeleporterEffects()
{
    const bool isUsable = !bossTeleports_.empty() && IsBossTeleportUsable();
    if (minimap_) {
        minimap_->SetTeleportersActive(isUsable);
    }
    if (!isUsable || !particleSystem_) {
        teleporterEffectTime_ = 0.0f;
        return;
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;
    constexpr float kTwoPi = 6.28318530f;
    // 螺旋の本数・回転速度・上昇速度
    constexpr int kSpiralArmCount = 3;
    constexpr float kSpinSpeed = 4.0f;
    constexpr float kRiseSpeed = 1.6f;
    constexpr float kLifeTime = 1.1f;
    teleporterEffectTime_ += kDeltaTime;

    for (const BossTeleportData& teleport : bossTeleports_) {
        // テレポーターの外周を回る半径と、足元の高さ
        const float radius = std::max(teleport.size.x, teleport.size.z) * 0.5f + 0.3f;
        const float baseY = teleport.center.y - teleport.size.y * 0.5f + 0.1f;

        for (int arm = 0; arm < kSpiralArmCount; ++arm) {
            // 発生位置を毎フレーム回転させ、上へ昇るくるくるした螺旋を作る
            const float angle = teleporterEffectTime_ * kSpinSpeed +
                kTwoPi * static_cast<float>(arm) / static_cast<float>(kSpiralArmCount);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const Vector3 position{ teleport.center.x + c * radius, baseY, teleport.center.z + s * radius };
            // 回転方向(接線)へ少し流しながら上昇させる
            const Vector3 velocity{ -s * radius * 1.2f, kRiseSpeed, c * radius * 1.2f };

            particleSystem_->Emit(position, { 0.45f, 0.45f, 0.45f }, velocity,
                { 0.2f, 0.6f, 1.0f, 0.85f }, kLifeTime);
            // 中心に白い光を重ねて青い粒を目立たせる
            particleSystem_->Emit(position, { 0.2f, 0.2f, 0.2f }, velocity,
                { 0.85f, 0.95f, 1.0f, 1.0f }, kLifeTime * 0.8f);
        }
    }
}

// 発電機の操作案内と起動進捗のゲージを描画する。
void GamePlayScene::DrawGeneratorUi()
{
    if (generatorUiIndex_ < 0 || !player_ || player_->IsDead() ||
        (GetContext().isDebugMode && *GetContext().isDebugMode)) {
        return;
    }

    const auto& activation = generators_[generatorUiIndex_].activation;
    const bool isIdle = activation.GetState() == GeneratorActivation::State::Idle;
    const bool isActive = activation.GetState() == GeneratorActivation::State::Active;
    const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
    const float topY = static_cast<float>(WinApp::kClientHeight) - 150.0f;

    // 画面下中央に操作案内を表示し、起動を始めたらその下へメーターを出す
    GetContext().spriteCommon->CommonDrawSetting();
    generatorUiPanel_->SetPosition({ centerX - 200.0f, topY });
    generatorUiPanel_->SetSize({ 400.0f, isIdle ? 64.0f : 100.0f });
    generatorUiPanel_->Update();
    generatorUiPanel_->Draw();

    // ラベル画像の上から、操作案内・起動中・起動完了を切り替える
    const float labelRow = isIdle ? 0.0f : (isActive ? 2.0f : 1.0f);
    generatorUiLabel_->SetTextureLeftTop({ 0.0f, labelRow * 64.0f });
    generatorUiLabel_->SetPosition({ centerX - 180.0f, topY + 10.0f });
    generatorUiLabel_->SetSize({ 360.0f, 45.0f });
    generatorUiLabel_->SetColor(isActive
        ? Vector4{ 0.3f, 1.0f, 0.45f, 1.0f } : Vector4{ 1.0f, 0.9f, 0.2f, 1.0f });
    generatorUiLabel_->Update();
    generatorUiLabel_->Draw();

    if (!isIdle) {
        // メーターは左から右へ伸び、起動完了時は満タンの緑色にする
        generatorGaugeFrame_->SetPosition({ centerX - 162.0f, topY + 66.0f });
        generatorGaugeFrame_->SetSize({ 324.0f, 18.0f });
        generatorGaugeFrame_->Update();
        generatorGaugeFrame_->Draw();
        generatorGaugeBackground_->SetPosition({ centerX - 160.0f, topY + 68.0f });
        generatorGaugeBackground_->SetSize({ 320.0f, 14.0f });
        generatorGaugeBackground_->Update();
        generatorGaugeBackground_->Draw();
        if (activation.GetProgress() > 0.0f) {
            generatorGaugeFill_->SetPosition({ centerX - 160.0f, topY + 68.0f });
            generatorGaugeFill_->SetSize({ 320.0f * activation.GetProgress(), 14.0f });
            generatorGaugeFill_->SetColor(isActive
                ? Vector4{ 0.2f, 0.9f, 0.35f, 1.0f } : Vector4{ 1.0f, 0.85f, 0.1f, 1.0f });
            generatorGaugeFill_->Update();
            generatorGaugeFill_->Draw();
        }
    }
}
