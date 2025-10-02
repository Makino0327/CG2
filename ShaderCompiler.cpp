#include "ShaderCompiler.h"
#include "Util.h"
#include <cassert>
#include <format>

// シェーダーをコンパイルしてバイナリに変換する関数
IDxcBlob* ShaderCompiler::CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	// ファイル読み込み
	Log(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile));
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	// 読み込んだファイルをCompileできる形に変換する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E",L"main",
		L"-T",profile,
		L"-Zi",L"Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	// コンパイル実行
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	// 警告エラーが出ていないか確認する
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(ConvertString(shaderError->GetStringPointer()));
		assert(false); // 本当にエラーなら止め
	}

	// Compile結果からバイナリを取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	Log(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile));

	// リソース解放
	shaderSource->Release();
	shaderResult->Release();


	return shaderBlob;
}
