#pragma once
#define NOMINMAX

#include <memory>
#include <array>
#include <vector>

#include "../../engine/3d/obj3d/Object3d.h"
#include "../../engine/input/Input.h"
#include "../map/MapChipField.h"
#include "../camera/Camera.h"
#include "../collision/Collision.h"
#include "PlayerBullet.h"
#include "PlayerGrenade.h"
#include "Gun.h"
#include "../../engine/utils/ObjectPool.h"

#include "../../scene/LevelLoader.h"

class ParticleSystem;

class Player
{
public:
    // 生成時は通常状態から始め、操作と死亡の振る舞いを状態へ委譲する。
    Player();
    // プレイヤーが使う攻撃方法
    enum class AttackMode {
        Gun,
        AssaultRifle,
        Shotgun,
        Knife,
    };
    void Initialize(
        Object3dCommon* object3dCommon,
        Input* input,
        ParticleSystem* particleSystem);
    // 本編の読み込みでは、アニメーションを1段階ずつ準備して描画へ戻る。
    void BeginInitialize(Object3dCommon* common, Input* input, ParticleSystem* particles);
    bool ContinueInitialize();

    void Update(Camera* camera);
    // 展示中は操作入力を使わず、通常と同じ移動・被弾・アニメーションを進める。
    void UpdateDemo(Camera* camera, const Vector3& movement, const Vector3* aimTarget = nullptr, bool shoot = false);
    // クリアデモの次の周回に、弾薬や命中率を持ち越さない。
    void ResetDemoShooting();
    // 再開のクリックを射撃へ持ち越さず、ボタンを離してから次の発砲を受け付ける。
    void SuppressFireUntilRelease() { suppressFireUntilRelease_ = input_ && input_->PushMouseLeft(); }

    void Draw();
    // 床の後に描き、半透明の縁を床の色と混ぜる。
    void DrawBloodPool();

    // マップ情報とタイルサイズを設定する
    void SetMap(MapChipField* mapField, float tileSize) {
        mapField_ = mapField;
        tileSize_ = tileSize;
    }

    // Blender JSON から読んだ床コライダー一覧を受け取る
    void SetFloorColliders(const std::vector<LevelColliderData>* floorColliders) {
        floorColliders_ = floorColliders;
    }

    // プレイヤーの座標を返す
    Vector3 GetWorldPosition() const;

    // 近接攻撃演出などでプレイヤーの座標を直接設定する
    void SetPosition(const Vector3& position);

    // プレイヤーの当たり判定を返す
    SphereCollider GetCollider() const;

    // 攻撃元から離れる方向へ吹き飛ばし、実際に被弾した場合だけ真を返す。
    bool OnHit(const Vector3& attackerPosition);

    // 床と壁を確認しながらノックバックで移動する。
    void MoveWithCollision(const Vector3& movement);

    // 弾の所有コンテナは公開せず、判定に必要な情報だけを読み取らせる。
    size_t GetBulletCount() const { return bullets_.size(); }
    const PlayerBullet* GetBullet(size_t index) const {
        return index < bullets_.size() ? bullets_[index].get() : nullptr;
    }
    // 命中記録と弾の停止をまとめる。木箱への命中は命中率に数えない。
    void ResolveBulletHit(size_t index, float hitTime, bool countAsShotHit);
    // 判定後の壁への命中確定と軌跡生成を、弾の所有者から実行する。
    void FinishBulletUpdate(size_t index);

    // マウスが重なる敵を調べるため、発射直前の当たり判定を受け取る。
    void SetBulletAimTargets(const std::vector<BoxCollider>& boxes,
        const std::vector<SphereCollider>& spheres) {
        bulletAimBoxes_ = boxes;
        bulletAimSpheres_ = spheres;
    }

    // グレネード爆発によるカメラシェイク要求を一度だけ返す
    bool ConsumeGrenadeShakeRequest();

