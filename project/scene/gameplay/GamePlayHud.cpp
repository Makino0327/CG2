#include "GamePlayScene.h"

#include <algorithm>
#include <string>

#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/2d/texture/TextureManager.h"
#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/base/DirectX/DirectXCommon.h"
#include "../../engine/base/winapp/WinApp.h"

namespace {

    // 残弾UIの並び方をまとめた情報
    struct AmmoUiLayout {
        Vector2 bulletSize{};
        float gapX = 0.0f;
        float gapY = 0.0f;
        int bulletsPerRow = 1;
        float startX = 0.0f;
        float startY = 0.0f;
    };

    // 弾番号(0が最後に撃つ弾)と現在の残弾数から描画位置を求める
    // 下段(一段目)が先に消費され、撃つたびに下段の弾は出口(右端)へ詰まる
    // 上段(2段目)は列を保ったまま待機し、下段を撃ち切ると真下へ落ちて下段になる
    Vector2 GetAmmoBulletPosition(
        const AmmoUiLayout& layout, int currentAmmo, int bulletIndex)
    {
        // 下段の弾数を求める(あふれた分だけ上段が満杯で残る)
        int bottomCount = currentAmmo;
        int topCount = 0;
        if (currentAmmo > layout.bulletsPerRow) {
            topCount = layout.bulletsPerRow;
            bottomCount = currentAmmo - layout.bulletsPerRow;
        }

        int row = 0;
        int column = 0;
        if (bulletIndex < topCount) {
            // 上段は左端から列を固定して並ぶ
            row = 1;
            column = bulletIndex;
        } else {
            // 下段は出口(右端)へ右詰めで並ぶ
            row = 0;
            column = layout.bulletsPerRow - bottomCount + (bulletIndex - topCount);
        }

        return {
            layout.startX + static_cast<float>(column) * (layout.bulletSize.x + layout.gapX),
            layout.startY - static_cast<float>(row) * (layout.bulletSize.y + layout.gapY)
        };
    }
}

