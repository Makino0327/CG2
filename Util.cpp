#include "Util.h"
#include <Windows.h>

std::wstring ConvertString(const std::string& str)
{
	// 1.バッファサイズを取得
	int len = MultiByteToWideChar(
		CP_ACP,     // 現在のコードページ
		0,          // フラグ
		str.c_str(),// 変換する文字列
		-1,         // 終端文字まで自動的に処理
		nullptr,    // 出力バッファはまだない
		0
	);

	// 2.必要なサイズでwstringを初期化
	std::wstring wstr(len, 0);

	// 3.変換実行
	MultiByteToWideChar(
		CP_ACP,     // 現在のコードページ
		0,          // フラグ
		str.c_str(),// 変換する文字列
		-1,         // 終端文字まで自動的に処理
		&wstr[0],   // 出力バッファ
		len         // バッファサイズ
	);
	return wstr;
};

void Log(const std::wstring& message)
{
	OutputDebugStringW(message.c_str());
	OutputDebugStringW(L"\n");
}