    // 未処理のグレネード爆発位置をSceneへまとめて渡す
    std::vector<Vector3> ConsumeGrenadeExplosions();

    // 弾を撃った位置をSceneへまとめて渡す
    std::vector<Vector3> ConsumeBulletShockwavePositions();

    // Sceneが所有する煙用の通常アルファパーティクルを設定する
    void SetGrenadeSmokeParticleSystem(ParticleSystem* particleSystem) {
        grenadeSmokeParticleSystem_ = particleSystem;
    }

    // 敵と共有する、発光しない血しぶき用パーティクルを受け取る。
    void SetBloodParticleSystem(ParticleSystem* particleSystem) {
        bloodParticleSystem_ = particleSystem;
    }

    // プレイヤーの現在HPを返す
    int GetHp() const { return hp_; }
    // 体力の減り具合に応じた画面演出で使う最大HPを返す。
    int GetMaxHp() const { return maxHp_; }

    // プレイヤーが消えたかを返す
    bool IsDead() const;

    // その場で倒れる動作を開始し、最後の姿勢を保持する。
    void Die();

    // 現在の攻撃方法を返す
    AttackMode GetAttackMode() const { return attackMode_; }

    // 弾UIとリロードで使う選択中の銃を返す
    AttackMode GetSelectedGunMode() const { return selectedGunMode_; }

    // ナイフを使うモードかどうかを返す
    bool IsMeleeMode() const { return attackMode_ == AttackMode::Knife; }

    // 現在の武器に残っている弾数を返す
    int GetCurrentAmmo() const;

    // UIへ、まだ投げていないグレネードの残数を渡す。
    int GetGrenadeCount() const { return grenadeCount_; }
    // チュートリアルでは残数を減らさず、何度でも投げられるようにする。
    void SetUnlimitedGrenades(bool unlimited) { unlimitedGrenades_ = unlimited; }
    bool HasUnlimitedGrenades() const { return unlimitedGrenades_; }

    // 現在の武器の最大弾数を返す
    int GetCurrentMaxAmmo() const;

    // 拾った弾を種類ごとの予備弾に追加する。
    void AddReserveAmmo(AttackMode weapon, int rounds);
    // 選択中の銃が管理する予備弾数を返す。
    int GetCurrentReserveAmmo() const;
    // チュートリアル用。ハンドガンは予備弾を減らさず、何度でもリロードできる。
    void SetUnlimitedHandgunReserve(bool unlimited) { guns_[0]->SetUnlimitedReserve(unlimited); }

    // このフレームに発砲したかを返す(銃声の発生検知用)
    bool HasFiredThisFrame() const { return firedThisFrame_; }
    size_t GetShotCount() const { return shotHits_.size(); }
    size_t GetHitShotCount() const { return hitShotCount_; }

    // Sceneへ銃口の短い発射光の位置と強度を渡す。
    const Vector3& GetMuzzleLightPosition() const { return muzzleLightPosition_; }
    float GetMuzzleLightIntensity() const;

    // 銃声が敵に届く範囲の半径を返す
    float GetGunshotSoundRange() const { return gunshotSoundRange_; }

    // 現在リロード中かどうかを返す
    bool IsReloading() const { return GetSelectedGun().IsReloading(); }

    // リロードの残りフレームを返す
    int GetReloadTimer() const { return GetSelectedGun().GetReloadTimer(); }

    // プレイヤーを復活させる
    void Respawn();

    // Blender JSON から読んだ壁コライダー一覧を受け取る
    void SetWallColliders(const std::vector<LevelColliderData>* wallColliders) {
        wallColliders_ = wallColliders;
    }

    // デバッグカメラ確認用に描画行列だけ更新する。
    void UpdateRenderOnly();

    // Blenderから読んだプレイヤー開始位置を保存する
    void SetSpawnPosition(const Vector3& spawnPosition) {
        spawnPosition_ = spawnPosition;
        translate_ = spawnPosition;
    }

private:
    // 命中記録は弾の命中処理からだけ更新し、散弾の二重計上を防ぐ。
    void RecordShotHit(const PlayerBullet& bullet);

