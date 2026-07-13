#pragma once
#define NOMINMAX
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>


#include "../../engine/math/Math.h"

class Sprite;
class SpriteCommon;

class Minimap
{
public:
    Minimap() = default;
    ~Minimap();

    // ミニマップJSONを読み込み、描画用Spriteを作成する
    void Initialize(
        SpriteCommon* spriteCommon,
        ID3D12Resource* directionalLightResource,
        const std::string& filePath);

    // プレイヤーと敵の現在位置をミニマップへ反映する
    void Update(
        const Vector3& playerPosition,
        const std::vector<Vector3>& enemyPositions);

    // ミニマップを画面へ描画する
    void Draw();

    // ミニマップの通常表示と拡大表示を切り替える
    void ToggleExpanded();

    // 銃声が届く範囲(ワールド座標の半径)を設定する
    void SetSoundRange(float worldRadius);

    // 発砲した瞬間に音の範囲円を明るく光らせる
    void NotifyGunshot();

private:
    // ゲーム内のX/Z座標をミニマップ上の画面座標へ変換する
    Vector2 ConvertWorldToScreen(float worldX, float worldZ) const;

    // 敵マーカーが不足している場合に追加する
    void EnsureEnemyMarkerCount(size_t requiredCount);

    // 壁を塗りつぶさず、4本の線で輪郭として作成する
    void CreateWallOutline(
        float worldX,
        float worldZ,
        float worldWidth,
        float worldHeight,
        float rotationDegree);

private:
    SpriteCommon* spriteCommon_ = nullptr;
    ID3D12Resource* directionalLightResource_ = nullptr;

    // ミニマップ外周の灰色枠
    std::unique_ptr<Sprite> borderSprite_;

    // ミニマップ内部の黒背景
    std::unique_ptr<Sprite> backgroundSprite_;

    // JSONから作成した床と壁
    std::vector<std::unique_ptr<Sprite>> mapSprites_;

    // 現在のプレイヤー位置
    std::unique_ptr<Sprite> playerMarker_;

    // プレイヤー中心に表示する銃声の届く範囲円
    std::unique_ptr<Sprite> soundRangeSprite_;

    // 銃声が届く範囲(ワールド座標の半径)
    float soundRangeWorldRadius_ = 0.0f;

    // 発砲直後に範囲円を明るくする残りフレーム
    float gunshotFlashTimer_ = 0.0f;

    // 現在の敵位置
    std::vector<std::unique_ptr<Sprite>> enemyMarkers_;
    size_t visibleEnemyCount_ = 0;

    // JSONに保存されているマップ範囲
    Vector2 boundsMin_ = { 0.0f, 0.0f };
    Vector2 boundsSize_ = { 1.0f, 1.0f };

    // ミニマップの左上座標と拡大率
    Vector2 minimapOrigin_ = { 0.0f, 0.0f };
    float minimapScale_ = 1.0f;

    bool initialized_ = false;

    // 現在ミニマップを拡大表示しているか
    bool isExpanded_ = false;
};