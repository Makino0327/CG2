#include "Enemy.h"
#include <cfloat>
#include <cmath>

#include "../../engine/3d/obj3d/Object3dCommon.h"

namespace {
    // Return a - b.
    Vector3 SubtractVector3(const Vector3& a, const Vector3& b) {
        return {
            a.x - b.x,
            a.y - b.y,
            a.z - b.z
        };
    }

    // Return the dot product of two vectors.
    float DotVector3(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

void Enemy::Initialize(Object3dCommon* object3dCommon, Camera* camera, const Vector3& position)
{
    // 蛻晄悄菴咲ｽｮ繧剃ｿ晏ｭ倥☆繧・
    position_ = position;

    // WaypointMover 繧貞・譛溷喧縺吶ｋ
    waypointMover_.Initialize(object3dCommon, camera);
    waypointMover_.SetModel("enemy/enemy.obj");
    waypointMover_.SetScale(scale_);
    waypointMover_.SetRotation(rotation_);
    waypointMover_.SetPosition(position_);
    waypointMover_.SetMoveSpeed(moveSpeed_);
    waypointMover_.SetLoop(true);

    // 蛻晄悄謠冗判陦悟・繧呈峩譁ｰ縺吶ｋ
    waypointMover_.Update();
}

void Enemy::Update()
{
    Object3d* object = waypointMover_.GetObject3d();
    if (!object || isDead_) {
        return;
    }

    // 蜑阪ヵ繝ｬ繝ｼ繝菴咲ｽｮ繧剃ｿ晏ｭ倥☆繧・
    prevPosition_ = position_;
    wasChasing_ = isChasing_;

    // 縺・▲縺溘ｓ迴ｾ蝨ｨ菴咲ｽｮ繧呈ｬ｡縺ｮ菴咲ｽｮ縺ｨ縺励※謖√▲縺ｦ縺翫￥
    Vector3 nextPosition = object->GetTranslate();

    // 繝励Ξ繧､繝､繝ｼ縺ｾ縺ｧ縺ｮXZ蟷ｳ髱｢霍晞屬繧定ｨ育ｮ励☆繧・
    Vector3 toPlayer = {
        targetPosition_.x - position_.x,
        0.0f,
        targetPosition_.z - position_.z
    };

    // 繝励Ξ繧､繝､繝ｼ縺ｾ縺ｧ縺ｮ霍晞屬繧呈ｱゅａ繧・
    float distanceToPlayer = std::sqrt(
        toPlayer.x * toPlayer.x +
        toPlayer.z * toPlayer.z
    );
    isTargetInSight_ = CheckTargetInSight();

    // 縺ｾ縺霑ｽ蟆ｾ縺励※縺・↑縺・→縺阪・縲∬ｦ九▽縺代ｋ霍晞屬縺ｫ蜈･縺｣縺溘ｉ霑ｽ蟆ｾ髢句ｧ・
    if (!isChasing_ && isTargetInSight_) {
        // 繝励Ξ繧､繝､繝ｼ繧定ｦ九▽縺代◆縺ｮ縺ｧ霑ｽ蟆ｾ髢句ｧ・
        isChasing_ = true;
    }

    // 縺吶〒縺ｫ霑ｽ蟆ｾ荳ｭ縺ｪ繧峨√°縺ｪ繧企屬繧後ｋ縺ｾ縺ｧ霑ｽ蟆ｾ繧堤ｶ壹￠繧・
    if (isChasing_ && distanceToPlayer > chaseKeepRange_) {
        // 繝励Ξ繧､繝､繝ｼ繧定ｦ句､ｱ縺｣縺溘・縺ｧ霑ｽ蟆ｾ邨ゆｺ・
        isChasing_ = false;
    }

    // 霑ｽ蟆ｾ荳ｭ縺ｮ縺ｨ縺阪・繝励Ξ繧､繝､繝ｼ縺ｸ蜷代°縺｣縺ｦ遘ｻ蜍輔☆繧・
    if (wasChasing_ && !isChasing_) {
        // 隕句､ｱ縺｣縺溽椪髢薙↓蟾｡蝗樒憾諷九∈謌ｻ縺励※縲∬ｦ句ｼｵ繧雁慍轤ｹ縺ｸ蠕ｩ蟶ｰ縺ｧ縺阪ｋ繧医≧縺ｫ縺吶ｋ
        waypointMover_.ResumePatrol();
    }

    if (isChasing_ && distanceToPlayer > 0.001f) {
        // 繝励Ξ繧､繝､繝ｼ譁ｹ蜷代∈縺ｮ蜊倅ｽ阪・繧ｯ繝医Ν繧剃ｽ懊ｋ
        Vector3 direction = Normalize(toPlayer);

        // 繝励Ξ繧､繝､繝ｼ縺ｮ譁ｹ蜷代∈遘ｻ蜍輔☆繧・
        nextPosition.x += direction.x * moveSpeed_;
        nextPosition.z += direction.z * moveSpeed_;

        // 繝励Ξ繧､繝､繝ｼ縺ｮ譁ｹ蜷代ｒ蜷代￥
        rotation_.y = std::atan2(direction.x, direction.z);
    } else {
        // 霑ｽ蟆ｾ縺励※縺・↑縺・→縺阪・莉翫∪縺ｧ騾壹ｊ繧ｦ繧ｧ繧､繝昴う繝ｳ繝育ｧｻ蜍輔☆繧・
        waypointMover_.Update();

        // 繧ｦ繧ｧ繧､繝昴う繝ｳ繝育ｧｻ蜍募ｾ後・菴咲ｽｮ縺ｨ蝗櫁ｻ｢繧貞女縺大叙繧・
        nextPosition = object->GetTranslate();
        rotation_ = object->GetRotate();
    }

    // Blender JSON 縺ｮ蠎翫さ繝ｩ繧､繝繝ｼ縺ｧ鬮倥＆繧貞粋繧上○繧・
    ResolveGroundHeight(nextPosition);

    // Blender JSON 縺ｮ螢√さ繝ｩ繧､繝繝ｼ縺ｧ讓ｪ遘ｻ蜍輔ｒ豁｢繧√ｋ
    ResolveWallCollision(nextPosition);

    // 陬懈ｭ｣蠕後・菴咲ｽｮ繧剃ｿ晏ｭ倥☆繧・
    position_ = nextPosition;

    // 陬懈ｭ｣蠕後・ transform 繧呈緒逕ｻ縺ｸ謌ｻ縺・
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

    // WaypointMover 縺梧戟縺､繧ｪ繝悶ず繧ｧ繧ｯ繝医ｒ謠冗判縺吶ｋ
    waypointMover_.Draw();
}

void Enemy::SetPosition(const Vector3& position)
{
    // 螟悶°繧芽｣懈ｭ｣縺輔ｌ縺滉ｽ咲ｽｮ繧剃ｿ晏ｭ倥☆繧・
    position_ = position;

    // 謠冗判菴咲ｽｮ繧ょ酔縺伜ｺｧ讓吶∈蜷医ｏ縺帙ｋ
    Object3d* object = waypointMover_.GetObject3d();
    if (object) {
        object->SetTranslate(position_);
    }
}

Vector3 Enemy::GetWorldPosition() const
{
    // 迴ｾ蝨ｨ菴咲ｽｮ繧定ｿ斐☆
    return position_;
}

SphereCollider Enemy::GetCollider() const
{
    // 迴ｾ蝨ｨ菴咲ｽｮ縺ｨ蜊雁ｾ・°繧臥帥繧ｳ繝ｩ繧､繝繝ｼ繧定ｿ斐☆
    return { position_, colliderRadius_ };
}

void Enemy::OnHit()
{
    // 蠖薙◆縺｣縺滓雰縺ｯ蛟偵☆
    isDead_ = true;
}

void Enemy::SetTargetPosition(const Vector3& targetPosition)
{
    // 譌ｧ霑ｽ蟆ｾ繝ｭ繧ｸ繝・け莠呈鋤縺ｮ縺溘ａ谿九＠縺ｦ縺翫￥
    targetPosition_ = targetPosition;
}

void Enemy::SetWaypoints(const std::vector<Vector3>& waypoints)
{
    // Blender JSON 縺九ｉ隱ｭ繧薙□邨瑚ｷｯ轤ｹ繧呈ｸ｡縺・
    waypointMover_.SetWaypoints(waypoints);
}

void Enemy::SetMap(const MapChipField* mapField, float tileSize)
{
    // 譌ｧ CSV 蛻､螳夂畑縺ｮ蜿ら・繧剃ｿ晏ｭ倥☆繧・
    mapField_ = mapField;
    tileSize_ = tileSize;
}

bool Enemy::CheckTargetInSight() const
{
    // 謨ｵ縺九ｉ繧ｿ繝ｼ繧ｲ繝・ヨ縺ｸ縺ｮ繝吶け繝医Ν繧剃ｽ懊ｋ
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

    // 霍晞屬螟悶ｄ蜷御ｽ咲ｽｮ縺ｯ隕夜㍽螟匁桶縺・↓縺吶ｋ
    if (distance3D <= 0.0001f || distanceXZ > detectRange_) {
        return false;
    }

    // 迴ｾ蝨ｨ縺ｮ蜷代″縺九ｉ蜑肴婿蜷代・繧ｯ繝医Ν繧剃ｽ懊ｋ
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

    // 豌ｴ蟷ｳ蜊願ｧ剃ｻ･蜀・↑繧牙燕譁ｹ隕夜㍽縺ｫ蜈･縺｣縺ｦ縺・ｋ
    const float horizontalAngle = std::acos(dot);
    if (horizontalAngle > sightHalfAngleRad_) {
        return false;
    }

    // 邵ｦ譁ｹ蜷代ｂ蜊願ｧ剃ｻ･蜀・↑繧芽ｦ夜㍽蜀・→縺ｿ縺ｪ縺・
    const float verticalAngle = std::atan2(std::fabs(toTarget.y), distanceXZ);
    return verticalAngle <= sightVerticalHalfAngleRad_;
}

void Enemy::AppendVisionDebugLines(DebugLine3D& debugLine) const
{
    // 視野の始点は敵の視界の高さに合わせる
    Vector3 origin = position_;
    origin.y += sightHeight_;

    // 左右の視野境界の向きを計算する
    const float centerYaw = rotation_.y;
    const float leftYaw = centerYaw - sightHalfAngleRad_;
    const float rightYaw = centerYaw + sightHalfAngleRad_;

    // 視野表示は2Dにして、XZ平面上の左右端点だけを使う
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

    // 視認中は緑、非視認時は黄色で表示する
    Vector4 edgeColor = isTargetInSight_
        ? Vector4{ 0.1f, 0.8f, 0.1f, 1.0f }
        : Vector4{ 0.9f, 0.5f, 0.1f, 1.0f };

    // 真ん中の線は消して、左右の境界線と先端の辺だけを描く
    debugLine.AddLine(origin, leftEnd, edgeColor);
    debugLine.AddLine(origin, rightEnd, edgeColor);
    debugLine.AddLine(leftEnd, rightEnd, edgeColor);
}
void Enemy::ResolveLeftCollisionWithMap(Vector3& pos)
{
    const float halfSize = tileSize_ * 0.5f;

    // 蟾ｦ縺ｫ蜍輔＞縺ｦ縺・↑縺・凾縺ｯ蜃ｦ逅・＠縺ｪ縺・
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

    // 蜿ｳ縺ｫ蜍輔＞縺ｦ縺・↑縺・凾縺ｯ蜃ｦ逅・＠縺ｪ縺・
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

    // 荳翫↓蜍輔＞縺ｦ縺・↑縺・凾縺ｯ蜃ｦ逅・＠縺ｪ縺・
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

    // 荳九↓蜍輔＞縺ｦ縺・↑縺・凾縺ｯ蜃ｦ逅・＠縺ｪ縺・
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
    // 蠎翫さ繝ｩ繧､繝繝ｼ縺檎┌縺代ｌ縺ｰ菴輔ｂ縺励↑縺・
    if (!floorColliders_) {
        return;
    }

    bool foundGround = false;
    float bestGroundY = -FLT_MAX;

    // 謨ｵ縺ｮ莉翫・ XZ 蠎ｧ讓吶′荵励▲縺ｦ縺・ｋ蠎翫ｒ謗｢縺・
    for (const LevelColliderData& collider : *floorColliders_) {
        // BOX collider 莉･螟悶・莉翫・菴ｿ繧上↑縺・
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

        // 謨ｵ縺後％縺ｮ蠎翫・荳翫↓縺・ｋ縺九ｒ XZ 縺ｧ蛻､螳壹☆繧・
        if (pos.x < minX || pos.x > maxX || pos.z < minZ || pos.z > maxZ) {
            continue;
        }

        // 蠎翫・荳企擇 Y 繧呈ｱゅａ繧・
        float groundY = collider.center.y + halfY;

        // 縺・■縺ｰ繧馴ｫ倥＞蠎翫ｒ謗｡逕ｨ縺吶ｋ
        if (!foundGround || groundY > bestGroundY) {
            bestGroundY = groundY;
            foundGround = true;
        }
    }

    // 隕九▽縺九▲縺溷ｺ翫・荳翫↓謨ｵ繧剃ｹ励○繧・
    if (foundGround) {
        pos.y = bestGroundY + colliderRadius_;
    }
}

void Enemy::ResolveWallCollision(Vector3& pos)
{
    // 螢√さ繝ｩ繧､繝繝ｼ縺檎┌縺代ｌ縺ｰ菴輔ｂ縺励↑縺・
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
    // 繝・ヰ繝・げ繧ｫ繝｡繝ｩ遒ｺ隱咲畑縺ｫ謠冗判陦悟・縺縺第峩譁ｰ縺吶ｋ
    Object3d* object = waypointMover_.GetObject3d();
    if (object && !isDead_) {
        object->SetScale(scale_);
        object->SetRotate(rotation_);
        object->SetTranslate(position_);
        object->Update();
    }
}
