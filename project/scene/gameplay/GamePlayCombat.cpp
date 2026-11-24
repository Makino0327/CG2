#include "GamePlayScene.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "../../engine/base/offscreen/OffscreenRenderer.h"
#include "../../engine/input/Input.h"
#include "../../engine/particle/Particle.h"
#include "../../engine/utils/Logger.h"
#include "../../game/camera/Camera.h"
#include "../../game/collision/BodyContact.h"
#include "../../game/player/PlayerBullet.h"

// 近接攻撃の位置に縦または横の斬撃エフェクトを出す。
void GamePlayScene::EmitMeleeSlashEffect(const Vector3& center, const Vector3& direction)
{
    if (!particleSystem_) {
        return;
    }

    Vector3 slashDirection = { direction.x, 0.0f, direction.z };
    if (slashDirection.x == 0.0f && slashDirection.z == 0.0f) {
        slashDirection = { 0.0f, 0.0f, 1.0f };
    } else {
        slashDirection = Normalize(slashDirection);
    }

    // 球を細長く伸ばした斬撃を1個だけ出す
    const Vector3 slashPosition = {
        center.x,
        center.y + 1.0f,
        center.z
    };

    static std::mt19937 randomEngine{ std::random_device{}() };
    std::uniform_int_distribution<int> randomDirection(0, 1);
    const bool isVerticalSlash = randomDirection(randomEngine) == 0;
    const Vector3 slashScale = isVerticalSlash
        ? Vector3{ 0.28f, 1.95f, 0.28f }
        : Vector3{ 1.95f, 0.28f, 0.28f };

    // エンジン側を変えず、縦長か横長かをランダムに切り替える
    particleSystem_->Emit(
        slashPosition,
        slashScale,
        { 0.0f, 0.0f, 0.0f },
        { 0.95f, 0.98f, 1.0f, 0.90f },
        0.11f);
}

// ナイフ攻撃の範囲を調べ、当たった敵や木箱にダメージを与える。
void GamePlayScene::UpdateMeleeAttack()
{
    if (!player_ || player_->IsDead() || !GetContext().input) {
        return;
    }

    // ナイフを使っているときだけ、左クリックで斬撃を出す。
    if (!player_->IsMeleeMode() || !GetContext().input->TriggerMouseLeft()) {
        return;
    }

    const Vector3 playerPosition = player_->GetWorldPosition();
    Vector3 slashDirection = PlayerBullet::CalcDirectionToMouseGround(
        playerPosition,
        GetContext().camera,
        GetContext().input);

    if (slashDirection.x == 0.0f && slashDirection.z == 0.0f) {
        slashDirection = { 0.0f, 0.0f, 1.0f };
    } else {
        slashDirection = Normalize(slashDirection);
    }

    // 斬撃の判定範囲の中央にエフェクトを出す。
    const float normalMeleeEffectDistance = normalMeleeAttackRange_ * 0.5f;
    const Vector3 normalMeleeEffectPosition = {
        playerPosition.x + slashDirection.x * normalMeleeEffectDistance,
        playerPosition.y,
        playerPosition.z + slashDirection.z * normalMeleeEffectDistance
    };

    EmitMeleeSlashEffect(normalMeleeEffectPosition, slashDirection);

    Enemy* normalMeleeHitTarget = nullptr;
    float nearestNormalHitForward = normalMeleeAttackRange_;
    constexpr float kNormalMeleeHalfWidth = 1.25f;

    for (auto& enemy : enemies_) {
        // 死んでいる敵には通常近接を当てない
        if (enemy->IsDead()) {
            continue;
        }

        const Vector3 enemyPosition = enemy->GetWorldPosition();
        Vector3 toEnemy = {
            enemyPosition.x - playerPosition.x,
            0.0f,
            enemyPosition.z - playerPosition.z
        };

        const float forwardDistance =
            toEnemy.x * slashDirection.x +
            toEnemy.z * slashDirection.z;
        if (forwardDistance < 0.0f || forwardDistance > normalMeleeAttackRange_) {
            continue;
        }

        const float sideX = toEnemy.x - slashDirection.x * forwardDistance;
        const float sideZ = toEnemy.z - slashDirection.z * forwardDistance;
        const float sideDistanceSq = sideX * sideX + sideZ * sideZ;
        if (sideDistanceSq > kNormalMeleeHalfWidth * kNormalMeleeHalfWidth) {
            continue;
        }

        // 斬撃の帯に入った一番手前の敵だけに1ダメージを与える
        if (forwardDistance <= nearestNormalHitForward) {
            nearestNormalHitForward = forwardDistance;
            normalMeleeHitTarget = enemy.get();
        }
    }

    if (normalMeleeHitTarget) {
        normalMeleeHitTarget->OnMeleeDamage(
            normalMeleeHitTarget->GetWorldPosition(),
            slashDirection);
    }

    // 弾切れでも補給できるよう、近くで正面を向いて切れば木箱を壊せる。
    for (size_t index = 0; index < ammoCrates_.size(); ++index) {
        const auto& crate = ammoCrates_[index];
        if (crate.hp <= 0) { continue; }
        const float dx = crate.collider.center.x - playerPosition.x;
        const float dz = crate.collider.center.z - playerPosition.z;
        const float forward = dx * slashDirection.x + dz * slashDirection.z;
        const float side = std::abs(dx * slashDirection.z - dz * slashDirection.x);
        if (forward >= 0.0f && forward <= normalMeleeAttackRange_ + crate.collider.halfSize.z &&
            side <= kNormalMeleeHalfWidth + crate.collider.halfSize.x &&
            std::abs(crate.collider.center.y - playerPosition.y) <= crate.collider.halfSize.y + 1.0f) {
            DamageAmmoCrate(index, 2);
        }
    }
}

