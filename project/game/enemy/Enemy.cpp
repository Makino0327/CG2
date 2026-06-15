#include "Enemy.h"
#include <cfloat>
#include <cmath>

#include "../../engine/3d/obj3d/Object3dCommon.h"

void Enemy::Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position)
{
    // 初期位置を保存する
    position_ = position;

    // WaypointMover を初期化する
    waypointMover_.Initialize(object3dCommon, camera);
    waypointMover_.SetModel("enemy/enemy.obj");
    waypointMover_.SetScale(scale_);
    waypointMover_.SetRotation(rotation_);
    waypointMover_.SetPosition(position_);
    waypointMover_.SetMoveSpeed(moveSpeed_);
    waypointMover_.SetLoop(true);

    // 初期描画行列を更新する
    waypointMover_.Update();
}

void Enemy::Update()
{
    Object3d* object = waypointMover_.GetObject3d();
    if (!object || isDead_) {
        return;
    }

    // 前フレーム位置を保存する
    prevPosition_ = position_;
    wasChasing_ = isChasing_;

    // いったん現在位置を次の位置として持っておく
    Vector3 nextPosition = object->GetTranslate();

    // プレイヤーまでのXZ平面距離を計算する
    Vector3 toPlayer = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // プレイヤーまでの距離を求める
    float distanceToPlayer = std::sqrt(
        toPlayer.x * toPlayer.x +
        toPlayer.z * toPlayer.z
    );

    // まだ追尾していないときは、見つける距離に入ったら追尾開始
    if (!isChasing_ && distanceToPlayer <= detectRange_) {
        // プレイヤーを見つけたので追尾開始
        isChasing_ = true;
    }

    // すでに追尾中なら、かなり離れるまで追尾を続ける
    if (isChasing_ && distanceToPlayer > chaseKeepRange_) {
        // プレイヤーを見失ったので追尾終了
        isChasing_ = false;
    }

    // 追尾中のときはプレイヤーへ向かって移動する
    if (wasChasing_ && !isChasing_) {
        // 見失った瞬間に巡回状態へ戻して、見張り地点へ復帰できるようにする
        waypointMover_.ResumePatrol();
    }

    if (isChasing_ && distanceToPlayer > 0.001f) {
        // プレイヤー方向への単位ベクトルを作る
        Vector3 direction = Normalize(toPlayer);

        // プレイヤーの方向へ移動する
        nextPosition.x += direction.x * moveSpeed_;
        nextPosition.z += direction.z * moveSpeed_;

        // プレイヤーの方向を向く
        rotation_.y = std::atan2(direction.x, direction.z);
    } else {
        // 追尾していないときは今まで通りウェイポイント移動する
        waypointMover_.Update();

        // ウェイポイント移動後の位置と回転を受け取る
        nextPosition = object->GetTranslate();
        rotation_ = object->GetRotate();
    }

    // Blender JSON の床コライダーで高さを合わせる
    ResolveGroundHeight(nextPosition);

    // Blender JSON の壁コライダーで横移動を止める
    ResolveWallCollision(nextPosition);

    // 補正後の位置を保存する
    position_ = nextPosition;

    // 補正後の transform を描画へ戻す
    object->SetScale(scale_);
    object->SetRotate(rotation_);
    object->SetTranslate(position_);
    object->Update();
}

void Enemy::Draw()
{
    if (isDead_) {
        return;
    }

    // WaypointMover が持つオブジェクトを描画する
    waypointMover_.Draw();
}

void Enemy::SetPosition(const Vector3& position)
{
    // 外から補正された位置を保存する
    position_ = position;

    // 描画位置も同じ座標へ合わせる
    Object3d* object = waypointMover_.GetObject3d();
    if (object) {
        object->SetTranslate(position_);
    }
}

Vector3 Enemy::GetWorldPosition() const
{
    // 現在位置を返す
    return position_;
}

SphereCollider Enemy::GetCollider() const
{
    // 現在位置と半径から球コライダーを返す
    return { position_, colliderRadius_ };
}

void Enemy::OnHit()
{
    // 当たった敵は倒す
    isDead_ = true;
}

void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 旧追尾ロジック互換のため残しておく
    targetPosition_ = targetPosition;
}

void Enemy::SetWaypoints(const std::vector<Vector3>& waypoints)
{
    // Blender JSON から読んだ経路点を渡す
    waypointMover_.SetWaypoints(waypoints);
}

void Enemy::SetMap(const MapChipField* mapField, float tileSize)
{
    // 旧 CSV 判定用の参照を保存する
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
        pos.y = bestGroundY + colliderRadius_;
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
    // デバッグカメラ確認用に描画行列だけ更新する
    Object3d* object = waypointMover_.GetObject3d();
    if (object && !isDead_) {
        object->SetScale(scale_);
        object->SetRotate(rotation_);
        object->SetTranslate(position_);
        object->Update();
    }
}
