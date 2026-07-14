#include "VlcAudioSample.h"

bool FVlcAudioSample::Initialize(
		const void* InBuffer,
		uint32 InBufferSize,
		uint32 InSampleRate,
		uint32 InNumChannels,
		uint32 InNumFrames,
		EMediaAudioSampleFormat InFormat,
		FTimespan InTime,
		FTimespan InDuration)
{
	if (InBuffer == nullptr || InBufferSize == 0 || InFormat == EMediaAudioSampleFormat::Undefined)
	{
		return false;
	}

	if (InBufferSize > BufferSize)
	{
		Buffer = FMemory::Realloc(Buffer, InBufferSize);
		BufferSize = InBufferSize;
	}

	FMemory::Memcpy(Buffer, InBuffer, InBufferSize);

	NumChannels = InNumChannels;
	SampleRate = InSampleRate;
	NumFrames = InNumFrames;
	Format = InFormat;
	Time = InTime;
	Duration = InDuration;
	
	return true;
}

const void* FVlcAudioSample::GetBuffer()
{
	return Buffer;
}

uint32 FVlcAudioSample::GetChannels() const
{
	return NumChannels;
}

FTimespan FVlcAudioSample::GetDuration() const
{
	return Duration;
}

EMediaAudioSampleFormat FVlcAudioSample::GetFormat() const
{
	return Format;
}

uint32 FVlcAudioSample::GetFrames() const
{
	return NumFrames;
}

uint32 FVlcAudioSample::GetSampleRate() const
{
	return SampleRate;
}

FMediaTimeStamp FVlcAudioSample::GetTime() const
{
	return FMediaTimeStamp(Time);
}

void FVlcAudioSample::FreeBuffer()
{
	if (Buffer != nullptr)
	{
		FMemory::Free(Buffer);
		Buffer = nullptr;
		BufferSize = 0;
	}
}
