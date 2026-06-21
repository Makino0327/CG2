#include "Player.h"
#include <cfloat>   // FLT_MAX
#include <algorithm>
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

    // 被弾後は入力移動へノックバック速度を加え、徐々に弱める
    pos.x += knockbackVelocity_.x;
    pos.z += knockbackVelocity_.z;
    knockbackVelocity_.x *= knockbackDamping_;
    knockbackVelocity_.z *= knockbackDamping_;

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

    // Gキーを押した瞬間にグレネードを1個投げる
    if (input_->TriggerKey(DIK_G)) {
        ThrowGrenade(camera);
    }

    // Hキーを押した瞬間にHPを1回復する
    if (input_->TriggerKey(DIK_H)) {
        TryHeal();
    }

    // 回復成功後は一定時間、周囲を回る光を更新する
    UpdateHealEffect();

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

void Player::OnHit(const Vector3& hitDirection)
{
    // 死亡中または無敵時間中は新しいダメージを受けない
    if (isDead_ || invincibleTimer_ > 0) {
        return;
    }

    // 被弾状態を保存し、敵から離れる方向へ押し戻す
    isHit_ = true;
    knockbackVelocity_ = {
        hitDirection.x * 0.48f,
        0.0f,
        hitDirection.z * 0.48f
    };

    // 敵と同じ血しぶき用パーティクルをプレイヤーから発生させる
    EmitBloodSplatter(hitDirection);

    // HPを1減らして無敵時間を開始する
    if (hp_ > 0) {
        hp_ -= 1;
    }
    invincibleTimer_ = invincibleDuration_;

    // HPが0になったら操作と本体描画を停止する
    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        // 死亡した場合は回復中の光柱と光点の生成を停止する
        isHealEffectPlaying_ = false;
        healEffectTimer_ = 0.0f;

    }
}

void Player::EmitBloodSplatter(const Vector3& hitDirection)
{
    if (!bloodParticleSystem_ || !object_) {
        return;
    }

    // 敵から離れる方向を血しぶきの中心方向として使う
    Vector3 forward = hitDirection;
    Vector3 side = { -forward.z, 0.0f, forward.x };
    Vector3 bloodPosition = object_->GetTranslate();
    bloodPosition.y += 0.55f;

    // 敵被弾と同じ赤色を使い、プレイヤー用に粒子数だけ少し抑える
    constexpr int kBloodParticleCount = 16;
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> sideDistribution(-0.32f, 0.32f);
    std::uniform_real_distribution<float> heightDistribution(-0.18f, 0.30f);
    std::uniform_real_distribution<float> sizeDistribution(0.42f, 0.78f);

    for (int index = 0; index < kBloodParticleCount; ++index) {
        const float sidePower =
            (static_cast<float>(index % 5) - 2.0f) * 0.22f;
        const float forwardPower = 0.9f + 0.13f * static_cast<float>(index % 6);
        const float upwardPower = 0.22f + 0.12f * static_cast<float>(index % 5);
        const float size = sizeDistribution(randomEngine);

        Vector3 spawnPosition = {
            bloodPosition.x + side.x * sideDistribution(randomEngine),
            bloodPosition.y + heightDistribution(randomEngine),
            bloodPosition.z + side.z * sideDistribution(randomEngine)
        };
        Vector3 velocity = {
            forward.x * forwardPower + side.x * sidePower,
            upwardPower,
            forward.z * forwardPower + side.z * sidePower
        };

        Vector4 color = (index % 2 == 0)
            ? Vector4{ 0.48f, 0.012f, 0.008f, 0.52f }
            : Vector4{ 0.68f, 0.025f, 0.012f, 0.46f };

        bloodParticleSystem_->Emit(
            spawnPosition,
            { size, size, size },
            velocity,
            color,
            0.28f + 0.04f * static_cast<float>(index % 5));
    }
}

