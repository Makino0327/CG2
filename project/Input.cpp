#include "Input.h"

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	// DirectInputのインスタンス生成
	ComPtr<IDirectInput8> directInput = nullptr;
	result_ = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result_));
	// キーボードデバイスの生成
	result_ = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(result_));
	// 入力データ形式のセット
	result_ = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result_));
	// 排他制御レベルのセット
	result_ = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE|DISCL_NOWINKEY);
	assert(SUCCEEDED(result_));
}

void Input::Update()
{
	// キーボード情報の取得開始
	keyboard->Acquire();
	// 全キーの入力情報を取得する
	BYTE key[256] = {};
	keyboard->GetDeviceState(sizeof(key), key);
}