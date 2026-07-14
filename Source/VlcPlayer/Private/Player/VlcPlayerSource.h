#pragma once

#include <CoreMinimal.h>

struct libvlc_media_t;
struct libvlc_instance_t;

class FVlcPlayerSource
{
public:
	explicit FVlcPlayerSource(libvlc_instance_t* InVlcInstance)
	: VlcInstance(InVlcInstance)
	, Media(nullptr)
{	}

	libvlc_media_t* OpenUrl(const FString& Url);
	libvlc_media_t* OpenArchive(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl);
	void Close();
	
	libvlc_media_t* GetMedia() const {return Media;}
	FString GetUrl() const { return CurrentUrl; }
	FTimespan GetDuration() const;

	static int32 HandleMediaOpen(void *Opaque, void **OutData, uint64 *OutSize);
	static int64 HandleMediaRead(void *Opaque, uint8* Buffer, uint64 Length);
	static int32 HandleMediaSeek(void *Opaque, uint64 Offset);
	static void HandleMediaClose(void *Opaque);
	
private:
	TSharedPtr<FArchive, ESPMode::ThreadSafe> Data;
	
	FString CurrentUrl;
	libvlc_instance_t* VlcInstance;
	libvlc_media_t* Media;
};