// 選択中の武器に合わせて、残弾の並びと発射・リロード時の表示を更新する。
void GamePlayScene::UpdateAmmoUiSprites()
{
    if (!player_) {
        return;
    }

    // 近距離攻撃中でも、弾UIは選択中の銃を表示する
    const Player::AttackMode attackMode = player_->GetSelectedGunMode();
    const int maxAmmo = player_->GetCurrentMaxAmmo();
    const int currentAmmo = player_->GetCurrentAmmo();
    const bool isAssaultRifle = attackMode == Player::AttackMode::AssaultRifle;
    const bool isShotgun = attackMode == Player::AttackMode::Shotgun;

    // 武器ごとに弾UIの大きさと並びを変える
    AmmoUiLayout layout{};
    layout.bulletSize = isAssaultRifle
        ? Vector2{ 10.0f, 24.0f }
        : (isShotgun ? Vector2{ 22.0f, 38.0f } : Vector2{ 18.0f, 34.0f });
    layout.gapX = isAssaultRifle ? 5.0f : (isShotgun ? 10.0f : 8.0f);
    layout.gapY = isAssaultRifle ? 7.0f : 0.0f;
    // ライフルは1行15発、ほかの銃は弾倉の上限に合わせて1行に並べる。
    layout.bulletsPerRow = isAssaultRifle ? 15 : std::max(1, maxAmmo);
    layout.startX = 32.0f;
    layout.startY = static_cast<float>(WinApp::kClientHeight) - 58.0f;

    // ショットガンだけ残弾UIの弾を赤くする
    const Vector4 ammoColor = isShotgun
        ? Vector4{ 1.0f, 0.18f, 0.08f, 0.95f }
        : Vector4{ 1.0f, 0.86f, 0.10f, 0.95f };

    // 武器が替わったら並びが変わるので、アニメーションせず即座に整列させる
    const bool layoutChanged =
        (attackMode != ammoUiPrevMode_) || (maxAmmo != ammoUiPrevMaxAmmo_);
    if (layoutChanged) {
        for (int index = 0;
             index < currentAmmo && index < static_cast<int>(ammoBulletAnims_.size());
             ++index) {
            ammoBulletAnims_[index].position =
                GetAmmoBulletPosition(layout, currentAmmo, index);
            ammoBulletAnims_[index].alpha = 0.95f;
            ammoBulletAnims_[index].delay = 0.0f;
        }
        for (AmmoFireEffect& effect : ammoFireEffects_) {
            effect.active = false;
        }
    } else if (currentAmmo < ammoUiPrevAmmo_) {
        // 撃った弾は出口(下段の右端)の位置から前へ飛び出して消える
        const Vector2 exitPosition =
            GetAmmoBulletPosition(layout, ammoUiPrevAmmo_, ammoUiPrevAmmo_ - 1);
        const Vector2 exitCenter = {
            exitPosition.x + layout.bulletSize.x * 0.5f,
            exitPosition.y
        };
        for (int shot = 0; shot < ammoUiPrevAmmo_ - currentAmmo; ++shot) {
            SpawnAmmoFireEffect(exitCenter, layout.bulletSize);
        }
    } else if (currentAmmo > ammoUiPrevAmmo_) {
        // リロードで増えた弾は左端の外から順番に入ってくる
        // 残っていた弾は出口側のスロットに居座り、新しい弾を奥へ詰める
        const int addedCount = currentAmmo - ammoUiPrevAmmo_;
        for (int index = ammoUiPrevAmmo_ - 1; index >= 0; --index) {
            ammoBulletAnims_[index + addedCount] = ammoBulletAnims_[index];
        }
        const float entryX = layout.startX - layout.bulletSize.x - 40.0f;
        for (int index = 0; index < addedCount; ++index) {
            const Vector2 target =
                GetAmmoBulletPosition(layout, currentAmmo, index);
            ammoBulletAnims_[index].position = { entryX, target.y };
            ammoBulletAnims_[index].alpha = 0.0f;
            // 奥(右寄り)のスロットへ入る弾から順番に装填する
            ammoBulletAnims_[index].delay =
                static_cast<float>(addedCount - 1 - index) * 2.0f;
        }
    }

    ammoUiPrevAmmo_ = currentAmmo;
    ammoUiPrevMaxAmmo_ = maxAmmo;
    ammoUiPrevMode_ = attackMode;

    // UI全体の後ろへ黒い半透明パネルを敷いて見えやすくする
    if (ammoBackgroundSprite_) {
        const float padding = 10.0f;
        const int rowCount =
            (maxAmmo + layout.bulletsPerRow - 1) / layout.bulletsPerRow;
        float width =
            static_cast<float>(layout.bulletsPerRow) *
                (layout.bulletSize.x + layout.gapX) -
            layout.gapX + padding * 2.0f;
        // 予備弾を表示する武器は、表示する桁数に合わせて背景を右へ伸ばす。
        const int reserveAmmo = player_->GetCurrentReserveAmmo();
        if (reserveAmmo >= 0) {
            const float glyphCount = static_cast<float>(std::to_string(reserveAmmo).size() + (currentAmmo == 0 ? 2 : 1));
            width += padding + glyphCount * reserveAmmoSprites_.front()->GetSize().x;
        }
        const float height =
            static_cast<float>(rowCount) * (layout.bulletSize.y + layout.gapY) -
            layout.gapY + padding * 2.0f;
        const float topY = layout.startY -
            static_cast<float>(rowCount - 1) * (layout.bulletSize.y + layout.gapY) -
            layout.bulletSize.y * 0.5f - padding;
        const float headerHeight = weaponNameSprite_->GetSize().y + 4.0f;
        ammoBackgroundSprite_->SetPosition({ layout.startX - padding, topY - headerHeight });
        ammoBackgroundSprite_->SetSize({ width, height + headerHeight });
        ammoBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.35f });
        ammoBackgroundSprite_->Update();

        // 武器を切り替えたフレームで、残弾の上に選択中の武器名を出す。
        if (layoutChanged) {
            const float labelRow = isAssaultRifle ? 1.0f : (isShotgun ? 2.0f : 0.0f);
            weaponNameSprite_->SetTextureLeftTop({ 0.0f, labelRow * weaponNameSprite_->GetTextureSize().y });
            weaponNameSprite_->SetPosition({ layout.startX, topY - headerHeight + padding });
            weaponNameSprite_->Update();
        }
        // リロード中だけ、武器名と同じ高さに状態を表示する。
        if (player_->IsReloading()) {
            reloadLabelSprite_->SetPosition({ layout.startX + width - padding * 2.0f,
                weaponNameSprite_->GetPosition().y });
            reloadLabelSprite_->Update();
        }
    }

    // 残っている弾を目標スロットへ滑らかに寄せる
    for (size_t index = 0; index < ammoSprites_.size(); ++index) {
        Sprite* sprite = ammoSprites_[index].get();
        if (!sprite) {
            continue;
        }

        // 使わないSpriteは透明にして描画に出ないようにする
        if (static_cast<int>(index) >= currentAmmo) {
            sprite->SetColor({ ammoColor.x, ammoColor.y, ammoColor.z, 0.0f });
            sprite->Update();
            continue;
        }

        AmmoBulletAnim& anim = ammoBulletAnims_[index];
        const Vector2 target =
            GetAmmoBulletPosition(layout, currentAmmo, static_cast<int>(index));

        // リロード直後の弾は時間差を消化してから入ってくる
        if (anim.delay > 0.0f) {
            anim.delay -= 1.0f;
        } else {
            anim.position.x += (target.x - anim.position.x) * 0.30f;
            anim.position.y += (target.y - anim.position.y) * 0.30f;
            anim.alpha += (0.95f - anim.alpha) * 0.35f;
        }

        sprite->SetPosition(anim.position);
        sprite->SetSize(layout.bulletSize);
        sprite->SetColor({ ammoColor.x, ammoColor.y, ammoColor.z, anim.alpha });
        sprite->Update();
    }

    UpdateAmmoFireEffects();
}

