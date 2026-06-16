#include "Enemy.h"
#include <cfloat>
#include <cmath>

#include "../../engine/3d/obj3d/Object3dCommon.h"

namespace {
    // a から b を引いたベクトルを返す
    Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
        return {
            a.x - b.x,
            a.y - b.y,
            a.z - b.z
        };
    }

    // 2つのベクトルの内積を返す
    float DotVector3(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

void Enemy::Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position)
{
    // 初期位置を設定する
    position_ = position;

    // WaypointMover を初期化する
    waypointMover_.Initialize(object3dCommon, camera);
    waypointMover_.SetModel("enemy/enemy.obj");
    waypointMover_.SetScale(scale_);
    waypointMover_.SetRotation(rotation_);
    waypointMover_.SetPosition(position_);
    waypointMover_.SetMoveSpeed(moveSpeed_);
    waypointMover_.SetLoop(true);

    // 初期状態を反映する
    waypointMover_.Update();
}

void Enemy::Update()
{
    Object3d* object = waypointMover_.GetObject3d();
    if (!object || isDead_) {
        return;
    }

    // 前フレームの状態を保存する
    prevPosition_ = position_;
    wasChasing_ = isChasing_;

    // 次に使う位置を現在の描画位置から取得する
    Vector3 nextPosition = object->GetTranslate();

    // プレイヤーまでのXZ方向ベクトルを求める
    Vector3 toPlayer = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // プレイヤーまでのXZ距離を求める
    float distanceToPlayer = std::sqrt(
        toPlayer.x * toPlayer.x +
        toPlayer.z * toPlayer.z
    );
    isTargetInSight_ = CheckTargetInSight();

    // 視界に入ったら追跡を開始する
    if (!isChasing_ && isTargetInSight_) {
        // 追跡開始フラグを立てる
        isChasing_ = true;
    }

    // 一定距離以上離れたら追跡をやめる
    if (isChasing_ && distanceToPlayer > chaseKeepRange_) {
        // 追跡終了フラグを下ろす
        isChasing_ = false;
    }

    // 追跡終了時は巡回へ戻す
    if (wasChasing_ && !isChasing_) {
        // 巡回ルートへ復帰する
        waypointMover_.ResumePatrol();
    }

    if (isChasing_ && distanceToPlayer > 0.001f) {
        // 追跡方向の単位ベクトルを求める
        Vector3 direction = Normalize(toPlayer);

        // 追跡方向へ移動する
        nextPosition.x += direction.x * moveSpeed_;
        nextPosition.z += direction.z * moveSpeed_;

        // 追跡方向を向く
        rotation_.y = std::atan2(direction.x, direction.z);
    } else {
        // 追跡していないときは巡回を更新する
        waypointMover_.Update();

        // 巡回更新後の位置と回転を反映する
        nextPosition = object->GetTranslate();
        rotation_ = object->GetRotate();
    }

    // 床コライダーで高さを補正する
    ResolveGroundHeight(nextPosition);

    // 壁コライダーで衝突を解決する
    ResolveWallCollision(nextPosition);

    // 計算後の位置を確定する
    position_ = nextPosition;

    // transform を更新して描画へ反映する
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

    // WaypointMover 経由で描画する
    waypointMover_.Draw();
}

void Enemy::SetPosition(const Vector3& position)
{
    // 補正後の位置を設定する
    position_ = position;

    // 内部 Object3d の位置も更新する
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
    // 位置と半径から球コライダーを作る
    return { position_, colliderRadius_ };
}

void Enemy::OnHit()
{
    // 被弾したら倒れた扱いにする
    isDead_ = true;
}

void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 追跡対象の位置を保存する
    targetPosition_ = targetPosition;
}

void Enemy::SetWaypoints(const std::vector<Vector3>& waypoints)
{
    // JSON から読んだウェイポイントを設定する
    waypointMover_.SetWaypoints(waypoints);
}

void Enemy::SetMap(const MapChipField* mapField, float tileSize)
{
    // CSV マップへの参照を保存する
    mapField_ = mapField;
    tileSize_ = tileSize;
}

