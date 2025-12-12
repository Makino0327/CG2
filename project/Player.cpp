#include "Player.h"
#include <cfloat>   // FLT_MAX
#include <cmath>    // std::fabs, std::floor

void Player::Initialize(Object3dCommon* object3dCommon, Input* input)
{
    input_ = input;

    object_ = new Object3d();
    object_->Initialize(object3dCommon);
    object_->SetModel("cube.obj");
    object_->SetTexture("Resources/cube.jpg");
    object_->SetScale({ 1.0f, 1.0f, 1.0f });

    // とりあえず箱の内側あたりに置く（好きな値で OK）
    object_->SetTranslate({ 4.0f, 4.0f, 0.0f });

    velocityY_ = 0.0f;
    onGround_ = false;

    jumpCount_ = 0;
    touchingLeftWall_ = false;
    touchingRightWall_ = false;
    extraVelX_ = 0.0f;
}

void Player::Update()
{
    if (!object_ || !input_) { return; }

    Vector3 pos = object_->GetTranslate();

    prevPos_ = pos;

    // ========= 左右移動（入力） =========
    if (input_->PushKey(DIK_A)) { pos.x -= moveSpeed_; }
    if (input_->PushKey(DIK_D)) { pos.x += moveSpeed_; }

    // ========= ジャンプ入力読み取り =========
    bool wantJump = input_->TriggerKey(DIK_SPACE);

    // ========= 重力 =========
    velocityY_ += gravity_;

    // ========= 縦移動 =========
    pos.y += velocityY_;

    // ========= 壁ジャンプ用の横速度を反映 =========
    pos.x += extraVelX_;

    // ========= マップ当たり判定 =========
    ResolveBottomCollisionWithMap(pos);
    ResolveLeftCollisionWithMap(pos);
    ResolveTopCollisionWithMap(pos);
    ResolveRightCollisionWithMap(pos);

    // ★ ここで onGround_ / touching◯Wall_ が確定した状態になっている

    // ========= ジャンプ処理（状態を見て決める） =========
    if (wantJump) {
        if (onGround_) {
            // 地上ジャンプ
            velocityY_ = jumpPower_;
            onGround_ = false;
            jumpCount_ = 1;
        } else if (touchingLeftWall_ || touchingRightWall_) {
            // 壁ジャンプ
            velocityY_ = jumpPower_;

            if (touchingLeftWall_) {
                extraVelX_ = +wallJumpPushX_;
            } else if (touchingRightWall_) {
                extraVelX_ = -wallJumpPushX_;
            }

            // ここが「二段ジャンプをまだ残している」原因
            // jumpCount_ = 1;
            // ↓ 壁ジャンプをした時点でジャンプ回数を使い切ったことにする
            jumpCount_ = maxJumpCount_;

            onGround_ = false;
        

        } else if (jumpCount_ < maxJumpCount_) {
            // 空中二段ジャンプ
            velocityY_ = jumpPower_;
            jumpCount_++;
        }
    }

    // ========= 横速度の減衰 =========
    extraVelX_ *= wallJumpDamping_;
    if (std::fabs(extraVelX_) < 0.001f) {
        extraVelX_ = 0.0f;
    }

    // 地面についているときは滑りを完全に止めたいならここで0にしてもOK
    if (onGround_) {
        extraVelX_ = 0.0f;
    }

    object_->SetTranslate(pos);
    object_->Update();
}

void Player::Draw()
{
    if (!object_) { return; }
    object_->Draw();
}

// ----------------------------
// 下方向のマップ当たり判定
// ----------------------------
void Player::ResolveBottomCollisionWithMap(Vector3& pos)
{
    if (!mapField_) {
        return;
    }

    // 上向き速度なら足元判定不要
    if (velocityY_ > 0.0f) {
        onGround_ = false;
        return;
    }

    const float halfHeight = tileSize_ * 0.5f;

    float feetY = pos.y - halfHeight;
    float prevFeetY = feetY - velocityY_;

    // 中心X → タイルX（＋0.5で補正）
    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));

    int mapH = mapField_->GetHeight();

    float bestTopY = -FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < mapH; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        float centerY = static_cast<float>(mapH - 1 - ty) * tileSize_;
        float topY = centerY + halfHeight;

        if (prevFeetY >= topY && feetY <= topY) {
            if (topY > bestTopY) {
                bestTopY = topY;
                hit = true;
            }
        }
    }

    if (hit) {
        float feetAlignY = bestTopY;
        pos.y = feetAlignY + halfHeight;
        velocityY_ = 0.0f;
        onGround_ = true;

        // 地面に着いたらジャンプ回数リセット
        jumpCount_ = 0;
    } else {
        onGround_ = false;
    }
}

