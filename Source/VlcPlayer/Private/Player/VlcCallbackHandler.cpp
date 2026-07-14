#include "VlcCallbackHandler.h"

#include "MediaSamples.h"

#include <VlcHeader.h>

FVlcCallbackHandler::FVlcCallbackHandler()
	: CurrentTime(FTimespan::Zero())
	, StartTime(FTimespan::Zero())
	, AccumulatedAudioTime(FTimespan::Zero())
	, bResetAudioTime(true)
	, AccumulatedTime(FTimespan::Zero())
	, AccumulatedDurationSec(0.0)
	, AccumulatedFrames(0)
	, Samples(new FMediaSamples)
	, AudioChannels(0)
	, AudioSampleRate(0)
	, AudioSampleSize(0)
	, AudioSampleFormat(EMediaAudioSampleFormat::Int16)
	, AudioSamplePool(new FVlcAudioSamplePool)
	, VideoBufferDim(FIntPoint::ZeroValue)
	, VideoOutputDim(FIntPoint::ZeroValue)
	, VideoStride(0)
	, VideoFrameDuration(FTimespan::Zero())
	, VideoPreviousTime(FTimespan::MinValue())
	, VideoSampleFormat(EMediaTextureSampleFormat::CharAYUV)
	, VideoSamplePool(new FVlcVideoSamplePool)
	, Player(nullptr)
{ }

FVlcCallbackHandler::~FVlcCallbackHandler()
{
	Shutdown();

	delete AudioSamplePool;
	AudioSamplePool = nullptr;
	
	delete VideoSamplePool;
	VideoSamplePool = nullptr;
	
	delete Samples;
	Samples = nullptr;
}

IMediaSamples& FVlcCallbackHandler::GetSamples()
{
	return *Samples;
}

void FVlcCallbackHandler::Initialize(libvlc_media_player_t* InPlayer)
{
	Shutdown();

	Player = InPlayer;
	
	libvlc_audio_set_format_callbacks(
		Player,
		&FVlcCallbackHandler::AudioSetupCallback,
		&FVlcCallbackHandler::AudioCleanupCallback);

	libvlc_audio_set_callbacks(
		Player,
		&FVlcCallbackHandler::AudioPlayCallback,
		&FVlcCallbackHandler::AudioPauseCallback,
		&FVlcCallbackHandler::AudioResumeCallback,
		&FVlcCallbackHandler::AudioFlushCallback,
		&FVlcCallbackHandler::AudioDrainCallback,
		this);
	
	libvlc_video_set_format_callbacks(
		Player,
		&FVlcCallbackHandler::VideoSetupCallback,
		&FVlcCallbackHandler::VideoCleanupCallback);
	
	libvlc_video_set_callbacks(
		Player,
		&FVlcCallbackHandler::VideoLockCallback,
		&FVlcCallbackHandler::VideoUnlockCallback,
		&FVlcCallbackHandler::VideoDisplayCallback,
		this);
}

void FVlcCallbackHandler::Shutdown()
{
	if (Player == nullptr)
	{
		return;
	}

	/*libvlc_audio_set_callbacks(Player, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	libvlc_audio_set_format_callbacks(Player, nullptr, nullptr);
	
	libvlc_video_set_callbacks(Player, nullptr, nullptr, nullptr, nullptr);
	libvlc_video_set_format_callbacks(Player, nullptr, nullptr);*/
	
	AudioSamplePool->Reset();
	VideoSamplePool->Reset();

	CurrentTime = FTimespan::Zero();

	Player = nullptr;
}