// 弾が発射された位置に画面を歪ませる衝撃波を出す。
void GamePlayScene::StartBulletShockwaves()
{
    if (!player_ || !GetContext().camera || !GetContext().offscreenRenderer) {
        return;
    }

    // Playerが撃った弾の発射位置を受け取り、Scene側で画面UVへ変換する
    const std::vector<Vector3> shotPositions =
        player_->ConsumeBulletShockwavePositions();

    for (const Vector3& shotPosition : shotPositions) {
        Vector2 shockwaveUV{};
        if (!TryConvertWorldToScreenUV(
            shotPosition,
            GetContext().camera->GetViewProjectionMatrix(),
            shockwaveUV)) {
            continue;
        }

        // 画面上の弾の発射位置を中心にポストエフェクトの歪みを始める
        // 銃の衝撃波は短い時間で広げて、前より速く見せる
        GetContext().offscreenRenderer->SetShockwaveDuration(0.16f);
        GetContext().offscreenRenderer->StartShockwave(shockwaveUV);
    }
}

// 手榴弾の爆発範囲を調べ、敵と木箱への効果や画面の衝撃波を処理する。
void GamePlayScene::CheckGrenadeExplosions()
{
    if (!player_) {
        return;
    }

    // このフレームに発生した全爆発位置をPlayerから受け取る
    const std::vector<Vector3> explosionPositions =
        player_->ConsumeGrenadeExplosions();
    const float explosionRadiusSq =
        grenadeExplosionRadius_ * grenadeExplosionRadius_;

    for (const Vector3& explosionPosition : explosionPositions) {
        // グレネードの爆発位置を画面UVに変換して、大きめの衝撃波を出す
        if (GetContext().camera && GetContext().offscreenRenderer) {
            Vector2 grenadeShockwaveUV{};
            if (TryConvertWorldToScreenUV(
                explosionPosition,
                GetContext().camera->GetViewProjectionMatrix(),
                grenadeShockwaveUV)) {
                // グレネードの衝撃波は通常時間に戻して、大きさだけ個別に変える
                GetContext().offscreenRenderer->SetShockwaveDuration(0.28f);
                GetContext().offscreenRenderer->StartShockwave(grenadeShockwaveUV, 0.22f);
            }
        }

        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) {
                continue;
            }

            const Vector3 enemyPosition = enemy->GetWorldPosition();
            const float diffX = enemyPosition.x - explosionPosition.x;
            const float diffY = enemyPosition.y - explosionPosition.y;
            const float diffZ = enemyPosition.z - explosionPosition.z;
            const float distanceSq =
                diffX * diffX + diffY * diffY + diffZ * diffZ;

            // 爆発半径内なら残りHPに関係なく即死させる
            if (distanceSq <= explosionRadiusSq) {
                enemy->OnExplosionHit();
            }
        }
        // グレネードの爆発でも木箱を壊し、補給を取り出せるようにする。
        for (size_t index = 0; index < ammoCrates_.size(); ++index) {
            const auto& crate = ammoCrates_[index];
            if (crate.hp <= 0) { continue; }
            const Vector3 nearest = Collision::ClosestPointOnBox(explosionPosition, crate.collider);
            const float dx = nearest.x - explosionPosition.x;
            const float dy = nearest.y - explosionPosition.y;
            const float dz = nearest.z - explosionPosition.z;
            if (dx * dx + dy * dy + dz * dz <= explosionRadiusSq) { DamageAmmoCrate(index, 2); }
        }
    }
}
// プレイヤーの被弾が成立したときに回数を記録し、画面の揺れとノイズを出す。
void GamePlayScene::ApplyPlayerDamage(const Vector3& attackerPosition, float shakePower, int shakeFrames)
{
    // クリアデモは射撃と撃破の流れを毎周必ず再生できるようにする。
    if (demoMode_ == DemoMode::Clear) { return; }
    if (!player_ || !player_->OnHit(attackerPosition)) { return; }
    // 無敵時間で弾かれた接触は、被弾回数に加えない。
    ++runStats_.damageTakenCount;
    // 展示の確認ログには、通常攻撃で減った体力を残す。
    if (demoMode_ == DemoMode::GameOver) { Logger::Log("[GameOverDemo] Enemy hit, HP: " + std::to_string(player_->GetHp())); }
    // 無敵時間で弾かれた接触では、画面エフェクトの時間を再開しない。
    if (followCamera_) { followCamera_->StartShake(shakePower,shakeFrames); }
    // ボス撃破までの戦闘中だけ被弾ノイズを出す。
    if (!completedRun_ && GetContext().offscreenRenderer) { GetContext().offscreenRenderer->StartDamageNoise(); }
}

