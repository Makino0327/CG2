#include "GamePlayScene.h"
#include "../title/TitleScene.h"
#include "../SceneManager.h"
#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/2d/texture/TextureManager.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/base/winapp/WinApp.h"
#include "../../engine/particle/Particle.h"
#include "../../game/camera/Camera.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
    constexpr const char* kTutorialLevelPath = "Resources/level/tutorial.json";
    constexpr const char* kTutorialMinimapPath = "Resources/level/tutorial_minimap.json";
    constexpr const char* kTutorialLabels = "Resources/hud/tutorial_labels.png";
    constexpr const char* kControlsGuide = "Resources/hud/controls_guide.png";
    constexpr Vector2 kControlsGuideSize{ 324.0f, 256.0f };

    // tutorial_labels.png は横1024px・1行64pxの文字画像を縦に並べている。
    // 課題ごとに「見出し・説明1・説明2」の3行、その後にチェック項目とお知らせが続く。
    constexpr float kRowWidth = 1024.0f;
    constexpr float kRowHeight = 64.0f;
    constexpr int kCheckLabelRow = TutorialFlow::kStepCount * 3;
    enum CheckLabel { kCheckWalk, kCheckDefeat, kCheckPickup, kCheckGuns, kCheckGenerator, kCheckPortal, kCheckCount };
    constexpr int kBannerRow = kCheckLabelRow + kCheckCount;
    enum BannerLabel { kBannerClear, kBannerDefeated, kBannerFinished,
        kBannerNextRoom, kBannerRetry, kBannerBackToTitle };

    // 左上の課題パネルの配置
    constexpr float kPanelX = 20.0f;
    constexpr float kPanelY = 20.0f;
    constexpr float kPanelWidth = 560.0f;
    constexpr float kCheckTop = 132.0f;
    constexpr float kCheckSpacing = 30.0f;

    // 光っている目印に、このXZ距離まで近づいたら到着とする
    constexpr float kGoalRadius = 2.0f;
    // 倒れる動作を見せてから入口へ戻すまで
    constexpr int kDefeatedResetFrames = 80;

    // 名前の接頭辞の後ろにある番号を読む。合わなければ-1
    int ReadNumberAfter(const std::string& name, const char* prefix)
    {
        const size_t length = std::strlen(prefix);
        if (name.rfind(prefix, 0) != 0 || name.size() <= length) { return -1; }
        return std::atoi(name.c_str() + length);
    }

    // 開始演出・ポーズ画面と同じ、白と緑を中心にした配色
    constexpr Vector4 kUiGreen{ 0.3f, 1.0f, 0.45f, 1.0f };        // 達成・強調(シャッターの線と同じ)
    constexpr Vector4 kUiDeepGreen{ 0.16f, 0.72f, 0.34f, 1.0f };  // 進行中(ポーズの選択印と同じ)
    constexpr Vector4 kUiWhite{ 0.95f, 0.97f, 0.96f, 1.0f };
    constexpr Vector4 kUiSubText{ 0.78f, 0.84f, 0.80f, 1.0f };
    constexpr Vector4 kUiPanel{ 0.008f, 0.016f, 0.012f, 0.82f };  // 緑がかった黒
    constexpr Vector4 kUiInactive{ 0.22f, 0.29f, 0.25f, 1.0f };
    constexpr Vector4 kUiDanger{ 0.95f, 0.32f, 0.28f, 1.0f };     // やられた時だけ赤で知らせる

    Vector4 WithAlpha(Vector4 color, float alpha)
    {
        color.w *= alpha;
        return color;
    }

    float SmoothStep(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }
}

