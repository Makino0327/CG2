#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cassert>

#include "../base/winapp/WinApp.h"
#include "../math/Math.h"

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input
{
public:
	// 初期化
	void Initialize(WinApp* winApp);

	// 更新
	void Update();

	/// <summary>
	/// キーの押したかをチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーのトリガーチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber);

	// マウスの現在座標を取得する
	Vector2 GetMousePosition() const { return mousePosition_; }

	// マウスの移動量を取得する
	Vector2 GetMouseDelta() const { return mouseDelta_; }

	// 右ボタンを押しているか
	bool PushMouseRight() const { return isMouseRightPressed_; }

	// 左ボタンを押しているか
	bool PushMouseLeft() const { return isMouseLeftPressed_; }

	// 左ボタンを押した瞬間か
	bool TriggerMouseLeft() const { return isMouseLeftPressed_ && !wasMouseLeftPressed_; }


public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	ComPtr<IDirectInput8> directInput = nullptr;
	HRESULT result_;
	ComPtr<IDirectInputDevice8> keyboard;
	BYTE key[256] = {};
	BYTE prevKey[256] = {};

	// WindowsAPI
	WinApp* winApp_ = nullptr;

	// マウスの現在座標
	Vector2 mousePosition_ = { 0.0f, 0.0f };

	// 1フレーム分のマウス移動量
	Vector2 mouseDelta_ = { 0.0f, 0.0f };

	// 前フレームのマウス座標
	Vector2 prevMousePosition_ = { 0.0f, 0.0f };

	// 右ボタン押下中か
	bool isMouseRightPressed_ = false;

	// 左ボタンが押されているか
	bool isMouseLeftPressed_ = false;

	// 前フレームの左ボタン状態
	bool wasMouseLeftPressed_ = false;

};

