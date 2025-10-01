#pragma once
#include <string>

// 文字列変換(std::string -> std::wstring)
std::wstring ConvertString(const std::string& str);

// デバッグログ出力
void Log(const std::wstring& message);

