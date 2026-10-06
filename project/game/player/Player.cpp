#include "Player.h"
#include "PlayerMuzzle.h"
#include "ShootingBloom.h"
#include <cfloat>   // FLT_MAX
#include <algorithm>
#include <cmath>
#include <random>
#include "../../engine/particle/Particle.h"

void Player::Initialize(
    Object3dCommon* object3dCommon,
    Input* input,
    ParticleSystem* particleSystem)
{
    input_ = input;

    // 弾の軌跡を生成する共有パーティクルを保存する
    particleSystem_ = particleSystem;
    // 3D描画の共通設定を保存する
    object3dCommon_ = object3dCommon;

    // プレイヤーのモデルを作る
    object_ = std::make_unique<Object3d>();

    // 3D描画の共通設定を渡す
    object_->Initialize(object3dCommon);

    // 銃と骨が含まれるQuaterniusのSWATモデルを使う。
    object_->SetModel("player_modular/Swat.gltf");
    object_->SetSkeletonVisible(false);
    InitializeAnimations();

    // プレイヤーの大きさを設定する
    object_->SetScale(scale_);
    // 素材の足元はY=-2/3。拡大後も当たり判定の下端へ足を合わせる。
    object_->SetModelOffset({ 0.0f, 2.0f / 3.0f - colliderRadius_ / scale_.y, 0.0f });

    // プレイヤーの回転を設定する
    object_->SetRotate(rotate_);

    // プレイヤーの位置を設定する
    object_->SetTranslate(translate_);
    

}

void Player::InitializeAnimations()
{
    // 番号を決め打ちせず、配布ファイルのアニメーション名から検索する。
    const std::string directory = "Resources/player_modular";
    const std::string file = "Swat.gltf";
    const auto names = GetAnimationNames(directory, file);
    const auto load = [&](const std::string& name) {
        const auto found = std::find(names.begin(), names.end(), name);
        assert(found != names.end());
        return LoadAnimationFile(directory, file, static_cast<uint32_t>(found - names.begin()));
    };
    const auto index = [](Motion motion) { return static_cast<size_t>(motion); };
    animations_[index(Motion::Idle)] = load("Idle_Gun");
    animations_[index(Motion::Run)] = load("Run");
    animations_[index(Motion::RunBack)] = load("Run_Back");
    animations_[index(Motion::RunLeft)] = load("Run_Left");
    animations_[index(Motion::RunRight)] = load("Run_Right");
    // 両手構えと反動は、このゲーム向けに調整した専用のモーションを読む。
    animations_[index(Motion::AimIdle)] = LoadAnimationFile(directory, "SwatTwoHand.gltf", 0);
    shootingOverlay_ = LoadAnimationFile(directory, "SwatTwoHand.gltf", 1);

    // 構え中は移動速度が半分になるので、足のモーションも半分の速さにする。
    // 上半身だけを構え姿勢に固定し、前後左右の足運びは元の走行から残す。
    const Animation& aiming = animations_[index(Motion::AimIdle)];
    object_->SetAnimationParentReference("Abdomen", aiming);
    const Skeleton& skeleton = object_->GetSkeleton();
    const auto isUpperBody = [&](const std::string& name) {
        const auto found = skeleton.jointMap.find(name);
        if (found == skeleton.jointMap.end()) { return false; }
        std::optional<int32_t> jointIndex = found->second;
        while (jointIndex) {
            const Joint& joint = skeleton.joints[*jointIndex];
            if (joint.name == "Abdomen") { return true; }
            jointIndex = joint.parent;
        }
        return false;
    };
    // 射撃で脚や足を上書きしないよう、上半身以外のキーを取り除く。
    std::erase_if(shootingOverlay_.nodeAnimations,
        [&](const auto& node) { return !isUpperBody(node.first); });
    for (size_t direction = 0; direction < 4; ++direction) {
        Animation aimed = animations_[index(Motion::Run) + direction];
        aimed.duration *= 2.0f;
        for (auto& [name, node] : aimed.nodeAnimations) {
            for (auto& key : node.translate.keyframes) { key.time *= 2.0f; }
            for (auto& key : node.rotate.keyframes) { key.time *= 2.0f; }
            for (auto& key : node.scale.keyframes) { key.time *= 2.0f; }

            // 腹部より上の骨だけを対象にし、腰・脚・足は移動アニメーションに任せる。
            const auto foundPose = aiming.nodeAnimations.find(name);
            if (!isUpperBody(name) || foundPose == aiming.nodeAnimations.end()) { continue; }
            const NodeAnimation& pose = foundPose->second;
            if (!pose.translate.keyframes.empty()) {
                node.translate.keyframes = { { 0.0f, CalculateValue(pose.translate.keyframes, 0.0f) } };
            }
            if (!pose.rotate.keyframes.empty()) {
                node.rotate.keyframes = { { 0.0f, CalculateValue(pose.rotate.keyframes, 0.0f) } };
            }
            if (!pose.scale.keyframes.empty()) {
                node.scale.keyframes = { { 0.0f, CalculateValue(pose.scale.keyframes, 0.0f) } };
            }
        }
        animations_[index(Motion::AimRun) + direction] = std::move(aimed);
    }
    PlayMotion(Motion::Idle, true);
}

