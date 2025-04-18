#include <Windows.h>  
#include <string>

void Log(const std::string& message) {
   OutputDebugStringA(message.c_str());
}

// Windowsアプリでのエントリーポイント(main関数)  
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {  
   
   std::string str0{ "STRING!!!" };

   std::string str1{ std::to_string(10) };

   return 0;  
}