// 敵との接触、銃弾の命中、ボスの攻撃を判定し、撃破後の処理を進める。
void GamePlayScene::CheckCollisions()
{
    if (!player_) {
        return;
    }

    // テレポートでレベルを作り直したフレームは、古い敵や弾との判定を続けない
    if (!IsDemo() && CheckBossTeleport()) {
        return;
    }

    // 無敵時間中も接触補正を続け、複数の敵との重なりを反復して解消する。
    for (int pass = 0; pass < 4; ++pass) {
        for (auto& enemy : enemies_) {
            if (enemy->IsDead() || player_->IsDead()) { continue; }
            SphereCollider playerCollider = player_->GetCollider();
            SphereCollider enemyCollider = enemy->GetCollider();
            if (!Collision::IsHit(playerCollider, enemyCollider)) { continue; }

            // 実際にHPが減った接触だけ、揺れと短いノイズを開始する。練習用の敵は押し返すだけ。
            if (!enemy->IsPassive()) { ApplyPlayerDamage(enemyCollider.center); }
            BodyContact::Separate(playerCollider, enemyCollider, &wallColliders_, &floorColliders_);
            player_->SetPosition(playerCollider.center);
            enemy->SetPosition(enemyCollider.center);
        }
    }

    const auto& bullets = player_->GetBullets();
    for (const auto& bullet : bullets) {
        if (bullet->IsDead()) { continue; }
        float nearestTime = 1.0f;
        Enemy* hitEnemy = nullptr;
        Boss* hitBoss = nullptr;
        size_t hitCrate = ammoCrates_.size();
        // 壁までの区間で敵とボスをまとめて調べ、最も手前の対象にだけ当てる。
        for (auto& enemy : enemies_) {
            if (enemy->IsDead()) { continue; }
            float hitTime = 0.0f;
            if (bullet->CheckHit(enemy->GetBulletCollider(), enemy->GetPreviousPosition(), hitTime) &&
                ((!hitEnemy && !hitBoss) || hitTime < nearestTime)) {
                nearestTime = hitTime;
                hitEnemy = enemy.get();
            }
        }
        for (auto& boss : bosses_) {
            if (boss->IsDead()) { continue; }
            float hitTime = 0.0f;
            if (bullet->CheckHit(boss->GetCollider(), hitTime) &&
                ((!hitEnemy && !hitBoss) || hitTime < nearestTime)) {
                nearestTime = hitTime;
                hitEnemy = nullptr;
                hitBoss = boss.get();
            }
        }
        // 敵やボスより手前にある木箱へ最初に当たった弾だけを消費する。
        for (size_t index = 0; index < ammoCrates_.size(); ++index) {
            if (ammoCrates_[index].hp <= 0) { continue; }
            float hitTime = 0.0f;
            if (bullet->CheckHit(wallColliders_[ammoCrates_[index].wallColliderIndex], hitTime) &&
                ((!hitEnemy && !hitBoss && hitCrate == ammoCrates_.size()) || hitTime < nearestTime)) {
                nearestTime = hitTime;
                hitEnemy = nullptr;
                hitBoss = nullptr;
                hitCrate = index;
            }
        }
        if (hitEnemy) {
            player_->RecordShotHit(*bullet);
            bullet->OnHit(nearestTime);
            const Vector3 hitDirection = Normalize(bullet->GetVelocity());
            const BoxCollider enemyCollider = hitEnemy->GetBulletCollider(nearestTime);
            const Vector3 bulletPosition = bullet->GetCollider().center;
            // 球の表面ではなく、実際に当たった敵の箱の表面から血しぶきを出す。
            const Vector3 hitPosition = Collision::ClosestPointOnBox(bulletPosition, enemyCollider);
            hitEnemy->OnHit(hitPosition, hitDirection);
        } else if (hitBoss) {
            // ボスへの命中位置で止めて、HPを1減らす。
            player_->RecordShotHit(*bullet);
            bullet->OnHit(nearestTime);
            hitBoss->OnHit();
            // ボスが複数いる場合も、最後に倒したボスを映す。
            if (hitBoss->IsDead()) { bossDefeatFocus_ = hitBoss->GetCollider().center; }
        } else if (hitCrate < ammoCrates_.size()) {
            bullet->OnHit(nearestTime);
            DamageAmmoCrate(hitCrate, 1);
        }
        // 敵に当たらなかった場合は壁への命中を確定し、軌跡を生成する。
        bullet->FinishUpdate();
    }

    // 弾によるボス撃破を先に処理し、生きているボスの着地攻撃だけを当てる
    for (const auto& boss : bosses_) {
        if (!player_->IsDead() && boss->IsAttackHit(player_->GetCollider())) {
            // ボスの攻撃も、通常の被弾と同じノイズを使う。
            ApplyPlayerDamage(boss->GetCollider().center,0.40f,12);
        }
    }

    // 銃・ナイフ・爆発のいずれで撃破しても、敵を削除する前にドロップを出す。
    if (demoMode_ != DemoMode::GameOver) { UpdateAmmoDrops(); }

    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                // 死亡直後には消さず、破片演出が終わってから削除する
                return enemy->IsReadyToRemove();
            }),
        enemies_.end());
}