void Player::TryHeal()
{
    // 死亡中または最大HPなら回復処理を行わない
    if (isDead_ || hp_ >= maxHp_) {
        return;
    }

    // HPを1だけ回復し、最大HPを超えないようにする
    hp_ += 1;
    if (hp_ > maxHp_) {
        hp_ = maxHp_;
    }

    // 回復できた時だけ緑色のエフェクトを発生させる
    // 回復演出を最初から再生する
    isHealEffectPlaying_ = true;
    healEffectTimer_ = 0.0f;
    EmitHealEffect();
}

void Player::EmitHealEffect()
{
    if (!particleSystem_ || !object_) {
        return;
    }

    // プレイヤーの周囲から緑色の粒子を上方向へ流す
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> sideDistribution(-1.15f, 1.15f);
    std::uniform_real_distribution<float> heightDistribution(0.0f, 1.2f);
    std::uniform_real_distribution<float> upwardDistribution(1.0f, 2.2f);
    std::uniform_real_distribution<float> sizeDistribution(0.16f, 0.30f);
    std::uniform_real_distribution<float> lifeDistribution(0.55f, 0.90f);

    const Vector3 playerPosition = object_->GetTranslate();
    constexpr int kHealParticleCount = 18;

    for (int index = 0; index < kHealParticleCount; ++index) {
        const float size = sizeDistribution(randomEngine);
        const Vector3 spawnPosition = {
            playerPosition.x + sideDistribution(randomEngine),
            playerPosition.y + heightDistribution(randomEngine),
            playerPosition.z + sideDistribution(randomEngine)
        };
        const Vector3 velocity = {
            sideDistribution(randomEngine) * 0.20f,
            upwardDistribution(randomEngine),
            sideDistribution(randomEngine) * 0.20f
        };

        // 回復粒子は同じ明るい緑色へ統一する
        const Vector4 color = { 0.18f, 1.0f, 0.32f, 0.88f };

        particleSystem_->Emit(
            spawnPosition,
            { size, size, size },
            velocity,
            color,
            lifeDistribution(randomEngine));
    }

    // 足元へ大きさの違う緑色の光を重ね、濃いオーラを作る
    const float floorHeight = playerPosition.y - colliderRadius_ + 0.12f;
    particleSystem_->Emit(
        { playerPosition.x, floorHeight, playerPosition.z },
        { 1.85f, 1.85f, 1.85f },
        { 0.0f, 0.25f, 0.0f },
        { 0.12f, 1.0f, 0.24f, 0.52f },
        0.75f);
    particleSystem_->Emit(
        { playerPosition.x, floorHeight + 0.35f, playerPosition.z },
        { 1.35f, 1.35f, 1.35f },
        { 0.0f, 0.45f, 0.0f },
        { 0.18f, 1.0f, 0.32f, 0.62f },
        0.65f);

    // 回復開始時に複数の縦長の光柱を一斉に立ち上げる
    std::uniform_real_distribution<float> pillarHeightDistribution(0.0f, 0.40f);
    std::uniform_real_distribution<float> pillarSpeedDistribution(2.4f, 3.6f);
    constexpr int kInitialPillarCount = 12;
    for (int index = 0; index < kInitialPillarCount; ++index) {
        const Vector3 pillarPosition = {
            playerPosition.x + sideDistribution(randomEngine),
            floorHeight + pillarHeightDistribution(randomEngine),
            playerPosition.z + sideDistribution(randomEngine)
        };

        particleSystem_->Emit(
            pillarPosition,
            { 0.12f, 1.00f, 0.12f },
            { 0.0f, pillarSpeedDistribution(randomEngine), 0.0f },
            { 0.18f, 1.0f, 0.32f, 0.95f },
            0.48f);
    }

    // プレイヤー中心へ短時間の緑色の光を重ねる
    particleSystem_->Emit(
        { playerPosition.x, playerPosition.y + 0.65f, playerPosition.z },
        { 1.10f, 1.10f, 1.10f },
        { 0.0f, 0.15f, 0.0f },
        { 0.18f, 1.0f, 0.32f, 0.72f },
        0.20f);
}

