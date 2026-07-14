#pragma once

#include "IMediaAudioSample.h"
#include "VlcAudioSample.h"
#include "VlcVideoSample.h"

class IMediaSamples;
class FMediaSamples;
struct libvlc_media_player_t;

class FVlcCallbackHandler
{
public:
	FVlcCallbackHandler();
	virtual ~FVlcCallbackHandler();

	IMediaSamples& GetSamples();

	void Initialize(libvlc_media_player_t* InPlayer);

	void SetCurrentTime(FTimespan InTime)
	{
		CurrentTime = InTime;
	}

	void ClearStartTime()
	{
		StartTime = FTimespan::Zero();
		AccumulatedAudioTime = FTimespan::Zero();
		bResetAudioTime = true;
		AccumulatedData.Empty();
		AccumulatedDurationSec = 0.0;
		AccumulatedFrames = 0;
		{
			TSharedPtr<FVlcAudioSample, ESPMode::ThreadSafe> Dummy;
			while (AudioPacketQueue.Dequeue(Dummy)) {}
		}
	}

	void ServiceAudioQueue();

	void Shutdown();

private:
	static int AudioSetupCallback(void** Opaque, ANSICHAR* Format, uint32* Rate, uint32* Channels);
	static void AudioCleanupCallback(void* Opaque);

	static void AudioPlayCallback(void* Opaque, const void* Buffer, uint32 Count, int64 Pts);
	static void AudioPauseCallback(void* Opaque, int64 Pts);
	static void AudioResumeCallback(void* Opaque, int64 Pts);
	static void AudioFlushCallback(void* Opaque, int64 Pts);
	static void AudioDrainCallback(void* Opaque);

	static unsigned VideoSetupCallback(void** Opaque, ANSICHAR* Chroma, uint32* Width, uint32* Height, uint32* Pitches, uint32* Lines);
	static void VideoCleanupCallback(void* Opaque);

	static void* VideoLockCallback(void* Opaque, void** Planes);
	static void VideoUnlockCallback(void* Opaque, void* Picture, void* const* Planes);
	static void VideoDisplayCallback(void* Opaque, void* Picture);


private:
	FTimespan CurrentTime;
	FTimespan StartTime;

	/** 累积音频时长 —— 保证音频时间戳无间隙 */
	FTimespan AccumulatedAudioTime;

	/** Seek/重播后首次音频帧需对齐到 CurrentTime */
	bool bResetAudioTime;

	static constexpr double MinSampleDurationSec = 0.128;
	TArray<uint8> AccumulatedData;
	FTimespan AccumulatedTime;
	double AccumulatedDurationSec;
	uint32 AccumulatedFrames;

	TQueue<TSharedPtr<FVlcAudioSample, ESPMode::ThreadSafe>, EQueueMode::Mpsc> AudioPacketQueue;

	FMediaSamples* Samples;

	uint32 AudioChannels;
	uint32 AudioSampleRate;
	uint64 AudioSampleSize;
	EMediaAudioSampleFormat AudioSampleFormat;

	FVlcAudioSamplePool* AudioSamplePool;

	FIntPoint VideoBufferDim;
	FIntPoint VideoOutputDim;
	uint32 VideoStride;
	FTimespan VideoFrameDuration;
	FTimespan VideoPreviousTime;
	EMediaTextureSampleFormat VideoSampleFormat;

	FVlcVideoSamplePool* VideoSamplePool;

	libvlc_media_player_t* Player;
};
