#include "Enemy.h"
#include <cfloat>
#include <cmath>

#include "../../engine/3d/obj3d/Object3dCommon.h"

void Enemy::Initialize(Object3dCommon* object3dCommon, const Vector3& position)
{
    // 初期座標を保存する
    position_ = position;

    // 敵の3Dオブジェクトを作る
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);

    // 敵モデルを設定する
    object_->SetModel("enemy/enemy.obj");

    object_->SetScale(scale_);
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);

    // 初期状態の描画行列を作る
    object_->Update();
}

void Enemy::Update()
{
    if (!object_ || isDead_) {
        return;
    }

    // 前フレーム座標を保存する
    prevPosition_ = position_;

    // 敵からターゲットへの方向を求める
    Vector3 direction = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // 長さがある時だけ正規化する
    const float lengthSq =
        direction.x * direction.x +
        direction.z * direction.z;

    if (lengthSq > 0.0001f) {
        direction = Normalize(direction);
    } else {
        direction = { 0.0f, 0.0f, 0.0f };
    }

    // 移動先を計算する
    Vector3 nextPosition = position_;
    nextPosition.x += direction.x * moveSpeed_;
    nextPosition.z += direction.z * moveSpeed_;

    // マップとの当たり判定を解決する
    //ResolveLeftCollisionWithMap(nextPosition);
    //ResolveRightCollisionWithMap(nextPosition);
    //ResolveTopCollisionWithMap(nextPosition);
    //ResolveBottomCollisionWithMap(nextPosition);

    // Blender JSON の床コライダーを使って地面の高さを合わせる
    ResolveGroundHeight(nextPosition);

    // Blender JSON の壁コライダーを使って横移動の衝突を解決する
    ResolveWallCollision(nextPosition);

    // 位置を反映する
    position_ = nextPosition;

    // 移動方向を向く
    if (direction.x != 0.0f || direction.z != 0.0f) {
        rotation_.y = std::atan2(direction.x, direction.z);
    }

    // オブジェクトへ反映する
    object_->SetScale(scale_);
    object_->SetRotate(rotation_);
    object_->SetTranslate(position_);
    object_->Update();
}

void Enemy::Draw()
{
    if (!object_ || isDead_) {
        return;
    }

    // 敵を描画する
    object_->Draw();
}

void Enemy::SetPosition(const Vector3& position)
{
    // 外から敵の座標を変えられるようにする
    position_ = position;
}

Vector3 Enemy::GetWorldPosition() const
{
    // 現在の座標を返す
    return position_;
}
SphereCollider Enemy::GetCollider() const
{
    // 敵の現在位置を球の当たり判定として返す
    return { position_, colliderRadius_ };
}

void Enemy::OnHit()
{
    // 弾が当たった敵は倒された扱いにする
    isDead_ = true;
}

void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 追いかける対象の座標を保存する
    targetPosition_ = targetPosition;
}

void Enemy::SetMap(const MapChipField* mapField, float tileSize)
{
    // 敵が参照するマップ情報を保存する
    mapField_ = mapField;
    tileSize_ = tileSize;
}

void Enemy::ResolveLeftCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    // 左に動いていない時は処理しない
    if (pos.x >= prevPosition_.x) {
        return;
    }

    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;
    float prevLeft = prevPosition_.x - halfSize;

    int tileX = static_cast<int>(std::floor(enemyLeft / tileSize_));
    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockRight = -FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        if (blockFront <= enemyBack || blockBack >= enemyFront) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (prevLeft >= blockRight && enemyLeft <= blockRight && enemyRight > blockLeft) {
            if (blockRight > bestBlockRight) {
                bestBlockRight = blockRight;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.x = bestBlockRight + halfSize;
    }
}

