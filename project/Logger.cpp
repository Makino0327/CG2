#include "Logger.h"
#include <debugapi.h>

namespace Logger
{
    void Log(const std::string& message)
    {
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
    }
}