void GamePlayScene::EnableTutorial()
{
    tutorialMode_ = true;
    // 読み込み前に指定された時は、本編の準備が終わってから切り替える
    if (!initialized_) { return; }
    // タイトルで準備済みの本編を使い回し、モデルやUIを読み直さずにステージだけ替える。
    levelFilePath_ = kTutorialLevelPath;
    if (minimap_) {
        minimap_->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), kTutorialMinimapPath);
    }
    levelHotReload_.Initialize(levelFilePath_);
    ReloadLevel(false);
    // 弾切れで課題を進められなくならないよう、ハンドガンは何度でもリロードできる
    player_->SetUnlimitedHandgunReserve(true);
    // グレネードも弾切れを気にせず練習できるようにする。
    player_->SetUnlimitedGrenades(true);
    reloadNoticeText_ = "Tutorial loaded";
}

std::unique_ptr<Enemy> GamePlayScene::CreateLevelEnemy(const std::string& name, const Vector3& position,
    const std::vector<Vector3>& waypoints)
{
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(GetContext().object3dCommon, GetContext().camera, position);
    ConfigureEnemyForLevel(*enemy);
    enemy->SetWaypoints(waypoints);
    ConfigureTutorialEnemy(*enemy, name);
    return enemy;
}

void GamePlayScene::ConfigureTutorialEnemy(Enemy& enemy, const std::string& name)
{
    // Enemy_T2_00 なら2番目の課題の敵。ナイフ・射撃・グレネードの練習用なので動かさない。
    const int step = ReadNumberAfter(name, "Enemy_T");
    if (step <= 0) { return; }
    enemy.SetTutorialStep(step);
    enemy.SetPassive(true);
}

void GamePlayScene::ResetTutorial(const std::vector<LevelObjectData>& allObjects)
{
    tutorial_.Reset();
    tutorialFiredWeapons_ = 0;
    tutorialDeathFrames_ = 0;
    tutorialLeaving_ = false;
    tutorialCheckpoints_.clear();
    tutorialAmmoTotal_ = 0;

    // やり直し地点の高さは、ステージに置いたプレイヤーの高さに合わせる
    float playerY = 1.0f;
    for (const auto& object : allObjects) {
        if (object.objectKind == "player" || object.name == "Player") { playerY = object.translation.y; }
    }

    // 床の上面の高さ。弾薬を床に置くために使う
    auto groundHeight = [this](float x, float z) {
        float top = 0.0f;
        bool found = false;
        for (const auto& floor : floorColliders_) {
            if (!floor.hasCollider || std::abs(x - floor.center.x) > floor.size.x * 0.5f ||
                std::abs(z - floor.center.z) > floor.size.z * 0.5f) { continue; }
            const float floorTop = floor.center.y + floor.size.y * 0.5f;
            if (!found || floorTop > top) { top = floorTop; found = true; }
        }
        return top;
    };

    for (const auto& object : allObjects) {
        const int checkpoint = ReadNumberAfter(object.name, "TutorialCheckpoint_");
        if (checkpoint > 0) {
            tutorialCheckpoints_[checkpoint] = { object.translation.x, playerY, object.translation.z };
            continue;
        }
        if (object.name == "TutorialGoal_1") {
            tutorialGoal_ = object.translation;
            continue;
        }
        // 弾薬の種類は、ラベル画像と同じ順番(ハンドガン・ライフル・ショットガン)で選ぶ
        size_t styleIndex = kAmmoDropStyles.size();
        if (object.name == "TutorialAmmo_Handgun") { styleIndex = 0; }
        if (object.name == "TutorialAmmo_AssaultRifle") { styleIndex = 1; }
        if (object.name == "TutorialAmmo_Shotgun") { styleIndex = 2; }
        if (styleIndex >= kAmmoDropStyles.size()) { continue; }

        constexpr float kHoverHeight = 0.36f;
        const float groundY = groundHeight(object.translation.x, object.translation.z);
        auto model = std::make_unique<Object3d>();
        model->Initialize(GetContext().object3dCommon);
        model->SetModel(kAmmoDropStyles[styleIndex].modelPath);
        model->SetCamera(GetContext().camera);
        model->SetTranslate({ object.translation.x, groundY + kHoverHeight, object.translation.z });
        model->UpdateTransform();
        AmmoDrop drop{ std::move(model), groundY, styleIndex };
        drop.pickupDelay = 0;
        drop.tutorial = true;
        ammoDrops_.push_back(std::move(drop));
        ++tutorialAmmoTotal_;
    }
}

