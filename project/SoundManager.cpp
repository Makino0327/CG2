#include "SoundManager.h"
#include <fstream>
#include <cassert>
#include <cstring>

// ------------------------------
// 再生終了時に SourceVoice を破棄するコールバック
// ------------------------------
class VoiceCallback final : public IXAudio2VoiceCallback {
public:
    IXAudio2SourceVoice* voice = nullptr;

    void __stdcall OnBufferEnd(void* /*pBufferContext*/) override {
        if (voice) {
            voice->DestroyVoice();
            voice = nullptr;
        }
        delete this; // Playごとにnewしてるのでここで回収
    }

    // 使わないコールバックは空実装でOK
    void __stdcall OnVoiceProcessingPassStart(UINT32) override {}
    void __stdcall OnVoiceProcessingPassEnd() override {}
    void __stdcall OnStreamEnd() override {}
    void __stdcall OnBufferStart(void*) override {}
    void __stdcall OnLoopEnd(void*) override {}
    void __stdcall OnVoiceError(void*, HRESULT) override {}
};

SoundManager::SoundManager()
    : masteringVoice_(nullptr) {
}

SoundManager::~SoundManager() {
    Finalize();
}

bool SoundManager::Initialize() {
    HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        return false;
    }
    hr = xAudio2_->CreateMasteringVoice(&masteringVoice_);
    return SUCCEEDED(hr);
}

void SoundManager::Finalize() {
    // ★順番：MasteringVoice を先に破棄 → XAudio2 を解放
    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    xAudio2_.Reset();
}

SoundData SoundManager::SoundLoadWave(const char* filename) {
    std::ifstream file;
    file.open(filename, std::ios::binary);
    assert(file.is_open());

    // RIFF
    RiffHeader riff;
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) { assert(0); }
    if (strncmp(riff.type, "WAVE", 4) != 0) { assert(0); }

    // fmt を探す（JUNK等に強い）
    FormatChunk format = {};
    ChunkHeader chunk{};
    while (true) {
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));
        if (file.eof()) {
            assert(0 && "fmtチャンクが見つかりませんでした");
        }
        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            format.chunk = chunk;
            assert(format.chunk.size <= sizeof(format.fmt));
            file.read(reinterpret_cast<char*>(&format.fmt), format.chunk.size);
            break;
        }
        file.seekg(chunk.size, std::ios_base::cur);
    }

    // data を探す（JUNK等に強い）
    ChunkHeader data{};
    while (true) {
        file.read(reinterpret_cast<char*>(&data), sizeof(data));
        if (file.eof()) { assert(0 && "dataチャンクが見つかりませんでした"); }
        if (strncmp(data.id, "data", 4) == 0) { break; }
        file.seekg(data.size, std::ios_base::cur);
    }

    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);
    file.close();

    SoundData soundData{};
    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;
    return soundData;
}

void SoundManager::SoundUnload(SoundData* soundData) {
    if (!soundData) { return; }
    delete[] soundData->pBuffer;
    soundData->pBuffer = nullptr;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void SoundManager::SoundPlayWave(const SoundData& soundData) {
    assert(xAudio2_);

    // ★ Playごとにコールバックを作って、終了時に voice をDestroyする
    VoiceCallback* callback = new VoiceCallback();

    IXAudio2SourceVoice* sourceVoice = nullptr;
    HRESULT hr = xAudio2_->CreateSourceVoice(&sourceVoice, &soundData.wfex, 0, XAUDIO2_DEFAULT_FREQ_RATIO, callback);
    assert(SUCCEEDED(hr));

    callback->voice = sourceVoice;

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    hr = sourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hr));
    hr = sourceVoice->Start();
    assert(SUCCEEDED(hr));
}
