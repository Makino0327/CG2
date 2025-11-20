#pragma once
#include <map>
#include "Model.h"
#include <memory>
#include <string>

class ModelManager
{
public:
    void Initialize(DirectXCommon* dxCommon);
    // インスタンス取得（シングルトン）
    static ModelManager* GetInstance();

    // 終了（インスタンス破棄）
    void Finalize();

    // モデル検索
    Model* FindModel(const std::string& filePath);

    // ロードモデル
    void LoadModel(const std::string& filePath);
private:

    // シングルトン：唯一のインスタンス
    static ModelManager* instance;

    // コンストラクタ / デストラクタ
    ModelManager() = default;
    ~ModelManager() = default;

    // コピー禁止
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

private:
    // モデルデータ
    std::map<std::string, std::unique_ptr<Model>> models;

    ModelCommon* modelCommon = nullptr;
};