int GamePlayScene::CountTutorialEnemies(int step, bool aliveOnly) const
{
    if (!aliveOnly) {
        const auto total = tutorialEnemyTotals_.find(step);
        return total == tutorialEnemyTotals_.end() ? 0 : total->second;
    }
    return static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(),
        [step](const std::unique_ptr<Enemy>& enemy) {
            return !enemy->IsDead() && enemy->GetTutorialStep() == step;
        }));
}

int GamePlayScene::GetTutorialChecks(std::array<TutorialCheck, 2>& checks) const
{
    const int step = tutorial_.GetStep();
    // 倒した数 = 最初の数 - 生きている数
    auto defeated = [this](int enemyStep, int label) {
        const int total = CountTutorialEnemies(enemyStep, false);
        const int value = total - CountTutorialEnemies(enemyStep, true);
        return TutorialCheck{ label, value, total, total > 0 && value >= total };
    };

    switch (step) {
    case 1: {
        const Vector3 position = player_->GetWorldPosition();
        const float dx = position.x - tutorialGoal_.x;
        const float dz = position.z - tutorialGoal_.z;
        checks[0] = { kCheckWalk, 0, 0, dx * dx + dz * dz <= kGoalRadius * kGoalRadius };
        return 1;
    }
    case 2: checks[0] = defeated(2, kCheckDefeat); return 1;
    case 3: {
        const size_t remaining = static_cast<size_t>(std::count_if(ammoDrops_.begin(), ammoDrops_.end(),
            [](const AmmoDrop& drop) { return drop.tutorial; }));
        const int picked = static_cast<int>(tutorialAmmoTotal_ - std::min(remaining, tutorialAmmoTotal_));
        const int total = static_cast<int>(tutorialAmmoTotal_);
        checks[0] = { kCheckPickup, picked, total, picked >= total };
        const int guns = std::popcount(tutorialFiredWeapons_);
        checks[1] = { kCheckGuns, guns, 3, guns >= 3 };
        return 2;
    }
    case 4: checks[0] = defeated(4, kCheckDefeat); return 1;
    case 5: checks[0] = defeated(5, kCheckDefeat); return 1;
    case 6: {
        const bool active = std::any_of(generators_.begin(), generators_.end(), [](const GeneratorData& generator) {
            return generator.activation.GetState() == GeneratorActivation::State::Active;
        });
        checks[0] = { kCheckGenerator, 0, 0, active };
        return 1;
    }
    default:
        checks[0] = { kCheckPortal, 0, 0, tutorial_.IsFinished() };
        return 1;
    }
}

void GamePlayScene::UpdateTutorial()
{
    tutorial_.Update();
    if (tutorial_.IsReadyToLeave()) {
        // 完了の表示を見せ終えたらタイトルへ戻る
        if (!tutorialLeaving_) {
            tutorialLeaving_ = true;
            GetSceneManager()->SetNextScene(std::make_unique<TitleScene>());
        }
        return;
    }

    const int step = tutorial_.GetStep();
    // 弾薬の課題では、撃った銃の種類を覚えておく
    if (step == 3 && player_->HasFiredThisFrame()) {
        switch (player_->GetAttackMode()) {
        case Player::AttackMode::Gun: tutorialFiredWeapons_ |= 1u; break;
        case Player::AttackMode::AssaultRifle: tutorialFiredWeapons_ |= 2u; break;
        case Player::AttackMode::Shotgun: tutorialFiredWeapons_ |= 4u; break;
        default: break;
        }
    }

    if (tutorial_.IsAcceptingProgress() && step < TutorialFlow::kStepCount) {
        std::array<TutorialCheck, 2> checks{};
        const int count = GetTutorialChecks(checks);
        const bool allDone = std::all_of(checks.begin(), checks.begin() + count,
            [](const TutorialCheck& check) { return check.done; });
        if (allDone) { tutorial_.CompleteStep(); }
    }

    // 向かう場所を光で示す。最初の課題は到着地点、発電機の課題は起動前の発電機
    if (step == 1 && !tutorial_.IsStepCleared(1)) {
        EmitTutorialMarker(tutorialGoal_, 1.4f, { 0.3f, 1.0f, 0.45f, 0.85f });
    }
    if (step == 6) {
        for (const auto& generator : generators_) {
            if (generator.activation.GetState() != GeneratorActivation::State::Idle) { continue; }
            Vector3 center = generator.collider.center;
            center.y -= generator.collider.size.y * 0.5f;
            EmitTutorialMarker(center, 2.8f, { 0.3f, 1.0f, 0.45f, 0.8f });
        }
    }
}

