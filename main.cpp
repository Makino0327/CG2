#include <Windows.h>  
#include <cstdint>  
#include <string>  
#include <d3d12.h>  
#include <dxgi1_6.h>  
#include <cassert>  
#include <iostream> 
#include <format>   
#pragma comment(lib,"d3d12.lib")  
#pragma comment(lib,"dxgi.lib")  

// Log function definition  
void Log(const std::wstring& message) {  
   std::wcout << message << std::endl;  
}  

// ウィンドウプロシージャ  
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {  
   //メッセージに応じてゲーム固有の処理を行う  
   switch (msg) {  
       // ウィンドウが破棄された  
   case WM_DESTROY:  
       // OSに対して、アプリの終了を伝える  
       PostQuitMessage(0);  
       return 0;  
   }  
   // 標準のメッセージ処理を行う  
   return DefWindowProc(hwnd, msg, wparam, lparam);  
}  

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

   // DXGIファクトリーの生成  
   IDXGIFactory7* dxgiFactory = nullptr;  
   // HRESULTはWindows系のエラーコードであり、  
   // 関数が成功したかどうかをSUCCEEDEDマクロで判定できる  
   HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));  
   assert(SUCCEEDED(hr));  

   IDXGIAdapter4* useAdapter = nullptr;  

   for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {  
       DXGI_ADAPTER_DESC3 adapterDesc{};  
       hr = useAdapter->GetDesc3(&adapterDesc);  
       assert(SUCCEEDED(hr));  
       if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {  
           Log(std::format(L"Use Adapter: {}", adapterDesc.Description));  
           break;  
       }  
       useAdapter = nullptr;  
   }  
   assert(useAdapter != nullptr);  

   ID3D12Device* device = nullptr;

   D3D_FEATURE_LEVEL featureLevels[] = {
       D3D_FEATURE_LEVEL_12_2,
       D3D_FEATURE_LEVEL_12_1,
       D3D_FEATURE_LEVEL_12_0,
   };
   const wchar_t* featureLevelStrings[] = { L"12.2", L"12.1", L"12.0" };


   for (size_t i = 0; i < _countof(featureLevels); ++i) {
	   hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
	   if (SUCCEEDED(hr)) {
		   Log(std::format(L"FeatureLevel: {}\n", featureLevelStrings[i]));
		   break;
	   }
   }
   assert(device != nullptr);
   Log(std::wstring(L"Complete create D3D12Device!!!\n"));

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

   // 出力ウィンドウへの文字出力  
   OutputDebugStringA("Hello, DirectX!\n");  

   return 0;  
}