void Player::UpdateHealEffect()
{
    if (!isHealEffectPlaying_ || !particleSystem_ || !object_) {
        return;
    }

    constexpr float kDeltaTime = 1.0f / 60.0f;
    healEffectTimer_ += kDeltaTime;

    const Vector3 playerPosition = object_->GetTranslate();
    const float floorHeight = playerPosition.y - colliderRadius_ + 0.10f;

    // 毎フレーム位置を変えながら、細い光柱と小さな光点を上昇させる
    static std::mt19937 randomEngine(std::random_device{}());
    std::uniform_real_distribution<float> sideDistribution(-1.25f, 1.25f);
    std::uniform_real_distribution<float> heightDistribution(0.0f, 0.45f);
    std::uniform_real_distribution<float> speedDistribution(2.6f, 4.0f);

    // 大きさを周期的に変え、プレイヤー全体を包む緑色のオーラを作る
    const float auraScale =
        1.65f + std::sin(healEffectTimer_ * 12.0f) * 0.18f;
    particleSystem_->Emit(
        { playerPosition.x, playerPosition.y + 0.35f, playerPosition.z },
        { auraScale, auraScale, auraScale },
        { 0.0f, 0.12f, 0.0f },
        { 0.12f, 1.0f, 0.24f, 0.24f },
        0.14f);

    // 足元にも薄く大きな光を重ねてオーラの広がりを見せる
    const float floorAuraScale =
        2.05f + std::sin(healEffectTimer_ * 10.0f) * 0.15f;
    particleSystem_->Emit(
        { playerPosition.x, floorHeight + 0.12f, playerPosition.z },
        { floorAuraScale, floorAuraScale, floorAuraScale },
        { 0.0f, 0.08f, 0.0f },
        { 0.10f, 1.0f, 0.22f, 0.18f },
        0.16f);

    constexpr int kLightCountPerFrame = 5;
    for (int index = 0; index < kLightCountPerFrame; ++index) {
        const bool isPillar = index < 2;
        const Vector3 lightPosition = {
            playerPosition.x + sideDistribution(randomEngine),
            floorHeight + heightDistribution(randomEngine),
            playerPosition.z + sideDistribution(randomEngine)
        };
        const Vector3 lightScale = isPillar
            ? Vector3{ 0.12f, 0.95f, 0.12f }
            : Vector3{ 0.22f, 0.22f, 0.22f };
        const float lifeTime = isPillar ? 0.34f : 0.48f;

        particleSystem_->Emit(
            lightPosition,
            lightScale,
            { 0.0f, speedDistribution(randomEngine), 0.0f },
            { 0.18f, 1.0f, 0.32f, 0.92f },
            lifeTime);
    }

    // 演出終了時は光柱と光点の追加を止める
    if (healEffectTimer_ >= healEffectDuration_) {
        isHealEffectPlaying_ = false;
        healEffectTimer_ = 0.0f;
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

    // 弾と発射エフェクトをプレイヤー中心ではなく射出口へ移動する
    firePosition.x += direction.x * bulletMuzzleDistance_;
    firePosition.z += direction.z * bulletMuzzleDistance_;

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
        particleSystem_);

    if (particleSystem_) {
        // 発射口の外側へ広がるオレンジ色の光を作る
        particleSystem_->Emit(
            firePosition,
            { 1.2f, 1.2f, 1.2f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.34f, 0.05f, 0.38f },
            0.16f);

        // 発射口の中央へ黄色い閃光を重ねる
        particleSystem_->Emit(
            firePosition,
            { 0.68f, 0.68f, 0.68f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.72f, 0.22f, 0.9f },
            0.11f);

        // 最も明るい白い中心光を重ねる
        particleSystem_->Emit(
            firePosition,
            { 0.30f, 0.30f, 0.30f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 0.95f, 0.72f, 1.0f },
            0.075f);

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
            0.08f);

        particleSystem_->Emit(
            rightFlashPosition,
            { 0.18f, 0.18f, 0.18f },
            { -sideDirection.x * 0.6f, 0.0f, -sideDirection.z * 0.6f },
            { 1.0f, 0.46f, 0.08f, 0.4f },
            0.08f);
    }

    // 弾をリストに追加する
    bullets_.push_back(std::move(bullet));
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

    // 復活時は残っているノックバックを消す
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };

    // 開始位置は Blender から設定された値を使い続ける
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
