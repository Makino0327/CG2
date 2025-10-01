#pragma once
#include <xaudio2.h>
#include <string>
#include <wrl.h>

struct SoundData
{
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

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

class SoundManager
{
public: 
	SoundManager();
	~SoundManager();

	bool Initialize();  // XAudio2 初期化
	void Finalize();    // 終了処理

	SoundData SoundLoadWave(const char* filename);
	void SoundUnload(SoundData* soundData);
	void SoundPlayWave( const SoundData& soundData);

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masteringVoice_;
};