// 空いている演出枠を使い、発射で減った弾が飛び出す演出を開始する。
void GamePlayScene::SpawnAmmoFireEffect(const Vector2& center, const Vector2& size)
{
    // 空いている演出スロットを1つだけ使う
    for (AmmoFireEffect& effect : ammoFireEffects_) {
        if (effect.active) {
            continue;
        }
        effect.active = true;
        effect.position = center;
        effect.size = size;
        effect.timer = 0.0f;
        return;
    }
}

// 発射演出の位置と透明度を更新し、再生時間を過ぎた演出を終了する。
void GamePlayScene::UpdateAmmoFireEffects()
{
    // 発射演出の全体フレーム数
    constexpr float kDuration = 10.0f;

    for (size_t index = 0;
         index < ammoFireEffects_.size() && index < ammoFireEffectSprites_.size();
         ++index) {
        AmmoFireEffect& effect = ammoFireEffects_[index];
        Sprite* sprite = ammoFireEffectSprites_[index].get();
        if (!sprite) {
            continue;
        }

        if (effect.active) {
            effect.timer += 1.0f;
            if (effect.timer >= kDuration) {
                effect.active = false;
            }
        }
        if (!effect.active) {
            sprite->SetColor({ 1.0f, 0.9f, 0.35f, 0.0f });
            sprite->Update();
            continue;
        }

        // 勢いよく飛び出してすぐ減速する動きにする
        const float t = effect.timer / kDuration;
        const float pop = 1.0f - (1.0f - t) * (1.0f - t);

        // サイズは変えずにそのまま真上へ勢いよく飛ばす
        const Vector2 position = {
            effect.position.x,
            effect.position.y - 70.0f * pop
        };

        // 飛びながら徐々に薄くなっていき、上がり切ったところで消える
        const float alpha = 0.95f * (1.0f - t);

        sprite->SetPosition(position);
        sprite->SetSize(effect.size);
        sprite->SetColor({ 1.0f, 0.9f, 0.35f, alpha });
        sprite->Update();
    }
}

