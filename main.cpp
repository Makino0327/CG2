
#include <Windows.h>  
#include <cstdint>  

// Windowsアプリでのエントリーポイント(main関数)  
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {  


   WNDCLASS wc{};  
   // ウィンドウプロシージャ  
   wc.lpfnWndProc = WindowProc;  
   // ウィンドウクラス名(なんでもいい)  
   wc.lpszClassName = L"CG2WindowClass";  
   // インスタンスハンドル  
   wc.hInstance = GetModuleHandle(nullptr);  
   // カーソル  
   wc.hCursor = LoadCursor(nullptr, IDC_ARROW);  
   // ウィンドウクラスを登録する  
   RegisterClass(&wc);  

   // クライアント領域のサイズ  
   const int32_t kClientWidth = 1280;  
   const int32_t kClientHeight = 720;  

   // ウィンドウサイズを表す構造体にクライアント領域を入れる  
   RECT wrc = { 0,0, kClientWidth, kClientHeight };  

   // クライアント領域を元に実際のサイズにwrcを変更してもらう  
   AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);  

   HWND hwnd = CreateWindow(  
       wc.lpszClassName,  
       L"CG2",  
       WS_OVERLAPPEDWINDOW,  
       CW_USEDEFAULT,  
       CW_USEDEFAULT,  
       wrc.right - wrc.left,  
       wrc.bottom - wrc.top,  
       nullptr,  
       nullptr,  
       wc.hInstance,  
       nullptr  
   );  

   // ウィンドウを表示する  
   ShowWindow(hwnd, SW_SHOW);  

   MSG msg{};  
   // ウィンドウのxボタンが押されるまでループ  
   while (msg.message != WM_QUIT) {  
       // Windowにメッセージが来てたら最優先で処理させる  
       if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {  
           TranslateMessage(&msg);  
           DispatchMessage(&msg);  
       } else  
       {  
           // ゲームの処理  
       }  
   }  

   return 0;  
}
