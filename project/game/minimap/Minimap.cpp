#include "Minimap.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "../../engine/2d/sprite/Sprite.h"
#include "../../engine/2d/sprite/SpriteCommon.h"
#include "../../engine/base/winapp/WinApp.h"
#include "../../externals/nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
    // ミニマップを配置する最大サイズ
    constexpr float kMinimapAreaSize = 200.0f;
    // Mキーで拡大したときのミニマップサイズ
    constexpr float kExpandedMinimapAreaSize = 600.0f;

    // 画面端から離す距離
    constexpr float kMinimapMargin = 20.0f;

    // 度数法をラジアンへ変換する
    constexpr float kDegreeToRadian = 3.1415926535f / 180.0f;
}

Minimap::~Minimap() = default;

void Minimap::Initialize(
    SpriteCommon* spriteCommon,
    ID3D12Resource* directionalLightResource,
    const std::string& filePath)
{
    spriteCommon_ = spriteCommon;
    directionalLightResource_ = directionalLightResource;

    mapSprites_.clear();
    enemyMarkers_.clear();
    visibleEnemyCount_ = 0;
    initialized_ = false;

    std::ifstream file(filePath);

    // JSONが開けない場合はミニマップを無効にする
    if (!file.is_open()) {
        return;
    }

    json rootJson;
    file >> rootJson;

    // boundsが無いJSONは読み込まない
    if (!rootJson.contains("bounds") ||
        !rootJson.contains("objects")) {
        return;
    }

    const json& boundsJson = rootJson["bounds"];

    boundsMin_ = {
        boundsJson["min"][0].get<float>(),
        boundsJson["min"][1].get<float>()
    };

    boundsSize_ = {
        boundsJson["size"][0].get<float>(),
        boundsJson["size"][1].get<float>()
    };

    // 0除算を防ぐ
    if (boundsSize_.x <= 0.0f || boundsSize_.y <= 0.0f) {
        return;
    }

    // 縦横比を維持したまま200ピクセル以内へ収める
    const float scaleX = kMinimapAreaSize / boundsSize_.x;
    const float scaleY = kMinimapAreaSize / boundsSize_.y;
    minimapScale_ = std::min(scaleX, scaleY);

    const float minimapWidth = boundsSize_.x * minimapScale_;
    const float minimapHeight = boundsSize_.y * minimapScale_;

    // ミニマップを画面右上へ配置する
    minimapOrigin_.x =
        static_cast<float>(WinApp::kClientWidth) -
        kMinimapMargin -
        minimapWidth;

    minimapOrigin_.y =
        kMinimapMargin +
        (kMinimapAreaSize - minimapHeight) * 0.5f;

    // ミニマップ外周の灰色枠を作成する
    borderSprite_ = std::make_unique<Sprite>();
    borderSprite_->Initialize(
        spriteCommon_,
        directionalLightResource_,
        "Resources/white2x2.png");

    borderSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    borderSprite_->SetPosition({
        minimapOrigin_.x + minimapWidth * 0.5f,
        minimapOrigin_.y + minimapHeight * 0.5f
        });
    borderSprite_->SetSize({
        minimapWidth + 8.0f,
        minimapHeight + 8.0f
        });
    borderSprite_->SetColor({
        0.45f,
        0.45f,
        0.45f,
        1.0f
        });
    borderSprite_->Update();

    // 灰色枠の内側に黒背景を作成する
    backgroundSprite_ = std::make_unique<Sprite>();
    backgroundSprite_->Initialize(
        spriteCommon_,
        directionalLightResource_,
        "Resources/white2x2.png");

    backgroundSprite_->SetAnchorPoint({ 0.5f, 0.5f });
    backgroundSprite_->SetPosition({
        minimapOrigin_.x + minimapWidth * 0.5f,
        minimapOrigin_.y + minimapHeight * 0.5f
        });
    backgroundSprite_->SetSize({
        minimapWidth,
        minimapHeight
        });
    backgroundSprite_->SetColor({
        0.0f,
        0.0f,
        0.0f,
        1.0f
        });
    backgroundSprite_->Update();

    // JSONから床と壁のSpriteを作成する
    for (const json& objectJson : rootJson["objects"]) {
        const std::string kind =
            objectJson["kind"].get<std::string>();

        // 動くプレイヤーと敵はUpdateで描画する
        // 黒背景を別に描画しているので、壁だけを処理する
        if (kind != "wall") {
            continue;
        }
        const float worldX =
            objectJson["position"][0].get<float>();

        const float worldZ =
            objectJson["position"][1].get<float>();

        const float worldWidth =
            objectJson["size"][0].get<float>();

        const float worldHeight =
            objectJson["size"][1].get<float>();

        const float rotationDegree =
            objectJson["rotation"].get<float>();

        // 大きさが無いオブジェクトは描画しない
        if (worldWidth <= 0.0f || worldHeight <= 0.0f) {
            continue;
        }

        // 壁を塗りつぶさず、4本の線による輪郭として作成する
        CreateWallOutline(
            worldX,
            worldZ,
            worldWidth,
            worldHeight,
            rotationDegree);
    }

    // プレイヤーマーカーを作成する
    playerMarker_ = std::make_unique<Sprite>();
    playerMarker_->Initialize(
        spriteCommon_,
        directionalLightResource_,
        "Resources/circle2.png");

    playerMarker_->SetAnchorPoint({ 0.5f, 0.5f });
    playerMarker_->SetSize({ 14.0f, 14.0f });
    playerMarker_->SetColor({
        0.2f,
        1.0f,
        0.3f,
        1.0f
        });
    playerMarker_->Update();

    initialized_ = true;
}