// 武器名、残弾、予備弾、リロード状態と発射演出を描画する。
void GamePlayScene::DrawAmmoUiSprites()
{
    if (!player_) {
        return;
    }

    const int currentAmmo = player_->GetCurrentAmmo();

    // 奥から順に、背景パネル → 残弾 → 発射演出の順で重ねる
    if (ammoBackgroundSprite_) {
        ammoBackgroundSprite_->Draw();
        weaponNameSprite_->Draw();
        if (player_->IsReloading()) { reloadLabelSprite_->Draw(); }
    }
    for (int index = 0;
         index < currentAmmo && index < static_cast<int>(ammoSprites_.size());
         ++index) {
        if (ammoSprites_[index]) {
            ammoSprites_[index]->Draw();
        }
    }
    for (size_t index = 0;
         index < ammoFireEffects_.size() && index < ammoFireEffectSprites_.size();
         ++index) {
        if (ammoFireEffects_[index].active && ammoFireEffectSprites_[index]) {
            ammoFireEffectSprites_[index]->Draw();
        }
    }
    // スラッシュの後に予備弾数を表示し、弾倉が空のときは左にも0を出す。
    const int reserveAmmo = player_->GetCurrentReserveAmmo();
    if (reserveAmmo >= 0 && ammoBackgroundSprite_) {
        const std::string reserveText = (currentAmmo == 0 ? "0/" : "/") + std::to_string(reserveAmmo);
        const Vector2& glyphSize = reserveAmmoSprites_.front()->GetSize();
        const Vector2& panelPosition = ammoBackgroundSprite_->GetPosition();
        const Vector2& panelSize = ammoBackgroundSprite_->GetSize();
        float x = panelPosition.x + panelSize.x - 10.0f - glyphSize.x * static_cast<float>(reserveText.size());
        const float headerHeight = weaponNameSprite_->GetSize().y + 4.0f;
        const float y = panelPosition.y + headerHeight + (panelSize.y - headerHeight - glyphSize.y) * 0.5f;
        for (size_t index = 0; index < reserveText.size(); ++index) {
            auto& sprite = reserveAmmoSprites_[index];
            const int glyph = reserveText[index] == '/' ? 10 : reserveText[index] - '0';
            sprite->SetTextureLeftTop({ 32.0f * static_cast<float>(glyph), 0.0f });
            sprite->SetPosition({ x, y });
            sprite->Update();
            sprite->Draw();
            x += glyphSize.x;
        }
    }
}

// 手榴弾のモデルと残数を描画し、無制限の場合は残数の代わりに無限記号を表示する。
void GamePlayScene::DrawGrenadeUi()
{
    // 読み込みが完了しているときだけ、モデルと数字をまとめて描く。
    if (!player_ || !ammoBackgroundSprite_ || !grenadeUiModel_ ||
        !grenadeCountSprites_[0] || !grenadeCountSprites_[1]) {
        return;
    }

    // 銃ごとに弾の段数が変わっても、武器名の上へ追従させる。
    const Vector2& panelPosition = ammoBackgroundSprite_->GetPosition();
    const float left = panelPosition.x + 4.0f;
    const float top = panelPosition.y - 58.0f;
    constexpr float kIconSize = 48.0f;
    auto* commandList = GetContext().dxCommon->GetCommandList();

    // モデルを描く範囲だけ深度を消し、背景の壁などに隠れないようにする。
    const D3D12_RECT iconRect{
        static_cast<LONG>(left), static_cast<LONG>(top),
        static_cast<LONG>(left + kIconSize), static_cast<LONG>(top + kIconSize)
    };
    commandList->ClearDepthStencilView(GetContext().dxCommon->GetDSVHandle(),
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 1, &iconRect);

    // 専用カメラの映像を、48ピクセル四方に収める。
    const D3D12_VIEWPORT iconViewport{ left, top, kIconSize, kIconSize, 0.0f, 1.0f };
    commandList->RSSetViewports(1, &iconViewport);
    grenadeUiModel_->Draw();

    // 後続の数字や案内は、通常どおり画面全体の座標で描く。
    const D3D12_VIEWPORT fullViewport{
        0.0f, 0.0f,
        static_cast<float>(WinApp::kClientWidth),
        static_cast<float>(WinApp::kClientHeight), 0.0f, 1.0f
    };
    commandList->RSSetViewports(1, &fullViewport);

    // 通常プレイはxと残数、チュートリアルはxと無限記号を表示する。
    const bool unlimited = player_->HasUnlimitedGrenades();
    const int count = std::clamp(player_->GetGrenadeCount(), 0, 3);
    constexpr int kMultiplyGlyph = 11;
    constexpr int kInfinityGlyph = 12;
    const int glyphs[] = { kMultiplyGlyph, unlimited ? kInfinityGlyph : count };
    const Vector4 color = unlimited || count > 0
        ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
        : Vector4{ 0.5f, 0.5f, 0.5f, 1.0f };
    for (size_t index = 0; index < grenadeCountSprites_.size(); ++index) {
        auto& sprite = grenadeCountSprites_[index];
        sprite->SetTextureLeftTop({ 32.0f * static_cast<float>(glyphs[index]), 0.0f });
        sprite->SetPosition({ left + 52.0f + 18.0f * static_cast<float>(index), top + 24.0f });
        sprite->SetColor(color);
        sprite->Update();
        sprite->Draw();
    }
}

