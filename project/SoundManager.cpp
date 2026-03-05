#include "SoundManager.h"
#include <cassert>
#include <cstring>
#include <algorithm> // ★追加：リストからの削除用

// ------------------------------
// 再生終了を検知するコールバック
// ------------------------------
class VoiceCallback final : public IXAudio2VoiceCallback {
public:
    IXAudio2SourceVoice* voice = nullptr;
    bool isFinished = false; // ★追加：再生が終わったかどうかのフラグ

    void __stdcall OnBufferEnd(void*) override {}

    // ストリーム終了
    void __stdcall OnStreamEnd() override {
        // ★XAudio2の別スレッド内なので、ここでは消さずにフラグを立てるだけ！
        isFinished = true;
    }

    // エラー時
    void __stdcall OnVoiceError(void*, HRESULT) override {
        // ★同じくフラグを立てるだけ
        isFinished = true;
    }

    void __stdcall OnVoiceProcessingPassStart(UINT32) override {}
    void __stdcall OnVoiceProcessingPassEnd() override {}
    void __stdcall OnBufferStart(void*) override {}
    void __stdcall OnLoopEnd(void*) override {}
};

SoundManager::SoundManager() = default;

SoundManager::~SoundManager() {
    Finalize();
}

bool SoundManager::Initialize() {
    // ---- Media Foundation ----
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return false;
    }

    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }
    mfInitialized_ = true;

    // ---- XAudio2 ----
    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        Finalize();
        return false;
    }
    hr = xAudio2_->CreateMasteringVoice(&masteringVoice_);
    if (FAILED(hr)) {
        Finalize();
        return false;
    }

    return true;
}

// ★追加：毎フレーム呼んで、終わったボイスを安全にお掃除する
void SoundManager::Update() {
    for (auto it = activeVoices_.begin(); it != activeVoices_.end(); ) {
        VoiceCallback* cb = *it;
        if (cb->isFinished) {
            // 再生が終わっていたら、ここで安全に破棄する
            if (cb->voice) {
                cb->voice->DestroyVoice();
            }
            delete cb;
            it = activeVoices_.erase(it); // リストから削除
        } else {
            ++it;
        }
    }
}

void SoundManager::Finalize() {
    // ★追加：終了時にまだ再生中のボイスがあれば強制終了させる
    for (auto cb : activeVoices_) {
        if (cb->voice) {
            cb->voice->DestroyVoice();
        }
        delete cb;
    }
    activeVoices_.clear();

    // XAudio2
    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    xAudio2_.Reset();

    // Media Foundation
    if (mfInitialized_) {
        MFShutdown();
        mfInitialized_ = false;
    }

    // COM
    CoUninitialize();
}

SoundData SoundManager::SoundLoad(const wchar_t* filename) {
    // （元のコードから変更なしのため、中略・そのままコピペでお使いください）
    assert(mfInitialized_ && "SoundManager::Initialize()でMFStartupしてから呼んでください");

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(filename, nullptr, &reader);
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IMFMediaType> outType;
    hr = MFCreateMediaType(&outType);
    assert(SUCCEEDED(hr));

    hr = outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));
    hr = outType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(hr));

    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, outType.Get());
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IMFMediaType> currentType;
    hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &currentType);
    assert(SUCCEEDED(hr));

    WAVEFORMATEX* wfexTemp = nullptr;
    UINT32 wfexSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(currentType.Get(), &wfexTemp, &wfexSize);
    assert(SUCCEEDED(hr));

    SoundData sound{};
    sound.wfex = *wfexTemp;              // 値コピー
    CoTaskMemFree(wfexTemp);             // MFが確保したので解放

    while (true) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;

        hr = reader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &sample
        );
        assert(SUCCEEDED(hr));

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
        if (!sample) {
            continue;
        }

        Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
        hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
        assert(SUCCEEDED(hr));

        BYTE* data = nullptr;
        DWORD maxLen = 0;
        DWORD curLen = 0;
        hr = mediaBuffer->Lock(&data, &maxLen, &curLen);
        assert(SUCCEEDED(hr));

        size_t oldSize = sound.buffer.size();
        sound.buffer.resize(oldSize + curLen);
        std::memcpy(sound.buffer.data() + oldSize, data, curLen);

        hr = mediaBuffer->Unlock();
        assert(SUCCEEDED(hr));
    }

    return sound;
}

void SoundManager::SoundUnload(SoundData* soundData) {
    if (!soundData) { return; }
    soundData->buffer.clear();
    soundData->buffer.shrink_to_fit();
    soundData->wfex = {};
}

void SoundManager::SoundPlayWave(const SoundData& soundData) {
    assert(xAudio2_);
    assert(!soundData.buffer.empty());

    VoiceCallback* callback = new VoiceCallback();

    IXAudio2SourceVoice* sourceVoice = nullptr;
    HRESULT hr = xAudio2_->CreateSourceVoice(
        &sourceVoice,
        &soundData.wfex,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        callback
    );
    assert(SUCCEEDED(hr));

    callback->voice = sourceVoice;

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.buffer.data();
    buf.AudioBytes = (UINT32)soundData.buffer.size();
    buf.Flags = XAUDIO2_END_OF_STREAM;

    hr = sourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hr));

    hr = sourceVoice->Start();
    assert(SUCCEEDED(hr));

    // ★追加：破棄リストに登録して管理下に置く
    activeVoices_.push_back(callback);
}