void Enemy::ResolveRightCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    const float halfSize = tileSize_ * 0.5f;

    // 右に動いていない時は処理しない
    if (pos.x <= prevPosition_.x) {
        return;
    }

    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;
    float prevRight = prevPosition_.x + halfSize;

    int tileX = static_cast<int>(std::floor((enemyRight + halfSize) / tileSize_));
    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockLeft = FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;
        float blockFront = centerZ + halfSize;

        if (blockFront <= enemyBack || blockBack >= enemyFront) {
            continue;
        }

        float centerX = static_cast<float>(tileX) * tileSize_;
        float blockLeft = centerX - halfSize;
        float blockRight = centerX + halfSize;

        if (prevRight <= blockLeft && enemyRight >= blockLeft && enemyLeft < blockRight) {
            if (blockLeft < bestBlockLeft) {
                bestBlockLeft = blockLeft;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.x = bestBlockLeft - halfSize;
    }
}

void Enemy::ResolveTopCollisionWithMap(Vector3& pos)
{
    if (!mapField_) { return; }

    // 上に動いていない時は処理しない
    if (pos.z <= prevPosition_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float enemyFront = pos.z + halfSize;
    float prevFront = prevPosition_.z + halfSize;

    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));
    int mapWidth = mapField_->GetWidth();
    int mapHeight = mapField_->GetHeight();

    if (tileX < 0 || tileX >= mapWidth) {
        return;
    }

    float bestBlockBack = FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockBack = centerZ - halfSize;

        if (prevFront <= blockBack && enemyFront >= blockBack) {
            if (blockBack < bestBlockBack) {
                bestBlockBack = blockBack;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.z = bestBlockBack - halfSize;
    }
}

void Enemy::ResolveBottomCollisionWithMap(Vector3& pos)
{
    if (!mapField_) {
        return;
    }

    // 下に動いていない時は処理しない
    if (pos.z >= prevPosition_.z) {
        return;
    }

    const float halfSize = tileSize_ * 0.5f;

    float enemyBack = pos.z - halfSize;
    float prevBack = prevPosition_.z - halfSize;

    int tileX = static_cast<int>(std::floor(pos.x / tileSize_ + 0.5f));
    int mapHeight = mapField_->GetHeight();

    float bestBlockFront = -FLT_MAX;
    bool hit = false;

    for (int tileY = 0; tileY < mapHeight; ++tileY) {
        if (mapField_->GetChip(tileX, tileY) != MapChipType::Block) {
            continue;
        }

        float centerZ = static_cast<float>(mapHeight - 1 - tileY) * tileSize_;
        float blockFront = centerZ + halfSize;

        if (prevBack >= blockFront && enemyBack <= blockFront) {
            if (blockFront > bestBlockFront) {
                bestBlockFront = blockFront;
                hit = true;
            }
        }
    }

    if (hit) {
        pos.z = bestBlockFront + halfSize;
    }
}

void Enemy::ResolveGroundHeight(Vector3& pos)
{
    // 床コライダーが無ければ何もしない
    if (!floorColliders_) {
        return;
    }

    bool foundGround = false;
    float bestGroundY = -FLT_MAX;

    // 敵の今の XZ 座標が乗っている床を探す
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

        // 敵がこの床の上にいるかを XZ で判定する
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

    // 見つかった床の上に敵を乗せる
    if (foundGround) {
        pos.y = bestGroundY + colliderRadius_ ;
    }
}

void Enemy::ResolveWallCollision(Vector3& pos)
{
    // 壁コライダーが無ければ何もしない
    if (!wallColliders_) {
        return;
    }

    float halfSize = colliderRadius_;

    float enemyLeft = pos.x - halfSize;
    float enemyRight = pos.x + halfSize;
    float enemyBack = pos.z - halfSize;
    float enemyFront = pos.z + halfSize;

    float prevLeft = prevPosition_.x - halfSize;
    float prevRight = prevPosition_.x + halfSize;
    float prevBack = prevPosition_.z - halfSize;
    float prevFront = prevPosition_.z + halfSize;

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

        bool overlapX = (enemyRight > wallLeft && enemyLeft < wallRight);
        bool overlapZ = (enemyFront > wallBack && enemyBack < wallFront);
        if (!overlapX || !overlapZ) {
            continue;
        }

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

void Enemy::UpdateRenderOnly()
{
    // // 本体オブジェクトがあればカメラ反映用に更新する
    if (object_ && !isDead_) {
        object_->SetScale(scale_);
        object_->SetRotate(rotation_);
        object_->SetTranslate(position_);
        object_->Update();
    }
}