// ボスの残りHPに合わせてバーの長さを更新し、対象がいなければバーを隠す。
void GamePlayScene::UpdateBossHpUiSprites()
{
    constexpr float kBarWidth = 520.0f;
    constexpr float kBarHeight = 24.0f;
    constexpr float kFramePadding = 4.0f;
    const Vector2 barPosition = {
        static_cast<float>(WinApp::kClientWidth) * 0.5f,
        28.0f
    };

    Boss* activeBoss = nullptr;
    for (const auto& boss : bosses_) {
        if (!boss || boss->IsReadyToRemove()) {
            continue;
        }
        activeBoss = boss.get();
        break;
    }

    if (!activeBoss) {
        // ボスがいない時はHPバーを透明にする
        if (bossHpBackgroundSprite_) {
            bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
            bossHpBackgroundSprite_->Update();
        }
        if (bossHpFillSprite_) {
            bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.0f });
            bossHpFillSprite_->Update();
        }
        if (bossHpFrameSprite_) {
            bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
            bossHpFrameSprite_->Update();
        }
        return;
    }

    const float hpRate = std::clamp(
        static_cast<float>(activeBoss->GetHp()) /
            static_cast<float>(activeBoss->GetMaxHp()),
        0.0f,
        1.0f);

    if (bossHpFrameSprite_) {
        // 白い外枠を少し大きく敷く
        bossHpFrameSprite_->SetPosition({
            barPosition.x,
            barPosition.y - kFramePadding
        });
        bossHpFrameSprite_->SetSize({
            kBarWidth + kFramePadding * 2.0f,
            kBarHeight + kFramePadding * 2.0f
        });
        bossHpFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.85f });
        bossHpFrameSprite_->Update();
    }

    if (bossHpBackgroundSprite_) {
        // HPが減った後も最大HP幅が分かるように黒背景を置く
        bossHpBackgroundSprite_->SetPosition(barPosition);
        bossHpBackgroundSprite_->SetSize({ kBarWidth, kBarHeight });
        bossHpBackgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.75f });
        bossHpBackgroundSprite_->Update();
    }

    if (bossHpFillSprite_) {
        // 左端を固定して、HP割合分だけ赤バーを縮める
        bossHpFillSprite_->SetPosition({
            barPosition.x - kBarWidth * 0.5f,
            barPosition.y
        });
        bossHpFillSprite_->SetSize({ kBarWidth * hpRate, kBarHeight });
        bossHpFillSprite_->SetColor({ 0.95f, 0.05f, 0.03f, 0.95f });
        bossHpFillSprite_->Update();
    }
}

// ボスのHPバーを外枠、背景、残りHPの順に描画する。
void GamePlayScene::DrawBossHpUiSprites()
{
    if (bosses_.empty()) {
        return;
    }

    // 奥から外枠、背景、残量の順で描画する
    if (bossHpFrameSprite_) {
        bossHpFrameSprite_->Draw();
    }
    if (bossHpBackgroundSprite_) {
        bossHpBackgroundSprite_->Draw();
    }
    if (bossHpFillSprite_) {
        bossHpFillSprite_->Draw();
    }
}

