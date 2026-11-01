#pragma once
#include <Windows.h>
#include <cstdint>
#include <cassert>

#include "../../../externals/imgui/imgui.h"
#include "../../../externals/imgui/imgui_impl_win32.h"
#include "../../../externals/imgui/imgui_impl_dx12.h"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WinApp
{
public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
public:
	// 初期化
	void Initialize();
	// 更新
	void Update();
	// 終了
	void Finalize();

	// メッセージ処理
	bool ProcessMessage();
    // 短く押したF6も取りこぼさず、展示切り替えの要求を一度だけ渡す。
    bool ConsumeDemoToggleRequest() {
        const bool requested = demoToggleRequested_;
        demoToggleRequested_ = false;
        return requested;
    }

	// ゲッター
	HWND GetHwnd() const{ return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }
private:
    bool demoToggleRequested_ = false;
	// ウィンドウハンドル
	HWND hwnd = nullptr;
	// ウィンドウクラス
	WNDCLASS wc{};

public:
	// 画面サイズ
	static constexpr int32_t kClientWidth = 1280;
	static constexpr int32_t kClientHeight = 720;
};

