#pragma once
#include <vector>
#include <string>

// マップチップの種類
enum class MapChipType {
    Empty = 0, // 何もなし（0）
    Block = 1  // ブロック（1）
};

class MapChipField
{
public:
    // CSV から読み込む
    bool LoadFromCsv(const std::string& filePath);

    // マップサイズ
    int GetWidth() const { return width_; }   // x方向のタイル数
    int GetHeight() const { return height_; }  // y方向のタイル数

    // (x, y) のチップを取得（範囲外は Empty 扱い）
    MapChipType GetChip(int x, int y) const;

private:
    // tiles_[y][x] でアクセスする
    std::vector<std::vector<int>> tiles_;
    int width_ = 0;
    int height_ = 0;
};
