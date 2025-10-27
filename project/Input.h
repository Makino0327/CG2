#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input
{
public:
	// 初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);

	// 更新
	void Update();

public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	HRESULT result_;
	ComPtr<IDirectInputDevice8> keyboard;
};