// 左方向（←）のマップ当たり判定
void Player::ResolveLeftCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    // 右に動いている / 静止中なら左判定はいらない
    if (pos.x >= prevPos_.x) {
        touchingLeftWall_ = false;
        return;
    }

    int w = mapField_->GetWidth();
    int h = mapField_->GetHeight();

    // プレイヤーAABB
    float playerLeft = pos.x - halfSize;
    float playerRight = pos.x + halfSize;
    float playerBottom = pos.y - halfSize;
    float playerTop = pos.y + halfSize;

    float prevLeft = prevPos_.x - halfSize;

    int tileX = static_cast<int>(std::floor(playerLeft / tileSize_));
    if (tileX < 0 || tileX >= w) {
        touchingLeftWall_ = false;
        return;
    }

    float bestBlockRight = -FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < h; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;
        float blockTop = centerY + halfSize;

        if (blockTop <= playerBottom || blockBottom >= playerTop) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (playerLeft < blockRight && playerRight > blockLeft) {
            if (prevLeft >= blockRight && playerLeft <= blockRight) {
                if (blockRight > bestBlockRight) {
                    bestBlockRight = blockRight;
                    hit = true;
                }
            }
        }
    }

    if (hit) {
        pos.x = bestBlockRight + halfSize;

        // 左向きの速度は0にしておく
        if (extraVelX_ < 0.0f) {
            extraVelX_ = 0.0f;
        }
    }

    touchingLeftWall_ = hit;
}

// 上方向（↑）のマップ当たり判定
void Player::ResolveTopCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    if (velocityY_ <= 0.0f) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float topY = pos.y + halfSize;
    float prevTopY = topY - velocityY_;

    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));
    int w = mapField_->GetWidth();
    int h = mapField_->GetHeight();

    if (tileX < 0 || tileX >= w) {
        return;
    }

    float bestBottomY = FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < h; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;

        if (prevTopY <= blockBottom && topY >= blockBottom) {
            if (blockBottom < bestBottomY) {
                bestBottomY = blockBottom;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.y = bestBottomY - halfSize;
        velocityY_ = 0.0f;   // 頭ぶつけたら上方向速度を止める
    }
}

// 右方向（→）のマップ当たり判定
void Player::ResolveRightCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    float moveX = pos.x - prevPos_.x;
    if (moveX <= 0.0f) {
        touchingRightWall_ = false;
        return;
    }

    int w = mapField_->GetWidth();
    int h = mapField_->GetHeight();

    float playerLeft = pos.x - halfSize;
    float playerRight = pos.x + halfSize;
    float playerBottom = pos.y - halfSize;
    float playerTop = pos.y + halfSize;

    float prevRight = prevPos_.x + halfSize;

    int tileX = static_cast<int>(std::floor((playerRight + halfSize) / tileSize_));
    if (tileX < 0 || tileX >= w) {
        touchingRightWall_ = false;
        return;
    }

    float bestBlockLeft = FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < h; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;
        float blockTop = centerY + halfSize;

        if (blockTop <= playerBottom || blockBottom >= playerTop) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (playerRight > blockLeft && playerLeft < blockRight) {
            if (prevRight <= blockLeft && playerRight >= blockLeft) {
                if (blockLeft < bestBlockLeft) {
                    bestBlockLeft = blockLeft;
                    hit = true;
                }
            }
        }
    }

    if (hit) {
        pos.x = bestBlockLeft - halfSize;

        // 右向きの速度は0にする
        if (extraVelX_ > 0.0f) {
            extraVelX_ = 0.0f;
        }
    }

    touchingRightWall_ = hit;
}

Vector3 Player::GetPosition() const
{
    if (!object_) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return object_->GetTranslate();
}
