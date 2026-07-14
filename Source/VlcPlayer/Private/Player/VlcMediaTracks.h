#pragma once

#include "CoreTypes.h"
#include "IMediaTracks.h"

struct libvlc_media_player_t;

class FVlcMediaTracks : public IMediaTracks
{
public:
	struct FTrackInfo
	{
		FText DisplayName;
		int32 Id;
		FString Name;

		// Format details
		FString CodecDescription;
		uint32 BitsPerSample;
		uint32 NumChannels;
		uint32 SampleRate;
		FIntPoint VideoDim;
		float FrameRate;
		FString Language;
	};

	FVlcMediaTracks();

	void Initialize(libvlc_media_player_t* InPlayer, FString& OutInfo);

	void Shutdown();

	void AppendStats(FString& OutStats) const;

protected:
	// ~IMediaTracks Interface
	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:
	TArray<FTrackInfo> AudioTracks;
	TArray<FTrackInfo> VideoTracks;
	libvlc_media_player_t* Player;
};