void GamePlayScene::UpdateTutorialDeath()
{
    if (tutorialDeathFrames_ == 0) { tutorial_.ShowRetry(TutorialFlow::Notice::Defeated); }
    ++tutorialDeathFrames_;
    // 敵とカメラは止めたまま、倒れる動作とお知らせの時間だけを進める
    player_->Update(GetContext().camera);
    tutorial_.Update();
    constexpr float dt = 1.0f / 60.0f;
    if (particleSystem_) { particleSystem_->Update(dt); }
    if (bloodParticleSystem_) { bloodParticleSystem_->Update(dt); }
    if (grenadeSmokeParticleSystem_) { grenadeSmokeParticleSystem_->Update(dt); }
    generatorPreviousUpdate_ = std::chrono::steady_clock::now();
    if (auto* renderer = GetContext().offscreenRenderer) {
        renderer->SetLowHealthWarning(0.0f);
        renderer->SetVignetteIntensity(0.0f);
        renderer->SetPostEffectEnabled(PostEffectType::Vignette, false);
    }
    if (tutorialDeathFrames_ < kDefeatedResetFrames) { return; }

    tutorialDeathFrames_ = 0;
    RespawnTutorialPlayer();
}

void GamePlayScene::RespawnTutorialPlayer()
{
    const auto checkpoint = tutorialCheckpoints_.find(tutorial_.GetStep());
    if (checkpoint != tutorialCheckpoints_.end()) { player_->SetSpawnPosition(checkpoint->second); }
    player_->Respawn();
    player_->SuppressFireUntilRelease();
    aimingVignetteIntensity_ = 0.0f;
    if (auto* renderer = GetContext().offscreenRenderer) {
        renderer->ResetDamageFeedback();
        renderer->SetDeathBlur(0.0f);
        renderer->SetPostEffectType(PostEffectType::Copy);
    }
    if (followCamera_) { followCamera_->SetTarget(player_->GetWorldPosition()); }
}

void GamePlayScene::EmitTutorialMarker(const Vector3& center, float radius, const Vector4& color)
{
    if (!particleSystem_) { return; }
    // テレポーターと同じ螺旋の光を、色を変えて足元から上げる
    constexpr int kArmCount = 3;
    constexpr float kTwoPi = 6.28318530f;
    const float time = static_cast<float>(tutorial_.GetStepAge()) / 60.0f;
    for (int arm = 0; arm < kArmCount; ++arm) {
        const float angle = time * 3.0f + kTwoPi * static_cast<float>(arm) / static_cast<float>(kArmCount);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const Vector3 position{ center.x + c * radius, 0.1f, center.z + s * radius };
        const Vector3 velocity{ -s * radius, 1.2f, c * radius };
        particleSystem_->Emit(position, { 0.35f, 0.35f, 0.35f }, velocity, color, 0.9f);
    }
}