void Player::PlayMotion(Motion motion, bool restart)
{
    // 同じモーションを毎フレーム先頭へ戻さず、切り替え時だけリセットする。
    if (currentMotion_ == motion && !restart) { return; }
    const auto isLocomotion = [](Motion value) {
        return (value >= Motion::Run && value <= Motion::RunRight) ||
            (value >= Motion::AimRun && value <= Motion::AimRight);
    };
    if (!restart && currentMotion_ != Motion::Count) {
        // 足の周期を保ったまま約0.15秒で次の姿勢へつなぐ。
        object_->TransitionToAnimation(animations_[static_cast<size_t>(motion)], 0.15f,
            isLocomotion(currentMotion_) && isLocomotion(motion));
        currentMotion_ = motion;
        return;
    }
    currentMotion_ = motion;
    object_->ResetSkeletonPose();
    object_->SetAnimation(animations_[static_cast<size_t>(motion)]);
    object_->ResetAnimationTime();
    object_->SetIsAnimationPlaying(true);
}

void Player::UpdateAnimation(const Vector3& movement, bool isAiming)
{
    // 構え姿勢を使う間は、足運びに含まれる腰の回転で銃口を振らない。
    const bool usesAimingPose = isAiming && !isReloading_;
    object_->SetAnimationParentCorrectionEnabled(usesAimingPose);
    const bool moving = movement.x * movement.x + movement.z * movement.z > 0.000001f;
    if (!isAiming || isReloading_) { object_->StopAnimationOverlay(); }
    if (!moving) {
        PlayMotion(isAiming && !isReloading_ ? Motion::AimIdle : Motion::Idle);
        return;
    }

    // マウスへの向きと実際の移動方向を比較し、後退や横移動でも足を合わせる。
    const float yaw = rotate_.y - frontAngleOffset_;
    const float forward = movement.x * std::sin(yaw) + movement.z * std::cos(yaw);
    const float right = movement.x * std::cos(yaw) - movement.z * std::sin(yaw);
    size_t direction = 0;
    if (std::abs(forward) >= std::abs(right)) {
        direction = forward >= 0.0f ? 0 : 1;
    } else {
        direction = right < 0.0f ? 2 : 3;
    }
    const Motion first = usesAimingPose ? Motion::AimRun : Motion::Run;
    PlayMotion(static_cast<Motion>(static_cast<size_t>(first) + direction));
}

int Player::GetCurrentAmmo() const
{
    // 現在選んでいる銃の残弾数を返す
    if (selectedGunMode_ == AttackMode::AssaultRifle) {
        return assaultRifleAmmo_;
    }
    if (selectedGunMode_ == AttackMode::Shotgun) {
        return shotgunAmmo_;
    }
    if (selectedGunMode_ == AttackMode::Gun) {
        return handgunAmmo_;
    }
    return 0;
}

