#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "IMediaAudioSample.h"
#include "MediaObjectPool.h"

class FVlcAudioSample : public IMediaAudioSample, public IMediaPoolable
{
public:
	FVlcAudioSample()
		: Buffer(nullptr)
		, BufferSize(0)
		, NumChannels(0)
		, NumFrames(0)
		, SampleRate(0)
		, Format(EMediaAudioSampleFormat::Undefined)
	{
	}

	virtual ~FVlcAudioSample()
	{
		FreeBuffer();
	}

	bool Initialize(
		const void* InBuffer,
		uint32 InBufferSize,
		uint32 InSampleRate,
		uint32 InNumChannels,
		uint32 InNumFrames,
		EMediaAudioSampleFormat InFormat,
		FTimespan InTime,
		FTimespan InDuration);

	//~ IMediaAudioSample
	virtual const void* GetBuffer() override;
	virtual uint32 GetChannels() const override;
	virtual FTimespan GetDuration() const override;
	virtual EMediaAudioSampleFormat GetFormat() const override;
	virtual uint32 GetFrames() const override;
	virtual uint32 GetSampleRate() const override;
	virtual FMediaTimeStamp GetTime() const override;

protected:
	void FreeBuffer();

private:
	void* Buffer;
	
	uint64 BufferSize;

	uint32 NumChannels;

	uint32 NumFrames;

	uint32 SampleRate;

	EMediaAudioSampleFormat Format;
	
	FTimespan Time;
	
	FTimespan Duration;
};

class FVlcAudioSamplePool : public TMediaObjectPool<FVlcAudioSample> {};