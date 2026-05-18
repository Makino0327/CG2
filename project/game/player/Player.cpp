#include "Player.h"
#include <cfloat>   // FLT_MAX
#include <algorithm>

void Player::Initialize(Object3dCommon* object3dCommon, Input* input)
{
    input_ = input;
    // 3D描画の共通設定を保存する
    object3dCommon_ = object3dCommon;

    // プレイヤーのモデルを作る
    object_ = std::make_unique<Object3d>();

    // 3D描画の共通設定を渡す
    object_->Initialize(object3dCommon);

    // プレイヤーモデルを使う
    object_->SetModel("player/player.obj");

    // プレイヤーの大きさを設定する
    object_->SetScale(scale_);

    // プレイヤーの回転を設定する
    object_->SetRotate(rotate_);

    // プレイヤーの位置を設定する
    object_->SetTranslate(translate_);
    

}

void Player::Update(Camera* camera)
{
    if (!object_ || !input_) {
        return;
    }

    Vector3 pos = object_->GetTranslate();

    prevPos_ = pos;

    // 左に動く
    if (input_->PushKey(DIK_A)) {
        pos.x -= moveSpeed_;
    }

    // 右に動く
    if (input_->PushKey(DIK_D)) {
        pos.x += moveSpeed_;
    }

    // 前に動く
    if (input_->PushKey(DIK_W)) {
        pos.z += moveSpeed_;
    }

    // 後ろに動く
    if (input_->PushKey(DIK_S)) {
        pos.z -= moveSpeed_;
    }

    // 位置を反映する
    object_->SetTranslate(pos);

    // マウスの方向へ向ける
    RotateToMouse(camera);

    // プレイヤーを更新する
    object_->Update();


    // 左クリックで弾を撃つ
    if (input_->TriggerMouseLeft()) {
        FireBullet(camera);
    }

    // 弾を更新する
    UpdateBullets();
}


void Player::Draw()
{
    if (object_) {
        object_->Draw();
    }

    // プレイヤー弾を描画する
    for (auto& bullet : bullets_) {
        bullet->Draw();
    }
}

SphereCollider Player::GetCollider() const
{
    // プレイヤーの現在位置を球の当たり判定として返す
    return { GetWorldPosition(), colliderRadius_ };
}

void Player::OnHit()
{
    // 被弾したことが分かるように状態を保存する
    isHit_ = true;

    // HPが残っている時だけ1減らす
    if (hp_ > 0) {
        hp_ -= 1;
    }
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

Vector3 Player::GetWorldPosition() const
{
    if (!object_) {
        return { 0.0f, 0.0f, 0.0f };
    }

    // 現在位置を返す
    return object_->GetTranslate();
}

void Player::FireBullet(Camera* camera)
{
    if (!object_) {
        return;
    }

    // プレイヤーの位置を取る
    Vector3 playerPosition = object_->GetTranslate();

    // プレイヤー位置から少し上に出す
    Vector3 firePosition = {
        playerPosition.x,
        playerPosition.y + bulletSpawnHeight_,
        playerPosition.z
    };

    // マウス方向への発射方向を計算する
    Vector3 direction = PlayerBullet::CalcDirectionToMouseGround(
        firePosition,
        camera,
        input_);

    // 発射速度を作る
    Vector3 velocity = {
        direction.x * bulletSpeed_,
        direction.y * bulletSpeed_,
        direction.z * bulletSpeed_
    };

    // 弾を作る
    auto bullet = std::make_unique<PlayerBullet>();

    // 弾を初期化する
    bullet->Initialize(object3dCommon_, firePosition, velocity);

    // 弾をリストに追加する
    bullets_.push_back(std::move(bullet));
}

void Player::UpdateBullets()
{
    // プレイヤー弾を更新する
    for (auto& bullet : bullets_) {
        bullet->Update();
    }

    // 消えたプレイヤー弾を取り除く
    bullets_.erase(
        std::remove_if(
            bullets_.begin(),
            bullets_.end(),
            [](const std::unique_ptr<PlayerBullet>& bullet) {
                return bullet->IsDead();
            }),
        bullets_.end());
}

void Player::RotateToMouse(Camera* camera)
{
    if (!object_ || !input_ || !camera) {
        return;
    }

    // プレイヤーの位置を取る
    Vector3 playerPosition = object_->GetTranslate();

    // マウス方向への向きを計算する
    Vector3 direction = PlayerBullet::CalcDirectionToMouseGround(
        playerPosition,
        camera,
        input_);

    // 方向がない場合は回転しない
    if (direction.x == 0.0f && direction.z == 0.0f) {
        return;
    }

    // Z+ を正面としてY回転を作る
    rotate_.y = std::atan2(direction.x, direction.z) + frontAngleOffset_;

    // 回転を反映する
    object_->SetRotate(rotate_);
}
