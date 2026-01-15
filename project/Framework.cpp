#include "Framework.h"

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