int FVlcCallbackHandler::AudioSetupCallback(void** Opaque, ANSICHAR* Format, uint32* Rate, uint32* Channels)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(*Opaque);

	if (Handler == nullptr)
	{
		return -1;
	}

	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioSetupCallback (Format=%hs, Rate=%d, Channels=%d)"),
		Opaque, Format, *Rate, *Channels);

	// 没理由超过8个通道的
	if (*Channels > 8)
	{
		*Channels = 8;
	}

	if (FMemory::Memcmp(Format, "S8  ", 4) == 0)
	{
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Int8;
		Handler->AudioSampleSize = 1;
	}
	else if (FMemory::Memcmp(Format, "S16N", 4) == 0)
	{
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Int16;
		Handler->AudioSampleSize = 2;
	}
	else if (FMemory::Memcmp(Format, "S32N", 4) == 0)
	{
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Int32;
		Handler->AudioSampleSize = 4;
	}
	else if (FMemory::Memcmp(Format, "FL32", 4) == 0)
	{
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Float;
		Handler->AudioSampleSize = 4;
	}
	else if (FMemory::Memcmp(Format, "FL64", 4) == 0)
	{
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Double;
		Handler->AudioSampleSize = 8;
	}
	else if (FMemory::Memcmp(Format, "U8  ", 4) == 0)
	{
		FMemory::Memcpy(Format, "S8  ", 4);
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Int8;
		Handler->AudioSampleSize = 1;
	}
	else  // Unknown format?
	{
		FMemory::Memcpy(Format, "S16N", 4);
		Handler->AudioSampleFormat = EMediaAudioSampleFormat::Int16;
		Handler->AudioSampleSize = 2;
	}

	Handler->AudioChannels = *Channels;
	Handler->AudioSampleRate = *Rate;

	return 0;
}

void FVlcCallbackHandler::AudioCleanupCallback(void* Opaque)
{
	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioCleanupCallback"), Opaque);
}

void FVlcCallbackHandler::AudioPlayCallback(void* Opaque, const void* Buffer, uint32 Count, int64 Pts)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(Opaque);

	if (Handler == nullptr)
	{
		return;
	}

	// 丢弃严重延迟的帧（>500ms）
	const int64 DelayPts = libvlc_delay(Pts);
	if (DelayPts < -500000)
	{
		return;
	}

	const double DurationSec = static_cast<double>(Count) / Handler->AudioSampleRate;
	const int64 BufferSize = Count * Handler->AudioSampleSize * Handler->AudioChannels;

	// 使用累积时长生成无间隙的音频时间戳
	// 完全独立于 VLC 的 PTS/时钟域，避免时钟偏差造成周期性空洞
	if (Handler->bResetAudioTime)
	{
		// Seek 或重播后的首个音频帧：对齐到当前播放位置
		Handler->AccumulatedAudioTime = Handler->CurrentTime;
		Handler->bResetAudioTime = false;
	}

	const FTimespan AudioTime = Handler->AccumulatedAudioTime;
	Handler->AccumulatedAudioTime += FTimespan::FromSeconds(DurationSec);

	// 拼接到累积缓冲区
	const int32 Offset = Handler->AccumulatedData.Num();
	if (Handler->AccumulatedDurationSec == 0.0)
	{
		Handler->AccumulatedTime = AudioTime;
	}
	Handler->AccumulatedData.AddUninitialized(BufferSize);
	FMemory::Memcpy(Handler->AccumulatedData.GetData() + Offset, Buffer, BufferSize);
	Handler->AccumulatedDurationSec += DurationSec;
	Handler->AccumulatedFrames += Count;

	// 累积满 128ms 后创建大样本推送 UE
	if (Handler->AccumulatedDurationSec >= MinSampleDurationSec)
	{
		TSharedRef<FVlcAudioSample> BigSample = Handler->AudioSamplePool->AcquireShared();

		if (BigSample->Initialize(
			Handler->AccumulatedData.GetData(),
			Handler->AccumulatedData.Num(),
			Handler->AudioSampleRate,
			Handler->AudioChannels,
			Handler->AccumulatedFrames,
			Handler->AudioSampleFormat,
			Handler->AccumulatedTime,
			FTimespan::FromSeconds(Handler->AccumulatedDurationSec)))
		{
			if (Handler->Samples->NumAudioSamples() < 4)
			{
				Handler->Samples->AddAudio(BigSample);
			}
			else
			{
				Handler->AudioPacketQueue.Enqueue(BigSample);
			}
		}

		Handler->AccumulatedData.Empty();
		Handler->AccumulatedDurationSec = 0.0;
		Handler->AccumulatedFrames = 0;
	}
}

