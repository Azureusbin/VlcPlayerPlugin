#include "VlcVideoSample.h"

FVlcVideoSample::FVlcVideoSample()
	: Time(FTimespan::Zero())
	, Duration(FTimespan::Zero())
	, Dim(FIntPoint::ZeroValue)
	, OutputDim(FIntPoint::ZeroValue)
	, Buffer(nullptr)
	, BufferSize(0)
	, Stride(0)
	, Format(EMediaTextureSampleFormat::Undefined)
{ }

FVlcVideoSample::~FVlcVideoSample()
{
	FreeBuffer();
}

const void* FVlcVideoSample::GetBuffer()
{
	return Buffer;
}

FIntPoint FVlcVideoSample::GetDim() const
{
	return Dim;
}

FTimespan FVlcVideoSample::GetDuration() const
{
	return Duration;
}

EMediaTextureSampleFormat FVlcVideoSample::GetFormat() const
{
	return Format;
}

FIntPoint FVlcVideoSample::GetOutputDim() const
{
	return OutputDim;
}

uint32 FVlcVideoSample::GetStride() const
{
	return Stride;
}

#if WITH_ENGINE
FRHITexture* FVlcVideoSample::GetTexture() const
{
	return nullptr;
}
#endif

FMediaTimeStamp FVlcVideoSample::GetTime() const
{
	return FMediaTimeStamp(Time);
}

bool FVlcVideoSample::IsCacheable() const
{
	return true;
}

bool FVlcVideoSample::IsOutputSrgb() const
{
	return true;
}

bool FVlcVideoSample::Initialize(
	const FIntPoint& InDim,
	const FIntPoint& InOutputDim,
	EMediaTextureSampleFormat InFormat,
	uint32 InStride,
	FTimespan InDuration)
{
	if (InFormat == EMediaTextureSampleFormat::Undefined)
	{
		return false;
	}

	const uint64 RequiredBufferSize = InStride * InDim.Y;

	if (RequiredBufferSize == 0)
	{
		return false;
	}

	if (RequiredBufferSize > BufferSize)
	{
		Buffer = FMemory::Realloc(Buffer, RequiredBufferSize, 32);
		BufferSize = RequiredBufferSize;
	}

	Dim = InDim;
	OutputDim = InOutputDim;
	Format = InFormat;
	Stride = InStride;
	Duration = InDuration;

	return true;
}

void FVlcVideoSample::FreeBuffer()
{
	if (Buffer != nullptr)
	{
		FMemory::Free(Buffer);
		Buffer = nullptr;
		BufferSize = 0;
	}
}