void GamePlayScene::InitializeTutorialUi()
{
    // 描画中に初めて読み込むと描画設定が失われるため、先に読み込んでおく
    TextureManager::GetInstance()->LoadTexture(kTutorialLabels);
    TextureManager::GetInstance()->LoadTexture("Resources/hud/count_glyphs.png");
    auto create = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        return sprite;
    };
    tutorialPanel_ = create("Resources/white2x2.png");
    tutorialAccent_ = create("Resources/white2x2.png");
    tutorialBannerBack_ = create("Resources/white2x2.png");
    tutorialDivider_ = create("Resources/white2x2.png");
    tutorialControlsAccent_ = create("Resources/white2x2.png");
    for (auto& line : tutorialBannerLines_) {
        line = create("Resources/white2x2.png");
        line->SetAnchorPoint({ 0.5f, 0.5f });
    }
    for (auto& row : tutorialTextRows_) {
        row = create(kTutorialLabels);
        row->SetTextureSize({ kRowWidth, kRowHeight });
        row->SetSize({ kRowWidth * 0.5f, kRowHeight * 0.5f });
    }
    for (size_t i = 0; i < tutorialCheckLabels_.size(); ++i) {
        tutorialCheckLabels_[i] = create(kTutorialLabels);
        tutorialCheckLabels_[i]->SetTextureSize({ kRowWidth, kRowHeight });
        tutorialCheckLabels_[i]->SetSize({ kRowWidth * 0.5f, kRowHeight * 0.5f });
        tutorialCheckBoxes_[i] = create("Resources/white2x2.png");
        tutorialCheckFills_[i] = create("Resources/white2x2.png");
        for (auto& digit : tutorialCheckCounts_[i]) {
            digit = create("Resources/hud/count_glyphs.png");
            digit->SetTextureSize({ 32.0f, 64.0f });
            digit->SetSize({ 13.0f, 26.0f });
        }
    }
    for (auto& pip : tutorialStepPips_) { pip = create("Resources/white2x2.png"); }
    for (auto& label : tutorialBannerLabels_) {
        // お知らせの文字は画像と同じ大きさで、画面中央に表示する
        label = create(kTutorialLabels);
        label->SetAnchorPoint({ 0.5f, 0.0f });
        label->SetTextureSize({ kRowWidth, kRowHeight });
        label->SetSize({ kRowWidth, kRowHeight });
    }
    // 右下の操作説明。画像は2倍の解像度なので半分の大きさで表示する
    TextureManager::GetInstance()->LoadTexture(kControlsGuide);
    tutorialControls_ = create(kControlsGuide);
    tutorialControls_->SetSize(kControlsGuideSize);
    tutorialControls_->SetPosition({ static_cast<float>(WinApp::kClientWidth) - kControlsGuideSize.x - 20.0f,
        static_cast<float>(WinApp::kClientHeight) - kControlsGuideSize.y - 20.0f });
    tutorialControls_->Update();
    // 操作説明の左端にも、課題パネルと同じ緑の線を引く
    tutorialControlsAccent_->SetPosition(tutorialControls_->GetPosition());
    tutorialControlsAccent_->SetSize({ 3.0f, kControlsGuideSize.y });
    tutorialControlsAccent_->SetColor(kUiDeepGreen);
    tutorialControlsAccent_->Update();
}