int Player::GetCurrentMaxAmmo() const
{
    // 現在選んでいる銃の最大弾数を返す
    return GetMaxAmmoForCurrentWeapon();
}

int Player::GetMaxAmmoForCurrentWeapon() const
{
    // 選択中の銃に応じて最大弾数を返す
    if (selectedGunMode_ == AttackMode::AssaultRifle) {
        return assaultRifleMaxAmmo_;
    }
    if (selectedGunMode_ == AttackMode::Shotgun) {
        return shotgunMaxAmmo_;
    }
    if (selectedGunMode_ == AttackMode::Gun) {
        return handgunMaxAmmo_;
    }
    return 0;
}

int Player::GetReloadDurationForCurrentWeapon() const
{
    // 武器ごとにリロード時間を変える
    if (selectedGunMode_ == AttackMode::AssaultRifle) {
        return assaultRifleReloadDuration_;
    }
    if (selectedGunMode_ == AttackMode::Shotgun) {
        return shotgunReloadDuration_;
    }
    if (selectedGunMode_ == AttackMode::Gun) {
        return handgunReloadDuration_;
    }
    return 0;
}

void Player::StartReload()
{
    // 選択中の銃が満タンの時やリロード中は何もしない
    if (selectedGunMode_ == AttackMode::Knife || isReloading_) {
        return;
    }

    const int maxAmmo = GetMaxAmmoForCurrentWeapon();
    if (maxAmmo <= 0 || GetCurrentAmmo() >= maxAmmo) {
        return;
    }

    isReloading_ = true;
    reloadTimer_ = GetReloadDurationForCurrentWeapon();
}

void Player::UpdateReload()
{
    if (!isReloading_) {
        return;
    }

    if (reloadTimer_ > 0) {
        reloadTimer_--;
    }

    if (reloadTimer_ > 0) {
        return;
    }

    // リロード完了時に選択中の銃だけ満タンにする
    isReloading_ = false;
    if (selectedGunMode_ == AttackMode::AssaultRifle) {
        assaultRifleAmmo_ = assaultRifleMaxAmmo_;
    } else if (selectedGunMode_ == AttackMode::Shotgun) {
        shotgunAmmo_ = shotgunMaxAmmo_;
    } else if (selectedGunMode_ == AttackMode::Gun) {
        handgunAmmo_ = handgunMaxAmmo_;
    }
}

