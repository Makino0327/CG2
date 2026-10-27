#include "Game.h"
#include "Framework.h"
#include "../engine/base/leakchecker/D3DResourceLeakChecker.h"
#include <memory> // 追加
#include <filesystem>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    // 実行ファイルから直接確認する場合も、画像やシェーダーの場所を見つける。
    // 出力先には一部の素材だけがコピーされるため、必須の画像とシェーダーで判定する。
    const auto hasResources = [](const std::filesystem::path& directory) {
        return std::filesystem::exists(directory / L"Resources" / L"white1x1.png") &&
            std::filesystem::exists(directory / L"Resources" / L"shaders" / L"Sprite.VS.hlsl");
    };
    if (!hasResources(std::filesystem::current_path())) {
        wchar_t executablePath[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) > 0) {
            auto directory = std::filesystem::path(executablePath).parent_path();
            for (int depth = 0; depth < 5; ++depth) {
                if (hasResources(directory)) {
                    std::filesystem::current_path(directory);
                    break;
                }
                if (hasResources(directory / L"project")) {
                    std::filesystem::current_path(directory / L"project");
                    break;
                }
                directory = directory.parent_path();
            }
        }
    }

    D3DResourceLeakChecker leakChecker;

    std::unique_ptr<Framework> game = std::make_unique<Game>();

    game->Run();

    return 0;
}
