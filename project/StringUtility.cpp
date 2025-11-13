#include "StringUtility.h"
#include <WinNls.h>

namespace StringUtility
{
	std::wstring ConvertString(const std::string& str) {
		int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(len, 0);
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], len);
		return wstr;
	}

	std::string ConvertString(const std::wstring& str)
	{
		int len = WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string result(len, 0);
		WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, &result[0], len, nullptr, nullptr);
		return result;
	}
}