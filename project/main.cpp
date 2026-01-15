#include "Game.h"
#include "Framework.h"
#include "D3DResourceLeakChecker.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    D3DResourceLeakChecker leakChecker;

    Framework* game = new Game();

    game->Run();

    delete game;

    return 0;
}
