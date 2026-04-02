#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <string>
#include <vector>

// Media Foundation
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <memory>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

struct SoundData
{
    WAVEFORMATEX wfex{};
    std::vector<BYTE> buffer; // ★vector化（課題要件）
};

// ★前方宣言：コールバッククラスをポインタで扱えるようにする
class VoiceCallback;

class SoundManager
{
public:
    SoundManager();
    ~SoundManager();

    bool Initialize();  // XAudio2 + Media Foundation 初期化
    void Finalize();    // 終了処理

    // ★追加：メインループで毎フレーム呼び出し、再生が終わった音を掃除する関数
    void Update();

    // Media FoundationでデコードしてPCM化して読み込む（MP3/WAV対応）
    SoundData SoundLoad(const wchar_t* filename);

    void SoundUnload(SoundData* soundData);
    void SoundPlayWave(const SoundData& soundData);

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;

    bool mfInitialized_ = false;

    // ★追加：再生中のボイス（とコールバック）を管理するリスト
    std::vector<std::unique_ptr<VoiceCallback>> activeVoices_;
};