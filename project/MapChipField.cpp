#include "MapChipField.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool MapChipField::LoadFromCsv(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open map csv: " << filePath << std::endl;
        return false;
    }

    tiles_.clear();
    width_ = 0;
    height_ = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<int> row;
        std::stringstream ss(line);
        std::string cell;

        while (std::getline(ss, cell, ',')) {
            if (cell.empty()) {
                row.push_back(0);   // 空なら 0 扱い
            } else {
                row.push_back(std::stoi(cell));
            }
        }

        if (!row.empty()) {
            if (width_ == 0) {
                width_ = static_cast<int>(row.size());
            } else {
                // 行の長さを揃える（短い行は 0 で埋める）
                if (static_cast<int>(row.size()) < width_) {
                    row.resize(width_, 0);
                } else if (static_cast<int>(row.size()) > width_) {
                    width_ = static_cast<int>(row.size());
                }
            }

            tiles_.push_back(row);
        }
    }

    height_ = static_cast<int>(tiles_.size());
    std::cout << "Loaded map: " << width_ << " x " << height_ << std::endl;
    return true;
}

MapChipType MapChipField::GetChip(int x, int y) const
{
    if (y < 0 || y >= height_ || x < 0 || x >= width_) {
        return MapChipType::Empty;
    }

    int v = tiles_[y][x];
    if (v == 1) {
        return MapChipType::Block;
    } else {
        return MapChipType::Empty;
    }
}
