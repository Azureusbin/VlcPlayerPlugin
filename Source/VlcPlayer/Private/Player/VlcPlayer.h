#pragma once

#include "CoreMinimal.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaEventSink.h"
#include "IMediaView.h"
#include "VlcCallbackHandler.h"
#include "VlcMediaTracks.h"
#include "VlcPlayerSource.h"

#include <VlcHeader.h>

class FVlcPlayer :
	public IMediaPlayer,
	public IMediaCache,
	public IMediaControls,
	public IMediaView
{
public:
	FVlcPlayer(IMediaEventSink& InEventSink, libvlc_instance_t* InVlcInstance);
	virtual ~FVlcPlayer();
	
	// ~IMediaPlayer Interface
	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FGuid GetPlayerPluginGUID() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	bool InitializePlayer();
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl,
		const IMediaOptions* Options) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual bool GetPlayerFeatureFlag(EFeatureFlag) const override;

	// ~IMediaControls Interface
	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool Seek(const FTimespan& Time, const FMediaSeekParams& SeekParams) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	
	static void StaticEventCallback(const struct libvlc_event_t* P_Event, void* P_Data);

	/** Access to the underlying media source (for stats / monitoring). */
	const FVlcPlayerSource& GetMediaSource() const { return MediaSource; }

private:
	
	FVlcMediaTracks MediaTracks;
	FVlcPlayerSource MediaSource;
	FVlcCallbackHandler CallbackHandler;

	IMediaEventSink& EventSink;

	TQueue<libvlc_event_e, EQueueMode::Mpsc> Events;

	float CurrentRate;
	FTimespan CurrentTime;
	uint8 bLooping : 1;
	float DesiredRate;

	// VLC
	libvlc_instance_t* VlcInstance;
	libvlc_media_player_t* Player;

	FString Info;
};
