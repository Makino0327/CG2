// 標準ライブラリ
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <fstream>

#define _USE_MATH_DEFINES
#include <cmath>      // sinf, cosf, M_PI など

// DirectX / COM 周り
#include <d3d12.h>    // ID3D12GraphicsCommandList, ID3D12DescriptorHeap などで必要
#include <xaudio2.h>
#include <Xinput.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// ImGui
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

// 自作ヘッダー
#include "Math.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "D3DResourceLeakChecker.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelCommon.h"
#include "ModelManager.h"
#include "ParticleCommon.h"

// ライブラリリンク（ここにまとめておく）
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "xinput.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct ChunkHeader
{
	char id[4];
	int32_t size;
};

struct RiffHeader
{
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

struct SoundData
{
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

SoundData SoundLoadWave(const char* filename)
{
	/// 1.ファイルオープン
	// ファイル入力ストリームのインスタンス
	std::ifstream file;
	// .wavファイルをバイナリモードで開く
	file.open(filename, std::ios::binary);
	// ファイルオープン失敗を検出する
	assert(file.is_open());

	/// 2.wavデータ読み込み
	// RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	// ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
	{
		assert(0);
	}
	// タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(0);
	}
	// Formatチャンクの読み込み
	// Formatチャンクの読み込み（柔軟に探す）
	FormatChunk format = {};
	ChunkHeader chunk{};
	while (true) {
		file.read((char*)&chunk, sizeof(chunk));
		if (file.eof()) {
			assert(0 && "fmtチャンクが見つかりませんでした");
		}

		if (strncmp(chunk.id, "fmt ", 4) == 0) {
			format.chunk = chunk;
			assert(format.chunk.size <= sizeof(format.fmt));
			file.read((char*)&format.fmt, format.chunk.size);
			break;
		}

		// fmtじゃなかったらスキップ
		file.seekg(chunk.size, std::ios_base::cur);
	}

	// Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	// JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		// 読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		// 再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(0);
	}

	// Dataチャンクのデータ部（波型データ）の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);
	/// 3.ファイルクローズ
	// Waveファイルを閉じる
	file.close();

	/// 4.読み込んだ音声データをreturn
	// returnするための音声データ
	SoundData soundData = {};

	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

// 音声データ解放
void SoundUnload(SoundData* soundData)
{
	// バッファの解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// 音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData)
{
	HRESULT result;

	// 波形フォーマットをもとにSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// 再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	// 波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	result = pSourceVoice->Start();
}

// グローバル変数（各種DirectXオブジェクト）
ComPtr<IXAudio2> xAudio2;
IXAudio2MasteringVoice* masteringVoice;

SoundData soundData1 = SoundLoadWave("Resources/fanfare.wav");

/// ポインタ
// WindowsAPI
WinApp* winApp = nullptr;
// 入力処理
Input* input = nullptr;
// DirectX共通処理
DirectXCommon* dxCommon = nullptr;
// スプライト共通処理
SpriteCommon* spriteCommon = nullptr;
// スプライト
Sprite* sprite = nullptr;
// スプライト群
std::vector<Sprite*> sprites;
// 3Dオブジェクト共通処理
Object3dCommon* object3dCommon = nullptr;
// 3Dオブジェクト
Object3d* object3d = nullptr;
// 
ModelCommon* modelCommon = nullptr;

// どこでも使えるように（Drawで必要）
const uint32_t kNumInstance = 10;
Microsoft::WRL::ComPtr<ID3D12Resource> gInstancingResource;
TransformationMatrix* gInstancingData = nullptr;
D3D12_GPU_DESCRIPTOR_HANDLE gInstancingSrvHandleGPU{};
ParticleCommon* particleCommon = nullptr;


// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakChecker;

	// WindowsAPI初期化
	winApp = new WinApp();
	winApp->Initialize();

	// DirectX初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// ★ ここで先に TextureManager を初期化
	TextureManager::GetInstance()->Initialize(dxCommon);

	// SpriteCommon の初期化
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon);

	// 3d オブジェクト共通処理
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

	// 3D オブジェクト
	object3d = new Object3d();
	object3d->Initialize(object3dCommon);