void GamePlayScene::DrawTutorialUi()
{
    if (!tutorialPanel_) { return; }
    GetContext().spriteCommon->CommonDrawSetting();
    if (tutorial_.IsFinished()) {
        DrawTutorialNotice();
        return;
    }
    // 操作説明はチュートリアルの間ずっと右下に出しておく
    tutorialControls_->Draw();
    tutorialControlsAccent_->Draw();

    const int step = tutorial_.GetStep();
    std::array<TutorialCheck, 2> checks{};
    const int checkCount = GetTutorialChecks(checks);
    const bool cleared = tutorial_.IsStepCleared(step);

    // 課題が切り替わったら、左から少し滑り込ませて表示する
    const float appear = SmoothStep(static_cast<float>(tutorial_.GetStepAge()) / 18.0f);
    const float alpha = appear;
    const float offsetX = (1.0f - appear) * -24.0f;
    auto place = [](Sprite& sprite, Vector2 position, Vector4 color) {
        sprite.SetPosition(position);
        sprite.SetColor(color);
        sprite.Update();
        sprite.Draw();
    };

    // 緑がかった黒いパネルと、左端の緑の線。達成すると明るい緑に変わる
    const float panelHeight = kCheckTop - kPanelY + static_cast<float>(checkCount) * kCheckSpacing + 8.0f;
    tutorialPanel_->SetSize({ kPanelWidth, panelHeight });
    place(*tutorialPanel_, { kPanelX + offsetX, kPanelY }, WithAlpha(kUiPanel, alpha));
    tutorialAccent_->SetSize({ 4.0f, panelHeight });
    place(*tutorialAccent_, { kPanelX + offsetX, kPanelY }, WithAlpha(cleared ? kUiGreen : kUiDeepGreen, alpha));
    // 説明とチェック項目の間に、薄い緑の区切り線を引く
    tutorialDivider_->SetSize({ kPanelWidth - 40.0f, 1.0f });
    place(*tutorialDivider_, { kPanelX + 20.0f + offsetX, kCheckTop - 4.0f }, WithAlpha(kUiDeepGreen, alpha * 0.55f));

    // 右上に全体の進み具合を課題の数だけ四角で出す。済みは緑、今は白
    for (size_t i = 0; i < tutorialStepPips_.size(); ++i) {
        const int pipStep = static_cast<int>(i) + 1;
        Vector4 color = WithAlpha(kUiInactive, alpha);
        if (tutorial_.IsStepCleared(pipStep)) { color = WithAlpha(kUiGreen, alpha); }
        else if (pipStep == step) { color = WithAlpha(kUiWhite, alpha); }
        tutorialStepPips_[i]->SetSize({ 10.0f, 10.0f });
        const float x = kPanelX + kPanelWidth - 18.0f - static_cast<float>(tutorialStepPips_.size() - i) * 14.0f;
        place(*tutorialStepPips_[i], { x + offsetX, kPanelY + 14.0f }, color);
    }

    // 見出しと2行の説明
    const float textY[] = { kPanelY + 6.0f, kPanelY + 48.0f, kPanelY + 74.0f };
    for (size_t i = 0; i < tutorialTextRows_.size(); ++i) {
        const float row = static_cast<float>((step - 1) * 3 + static_cast<int>(i));
        tutorialTextRows_[i]->SetTextureLeftTop({ 0.0f, row * kRowHeight });
        place(*tutorialTextRows_[i], { kPanelX + 20.0f + offsetX, textY[i] },
            WithAlpha(i == 0 ? kUiWhite : kUiSubText, alpha));
    }

    // チェック項目。白い枠の四角は、達成すると緑で塗りつぶす。文字と数も緑に変わる
    for (int i = 0; i < checkCount; ++i) {
        const TutorialCheck& check = checks[i];
        const float y = kCheckTop + static_cast<float>(i) * kCheckSpacing;
        const Vector4 color = WithAlpha(check.done ? kUiGreen : kUiWhite, alpha);
        tutorialCheckBoxes_[i]->SetSize({ 14.0f, 14.0f });
        place(*tutorialCheckBoxes_[i], { kPanelX + 22.0f + offsetX, y + 9.0f },
            check.done ? color : WithAlpha(kUiSubText, alpha));
        tutorialCheckFills_[i]->SetSize({ 10.0f, 10.0f });
        place(*tutorialCheckFills_[i], { kPanelX + 24.0f + offsetX, y + 11.0f },
            check.done ? color : Vector4{ kUiPanel.x, kUiPanel.y, kUiPanel.z, alpha });
        tutorialCheckLabels_[i]->SetTextureLeftTop({ 0.0f, static_cast<float>(kCheckLabelRow + check.label) * kRowHeight });
        place(*tutorialCheckLabels_[i], { kPanelX + 46.0f + offsetX, y }, color);
        if (check.target <= 0) { continue; }
        // 「1/3」の3文字を、数字画像(0〜9と/)から切り出して並べる
        const int glyphs[] = { std::min(check.value, 9), 10, std::min(check.target, 9) };
        for (size_t g = 0; g < 3; ++g) {
            auto& digit = *tutorialCheckCounts_[i][g];
            digit.SetTextureLeftTop({ static_cast<float>(glyphs[g]) * 32.0f, 0.0f });
            const float x = kPanelX + kPanelWidth - 60.0f + static_cast<float>(g) * 13.0f;
            place(digit, { x + offsetX, y + 3.0f }, color);
        }
    }

    DrawTutorialNotice();
}