void FVlcCallbackHandler::AudioPauseCallback(void* Opaque, int64 Pts)
{
	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioPauseCallback"), Opaque);
}

void FVlcCallbackHandler::AudioResumeCallback(void* Opaque, int64 Pts)
{
	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioResumeCallback"), Opaque);
}

void FVlcCallbackHandler::AudioFlushCallback(void* Opaque, int64 Pts)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(Opaque);
	if (Handler != nullptr)
	{
		Handler->bResetAudioTime = true;
		Handler->AccumulatedData.Empty();
		Handler->AccumulatedDurationSec = 0.0;
		Handler->AccumulatedFrames = 0;
		{
			TSharedPtr<FVlcAudioSample, ESPMode::ThreadSafe> Dummy;
			while (Handler->AudioPacketQueue.Dequeue(Dummy)) {}
		}
	}
	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioFlushCallback"), Opaque);
}

void FVlcCallbackHandler::ServiceAudioQueue()
{
	TSharedPtr<FVlcAudioSample, ESPMode::ThreadSafe> Sample;
	while (Samples->NumAudioSamples() < 4 && AudioPacketQueue.Dequeue(Sample))
	{
		Samples->AddAudio(Sample.ToSharedRef());
	}
}

void FVlcCallbackHandler::AudioDrainCallback(void* Opaque)
{
	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: AudioDrainCallback"), Opaque);
}

uint32 FVlcCallbackHandler::VideoSetupCallback(void** Opaque, ANSICHAR* Chroma, uint32* Width, uint32* Height,
	uint32* Pitches, uint32* Lines)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(*Opaque);

	if (Handler == nullptr)
	{
		return 0;
	}

	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: VideoSetupCallback(Chroma=%hs, Dim=%ix%i)"), Opaque, Chroma, *Width, *Height);

	// 获取视频输出尺寸
	if (libvlc_video_get_size(
		Handler->Player, 0,
		reinterpret_cast<uint32*>(&Handler->VideoOutputDim.X),
		reinterpret_cast<uint32*>(&Handler->VideoOutputDim.Y)) != 0)
	{
		Handler->VideoOutputDim = FIntPoint::ZeroValue;
		Handler->VideoBufferDim = FIntPoint::ZeroValue;
		Handler->VideoStride = 0;

		return 0;
	}

	// 确保获取的尺寸可用
	if (Handler->VideoOutputDim.GetMin() <= 0)
	{
		return 0;
	}

	Handler->VideoBufferDim = FIntPoint(*Width, *Height);

	// https://www.zego.im/blog/604.html
	if (FCStringAnsi::Stricmp(Chroma, "AYUV") == 0)
	{
		Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharAYUV;
		Handler->VideoStride = *Width * 4;
	}
	else if (FCStringAnsi::Stricmp(Chroma, "RV32") == 0)
	{
		Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;
		Handler->VideoStride = *Width * 4;
	}
	else if ((FCStringAnsi::Stricmp(Chroma, "UYVY") == 0) ||
	(FCStringAnsi::Stricmp(Chroma, "Y422") == 0) ||
	(FCStringAnsi::Stricmp(Chroma, "UYNV") == 0) ||
	(FCStringAnsi::Stricmp(Chroma, "HDYC") == 0))
	{
		Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharUYVY;
		Handler->VideoStride = *Width * 2;
	}
	else if ((FCStringAnsi::Stricmp(Chroma, "YUY2") == 0) ||
		(FCStringAnsi::Stricmp(Chroma, "V422") == 0) ||
		(FCStringAnsi::Stricmp(Chroma, "YUYV") == 0))
	{
		Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharYUY2;
		Handler->VideoStride = *Width * 2;
	}
	else if (FCStringAnsi::Stricmp(Chroma, "YVYU") == 0)
	{
		Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharYVYU;
		Handler->VideoStride = *Width * 2;
	}
	else
	{
		// reconfigure output for natively supported format
		const vlc_chroma_description_t* ChromaDescr = vlc_fourcc_GetChromaDescription(*(vlc_fourcc_t*)Chroma);

		if (ChromaDescr->plane_count == 0)
		{
			return 0;
		}

		if (ChromaDescr->plane_count > 1)
		{
			FMemory::Memcpy(Chroma, "YUY2", 4);

			Handler->VideoBufferDim = FIntPoint(Align(Handler->VideoOutputDim.X, 16) / 2, Align(Handler->VideoOutputDim.Y, 16));
			Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharYUY2;
			Handler->VideoStride = Handler->VideoBufferDim.X * 4;
			*Height = Handler->VideoBufferDim.Y;
		}
		else
		{
			FMemory::Memcpy(Chroma, "RV32", 4);

			Handler->VideoBufferDim = Handler->VideoOutputDim;
			Handler->VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;
			Handler->VideoStride = Handler->VideoBufferDim.X * 4;
		}
	}

	//Handler->VideoFrameDuration = FTimespan::FromMilliseconds(1);
	Handler->VideoFrameDuration = FTimespan::FromSeconds(1.0 / libvlc_media_player_get_fps(Handler->Player));

	Pitches[0] = Handler->VideoStride;
	Lines[0] = Handler->VideoBufferDim.Y;

	return 1;
}