	// モデル共通処理
	modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);

	// ★ ParticleCommon 初期化
	particleCommon = new ParticleCommon();
	particleCommon->Initialize(dxCommon);

	// 3Dモデルマネージャー
	ModelManager::GetInstance()->Initialize(dxCommon);

	ModelManager::GetInstance()->LoadModel("fence.obj");
	// 追加
	ModelManager::GetInstance()->LoadModel("plane.obj");

	// （必要なら）テクスチャを事前ロード
	auto texMan = TextureManager::GetInstance();
	texMan->LoadTexture("Resources/uvChecker.png");
	texMan->LoadTexture("Resources/monsterBall.png");
	texMan->LoadTexture("Resources/checkerBoard.png");
	texMan->LoadTexture("Resources/fence.png");

	// ======================================
// Instancing用 Resource を作る
// ======================================
	ID3D12Device* device = dxCommon->GetDevice();
	ID3D12DescriptorHeap* srvHeap = dxCommon->GetSrvDescriptorHeap();
	UINT srvSize = dxCommon->GetDescriptorSizeSRV();

	// バッファリソース作成
	gInstancingResource = dxCommon->CreateBufferResource(sizeof(TransformationMatrix) * kNumInstance);

	// Map
	gInstancingResource->Map(0, nullptr, reinterpret_cast<void**>(&gInstancingData));

	// 初期値は単位行列
	for (uint32_t i = 0; i < kNumInstance; ++i) {
		gInstancingData[i].WVP = MakeIdentity4x4();
		gInstancingData[i].World = MakeIdentity4x4();
	}

	// SRV 設定
	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// ★ TextureManager が使ってなさそうなインデックスを1つ決める
	//   （ここでは例として 10 にしておく）
	UINT instancingIndex = 10;

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
		dxCommon->GetCPUDescriptorHandle(srvHeap, srvSize, instancingIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
		dxCommon->GetGPUDescriptorHandle(srvHeap, srvSize, instancingIndex);

	device->CreateShaderResourceView(
		gInstancingResource.Get(),
		&instancingSrvDesc,
		handleCPU
	);

	gInstancingSrvHandleGPU = handleGPU;


	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2->CreateMasteringVoice(&masteringVoice);
	assert(SUCCEEDED(result));

	// 通常モデル用のマテリアルリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData->uvTransform = MakeIdentity4x4();

	// ライト用の定数バッファリソースを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	// 書き込み用ポインタの定義（これが必要）
	DirectionalLight* directionalLightData = nullptr;
	// マップしてアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// 初期化（単位ベクトルで）
	directionalLightData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	directionalLightData->direction = Vector3(0.0f, -1.0f, 0.0f); // 正規化されてること
	directionalLightData->intensity = 4.0f;

	// 入力の初期化
	input = new Input();
	input->Initialize(winApp);

	// Spriteの初期化

	for (uint32_t i = 0; i < 5; ++i) {
		Sprite* sprite = new Sprite();

		// 交互にテクスチャファイルを切り替え
		std::string texPath;
		if (i % 2 == 0) {
			texPath = "Resources/uvChecker.png";
		} else {
			texPath = "Resources/monsterBall.png";
		}

		sprite->Initialize(spriteCommon, directionalLightResource.Get(), texPath);

		Vector2 pos = { 100.0f + 150.0f * i, 200.0f };
		sprite->SetPosition(pos);

		sprites.push_back(sprite);
	}

	// Object3d を２つ作る
	Object3d* objA = new Object3d();
	objA->Initialize(object3dCommon);
	objA->SetModel("fence.obj");     // ← 1つめは plane
	objA->SetTexture("Resources/fence.png");

	// 位置変える
	objA->SetTranslate({ 0, 0, 0 });

	// --- メインループ ---
	while (TRUE) {
		// メッセージ処理
		if (winApp->ProcessMessage()) {
			// break
			break;
		}

		input->Update();

		if (input->TriggerKey(DIK_0)) {
			OutputDebugStringA("push 0\n");
		}

		dxCommon->PreDraw();

		ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();



		// ===== スプライト =====
		spriteCommon->CommonDrawSetting(); // Sprite PSO 設定

		/*for (Sprite* sprite : sprites) {
			sprite->Update();
		}

		for (Sprite* sprite : sprites) {
			sprite->Draw();
		}*/

		// ======= Update =======
		objA->Update();

		// ======= インスタンス用行列更新 =======
		Matrix4x4 viewProjectionMatrix = objA->GetViewProjectionMatrix();

		for (uint32_t i = 0; i < kNumInstance; ++i) {
			Transform t{};
			t.scale = { 1.0f, 1.0f, 1.0f };
			t.rotate = { 0.0f, 0.0f, 0.0f };
			t.translate = { (float)i * 0.1f, (float)i * -0.1f, (float)i * 0.1f }; // X方向に 2.0 ずつ並べる

			Matrix4x4 world = MakeAffineMatrix(t.scale, t.rotate, t.translate);
			Matrix4x4 wvp = Multiply(world, viewProjectionMatrix);

			gInstancingData[i].World = world;
			gInstancingData[i].WVP = wvp;
		}

		// ======= Draw =======
		object3dCommon->CommonDrawSetting();

		//objA->Draw();

		// ======= Draw =======

// まずは普通の Object3d 表示
		object3dCommon->CommonDrawSetting();
		//objA->Draw();

		// そのあとインスタンス描画（Particle用PSO）
		particleCommon->CommonDrawSetting();

		// RootParam[0] = 行列 StructuredBuffer(t0)
		commandList->SetGraphicsRootDescriptorTable(0, gInstancingSrvHandleGPU);

		// RootParam[1] = テクスチャ(t1) → fence.png
		auto texMan = TextureManager::GetInstance();
		uint32_t texIndex = texMan->GetTextureIndexByFilePath("Resources/uvChecker.png");
		D3D12_GPU_DESCRIPTOR_HANDLE fenceTexHandle = texMan->GetSrvHandleGPU(texIndex);
		commandList->SetGraphicsRootDescriptorTable(1, fenceTexHandle);

		// fence モデルをインスタンス描画
		Model* fenceModel = ModelManager::GetInstance()->FindModel("plane.obj");
		if (fenceModel) {
			fenceModel->DrawInstanced(kNumInstance);
		}


		//描画

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 自作ウィンドウだけ表示する
		ImGui::Begin("Sprite Transform");

		// === モード別UI分岐 ===

		ImGui::Text("Create");
		ImGui::Separator();

		if (objA) {

			// ====== Transform ======

			Vector3 tr = objA->GetTranslate();
			if (ImGui::SliderFloat3("Translate", &tr.x, -10.0f, 10.0f)) {
				objA->SetTranslate(tr);
			}

			Vector3 rot = objA->GetRotate();
			if (ImGui::SliderFloat3("Rotate", &rot.x, -3.14f, 3.14f)) {
				objA->SetRotate(rot);
			}

			Vector3 sc = objA->GetScale();
			if (ImGui::SliderFloat3("Scale", &sc.x, 0.1f, 5.0f)) {
				objA->SetScale(sc);
			}

			// ====== Color ======

			Material* mat = objA->GetMaterial();
			Vector4 col = mat->color;
			if (ImGui::ColorEdit4("Color", &col.x)) {
				objA->SetColor(col);
			}
		}



		// 現在の選択中Lighting
		static LightingType currentLighting = LightingType::HalfLambert; // 初期はLambert

		// コンボボックスの選択肢
		const char* lightingItems[] = { "None", "Lambert", "HalfLambert" };

		// 選択状態のintを取得してUIに渡す

		int currentLightingIndex = static_cast<int>(currentLighting);

		// Comboで選択が変わったら…
		if (ImGui::Combo("Lighting", &currentLightingIndex, lightingItems, IM_ARRAYSIZE(lightingItems))) {
			currentLighting = static_cast<LightingType>(currentLightingIndex);
		}

		// ★こっちに入れる！！
		objA->GetMaterial()->lightingType = static_cast<int>(currentLighting);


		// フレームの一番最後で呼ぶ（描画後でも可）
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(0.35f); // 半透明にする（好みで調整）

		ImGui::End();



		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

		dxCommon->PostDraw();
	}

	// --- 後片付け --- 

// ImGuiは先に終了
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// XAudio2
	if (masteringVoice) {
		masteringVoice->DestroyVoice();
		masteringVoice = nullptr;
	}
	xAudio2.Reset();
	SoundUnload(&soundData1);

	// 3Dモデルマネージャーの終了
	ModelManager::GetInstance()->Finalize();
	// テクスチャマネージャーの終了
	TextureManager::GetInstance()->Finalize();

	delete spriteCommon; spriteCommon = nullptr;
	delete input;      input = nullptr;
	for (Sprite* sprite : sprites) {
		delete sprite;
	}
	sprites.clear();
	delete object3d;   object3d = nullptr;
	delete object3dCommon; object3dCommon = nullptr;
	delete modelCommon;    modelCommon = nullptr;
	delete particleCommon; particleCommon = nullptr;


	// LiveObjects の出力は D3DResourceLeakChecker に任せるのでここは削除
	// （IDXGIDebug1* をここで触らない）
	delete dxCommon;   dxCommon = nullptr;
	if (winApp) {
		winApp->Finalize();
		delete winApp;
		winApp = nullptr;
	}

	return 0;
}
