#include <Windows.h>  
#include <dbghelp.h>
#include <strsafe.h>
#include <cstdint> 

#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dump", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dump/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);

	HANDLE dumpFileHandle = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (dumpFileHandle != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
		minidumpInformation.ThreadId = GetCurrentThreadId();
		minidumpInformation.ExceptionPointers = exception;
		minidumpInformation.ClientPointers = TRUE;

		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
		CloseHandle(dumpFileHandle);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	SetUnhandledExceptionFilter(ExportDump);

	// ★わざと例外を発生させる (ACCESS_VIOLATION をRaiseする)
	RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);

	return 0;
}