Vector2 Minimap::ConvertWorldToScreen(
    float worldX,
    float worldZ) const
{
    // X座標をミニマップの横方向へ変換する
    const float screenX =
        minimapOrigin_.x +
        (worldX - boundsMin_.x) * minimapScale_;

    // ゲームのZ座標を画面の上方向へ表示する
    const float screenY =
        minimapOrigin_.y +
        (boundsSize_.y - (worldZ - boundsMin_.y)) *
        minimapScale_;

    return { screenX, screenY };
}

void Minimap::EnsureEnemyMarkerCount(size_t requiredCount)
{
    while (enemyMarkers_.size() < requiredCount) {
        auto marker = std::make_unique<Sprite>();
        marker->Initialize(
            spriteCommon_,
            directionalLightResource_,
            "Resources/circle2.png");

        marker->SetAnchorPoint({ 0.5f, 0.5f });
        marker->SetSize({ 10.0f, 10.0f });
        marker->SetColor({
            1.0f,
            0.15f,
            0.15f,
            1.0f
            });
        marker->Update();

        enemyMarkers_.push_back(std::move(marker));
    }
}

void Minimap::Update(
    const Vector3& playerPosition,
    const std::vector<Vector3>& enemyPositions)
{
    if (!initialized_) {
        return;
    }

    // プレイヤーの現在位置を反映する
    playerMarker_->SetPosition(
        ConvertWorldToScreen(
            playerPosition.x,
            playerPosition.z));

    playerMarker_->Update();

    // 現在の敵数に合わせてマーカーを確保する
    EnsureEnemyMarkerCount(enemyPositions.size());
    visibleEnemyCount_ = enemyPositions.size();

    for (size_t index = 0;
        index < enemyPositions.size();
        ++index) {
        enemyMarkers_[index]->SetPosition(
            ConvertWorldToScreen(
                enemyPositions[index].x,
                enemyPositions[index].z));

        enemyMarkers_[index]->Update();
    }
}

void Minimap::Draw()
{
    if (!initialized_) {
        return;
    }

    // 灰色の外枠を最初に描画する
    borderSprite_->Draw();

    // 灰色枠の内側へ黒背景を描画する
    backgroundSprite_->Draw();

    // 床と壁を描画する
    for (const auto& sprite : mapSprites_) {
        sprite->Draw();
    }

    // プレイヤーを描画する
    playerMarker_->Draw();

    // 生存中の敵だけを描画する
    for (size_t index = 0;
        index < visibleEnemyCount_;
        ++index) {
        enemyMarkers_[index]->Draw();
    }
}

