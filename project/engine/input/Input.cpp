#include "Input.h"

void Input::Initialize(WinApp* winApp)
{
	// WindowsAPIをセット
	winApp_ = winApp;

	// DirectInputのインスタンス生成
	result_ = DirectInput8Create(winApp_->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result_));
	// キーボードデバイスの生成
	result_ = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, nullptr);
	assert(SUCCEEDED(result_));
	// 入力データ形式のセット
	result_ = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result_));
	// 排他制御レベルのセット
	result_ = keyboard->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
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

	// マウス座標を取得する
	POINT cursorPos{};
	GetCursorPos(&cursorPos);
	ScreenToClient(winApp_->GetHwnd(), &cursorPos);

	// 前フレームとの差分を計算する
	prevMousePosition_ = mousePosition_;
	mousePosition_.x = static_cast<float>(cursorPos.x);
	mousePosition_.y = static_cast<float>(cursorPos.y);

	mouseDelta_.x = mousePosition_.x - prevMousePosition_.x;
	mouseDelta_.y = mousePosition_.y - prevMousePosition_.y;

	// 右ボタン押下状態を更新する
	isMouseRightPressed_ = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

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

