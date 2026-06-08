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

    // 死亡中にRキーで復活する
        // 死亡中の復活処理はシーン側でまとめて行う
    if (isDead_) {
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

    //// 左方向の壁判定を行う
    //ResolveLeftCollisionWithMap(pos);

    //// 右方向の壁判定を行う
    //ResolveRightCollisionWithMap(pos);

    //// 前方向の壁判定を行う
    //ResolveTopCollisionWithMap(pos);

    //// 後方向の壁判定を行う
    //ResolveBottomCollisionWithMap(pos);

    // Blender JSON の床コライダーを使って地面の高さを合わせる
    ResolveGroundHeight(pos);

    // Blender JSON の壁コライダーを使って横移動の衝突を解決する
    ResolveWallCollision(pos);

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

    // 無敵時間を減らす
    if (invincibleTimer_ > 0) {
        invincibleTimer_--;
    }

}


void Player::Draw()
{
    // 死んでいたら描画しない
    if (!object_ || isDead_) {
        return;
    }

    // 無敵中は赤く点滅させる
    if (invincibleTimer_ > 0) {
        // 一定フレームごとに表示色を切り替える
        if ((invincibleTimer_ / blinkInterval_) % 2 == 0) {
            object_->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
        } else {
            object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    } else {
        // 通常時は白に戻す
        object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    object_->Draw();

    // プレイヤーの弾を描画する
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
    // すでに消えていたら何もしない
    if (isDead_) {
        return;
    }

    // 無敵時間中はダメージを受けない
    if (invincibleTimer_ > 0) {
        return;
    }

    // 被弾状態を保存する
    isHit_ = true;

    // HPが残っている時だけ1減らす
    if (hp_ > 0) {
        hp_ -= 1;
    }

    // ダメージを受けたら無敵時間を開始する
    invincibleTimer_ = invincibleDuration_;

    // HPが0以下になったら消す
    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
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

    if (pos.z >= prevPos_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float playerBack = pos.z - halfSize;
    float prevBack = prevPos_.z - halfSize;

    // ★ 中心X → タイルX（＋0.5で補正）
    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));

    int mapH = mapField_->GetHeight();

    float bestTopY = -FLT_MAX;
    bool  hit = false;

    for (int ty = 0; ty < mapH; ++ty) {
        if (mapField_->GetChip(tileX, ty) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapH - 1 - ty) * tileSize_;
        float blockFront = centerZ + halfSize;

        if (prevBack >= blockFront && playerBack <= blockFront) {
            if (blockFront > bestTopY) {
                bestTopY = blockFront;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.z = bestTopY + halfSize;
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
    float playerBack = pos.z - halfSize;
    float playerFront = pos.z + halfSize;

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
        float centerZ = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        // 縦方向にかすってなければスキップ
        if (blockFront <= playerBack || blockBack >= playerFront) {
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
    if (pos.z <= prevPos_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float playerFront = pos.z + halfSize;
    float prevFront = prevPos_.z + halfSize;

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
        float centerZ = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBack = centerZ - halfSize;

        // 「前フレームは下にいて、今フレームで下端をまたいだ」なら頭がぶつかった
        if (prevFront <= blockBack && playerFront >= blockBack) {
            if (blockBack < bestBottomY) {
                bestBottomY = blockBack;
                hit = true;
            }
        }
    }

    if (hit) {
        // 頭をブロックの下端に揃える
        pos.z = bestBottomY - halfSize;
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
    float playerBack = pos.z - halfSize;
    float playerFront = pos.z + halfSize;

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
        float centerZ = static_cast<float>(h - 1 - ty) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        // 縦方向にかすってなければスキップ
        if (blockFront <= playerBack || blockBack >= playerFront) {
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
    bullet->Initialize(
        object3dCommon_,
        firePosition,
        velocity,
        mapField_,
        tileSize_);

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

void Player::Respawn()
{
    // HPを最大まで戻す
    hp_ = maxHp_;

    // 死亡状態を解除する
    isDead_ = false;

    // 被弾状態を解除する
    isHit_ = false;

    // 無敵時間をリセットする
    invincibleTimer_ = 0;

    // 初期位置に戻す
    translate_ = { 2.0f, 0.5f, 2.0f };
    rotate_ = { 0.0f, 0.0f, 0.0f };

    // 3Dオブジェクトにも座標と回転を反映する
    if (object_) {
        object_->SetTranslate(translate_);
        object_->SetRotate(rotate_);
        object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        object_->Update();
    }
}


void Player::ResolveGroundHeight(Vector3& pos)
{
    // 床コライダーが無ければ何もしない
    if (!floorColliders_) {
        return;
    }

    bool foundGround = false;
    float bestGroundY = -FLT_MAX;

    // プレイヤーの今の XZ 座標が乗っている床を探す
    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX collider 以外は今は使わない
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        float halfX = collider.size.x * 0.5f;
        float halfY = collider.size.y * 0.5f;
        float halfZ = collider.size.z * 0.5f;

        float minX = collider.center.x - halfX;
        float maxX = collider.center.x + halfX;
        float minZ = collider.center.z - halfZ;
        float maxZ = collider.center.z + halfZ;

        // プレイヤーがこの床の上にいるかを XZ で判定する
        if (pos.x < minX || pos.x > maxX || pos.z < minZ || pos.z > maxZ) {
            continue;
        }

        // 床の上面 Y を求める
        float groundY = collider.center.y + halfY;

        // いちばん高い床を採用する
        if (!foundGround || groundY > bestGroundY) {
            bestGroundY = groundY;
            foundGround = true;
        }
    }

    // 見つかった床の上にプレイヤーを乗せる
    if (foundGround) {
        pos.y = bestGroundY + colliderRadius_ ;
    }
}

void Player::ResolveWallCollision(Vector3& pos)
{
    // 壁コライダーが無ければ何もしない
    if (!wallColliders_) {
        return;
    }

    float halfSize = colliderRadius_;

    float playerLeft = pos.x - halfSize;
    float playerRight = pos.x + halfSize;
    float playerBack = pos.z - halfSize;
    float playerFront = pos.z + halfSize;

    float prevLeft = prevPos_.x - halfSize;
    float prevRight = prevPos_.x + halfSize;
    float prevBack = prevPos_.z - halfSize;
    float prevFront = prevPos_.z + halfSize;

    for (const LevelColliderData& collider : *wallColliders_) {
        if (!collider.hasCollider || collider.type != "BOX") {
            continue;
        }

        float halfX = collider.size.x * 0.5f;
        float halfZ = collider.size.z * 0.5f;

        float wallLeft = collider.center.x - halfX;
        float wallRight = collider.center.x + halfX;
        float wallBack = collider.center.z - halfZ;
        float wallFront = collider.center.z + halfZ;

        // まず今フレームで重なっているかを見る
        bool overlapX = (playerRight > wallLeft && playerLeft < wallRight);
        bool overlapZ = (playerFront > wallBack && playerBack < wallFront);
        if (!overlapX || !overlapZ) {
            continue;
        }

        // 前フレーム位置から、どちら側から入ったかを判定して押し戻す
        if (prevRight <= wallLeft) {
            pos.x = wallLeft - halfSize;
        } else if (prevLeft >= wallRight) {
            pos.x = wallRight + halfSize;
        } else if (prevFront <= wallBack) {
            pos.z = wallBack - halfSize;
        } else if (prevBack >= wallFront) {
            pos.z = wallFront + halfSize;
        }
    }
}

void Player::UpdateRenderOnly()
{
    // // 本体オブジェクトがあればカメラ反映用に更新する
    if (object_) {
        object_->Update();
    }

    // // 弾も見た目だけ更新する
    for (auto& bullet : bullets_) {
        if (!bullet->IsDead()) {
            bullet->Update();
        }
    }
}