void Minimap::CreateWallOutline(
    float worldX,
    float worldZ,
    float worldWidth,
    float worldHeight,
    float rotationDegree)
{
    // 壁の中心を画面座標へ変換する
    const Vector2 center =
        ConvertWorldToScreen(worldX, worldZ);

    // ワールド上の壁サイズをピクセルへ変換する
    const float width =
        worldWidth * minimapScale_;

    const float height =
        worldHeight * minimapScale_;

    // 壁の輪郭線の太さ
    constexpr float lineThickness = 1.0f;

    // 画面のY方向が反転しているため回転方向も反転する
    const float rotation =
        -rotationDegree * kDegreeToRadian;

    const float cosRotation = std::cos(rotation);
    const float sinRotation = std::sin(rotation);

    // ローカル座標の位置を壁の回転に合わせて変換する
    auto rotateOffset =
        [cosRotation, sinRotation](
            float offsetX,
            float offsetY) -> Vector2
        {
            return {
                offsetX * cosRotation -
                offsetY * sinRotation,

                offsetX * sinRotation +
                offsetY * cosRotation
            };
        };

    // 輪郭線用のSpriteを1本作成する
    auto createLine =
        [this, &center, rotation, &rotateOffset](
            float offsetX,
            float offsetY,
            float lineWidth,
            float lineHeight)
        {
            const Vector2 rotatedOffset =
                rotateOffset(offsetX, offsetY);

            auto lineSprite =
                std::make_unique<Sprite>();

            lineSprite->Initialize(
                spriteCommon_,
                directionalLightResource_,
                "Resources/white2x2.png");

            lineSprite->SetAnchorPoint({
                0.5f,
                0.5f
                });

            lineSprite->SetPosition({
                center.x + rotatedOffset.x,
                center.y + rotatedOffset.y
                });

            lineSprite->SetSize({
                lineWidth,
                lineHeight
                });

            lineSprite->SetRotation(rotation);

            // 壁の輪郭を緑色にする
            lineSprite->SetColor({
                0.1f,
                0.85f,
                0.2f,
                1.0f
                });

            lineSprite->Update();
            mapSprites_.push_back(
                std::move(lineSprite));
        };

    // 上側の線を作成する
    createLine(
        0.0f,
        -height * 0.5f,
        width,
        lineThickness);

    // 下側の線を作成する
    createLine(
        0.0f,
        height * 0.5f,
        width,
        lineThickness);

    // 左側の線を作成する
    createLine(
        -width * 0.5f,
        0.0f,
        lineThickness,
        height);

    // 右側の線を作成する
    createLine(
        width * 0.5f,
        0.0f,
        lineThickness,
        height);
}

void Minimap::ToggleExpanded()
{
    if (!initialized_) {
        return;
    }

    // 変更前の配置情報を保存する
    const Vector2 oldOrigin = minimapOrigin_;
    const float oldScale = minimapScale_;

    // 通常表示と拡大表示を切り替える
    isExpanded_ = !isExpanded_;

    const float targetAreaSize =
        isExpanded_
        ? kExpandedMinimapAreaSize
        : kMinimapAreaSize;

    // マップ全体が指定サイズ内へ収まる倍率を計算する
    const float scaleX =
        targetAreaSize / boundsSize_.x;

    const float scaleY =
        targetAreaSize / boundsSize_.y;

    minimapScale_ =
        (std::min)(scaleX, scaleY);

    const float minimapWidth =
        boundsSize_.x * minimapScale_;

    const float minimapHeight =
        boundsSize_.y * minimapScale_;

    if (isExpanded_) {
        // 拡大時はミニマップを画面中央へ配置する
        minimapOrigin_.x =
            (static_cast<float>(WinApp::kClientWidth) -
                minimapWidth) *
            0.5f;

        minimapOrigin_.y =
            (static_cast<float>(WinApp::kClientHeight) -
                minimapHeight) *
            0.5f;
    } else {
        // 通常時はミニマップを画面右上へ戻す
        minimapOrigin_.x =
            static_cast<float>(WinApp::kClientWidth) -
            kMinimapMargin -
            minimapWidth;

        minimapOrigin_.y =
            kMinimapMargin +
            (targetAreaSize - minimapHeight) *
            0.5f;
    }

    // 古い表示倍率から新しい表示倍率への比率
    const float scaleRatio =
        minimapScale_ / oldScale;

    // 灰色の外枠を新しい大きさへ更新する
    borderSprite_->SetPosition({
        minimapOrigin_.x + minimapWidth * 0.5f,
        minimapOrigin_.y + minimapHeight * 0.5f
        });

    borderSprite_->SetSize({
        minimapWidth + 8.0f,
        minimapHeight + 8.0f
        });

    borderSprite_->Update();

    // 黒背景を新しい大きさへ更新する
    backgroundSprite_->SetPosition({
        minimapOrigin_.x + minimapWidth * 0.5f,
        minimapOrigin_.y + minimapHeight * 0.5f
        });

    backgroundSprite_->SetSize({
        minimapWidth,
        minimapHeight
        });

    backgroundSprite_->Update();

    // 壁の位置と大きさを新しい倍率へ更新する
    for (auto& sprite : mapSprites_) {
        const Vector2 oldPosition =
            sprite->GetPosition();

        const Vector2 oldSize =
            sprite->GetSize();

        const Vector2 newPosition = {
            minimapOrigin_.x +
            (oldPosition.x - oldOrigin.x) *
            scaleRatio,

            minimapOrigin_.y +
            (oldPosition.y - oldOrigin.y) *
            scaleRatio
        };

        sprite->SetPosition(newPosition);

        sprite->SetSize({
            oldSize.x * scaleRatio,
            oldSize.y * scaleRatio
            });

        sprite->Update();
    }
}