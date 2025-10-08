#include "Input.h"  


void Input::Initialize(HINSTANCE hInst, HWND hwnd)
{
	/// 初期化処理
	// DirectInputのインスタンス生成
	directInput_ = nullptr;
	HRESULT hr = DirectInput8Create(
		hInst, 
		DIRECTINPUT_VERSION,
		IID_IDirectInput8, 
		(void**)&directInput_, nullptr);
	assert(SUCCEEDED(hr));
	// キーボードデバイスの生成
	hr = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, nullptr);
	assert(SUCCEEDED(hr));
	// 入力データ形式のセット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));
	// 排他制御レベルのセット
	hr = keyboard_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

}

void Input::Update()
{
	// 更新処理
}