#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

// レベルJSONなどの更新を監視して、再読み込みすべきタイミングを判定する
class LevelHotReload {
public:
    // 監視対象ファイルを設定して初期化する
    void Initialize(const std::string& filePath);

    // 毎フレーム呼び出して、再読み込みが必要なら true を返す
    bool Update();

    // 手動リロード成功後などに、現在の更新時刻を基準値として同期する
    void SyncCurrentWriteTime();

    // 監視対象ファイルパスを返す
    const std::string& GetFilePath() const { return filePath_; }

private:
    // 監視対象のファイルパス
    std::string filePath_;

    // 最後に読み込み済みとして扱う更新時刻
    std::filesystem::file_time_type lastWriteTime_{};

    // 書き込み途中を避けるために一時保持する更新時刻
    std::filesystem::file_time_type pendingWriteTime_{};

    // 更新時刻の基準値を持っているかどうか
    bool hasWriteTime_ = false;

    // 保留中の更新があるかどうか
    bool hasPendingWriteTime_ = false;

    // 同じ更新時刻が続いたフレーム数
    uint32_t stableFrameCount_ = 0;
};