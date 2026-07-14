#include "VlcPlayerSource.h"

#include <VlcHeader.h>

libvlc_media_t* FVlcPlayerSource::OpenUrl(const FString& Url)
{
	Media = libvlc_media_new_location(VlcInstance, TCHAR_TO_UTF8(*Url));
	
	if (Media == nullptr)
	{
		const FString Reason(libvlc_errmsg());
		UE_LOG(LogVlcMedia, Error, TEXT("无法打开URL：%s, 原因%s"), *Url, *Reason);
	}
	else
	{
		CurrentUrl = Url;
		libvlc_media_add_option(Media, ":clock-synchro=0");
	}

	return Media;
}

libvlc_media_t* FVlcPlayerSource::OpenArchive(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive,
	const FString& OriginalUrl)
{
	check(Media == nullptr);

	if (Archive->TotalSize() > 0)
	{
		Data = Archive;
		Media = libvlc_media_new_callbacks(
			VlcInstance,
			nullptr,
			&FVlcPlayerSource::HandleMediaRead,
			&FVlcPlayerSource::HandleMediaSeek,
			&FVlcPlayerSource::HandleMediaClose,
			this
			);

		if (Media == nullptr)
		{
			const FString Reason(libvlc_errmsg());
			UE_LOG(LogVlcMedia, Error, TEXT("无法打开URL：%s, 原因%s"), *OriginalUrl, *Reason);
			Data.Reset();
		}
		else
		{
			CurrentUrl = OriginalUrl;
			libvlc_media_add_option(Media, ":clock-synchro=0");
		}
	}

	return Media;
}

void FVlcPlayerSource::Close()
{
	if (Media != nullptr)
	{
		libvlc_media_release(Media);
		Media = nullptr;
	}

	Data.Reset();
	CurrentUrl.Reset();
}

FTimespan FVlcPlayerSource::GetDuration() const
{
	if (Media == nullptr)
	{
		return FTimespan::Zero();
	}

	int64 Duration = libvlc_media_get_duration(Media);

	if (Duration < 0)
	{
		return FTimespan::Zero();
	}

	return FTimespan(Duration * ETimespan::TicksPerMillisecond);
}

int32 FVlcPlayerSource::HandleMediaOpen(void* Opaque, void** OutData, uint64* OutSize)
{
	auto PlayerSource = static_cast<FVlcPlayerSource*>(Opaque);

	if (PlayerSource == nullptr || !PlayerSource->Data.IsValid())
		return 0;

	*OutSize = PlayerSource->Data->TotalSize();

	return 0;
}

int64 FVlcPlayerSource::HandleMediaRead(void* Opaque, uint8* Buffer, uint64 Length)
{
	auto PlayerSource = static_cast<FVlcPlayerSource*>(Opaque);

	if (PlayerSource != nullptr && PlayerSource->Data.IsValid())
	{
		uint64 TotalSize = PlayerSource->Data->TotalSize();
		uint64 BytesToRead = FMath::Min(Length, TotalSize);
		uint64 DataPos = PlayerSource->Data->Tell();

		// 读取长度超过文件总长度的数据，对读取的数据进行长度调整
		if (DataPos + BytesToRead > TotalSize)
		{
			BytesToRead = TotalSize - DataPos;
		}

		if (BytesToRead > 0)
		{
			PlayerSource->Data->Serialize(Buffer, BytesToRead);
		}

		return static_cast<int64>(BytesToRead);
	}

	return -1;
}

int32 FVlcPlayerSource::HandleMediaSeek(void* Opaque, uint64 Offset)
{
	auto PlayerSource = static_cast<FVlcPlayerSource*>(Opaque);

	if (PlayerSource != nullptr && PlayerSource->Data.IsValid())
	{
		if (Offset >= static_cast<uint64>(PlayerSource->Data->TotalSize()))
			return -1;
		
		PlayerSource->Data->Seek(Offset);
		
		return 0;
	}

	return -1;
}

void FVlcPlayerSource::HandleMediaClose(void* Opaque)
{
	auto PlayerSource = static_cast<FVlcPlayerSource*>(Opaque);
	
	if (PlayerSource != nullptr)
	{
		PlayerSource->Data->Seek(0);
	}
}
