// 標準ライブラリ
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdint>
#include <string>
#include <fstream>
#include <random>

#define _USE_MATH_DEFINES
#include <cmath>      // sinf, cosf, M_PI など

// DirectX / COM 周り
#include <d3d12.h>    // ID3D12GraphicsCommandList, ID3D12DescriptorHeap などで必要
#include <xaudio2.h>
#include <Xinput.h>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// ImGui
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx12.h"

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
#include "Camera.h"   
#include "Particle.h" 
#include "SrvManager.h"
#include "ImGuiManager.h"

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
SrvManager* srvManager = nullptr;

 //どこでも使えるように（Drawで必要）
const uint32_t kNumInstance = 10;
Microsoft::WRL::ComPtr<ID3D12Resource> gInstancingResource;
ParticleForGPU* gInstancingData = nullptr;
D3D12_GPU_DESCRIPTOR_HANDLE gInstancingSrvHandleGPU{};
ParticleCommon* particleCommon = nullptr;
ParticleSystem* particleSystem = nullptr;

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {

	D3DResourceLeakChecker leakChecker;

	// WindowsAPI初期化
	winApp = new WinApp();
	winApp->Initialize();

	// DirectX初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	// SrvManager 初期化
	srvManager = new SrvManager();
	srvManager->Initialize(dxCommon);

	// ★ ここで先に TextureManager を初期化
	TextureManager::GetInstance()->Initialize(dxCommon, srvManager);

	// SpriteCommon の初期化
	spriteCommon = new SpriteCommon();
	spriteCommon->Initialize(dxCommon,srvManager);

	// 3d オブジェクト共通処理
	object3dCommon = new Object3dCommon();
	object3dCommon->Initialize(dxCommon);

	// ★ カメラ生成 & デフォルトカメラに設定
	Camera* camera = new Camera();
	camera->SetRotate({ 0.3f, 0.0f, 0.0f });
	camera->SetTranslate({ 0.0f, 3.0f, -10.0f });
	object3dCommon->SetDefaultCamera(camera);

	// 3D オブジェクト
	object3d = new Object3d();
	object3d->Initialize(object3dCommon);

	// モデル共通処理
	modelCommon = new ModelCommon();
	modelCommon->Initialize(dxCommon);

	// ★ ParticleCommon 初期化
	particleCommon = new ParticleCommon();
	particleCommon->Initialize(dxCommon,srvManager);

	ImGuiManager* imguiManager = new ImGuiManager();

	// 初期化
	imguiManager->Initialize(winApp,dxCommon, srvManager);


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
	texMan->LoadTexture("Resources/circle.png");
	texMan->LoadTexture("Resources/fence.png");

	particleSystem = new ParticleSystem();
	particleSystem->Initialize(dxCommon, particleCommon, camera, srvManager, ParticleType::CircleBurst);
	particleSystem->SetPosition({ 0.0f, 0.0f, 0.0f });   // エミッタ基準位置


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
	objA->SetTexture("Resources/circle.png");

	//// 位置変える
	objA->SetTranslate({ 0, 0, 0 });
	//Particle particles[kNumInstance];
	const float kDeltaTime = 1.0f / 60.0f; // とりあえず60fps想定

	//std::random_device seedGenerator;
	//std::mt19937 randomEngine(seedGenerator());

	//for (uint32_t i = 0; i < kNumInstance; ++i) {
	//	particles[i] = MakeNewParticle(randomEngine);
	//}

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

		for (Sprite* sprite : sprites) {
			sprite->Update();
		}

		for (Sprite* sprite : sprites) {
			sprite->Draw();
		}

		// ======= Update =======
		objA->Update();

		camera->Update();

		particleSystem->Update(kDeltaTime);


		// ======= Draw =======
		object3dCommon->CommonDrawSetting();

		objA->Draw();

		// ======= Draw =======

// まずは普通の Object3d 表示
		object3dCommon->CommonDrawSetting();
		//objA->Draw();

		//// そのあとインスタンス描画（Particle用PSO）
		particleCommon->CommonDrawSetting();

		particleSystem->Draw();


		//描画

//		ImGui_ImplDX12_NewFrame();
//		ImGui_ImplWin32_NewFrame();
//		ImGui::NewFrame();
//
//		// ==========================
////  Particle Editor
//// ==========================
//		particleSystem->ShowImGui();
//
//
//		{
//			// ★ Camera の Transform を直接触る
//			Transform& camTrans = camera->GetTransform();
//
//			ImGui::Begin("Camera");
//
//			// 左手座標系：x=右, y=上, z=奥 という意識でOK
//			ImGui::DragFloat3("Position", &camTrans.translate.x, 0.1f);
//
//			// 回転はラジアン。-π〜π くらいでスライダーにしておく
//			ImGui::SliderFloat("Rot X", &camTrans.rotate.x, -3.14f, 3.14f);
//			ImGui::SliderFloat("Rot Y", &camTrans.rotate.y, -3.14f, 3.14f);
//			ImGui::SliderFloat("Rot Z", &camTrans.rotate.z, -3.14f, 3.14f);
//
//			ImGui::End();
//		}
//
//		ImGui::Render();
//		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

		dxCommon->PostDraw();
//	}
//
//	// --- 後片付け --- 
//
//// ImGuiは先に終了
//	ImGui_ImplDX12_Shutdown();
//	ImGui_ImplWin32_Shutdown();
//	ImGui::DestroyContext();
	}

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
	delete camera; camera = nullptr;
	delete particleSystem;  particleSystem = nullptr;
	// 解放
	delete imguiManager;
	delete srvManager; srvManager = nullptr;

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
