#include "Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>   // ← これで OutputDebugStringA が使える

namespace Logger
{
    void Log(const std::string& message)
    {
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
    }
}