bool Enemy::CheckTargetInSight() const
{
    // 敵からターゲットへのベクトルを求める
    Vector3 toTarget = {
        targetPosition_.x - position_.x,
        targetPosition_.y - (position_.y + sightHeight_),
        targetPosition_.z - position_.z
    };

    const float distance3D = std::sqrt(
        toTarget.x * toTarget.x +
        toTarget.y * toTarget.y +
        toTarget.z * toTarget.z
    );

    const float distanceXZ = std::sqrt(
        toTarget.x * toTarget.x +
        toTarget.z * toTarget.z
    );

    // 距離が短すぎるか射程外なら見えていない
    if (distance3D <= 0.0001f || distanceXZ > detectRange_) {
        return false;
    }

    // 敵の向きから前方ベクトルを求める
    Vector3 forward = {
        std::sin(rotation_.y),
        0.0f,
        std::cos(rotation_.y)
    };

    Vector3 directionXZSource = {
        toTarget.x,
        0.0f,
        toTarget.z
    };
    Vector3 directionXZ = Normalize(directionXZSource);
    float dot = DotVector3(forward, directionXZ);
    if (dot > 1.0f) {
        dot = 1.0f;
    }
    if (dot < -1.0f) {
        dot = -1.0f;
    }

    // 水平方向の視野角を超えたら見えていない
    const float horizontalAngle = std::acos(dot);
    if (horizontalAngle > sightHalfAngleRad_) {
        return false;
    }

    // 垂直方向の視野角も判定する
    const float verticalAngle = std::atan2(std::fabs(toTarget.y), distanceXZ);
    return verticalAngle <= sightVerticalHalfAngleRad_;
}

void Enemy::AppendVisionDebugLines(DebugLine3D& debugLine) const
{
    // 視野の始点を目線の高さに合わせる
    Vector3 origin = position_;
    origin.y += sightHeight_;

    // 左右の視野角を計算する
    const float centerYaw = rotation_.y;
    const float leftYaw = centerYaw - sightHalfAngleRad_;
    const float rightYaw = centerYaw + sightHalfAngleRad_;

    // 2D表示用にXZ平面上の左右端点だけを使う
    Vector3 leftEnd = {
        origin.x + std::sin(leftYaw) * detectRange_,
        origin.y,
        origin.z + std::cos(leftYaw) * detectRange_
    };
    Vector3 rightEnd = {
        origin.x + std::sin(rightYaw) * detectRange_,
        origin.y,
        origin.z + std::cos(rightYaw) * detectRange_
    };

    // 見えているときは緑、見えていないときは黄色にする
    Vector4 edgeColor = isTargetInSight_
        ? Vector4{ 0.1f, 0.8f, 0.1f, 1.0f }
        : Vector4{ 0.9f, 0.5f, 0.1f, 1.0f };

    // 中央線は描かず、左右の境界線と先端だけを描く
    debugLine.AddLine(origin, leftEnd, edgeColor);
    debugLine.AddLine(origin, rightEnd, edgeColor);
    debugLine.AddLine(leftEnd, rightEnd, edgeColor);
}
void Enemy::ResolveLeftCollisionWithMap(Vector3& pos)
{
    const float halfSize = tileSize_ * 0.5f;

    // 左へ動いていないときは判定しない
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

    // 右へ動いていないときは判定しない
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

    // 前へ動いていないときは判定しない
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

    // 後ろへ動いていないときは判定しない
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
    // 床コライダーがなければ何もしない
    if (!floorColliders_) {
        return;
    }

    bool foundGround = false;
    float bestGroundY = -FLT_MAX;

    // XZ が床の範囲外なら無視する
    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX コライダーだけを対象にする
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

        // XZ 範囲に入っている床だけを調べる
        if (pos.x < minX || pos.x > maxX || pos.z < minZ || pos.z > maxZ) {
            continue;
        }

        // 床の上面 Y 座標を求める
        float groundY = collider.center.y + halfY;

        // 最も高い床を採用する
        if (!foundGround || groundY > bestGroundY) {
            bestGroundY = groundY;
            foundGround = true;
        }
    }

    // 接地できたら床の上に乗せる
    if (foundGround) {
        pos.y = bestGroundY + colliderRadius_;
    }
}

void Enemy::ResolveWallCollision(Vector3& pos)
{
    // 壁コライダーがなければ何もしない
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
    // デバッグカメラ用に transform だけ更新する
    Object3d* object = waypointMover_.GetObject3d();
    if (object && !isDead_) {
        object->SetScale(scale_);
        object->SetRotate(rotation_);
        object->SetTranslate(position_);
        object->Update();
    }
}
