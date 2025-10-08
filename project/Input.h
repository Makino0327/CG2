#pragma once
#include <Windows.h>
// ComPtr
#include <wrl.h>
// DirectInput
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
// assert
#include <cassert>

// ライブラリのリンク
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// ComPtr
using Microsoft::WRL::ComPtr;

class Input
{
public:
	// 初期化
	void Initialize(HINSTANCE hInst, HWND hwnd);
	// 更新
	void Update();
	
private:
	// DirectInputオブジェクト
	ComPtr<IDirectInput8> directInput_;
	// キーボードデバイスの生成
	ComPtr<IDirectInputDevice8> keyboard_;
};

