#include "Framework.h"
#include "../scene/BaseScene.h" // ★ここで完全型にする（Framework.h には入れない）

void Framework::Run()
{
    Initialize();

    while (true) {
        Update();

        if (IsEndRequest()) {
            break;
        }

        Draw();
    }

    Finalize();
}
