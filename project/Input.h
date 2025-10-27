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

public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	ComPtr<IDirectInput8> directInput = nullptr;
	HRESULT result_;
	ComPtr<IDirectInputDevice8> keyboard;
	BYTE key[256] = {};
	BYTE prevKey[256] = {};
};