void GamePlayScene::DrawTutorialNotice()
{
    const TutorialFlow::Notice notice = tutorial_.GetNotice();
    if (notice == TutorialFlow::Notice::None || !tutorialBannerBack_) { return; }
    GetContext().spriteCommon->CommonDrawSetting();

    // 出始めと消え際をなめらかにする
    const float elapsed = static_cast<float>(tutorial_.GetNoticeDuration() - tutorial_.GetNoticeFrames());
    const float alpha = SmoothStep(elapsed / 10.0f) * SmoothStep(static_cast<float>(tutorial_.GetNoticeFrames()) / 20.0f);
    const float pop = 1.0f + 0.08f * (1.0f - SmoothStep(elapsed / 12.0f));

    int mainRow = kBannerClear;
    int subRow = kBannerNextRoom;
    // クリアは緑、完了は白で出し、上下の線はどちらも緑。やられた時だけ赤にする
    Vector4 mainColor = kUiGreen;
    Vector4 lineColor = kUiGreen;
    if (notice == TutorialFlow::Notice::Defeated) {
        mainRow = kBannerDefeated;
        subRow = kBannerRetry;
        mainColor = kUiDanger;
        lineColor = kUiDanger;
    } else if (notice == TutorialFlow::Notice::Finished) {
        mainRow = kBannerFinished;
        subRow = kBannerBackToTitle;
        mainColor = kUiWhite;
    }

    const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
    // チェック項目が2つある時の課題パネル(下端y=200)と重ならない高さに置く
    constexpr float kBannerTop = 222.0f;
    tutorialBannerBack_->SetPosition({ centerX - 330.0f, kBannerTop - 8.0f });
    tutorialBannerBack_->SetSize({ 660.0f, 128.0f });
    tutorialBannerBack_->SetColor(WithAlpha(kUiPanel, alpha));
    tutorialBannerBack_->Update();
    tutorialBannerBack_->Draw();
    // 開始演出のシャッターの合わせ目と同じく、上下の線を中央から左右へ伸ばす
    const float lineWidth = 660.0f * (1.0f - std::pow(1.0f - SmoothStep(elapsed / 14.0f), 3.0f));
    for (size_t i = 0; i < tutorialBannerLines_.size(); ++i) {
        auto& line = *tutorialBannerLines_[i];
        line.SetPosition({ centerX, i == 0 ? kBannerTop - 8.0f : kBannerTop + 120.0f });
        line.SetSize({ lineWidth, 2.0f });
        line.SetColor(WithAlpha(lineColor, alpha));
        line.Update();
        line.Draw();
    }

    const int rows[] = { mainRow, subRow };
    for (size_t i = 0; i < tutorialBannerLabels_.size(); ++i) {
        auto& label = *tutorialBannerLabels_[i];
        const float scale = i == 0 ? pop : 1.0f;
        label.SetTextureLeftTop({ 0.0f, static_cast<float>(kBannerRow + rows[i]) * kRowHeight });
        label.SetSize({ kRowWidth * scale, kRowHeight * scale });
        label.SetPosition({ centerX, kBannerTop + (i == 0 ? 0.0f : 58.0f) - (scale - 1.0f) * kRowHeight * 0.5f });
        label.SetColor(WithAlpha(i == 0 ? mainColor : kUiSubText, alpha));
        label.Update();
        label.Draw();
    }
}