void Player::Update(Camera* camera)
{
    if (!object_ || !input_) {
        return;
    }

    // 発砲検知用フラグを毎フレーム下ろす
    firedThisFrame_ = false;

    // 死亡中にRキーで復活する
        // 死亡中の復活処理はシーン側でまとめて行う
    if (isDead_) {
        return;
    }


    const bool isAimingGun = input_->PushMouseRight();
    const float currentMoveSpeed = isAimingGun ? aimingMoveSpeed_ : moveSpeed_; // 構え中はゆっくり歩く

    Vector3 pos = object_->GetTranslate();

    prevPos_ = pos;

    // 左に動く
    if (input_->PushKey(DIK_A)) {
        pos.x -= currentMoveSpeed;
    }

    // 右に動く
    if (input_->PushKey(DIK_D)) {
        pos.x += currentMoveSpeed;
    }

    // 前に動く
    if (input_->PushKey(DIK_W)) {
        pos.z += currentMoveSpeed;
    }

    // 後ろに動く
    if (input_->PushKey(DIK_S)) {
        pos.z -= currentMoveSpeed;
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

    // Qキーでは銃だけを切り替え、近距離攻撃は右クリックを離した時の通常状態にする
    if (input_->TriggerKey(DIK_Q)) {
        // Qキーを押すたびに銃を順番に切り替える
        if (selectedGunMode_ == AttackMode::Gun) {
            selectedGunMode_ = AttackMode::AssaultRifle;
        } else if (selectedGunMode_ == AttackMode::AssaultRifle) {
            selectedGunMode_ = AttackMode::Shotgun;
        } else {
            selectedGunMode_ = AttackMode::Gun;
        }

        // 銃を切り替えたらリロードや連射状態をリセットする
        isReloading_ = false;
        reloadTimer_ = 0;
        assaultFireTimer_ = 0;
        assaultContinuousShotCount_ = 0;
    }

    // 右クリック長押し中だけ選択中の銃、それ以外はナイフにする
    attackMode_ = isAimingGun ? selectedGunMode_ : AttackMode::Knife;

    if (!isAimingGun) {
        // 銃を構えていない間は連射状態だけ止め、リロードはそのまま進める
        assaultFireTimer_ = 0;
        assaultContinuousShotCount_ = 0;
    }

    // Rキーは構え中でなくても選択中の銃をリロードできる
    if (input_->TriggerKey(DIK_R)) {
        StartReload();
    }
    UpdateReload();

    // 移動・向き・構えの骨を先に1回更新し、描画中の銃口から弾を出せるようにする。
    UpdateAnimation({ pos.x - prevPos_.x, 0.0f, pos.z - prevPos_.z }, isAimingGun);
    object_->Update();

    // ハンドガンは左クリックした瞬間に1発撃つ
    if (attackMode_ == AttackMode::Gun && input_->TriggerMouseLeft()) {
        FireBullet(camera);
    }

    // ショットガンは1クリックで複数の弾を扇状に撃つ
    if (attackMode_ == AttackMode::Shotgun && input_->TriggerMouseLeft()) {
        FireShotgun(camera);
    }

    // アサルトライフルは左クリック押しっぱなしで一定間隔ごとに弾を撃つ
    if (attackMode_ == AttackMode::AssaultRifle) {
        if (assaultFireTimer_ > 0) {
            assaultFireTimer_--;
        }

        if (input_->PushMouseLeft() && assaultFireTimer_ <= 0) {
            // 連射するほど少しずつ弾をばらけさせる
            float spreadAngle = std::min(
                assaultMaxSpreadAngle_,
                static_cast<float>(assaultContinuousShotCount_) * assaultSpreadIncrease_);

            if (FireBullet(camera, spreadAngle)) {
                assaultContinuousShotCount_++;
                assaultFireTimer_ = assaultFireInterval_;
            }
        }

        if (!input_->PushMouseLeft()) {
            // 撃つのをやめたらばらけを元に戻す
            assaultContinuousShotCount_ = 0;
        }
    } else {
        assaultFireTimer_ = 0;
        assaultContinuousShotCount_ = 0;
    }

    // Gキーを押した瞬間にグレネードを1個投げる
    if (input_->TriggerKey(DIK_G)) {
        ThrowGrenade(camera);
    }

    if (firedThisFrame_ && isAimingGun && !isReloading_) {
        // 発射後に反動を開始する。次の骨更新で再生し、このフレームの銃口と弾を一致させる。
        object_->PlayAnimationOverlay(shootingOverlay_);
    }

    // 弾とグレネードを更新する
    UpdateBullets();
    UpdateGrenades();

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

    // プレイヤーが投げたグレネードを描画する
    for (auto& grenade : grenades_) {
        grenade->Draw();
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

void Player::SetPosition(const Vector3& position)
{
    // 内部座標と前フレーム座標を同時に更新する
    translate_ = position;
    prevPos_ = position;

    if (!object_) {
        return;
    }

    // 近接攻撃演出の位置をモデルへ即座に反映する
    object_->SetTranslate(position);
    object_->Update();
}

Vector3 Player::GetWorldPosition() const
{
    if (!object_) {
        return { 0.0f, 0.0f, 0.0f };
    }

    // 現在位置を返す
    return object_->GetTranslate();
}

bool Player::FireBullet(Camera* camera, float spreadAngle)
{
    if (!object_ || !camera || !input_) {
        return false;
    }

    // リロード中は発射できない
    if (isReloading_) {
        return false;
    }

    int* currentAmmo = nullptr;
    if (attackMode_ == AttackMode::Gun) {
        currentAmmo = &handgunAmmo_;
    } else if (attackMode_ == AttackMode::AssaultRifle) {
        currentAmmo = &assaultRifleAmmo_;
    } else {
        return false;
    }

    // 弾がない時は自動でリロードを開始する
    if (*currentAmmo <= 0) {
        StartReload();
        return false;
    }

    // タイトルと同じ銃口の10頂点から、弾と発射炎の位置を求める。
    const Vector3 firePosition = GetPlayerMuzzlePosition(*object_);

    // 銃口から発射し、中心付近を狙った時は目標位置を前方へ補正する。
    Vector3 direction = PlayerBullet::CalcDirectionToMouseAtHeight(
        firePosition,
        object_->GetTranslate(),
        camera,
        input_,
        object_->GetTranslate().y + bulletAimHeight_);

    // 発射方向が取れない時は弾を減らさず撃たない
    if (direction.x == 0.0f && direction.z == 0.0f) {
        return false;
    }

    // ばらけ角度がある時だけ弾の向きを少しランダムにずらす
    if (spreadAngle > 0.0f) {
        static std::random_device seedGenerator;
        static std::mt19937 randomEngine(seedGenerator());

        std::uniform_real_distribution<float> spreadDistribution(-spreadAngle, spreadAngle);
        float angle = spreadDistribution(randomEngine);

        float cosAngle = std::cos(angle);
        float sinAngle = std::sin(angle);

        Vector3 spreadDirection = {
            direction.x * cosAngle - direction.z * sinAngle,
            direction.y,
            direction.x * sinAngle + direction.z * cosAngle
        };

        direction = Normalize(spreadDirection);
    }

    // ここまで来たら発射できるので弾を1発減らす
    (*currentAmmo)--;

    // 銃声を発生させたことをシーンへ伝える
    firedThisFrame_ = true;

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
        wallColliders_,
        particleSystem_,
        object_->GetTranslate().y + bulletAimHeight_);

    // 弾の発射位置をScene側へ渡して画面歪みの中心に使う
    pendingBulletShockwavePositions_.push_back(firePosition);

    if (particleSystem_) {
        // 発射口の外側へ広がるオレンジ色の光を作る
        particleSystem_->Emit(
            firePosition,
            { 1.2f, 1.2f, 1.2f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.34f, 0.05f, 0.38f },
            0.16f, ShootingBloom::muzzleFlashStrength);

        // 発射口の中央へ黄色い強い光を重ねる
        particleSystem_->Emit(
            firePosition,
            { 0.68f, 0.68f, 0.68f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.72f, 0.22f, 0.9f },
            0.11f, ShootingBloom::muzzleFlashStrength);

        // 最も明るい白い中心を重ねる
        particleSystem_->Emit(
            firePosition,
            { 0.30f, 0.30f, 0.30f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.95f, 0.72f, 1.0f },
            0.075f, ShootingBloom::muzzleFlashStrength);

        // 発射方向に対して左右へ小さな火花を配置する
        Vector3 sideDirection = { -direction.z, 0.0f, direction.x };
        Vector3 leftFlashPosition = {
            firePosition.x + sideDirection.x * 0.30f,
            firePosition.y,
            firePosition.z + sideDirection.z * 0.30f
        };
        Vector3 rightFlashPosition = {
            firePosition.x - sideDirection.x * 0.30f,
            firePosition.y,
            firePosition.z - sideDirection.z * 0.30f
        };

        particleSystem_->Emit(
            leftFlashPosition,
            { 0.18f, 0.18f, 0.18f },
            { sideDirection.x * 0.6f, 0.0f, sideDirection.z * 0.6f },
            { 1.0f, 0.46f, 0.08f, 0.4f },
            0.08f, ShootingBloom::muzzleFlashStrength);

        particleSystem_->Emit(
            rightFlashPosition,
            { 0.18f, 0.18f, 0.18f },
            { -sideDirection.x * 0.6f, 0.0f, -sideDirection.z * 0.6f },
            { 1.0f, 0.46f, 0.08f, 0.4f },
            0.08f, ShootingBloom::muzzleFlashStrength);
    }

    // 弾をリストに追加する
    bullets_.push_back(std::move(bullet));
    return true;
}

bool Player::FireShotgun(Camera* camera)
{
    if (!object_ || !camera || !input_) {
        return false;
    }

    // リロード中は発射できない
    if (isReloading_) {
        return false;
    }

    // 弾がないときは自動でリロードを開始する
    if (shotgunAmmo_ <= 0) {
        StartReload();
        return false;
    }

    // 全ての散弾と発射炎を、同じ銃口の実頂点の位置から出す。
    const Vector3 firePosition = GetPlayerMuzzlePosition(*object_);

    // 散弾の中心方向にも、通常弾と同じ近距離の補正を使う。
    Vector3 baseDirection = PlayerBullet::CalcDirectionToMouseAtHeight(
        firePosition,
        object_->GetTranslate(),
        camera,
        input_,
        object_->GetTranslate().y + bulletAimHeight_);

    // マウス方向が取れない場合は弾を消費しない
    if (baseDirection.x == 0.0f && baseDirection.z == 0.0f) {
        return false;
    }

    // 1回のショットなので、弾薬はここで1つだけ減らす
    shotgunAmmo_--;
    firedThisFrame_ = true;

    const int pelletCount = std::max(1, shotgunPelletCount_);
    const float centerIndex = static_cast<float>(pelletCount - 1) * 0.5f;
    const float angleStep = (pelletCount > 1)
        ? shotgunSpreadAngle_ / static_cast<float>(pelletCount - 1)
        : 0.0f;

    static std::random_device seedGenerator;
    static std::mt19937 randomEngine(seedGenerator());
    std::uniform_real_distribution<float> randomAngleDistribution(
        -shotgunRandomSpreadAngle_,
        shotgunRandomSpreadAngle_);
    std::uniform_real_distribution<float> randomSpeedDistribution(
        shotgunMinSpeedScale_,
        shotgunMaxSpeedScale_);

    for (int index = 0; index < pelletCount; ++index) {
        // 均等な散弾角度にランダムなブレを足して自然に散らす
        const float angle =
            (static_cast<float>(index) - centerIndex) * angleStep +
            randomAngleDistribution(randomEngine);
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);

        Vector3 pelletDirection = Normalize(Vector3{
            baseDirection.x * cosAngle - baseDirection.z * sinAngle,
            baseDirection.y,
            baseDirection.x * sinAngle + baseDirection.z * cosAngle
        });

        // 発射位置を散らさず、進行方向と速度だけで散弾の広がりを表現する。
        const Vector3 pelletPosition = firePosition;

        const float speedScale = randomSpeedDistribution(randomEngine);
        Vector3 velocity = {
            pelletDirection.x * bulletSpeed_ * speedScale,
            pelletDirection.y * bulletSpeed_ * speedScale,
            pelletDirection.z * bulletSpeed_ * speedScale
        };

        auto bullet = std::make_unique<PlayerBullet>();
        bullet->Initialize(
            object3dCommon_,
            pelletPosition,
            velocity,
            wallColliders_,
            particleSystem_,
            object_->GetTranslate().y + bulletAimHeight_);

        bullets_.push_back(std::move(bullet));
    }

    // ショットガン全体で、銃口の位置から1つの衝撃波だけ出す。
    pendingBulletShockwavePositions_.push_back(firePosition);

    if (particleSystem_) {
        // 通常弾より大きい発射炎でショットガンらしさを出す
        particleSystem_->Emit(
            firePosition,
            { 1.8f, 1.8f, 1.8f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.38f, 0.06f, 0.48f },
            0.14f, ShootingBloom::muzzleFlashStrength);

        // 中心に短い白い閃光を重ねる
        particleSystem_->Emit(
            firePosition,
            { 0.75f, 0.75f, 0.75f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.92f, 0.58f, 0.9f },
            0.08f, ShootingBloom::muzzleFlashStrength);
    }

    return true;
}

void Player::ThrowGrenade(Camera* camera)
{
    if (!object_ || !camera || !input_) {
        return;
    }

    // プレイヤーからマウス方向への水平な投擲方向を求める
    Vector3 playerPosition = object_->GetTranslate();
    Vector3 throwDirection = PlayerBullet::CalcDirectionToMouseGround(
        playerPosition,
        camera,
        input_);

    if (throwDirection.x == 0.0f && throwDirection.z == 0.0f) {
        return;
    }

    // プレイヤーの少し上かつ前方を投擲開始位置にする
    Vector3 throwPosition = {
        playerPosition.x + throwDirection.x * grenadeMuzzleDistance_,
        playerPosition.y + grenadeSpawnHeight_,
        playerPosition.z + throwDirection.z * grenadeMuzzleDistance_
    };

    // Playerは生成と所有だけを担当し、飛行処理はPlayerGrenadeへ任せる
    auto grenade = std::make_unique<PlayerGrenade>();
    grenade->Initialize(
        object3dCommon_,
        camera,
        throwPosition,
        throwDirection,
        floorColliders_,
        wallColliders_);

    grenades_.push_back(std::move(grenade));
}

void Player::UpdateGrenades()
{
    // 投げた全グレネードの物理挙動と爆発タイマーを更新する
    for (auto& grenade : grenades_) {
        grenade->Update();

        Vector3 explosionPosition{};
        if (grenade->ConsumeExplosion(explosionPosition)) {
            // 爆発位置へパーティクルを出し、Sceneへシェイク開始を要求する
            EmitGrenadeExplosion(explosionPosition);
            grenadeShakeRequested_ = true;

            // Scene側で敵への即死判定に使う爆発位置を保存する
            pendingGrenadeExplosions_.push_back(explosionPosition);
        }
    }

    // Explodeが終わったグレネードを一覧から削除する
    grenades_.erase(
        std::remove_if(
            grenades_.begin(),
            grenades_.end(),
            [](const std::unique_ptr<PlayerGrenade>& grenade) {
                return grenade->IsDead();
            }),
        grenades_.end());
}

bool Player::ConsumeGrenadeShakeRequest()
{
    // 要求が無ければカメラを揺らさない
    if (!grenadeShakeRequested_) {
        return false;
    }

    // 同じ爆発で何度も揺れないよう、返すと同時に要求を下ろす
    grenadeShakeRequested_ = false;
    return true;
}

std::vector<Vector3> Player::ConsumeGrenadeExplosions()
{
    // 保存中の爆発位置を呼び出し側へ移し、Player側を空にする
    std::vector<Vector3> explosions;
    explosions.swap(pendingGrenadeExplosions_);
    return explosions;
}

std::vector<Vector3> Player::ConsumeBulletShockwavePositions()
{
    // 保存中の発射位置を呼び出し側へ移し、Player側を空にする
    std::vector<Vector3> positions;
    positions.swap(pendingBulletShockwavePositions_);
    return positions;
}

void Player::EmitGrenadeExplosion(const Vector3& explosionPosition)
{
    if (!particleSystem_) {
        return;
    }

    // 爆発ごとに飛ぶ方向と大きさを変える乱数を用意する
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> horizontalDistribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> upwardDistribution(0.25f, 1.0f);
    std::uniform_real_distribution<float> speedDistribution(3.0f, 6.5f);
    std::uniform_real_distribution<float> sizeDistribution(0.90f, 1.80f);
    std::uniform_real_distribution<float> lifeDistribution(0.30f, 0.65f);

    // 中心から全方向へ火球の粒子を飛ばす
    constexpr int kExplosionParticleCount = 80;
    for (int index = 0; index < kExplosionParticleCount; ++index) {
        Vector3 direction = {
            horizontalDistribution(randomEngine),
            upwardDistribution(randomEngine),
            horizontalDistribution(randomEngine)
        };
        direction = Normalize(direction);

        const float speed = speedDistribution(randomEngine);
        const float size = sizeDistribution(randomEngine);

        Vector3 velocity = {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed
        };

        // 中心は黄色、外へ飛ぶ粒子は赤橙色を混ぜる
        Vector4 color = (index % 3 == 0)
            ? Vector4{ 1.0f, 0.20f, 0.015f, 0.70f }
            : Vector4{ 1.0f, 0.58f, 0.045f, 0.78f };

        particleSystem_->Emit(
            explosionPosition,
            { size, size, size },
            velocity,
            color,
            lifeDistribution(randomEngine));
    }

    // 爆発中心へ短時間だけ大きな白黄色の閃光を重ねる
    particleSystem_->Emit(
        explosionPosition,
        { 6.5f, 6.5f, 6.5f },
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.90f, 0.42f, 0.85f },
        0.16f);

    // 少し遅れて残る赤い火球を中心へ追加する
    particleSystem_->Emit(
        explosionPosition,
        { 4.2f, 4.2f, 4.2f },
        { 0.0f, 0.35f, 0.0f },
        { 0.95f, 0.12f, 0.015f, 0.62f },
        0.48f);

    if (grenadeSmokeParticleSystem_) {
        // 小さい粒子を多数重ね、上へ漂いながら長く残る細かな煙を作る
        std::uniform_real_distribution<float> smokeSideDistribution(-1.05f, 1.05f);
        std::uniform_real_distribution<float> smokeUpDistribution(0.30f, 1.15f);
        std::uniform_real_distribution<float> smokeSizeDistribution(0.55f, 1.20f);
        std::uniform_real_distribution<float> smokeLifeDistribution(1.3f, 2.3f);

        constexpr int kSmokeParticleCount = 64;
        for (int index = 0; index < kSmokeParticleCount; ++index) {
            const float smokeSize = smokeSizeDistribution(randomEngine);
            Vector3 smokePosition = {
                explosionPosition.x + smokeSideDistribution(randomEngine) * 0.55f,
                explosionPosition.y + 0.25f,
                explosionPosition.z + smokeSideDistribution(randomEngine) * 0.55f
            };
            Vector3 smokeVelocity = {
                smokeSideDistribution(randomEngine),
                smokeUpDistribution(randomEngine),
                smokeSideDistribution(randomEngine)
            };

            // 濃い灰色と焦げ茶色を混ぜ、火球とは別の煙に見せる
            Vector4 smokeColor = (index % 3 == 0)
                ? Vector4{ 0.16f, 0.11f, 0.08f, 0.27f }
                : Vector4{ 0.12f, 0.12f, 0.12f, 0.22f };

            grenadeSmokeParticleSystem_->Emit(
                smokePosition,
                { smokeSize, smokeSize, smokeSize },
                smokeVelocity,
                smokeColor,
                smokeLifeDistribution(randomEngine));
        }
    }
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

    // 角度の境界をまたいでも最短方向へ回し、構え中は素早く狙いへ追従する。
    const float targetYaw = std::atan2(direction.x, direction.z) + frontAngleOffset_;
    const float difference = std::atan2(std::sin(targetYaw - rotate_.y), std::cos(targetYaw - rotate_.y));
    const float followRate = input_->PushMouseRight() ? 0.55f : 0.32f;
    rotate_.y += difference * followRate;

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

    // BlenderのJSONから読み込んだ開始位置へ戻す
    translate_ = spawnPosition_;

    // 前フレーム位置も開始位置へそろえる
    prevPos_ = spawnPosition_;

    // 落下速度をリセットする
    velocityY_ = 0.0f;

    // 接地状態をリセットして次の更新で再判定する
    onGround_ = false;

    // プレイヤーの向きを初期状態へ戻す
    rotate_ = { 0.0f, 0.0f, 0.0f };

    // 3Dオブジェクトにも座標と回転を反映する
    if (object_) {
        object_->SetTranslate(translate_);
        object_->SetRotate(rotate_);
        object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        // 復活時に直前の射撃や走行モーションを引き継がない。
        object_->StopAnimationOverlay();
        object_->SetAnimationParentCorrectionEnabled(false);
        PlayMotion(Motion::Idle, true);
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
