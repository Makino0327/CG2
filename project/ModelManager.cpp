#include "ModelManager.h"

ModelManager* ModelManager::instance = nullptr;

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon = new ModelCommon;
    modelCommon->Initialize(dxCommon);
}

ModelManager* ModelManager::GetInstance()
{
    // まだ作られていないときだけ new
    if (instance == nullptr) {
        instance = new ModelManager();
    }

    return instance;
}

void ModelManager::Finalize()
{
    // インスタンスが存在する時だけ delete
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

void ModelManager::LoadModel(const std::string& filePath)
{
    // 読み込み済みモデルを検索
    if (models.contains(filePath))
    {
        return;
    }

    // モデルの生成とファイル読み込み、初期化
    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(modelCommon, "Resources", filePath);

    // モデルをmapコンテナに格納する
    models.insert(std::make_pair(filePath, std::move(model)));

}

Model* ModelManager::FindModel(const std::string& filePath)
{
    //読み込み済みモデルを検索
    if (models.contains(filePath))
    {
        return models.at(filePath).get();
    }
    return nullptr;
}