void FVlcCallbackHandler::VideoCleanupCallback(void* Opaque)
{
	
}

void* FVlcCallbackHandler::VideoLockCallback(void* Opaque, void** Planes)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(Opaque);
	check(Handler != nullptr);
	
	// Why is 5?
	FMemory::Memzero(Planes, sizeof(void*) * 5);

	if (Handler->VideoPreviousTime == Handler->CurrentTime)
	{
		Planes[0] = FMemory::Malloc(Handler->VideoStride * Handler->VideoBufferDim.Y, 32);
		return nullptr;
	}

	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: VideoSetupCallback(Current Time = %s)"),
		Opaque, *Handler->CurrentTime.ToString());

	FVlcVideoSample* VideoSample = Handler->VideoSamplePool->Acquire();

	if (VideoSample == nullptr)
	{
		Planes[0] = FMemory::Malloc(Handler->VideoStride * Handler->VideoBufferDim.Y, 32);
		return nullptr;
	}

	if (!VideoSample->Initialize(
		Handler->VideoBufferDim,
		Handler->VideoOutputDim,
		Handler->VideoSampleFormat,
		Handler->VideoStride,
		Handler->VideoFrameDuration
		))
	{
		Planes[0] = FMemory::Malloc(Handler->VideoStride * Handler->VideoBufferDim.Y, 32);
		return nullptr;
	}

	Handler->VideoPreviousTime = Handler->CurrentTime;
	Planes[0] = VideoSample->GetMutableBuffer();
	
	return VideoSample;
}

void FVlcCallbackHandler::VideoUnlockCallback(void* Opaque, void* Picture, void* const* Planes)
{
	if (Opaque != nullptr && Picture != nullptr)
	{
		UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: VideoUnlockCallback"), Opaque);
	}
	if (Picture == nullptr && Planes != nullptr && Planes[0] != nullptr)
	{
		FMemory::Free(Planes[0]);
	}
}

void FVlcCallbackHandler::VideoDisplayCallback(void* Opaque, void* Picture)
{
	FVlcCallbackHandler* Handler = static_cast<FVlcCallbackHandler*>(Opaque);
	FVlcVideoSample* VideoSample = static_cast<FVlcVideoSample*>(Picture);

	if (Handler == nullptr || VideoSample == nullptr)
	{
		return;
	}

	UE_LOG(LogVlcMedia, VeryVerbose, TEXT("Handler %p: VideoDisplayCallback (Current Time = %s, Queue = %i)"),
		Opaque, *Handler->CurrentTime.ToString(), Handler->Samples->NumVideoSamples());

	
	VideoSample->SetTime(Handler->CurrentTime);

	Handler->Samples->AddVideo(Handler->VideoSamplePool->ToShared(VideoSample));
}
