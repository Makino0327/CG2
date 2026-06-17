#include "LevelHotReload.h"

#include <system_error>

// 監視対象ファイルを設定して初期化する
void LevelHotReload::Initialize(const std::string& filePath)
{
    filePath_ = filePath;
    hasWriteTime_ = false;
    hasPendingWriteTime_ = false;
    stableFrameCount_ = 0;

    // 初期化時点の更新時刻を基準値にそろえる
    SyncCurrentWriteTime();
}

// 毎フレーム呼び出して、再読み込みが必要なら true を返す
bool LevelHotReload::Update()
{
    std::error_code errorCode;
    std::filesystem::file_time_type currentWriteTime =
        std::filesystem::last_write_time(filePath_, errorCode);

    // ファイルに触れないときは何もしない
    if (errorCode) {
        return false;
    }

    // 初回だけ現在の更新時刻を基準値にする
    if (!hasWriteTime_) {
        lastWriteTime_ = currentWriteTime;
        hasWriteTime_ = true;
        return false;
    }

    // 変更がなければ保留状態をクリアする
    if (currentWriteTime == lastWriteTime_) {
        hasPendingWriteTime_ = false;
        stableFrameCount_ = 0;
        return false;
    }

    // 新しい更新を見つけた直後は、書き込み途中の可能性があるので保留する
    if (!hasPendingWriteTime_ || currentWriteTime != pendingWriteTime_) {
        pendingWriteTime_ = currentWriteTime;
        hasPendingWriteTime_ = true;
        stableFrameCount_ = 0;
        return false;
    }

    // 同じ更新時刻が少し続いたら、書き込み完了とみなす
    stableFrameCount_++;

    if (stableFrameCount_ < 10) {
        return false;
    }

    return true;
}

// 手動リロード成功後などに、現在の更新時刻を基準値として同期する
void LevelHotReload::SyncCurrentWriteTime()
{
    std::error_code errorCode;
    std::filesystem::file_time_type currentWriteTime =
        std::filesystem::last_write_time(filePath_, errorCode);

    if (errorCode) {
        return;
    }

    lastWriteTime_ = currentWriteTime;
    hasWriteTime_ = true;
    hasPendingWriteTime_ = false;
    stableFrameCount_ = 0;
}