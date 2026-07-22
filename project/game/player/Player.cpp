#include "Player.h"
#include <cfloat>   // FLT_MAX

void Player::Initialize(Object3dCommon* object3dCommon, Input* input)
{
    input_ = input;

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("cube.obj");
    object_->SetTexture("Resources/cube.jpg");
    object_->SetScale({ 1.0f, 1.0f, 1.0f });

    // とりあえず箱の内側あたりに置く（好きな値で OK）
    object_->SetTranslate({ 4.0f, 4.0f, 0.0f });

    velocityY_ = 0.0f;
    onGround_ = false;
}

void Player::Update()
{
    if (!object_ || !input_) { return; }

    Vector3 pos = object_->GetTranslate();

    prevPos_ = pos;

    // ==== 左右移動 ====
    // 左右入力を先に保存して、移動と向きの両方に使う
    const bool moveLeft = input_->PushKey(DIK_A);
    const bool moveRight = input_->PushKey(DIK_D);

    // ==== 左右移動 ====
    if (moveLeft) { pos.x -= moveSpeed_; }
    if (moveRight) { pos.x += moveSpeed_; }

    // プレイヤーの正面を移動方向へ向ける
    if (moveLeft && !moveRight) {
        Vector3 rotate = object_->GetRotate();
        rotate.y = 1.570796f;
        object_->SetRotate(rotate);
    } else if (moveRight && !moveLeft) {
        Vector3 rotate = object_->GetRotate();
        rotate.y = -1.570796f;
        object_->SetRotate(rotate);
    }

    // ==== ジャンプ ====
    if (onGround_ && input_->TriggerKey(DIK_SPACE)) {
        velocityY_ = jumpPower_;
        onGround_ = false;
    }

    // ==== 重力 ====
    velocityY_ += gravity_;

    // いったん速度をそのまま足す
    pos.y += velocityY_;

    // ==== マップとの下方向当たり判定 ====
    ResolveBottomCollisionWithMap(pos);

	ResolveLeftCollisionWithMap(pos);

	ResolveTopCollisionWithMap(pos);

	ResolveRightCollisionWithMap(pos);

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

    if (velocityY_ > 0.0f) {
        onGround_ = false;
        return;
    }

    const float halfHeight = tileSize_ * 0.5f;

    float feetY = pos.y - halfHeight;
    float prevFeetY = feetY - velocityY_;

    // ★ 中心X → タイルX（＋0.5で補正）
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
        return;
    }

    int w = mapField_->GetWidth();
    int h = mapField_->GetHeight();

    // プレイヤーの AABB
    float playerLeft = pos.x - halfSize;
    float playerRight = pos.x + halfSize;
    float playerBottom = pos.y - halfSize;
    float playerTop = pos.y + halfSize;

    // 前フレームの左端
    float prevLeft = prevPos_.x - halfSize;

    // 「今フレームの左端が入っているタイル列」を見る
    int tileX = static_cast<int>(std::floor(playerLeft / tileSize_));
    if (tileX < 0 || tileX >= w) {
        return;
    }

    float bestBlockRight = -FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < h; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        // ブロックのY範囲（描画と同じ式）
        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;
        float blockTop = centerY + halfSize;

        // 縦方向にかすってなければスキップ
        if (blockTop <= playerBottom || blockBottom >= playerTop) {
            continue;
        }

        // ブロックのX範囲
        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        // X方向で重なっている？
        if (playerLeft < blockRight && playerRight > blockLeft) {

            // 前フレームはブロックの右側にいて、
            // 今フレームで右端を跨いで左にめり込んだ場合だけ衝突とみなす
            if (prevLeft >= blockRight && playerLeft <= blockRight) {
                if (blockRight > bestBlockRight) {
                    bestBlockRight = blockRight;
                    hit = true;
                }
            }
        }
    }

    if (hit) {
        // 左端をブロックの右端に揃える
        pos.x = bestBlockRight + halfSize;
    }
}

// 上方向（↑）のマップ当たり判定
void Player::ResolveTopCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    // 下向き or 静止のときは上判定は不要
    if (velocityY_ <= 0.0f) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    // 今フレームの「頭の高さ」
    float topY = pos.y + halfSize;
    // 1フレーム前の頭の高さ
    float prevTopY = topY - velocityY_;

    // プレイヤーの真上のタイル列（中心Xから）を調べる
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

        // このブロックの下端（マップ描画と同じ座標系）
        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;

        // 「前フレームは下にいて、今フレームで下端をまたいだ」なら頭がぶつかった
        if (prevTopY <= blockBottom && topY >= blockBottom) {
            if (blockBottom < bestBottomY) {
                bestBottomY = blockBottom;
                hit = true;
            }
        }
    }

    if (hit) {
        // 頭をブロックの下端に揃える
        pos.y = bestBottomY - halfSize;
        velocityY_ = 0.0f;   // ジャンプ速度を止める
        // onGround_ は false のまま（天井にいるだけで地面ではない）
    }
}

// 右方向（→）のマップ当たり判定
void Player::ResolveRightCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    // 左に動いている / 静止中なら右判定はいらない
    float moveX = pos.x - prevPos_.x;
    if (moveX <= 0.0f) {
        return;
    }

    int w = mapField_->GetWidth();
    int h = mapField_->GetHeight();

    // プレイヤーの AABB
    float playerLeft = pos.x - halfSize;
    float playerRight = pos.x + halfSize;
    float playerBottom = pos.y - halfSize;
    float playerTop = pos.y + halfSize;

    // 1フレーム前の右端
    float prevRight = prevPos_.x + halfSize;

    // ★「右端」が入っているタイル列
    //   ブロックは [centerX - halfSize, centerX + halfSize] を占めるので
    //   (x + halfSize) / tileSize で列を取るのが正解
    int tileX = static_cast<int>(std::floor((playerRight + halfSize) / tileSize_));
    if (tileX < 0 || tileX >= w) {
        return;
    }

    float bestBlockLeft = FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < h; ++ty) {

        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        // ブロックのY範囲（描画と同じ式）
        float centerY = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBottom = centerY - halfSize;
        float blockTop = centerY + halfSize;

        // 縦方向にかすってなければスキップ
        if (blockTop <= playerBottom || blockBottom >= playerTop) {
            continue;
        }

        // ブロックのX範囲
        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        // X方向で少しでも重なっている？
        if (playerRight > blockLeft && playerLeft < blockRight) {

            // ★ 前フレームはブロックの左側にいて、
            //    今フレームで blockLeft を跨いで「中に入った」ときだけ衝突とみなす
            if (prevRight <= blockLeft && playerRight >= blockLeft) {
                if (blockLeft < bestBlockLeft) {
                    bestBlockLeft = blockLeft;
                    hit = true;
                }
            }
        }
    }

    if (hit) {
        // 右端をブロックの左端にぴったり揃える
        pos.x = bestBlockLeft - halfSize;
    }
}