    // 公式モデルのモーションを操作状態に合わせて選ぶ。
    enum class Motion : size_t {
        Idle, Run, RunBack, RunLeft, RunRight,
        AimIdle, AimRun, AimBack, AimLeft, AimRight,
        Death,
        Count
    };

    // モーションは初期化時に読み込み、毎フレームのファイルアクセスを避ける。
    void PrepareAimingAnimations();
    void CreateAimedLocomotion(size_t direction);
    bool IsUpperBodyJoint(const std::string& name) const;
    size_t animationLoadStep_ = 0;
    std::vector<std::string> loadingAnimationNames_;
    bool loadingAnimations_ = false;
    void UpdateAnimation(const Vector3& movement, bool isAiming);
    void PlayMotion(Motion motion, bool restart = false);

    std::array<Animation, static_cast<size_t>(Motion::Count)> animations_{};
    Motion currentMotion_ = Motion::Count;
    Animation shootingOverlay_; // 両手・肩・胸の反動だけを重ねるモーション

    // 下方向のマップ当たり判定を処理する
    void ResolveBottomCollisionWithMap(Vector3& pos);

    // 左方向のマップ当たり判定を処理する
    void ResolveLeftCollisionWithMap(Vector3& pos);

    // 上方向のマップ当たり判定を処理する
    void ResolveTopCollisionWithMap(Vector3& pos);

    // 右方向のマップ当たり判定を処理する
    void ResolveRightCollisionWithMap(Vector3& pos);

    // 弾を発射する
    bool FireBullet(Camera* camera, float spreadAngle = 0.0f, const Vector3* aimTarget = nullptr);

    // ショットガンの弾をまとめて発射する
    bool FireShotgun(Camera* camera, const ScatterShot& pattern);

    // マウスの位置か射線上に敵がいれば胴体を狙い、それ以外は高さの平面を狙う。
    Vector3 CalcBulletDirection(Camera* camera, const Vector3& muzzle, float& flightHeight);

    // 現在の銃をリロード状態にする
    void StartReload();

    // リロードの残り時間を進める
    void UpdateReload();

    // 現在の武器の最大弾数を返す
    int GetMaxAmmoForCurrentWeapon() const;


    // 弾を更新する
    void UpdateBullets();

    // 薬莢の移動、回転、寿命を管理する。床で止まった後も少しだけ残す。
    struct Casing {
        std::unique_ptr<Object3d> object;
        Vector3 position{};
        Vector3 velocity{};
        Vector3 rotation{};
        Vector3 angularVelocity{};
        float floorY = 0.0f;
        int lifeFrames = 180;
        bool resting = false;
    };

    // 発射成功時に銃の右側へ薬莢を1個排出する。
    void EjectCasing(const Vector3& muzzlePosition, const Vector3& direction);

    // 薬莢を落下させ、床で跳ねさせてから寿命切れでプールへ返す。
    void UpdateCasings();
    void PrepareCasing(Casing& casing); // 薬莢の描画リソースはプールへの新規登録時だけ作る。

    // 弾の当たり判定とは分けて、演出用の薬莢を保持する。
    static constexpr size_t kCasingCapacity = 80;
    ObjectPool<Casing> casingPool_{kCasingCapacity};
    std::vector<ObjectPool<Casing>::Handle> casings_;

    // 死亡時に体の下から広がる血だまり。形の違う板をずらして重ね、輪郭を崩す。
    struct BloodPoolLayer {
        std::unique_ptr<Object3d> object;
        float side = 0.0f;         // 体の横方向へのずれ
        float forward = 0.0f;      // 腰から頭の方向へのずれ
        float yaw = 0.0f;
        float maxScale = 1.0f;
        float delay = 0.0f;        // 広がり始めるまでの秒数
        float duration = 1.0f;     // ほぼ最大まで広がる秒数
        float heightOffset = 0.0f; // 重なりのちらつきを防ぐ床からの高さ
    };