// 現在の目標を表示するパネルとラベルの画像を用意する。
void GamePlayScene::InitializeGuideUi()
{
    // 数字画像は描画前に読み込む。描画中の初回読み込みはコマンドリストをリセットし、
    // 描画先や深度バッファの設定を失うため、数字Spriteの作成時にはキャッシュを使う
    TextureManager::GetInstance()->LoadTexture("Resources/hud/count_glyphs.png");

    // 日本語は事前生成した透過画像を使い、実行環境のフォントに依存させない
    auto createSprite = [this](const char* texture) {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), texture);
        sprite->SetAnchorPoint({ 0.0f, 0.0f });
        return sprite;
    };
    objectivePanel_ = createSprite("Resources/white2x2.png");
    objectivePanel_->SetPosition({ 20.0f, 88.0f });
    objectivePanel_->SetColor({ 0.02f, 0.025f, 0.03f, 0.78f });
    objectiveLabel_ = createSprite("Resources/hud/objective_labels.png");
    objectiveLabel_->SetPosition({ 36.0f, 94.0f });
    objectiveLabel_->SetTextureSize({ 640.0f, 128.0f });
    objectiveLabel_->SetSize({ 320.0f, 64.0f });
    objectiveCountSprites_.clear();


}

// 発電機の起動台数やワープへの案内など、現在の目標を表示する。
void GamePlayScene::DrawGuideUi()
{
    // 死亡演出中とデバッグカメラ中は、操作できない案内を隠す
    if (!player_ || player_->IsDead() || (GetContext().isDebugMode && *GetContext().isDebugMode)) {
        return;
    }
    // チュートリアルでは、目標の代わりに課題の説明を表示する
    if (tutorialMode_) {
        DrawTutorialUi();
        return;
    }
    GetContext().spriteCommon->CommonDrawSetting();

    // 起動途中は数に含めず、メーターが満タンになった発電機だけを数える
    const size_t activeCount = static_cast<size_t>(std::count_if(generators_.begin(), generators_.end(),
        [](const GeneratorData& generator) {
            return generator.activation.GetState() == GeneratorActivation::State::Active;
        }));
    const bool allActive = !generators_.empty() && activeCount == generators_.size();
    const bool showPortal = allActive ||
        (generators_.empty() && !bossTeleports_.empty() && IsBossTeleportUsable());
    // 発電機も使用可能なポータルもないステージでは、存在しない目的を表示しない
    if (generators_.empty() && !showPortal) {
        return;
    }

    const std::string countText = std::to_string(activeCount) + "/" + std::to_string(generators_.size());
    objectivePanel_->SetSize({ showPortal ? 320.0f :
        std::max(416.0f, 348.0f + static_cast<float>(countText.size()) * 16.0f), 76.0f });
    objectivePanel_->Update();
    objectivePanel_->Draw();
    objectiveLabel_->SetTextureLeftTop({ 0.0f, showPortal ? 128.0f : 0.0f });
    objectiveLabel_->Update();
    objectiveLabel_->Draw();
    if (showPortal) {
        return;
    }

    // 台数を固定せず、レベル再読み込みや桁数の変更にも対応する
    if (objectiveCountSprites_.size() != countText.size()) {
        objectiveCountSprites_.clear();
        for (size_t index = 0; index < countText.size(); ++index) {
            auto digit = std::make_unique<Sprite>();
            digit->Initialize(GetContext().spriteCommon, directionalLightResource_.Get(), "Resources/hud/count_glyphs.png");
            digit->SetTextureSize({ 32.0f, 64.0f });
            digit->SetSize({ 16.0f, 32.0f });
            digit->SetColor({ 1.0f, 0.9f, 0.3f, 1.0f });
            objectiveCountSprites_.push_back(std::move(digit));
        }
    }
    for (size_t index = 0; index < countText.size(); ++index) {
        // 画像は数字0〜9とスラッシュの順に横へ並べてある
        const int glyph = countText[index] == '/' ? 10 : countText[index] - '0';
        auto& digit = objectiveCountSprites_[index];
        digit->SetTextureLeftTop({ static_cast<float>(glyph) * 32.0f, 0.0f });
        digit->SetPosition({ 352.0f + static_cast<float>(index) * 16.0f, 122.0f });
        digit->Update();
        digit->Draw();
    }
}
