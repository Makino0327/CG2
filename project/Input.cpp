#include "Input.h"

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
	// DirectInputのインスタンス生成
	result_ = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result_));
	// キーボードデバイスの生成
	result_ = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(result_));
	// 入力データ形式のセット
	result_ = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result_));
	// 排他制御レベルのセット
	result_ = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result_));
}

void Input::Update()
{
	// 前回のキー入力を保存
	memcpy(prevKey, key, sizeof(key));
	// キーボード情報の取得開始
	keyboard->Acquire();
	// 全キーの入力情報を取得する
	keyboard->GetDeviceState(sizeof(key), key);
}

bool Input::PushKey(BYTE keyNumber)
{
	// 指定したキーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}
	return false;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	// 前フレームで押されておらず、今フレームで押されているなら true
	if (key[keyNumber] && !prevKey[keyNumber]) {
		return true;
	}
	return false;
}