    // 倒れた位置に血だまりを用意し、少し遅れて広げ始める。
    void StartBloodPool();
    // 倒れる途中は体を追い、その後は床の上で広げて暗くしていく。
    void UpdateBloodPool();
    // 倒れている体の腰の位置と向きを、血だまりの基準にする。
    void TrackBloodPoolAnchor();
    // 骨の原点を、描画と同じワールド座標で返す。
    bool GetJointWorldPosition(const std::string& name, Vector3& position) const;

    std::vector<BloodPoolLayer> bloodPoolLayers_;
    float bloodPoolTime_ = 0.0f;
    float bloodPoolFloorY_ = 0.0f;
    Vector3 bloodPoolCenter_{};
    Vector3 bloodPoolForward_{ 0.0f, 0.0f, 1.0f };

    // プレイヤーの向いている方向へグレネードを投げる
    void ThrowGrenade(Camera* camera);

    // 投げたグレネードを更新して、爆発後に削除する
    void UpdateGrenades();

    // 指定位置へ爆発パーティクルを発生させる
    void EmitGrenadeExplosion(const Vector3& explosionPosition);

    // マウスの方向へプレイヤーを向ける
    void RotateToMouse(Camera* camera);

    // 床コライダーから現在位置の地面高さを合わせる
    void ResolveGroundHeight(Vector3& pos);

    // 壁コライダーとの横移動衝突を解決する
    void ResolveWallCollision(Vector3& pos);

    

    // モデルの正面方向補正に使う角度
    float frontAngleOffset_ = 0.0f;

private:
    std::unique_ptr<Object3d> object_;
    Input* input_ = nullptr;

    // 3Dオブジェクト共通設定を保持する
    Object3dCommon* object3dCommon_ = nullptr;

    // プレイヤー弾が共有する軌跡用パーティクルシステム
    ParticleSystem* particleSystem_ = nullptr;

    // 被弾演出は射撃の発光粒子と分け、通常のアルファ合成で描画する。
    ParticleSystem* bloodParticleSystem_ = nullptr;

    // 現在選んでいる攻撃方法
    AttackMode attackMode_ = AttackMode::Knife;
    AttackMode selectedGunMode_ = AttackMode::Gun; // 右クリック中に使う銃の種類を保存する

    // 武器は基底型で所有し、個別の弾薬や連射状態は銃の内部に隠す。
    std::array<std::unique_ptr<Gun>,3> guns_{
        std::make_unique<Handgun>(), std::make_unique<AssaultRifle>(), std::make_unique<Shotgun>() };
    Gun& GetSelectedGun() { return *guns_.at(static_cast<size_t>(selectedGunMode_)); }
    const Gun& GetSelectedGun() const { return *guns_.at(static_cast<size_t>(selectedGunMode_)); }
    bool FireSelectedGun(Camera* camera, const GunTrigger& trigger, const Vector3* aimTarget = nullptr);

    // このフレームに発砲したか
    bool firedThisFrame_ = false;
    // 発砲順に命中済みかを保持する。ステージをまたいでも番号は継続する。
    std::vector<bool> shotHits_;
    size_t hitShotCount_ = 0;

    // 発射後の短い時間だけ光らせ、減衰中も実際の銃口へ追従させる。
    Vector3 muzzleLightPosition_{};
    int muzzleLightFrames_ = 0;

    // 銃声が敵に届く範囲の半径
    float gunshotSoundRange_ = 18.0f;

    // グレネード爆発後に残す煙用パーティクルシステム
    ParticleSystem* grenadeSmokeParticleSystem_ = nullptr;

    // 移動関係のパラメータ
    float moveSpeed_ = 0.2f;
    float aimingMoveSpeed_ = 0.10f; // 銃を構えている間のゆっくり歩く速度
    float velocityY_ = 0.0f;
    float jumpPower_ = 0.6f;
    float gravity_ = -0.03f;
    bool onGround_ = false;

