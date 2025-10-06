#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <dxcapi.h>

class ShaderCompiler
{
public:
	ShaderCompiler() = default;
	~ShaderCompiler() = default;

	// DXCの初期化
	bool Initialize();

	// DXCのユーティリティ
	void Finalize();

	static IDxcBlob* CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile, 
		IDxcUtils* dxcUtils, 
		IDxcCompiler3* dxcCompiler, 
		IDxcIncludeHandler* includeHandler
	);

};