// 重なった敵同士を水平方向に押し戻す。
void GamePlayScene::ResolveEnemyOverlap()
{
    for (size_t i = 0; i < enemies_.size(); ++i) {
        for (size_t j = i + 1; j < enemies_.size(); ++j) {
            Vector3 posA = enemies_[i]->GetWorldPosition();
            Vector3 posB = enemies_[j]->GetWorldPosition();

            Vector3 diff = {
                posB.x - posA.x,
                0.0f,
                posB.z - posA.z
            };

            float distanceSq = diff.x * diff.x + diff.z * diff.z;
            float radiusSum = enemies_[i]->GetBodyRadius() + enemies_[j]->GetBodyRadius();

            if (distanceSq <= 0.0001f) {
                diff = { 1.0f, 0.0f, 0.0f };
                distanceSq = 1.0f;
            }

            float distance = std::sqrt(distanceSq);

            if (distance < radiusSum) {
                float overlap = radiusSum - distance;

                Vector3 push = {
                    diff.x / distance,
                    0.0f,
                    diff.z / distance
                };

                posA.x -= push.x * overlap * 0.5f;
                posA.z -= push.z * overlap * 0.5f;

                posB.x += push.x * overlap * 0.5f;
                posB.z += push.z * overlap * 0.5f;

                enemies_[i]->SetPosition(posA);
                enemies_[j]->SetPosition(posB);
            }
        }
    }
}