    // マップ情報
    MapChipField* mapField_ = nullptr;
    float tileSize_ = 2.0f;

    Vector3 prevPos_{};



    // 死亡後に戻るプレイヤーの初期位置
    Vector3 spawnPosition_ = {
        2.0f,
        0.5f,
        2.0f
    };

    // プレイヤーの現在座標
    Vector3 translate_ = {
        2.0f,
        0.5f,
        2.0f
    };

    // プレイヤーの回転
    Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

    // プレイヤーの大きさ
    Vector3 scale_ = { 2.4f, 2.4f, 2.4f }; // 直前の2.1から約14%大きくし、構えを見やすくする

    // プレイヤーの当たり判定半径
    float colliderRadius_ = 1.0f;

    // 被弾状態を保持する
    bool isHit_ = false;

    // 被弾後に毎フレーム加える水平移動量。時間とともに減衰させる。
    Vector3 knockbackVelocity_{};

    // プレイヤーの現在HP
    int hp_ = 3;

    // プレイヤーの最大HP
    int maxHp_ = 3;

    // 無敵時間の残りフレーム
    int invincibleTimer_ = 0;

    // 無敵時間の長さ
    int invincibleDuration_ = 60;

    // 点滅の切り替え間隔
    int blinkInterval_ = 5;

    // 消えたかどうか
    // 装備の種類やアニメーション素材とは分けて、プレイヤーの行動状態を保持する。
    class State;
    class AliveState;
    class NormalState;
    class AimingState;
    class DeadState;
    template<class T> static const State& GetState();
    void ChangeState(const State& state);
    void UpdateLiving(Camera* camera, bool isAimingGun);
    const State* state_ = nullptr;

    // プレイヤーが撃った弾
    // 有効な弾だけを配列へ入れ、取り除いたハンドルは自動でプールへ返る。
    static constexpr size_t kBulletCapacity = 128;
    ObjectPool<PlayerBullet> bulletPool_{kBulletCapacity};
    std::vector<ObjectPool<PlayerBullet>::Handle> bullets_;
    // 狙い位置の選択だけに使う、シーンから受け取った敵の判定。
    std::vector<BoxCollider> bulletAimBoxes_;
    std::vector<SphereCollider> bulletAimSpheres_;

    // プレイヤーが投げたグレネード
    // 通常プレイは3個から始め、投げた分だけ残数を減らす。
    static constexpr int kInitialGrenadeCount = 3;
    int grenadeCount_ = kInitialGrenadeCount;
    // 無限設定は復活しても維持し、練習を続けられるようにする。
    bool unlimitedGrenades_ = false;
    std::vector<std::unique_ptr<PlayerGrenade>> grenades_;

    // GamePlaySceneへまだ渡していないカメラシェイク要求
    bool grenadeShakeRequested_ = false;

    // 敵との爆発判定をまだ処理していない爆発位置
    std::vector<Vector3> pendingGrenadeExplosions_;

    // 衝撃波の中心に使う弾の発射位置
    std::vector<Vector3> pendingBulletShockwavePositions_;

    // 弾の速度
    float bulletSpeed_ = 1.4f;
    bool suppressFireUntilRelease_ = false;

    // マウスの地面座標に対する狙いの高さ。発射位置は銃口の実頂点から求める。
    float bulletAimHeight_ = 0.7f;


    // グレネードの投擲開始位置の高さ
    float grenadeSpawnHeight_ = 0.8f;

    // プレイヤー中心からグレネードを出す前方向距離
    float grenadeMuzzleDistance_ = 0.8f;

    // Blender JSON から読んだ床コライダー一覧
    const std::vector<LevelColliderData>* floorColliders_ = nullptr;

    // Blender JSON から読んだ壁コライダー一覧
    const std::vector<LevelColliderData>* wallColliders_ = nullptr;
};
