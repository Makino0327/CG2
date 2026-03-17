#include "Game.h"
#include "Framework.h"
#include "D3DResourceLeakChecker.h"
#include <memory> // 追加

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    D3DResourceLeakChecker leakChecker;

    std::unique_ptr<Framework> game = std::make_unique<Game>();

    game->Run();

    return 0;
}