#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "IMediaTextureSample.h"
#include "MediaObjectPool.h"

class FVlcVideoSample : public IMediaTextureSample, public IMediaPoolable
{
public:
	FVlcVideoSample();
	virtual ~FVlcVideoSample();
	
	virtual const void* GetBuffer() override;
	virtual FIntPoint GetDim() const override;
	virtual FTimespan GetDuration() const override;
	virtual EMediaTextureSampleFormat GetFormat() const override;
	virtual FIntPoint GetOutputDim() const override;
	virtual uint32 GetStride() const override;
#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override;
#endif
	virtual FMediaTimeStamp GetTime() const override;
	virtual bool IsCacheable() const override;
	virtual bool IsOutputSrgb() const override;

	bool Initialize(
		const FIntPoint& InDim,
		const FIntPoint& InOutputDim,
		EMediaTextureSampleFormat InFormat,
		uint32 InStride,
		FTimespan InDuration);

	void* GetMutableBuffer()
	{
		return Buffer;
	}

	void SetTime(FTimespan InTime)
	{
		Time = InTime;
	}

protected:
	void FreeBuffer();

private:
	/* 该画面样本的播放时间 */
	FTimespan Time;
	
	/* 该画面样本的持续时间 */
	FTimespan Duration;

	/* 画面样本的长宽尺寸 */
	FIntPoint Dim;

	/* 画面样本输出尺寸 */
	FIntPoint OutputDim;

	void* Buffer;

	uint64 BufferSize;

	uint32 Stride;

	EMediaTextureSampleFormat Format;
};

/* Vlc 画面样本对象池 */
class FVlcVideoSamplePool : public TMediaObjectPool<FVlcVideoSample> {};