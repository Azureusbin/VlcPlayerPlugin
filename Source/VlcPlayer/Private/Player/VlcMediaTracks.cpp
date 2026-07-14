#include "VlcMediaTracks.h"

#include "CoreMinimal.h"
#include <VlcHeader.h>

#include "MediaHelpers.h"

FVlcMediaTracks::FVlcMediaTracks() : Player(nullptr)
{ }

void FVlcMediaTracks::Initialize(libvlc_media_player_t* InPlayer, FString& OutInfo)
{
	Shutdown();

	UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks Initializing %p"), this);

	Player = InPlayer;

	libvlc_media_t* Media = libvlc_media_player_get_media(Player);
	if (Media == nullptr)
	{
		UE_LOG(LogVlcMedia, Warning, TEXT("Tracks %p: No media available"), this);
		return;
	}

	// 使用 libvlc_media_tracks_get 获取详细的轨道信息（包含编解码器、采样率等）
	libvlc_media_track_t** Tracks = nullptr;
	unsigned TrackCount = libvlc_media_tracks_get(Media, &Tracks);

	if (TrackCount == 0 || Tracks == nullptr)
	{
		UE_LOG(LogVlcMedia, Warning, TEXT("Tracks %p: No tracks found"), this);
		return;
	}

	int32 StreamCount = 0;

	for (unsigned i = 0; i < TrackCount; ++i)
	{
		libvlc_media_track_t* Track = Tracks[i];
		if (Track == nullptr)
		{
			continue;
		}

		// 获取编解码器描述（如 "H.264 - MPEG-4 AVC (part 10) (avc1)"）
		const char* CodecDescr = libvlc_media_get_codec_description(Track->i_type, Track->i_codec);
		FString CodecDescription = CodecDescr ? ANSI_TO_TCHAR(CodecDescr) : TEXT("Unknown");

		if (Track->i_type == libvlc_track_audio)
		{
			FTrackInfo TrackInfo;
			{
				TrackInfo.Id = Track->i_id;
				TrackInfo.Name = Track->psz_description ? ANSI_TO_TCHAR(Track->psz_description) : TEXT("");
				TrackInfo.DisplayName = TrackInfo.Name.IsEmpty()
					? FText::FromString(FString::Format(TEXT("Audio Track {0}"), {AudioTracks.Num()}))
					: FText::FromString(TrackInfo.Name);

				// 音频格式详情
				TrackInfo.CodecDescription = CodecDescription;
				TrackInfo.BitsPerSample = 0; // libvlc 不直接提供，通过 audio setup callback 获取
				TrackInfo.NumChannels = Track->audio ? Track->audio->i_channels : 0;
				TrackInfo.SampleRate = Track->audio ? Track->audio->i_rate : 0;
				TrackInfo.VideoDim = FIntPoint::ZeroValue;
				TrackInfo.FrameRate = 0.0f;
				TrackInfo.Language = Track->psz_language ? ANSI_TO_TCHAR(Track->psz_language) : TEXT("");
			}

			AudioTracks.Add(TrackInfo);

			OutInfo += FString::Printf(TEXT("Stream %i\n"), StreamCount);
			OutInfo += TEXT("    Type: Audio\n");
			OutInfo += FString::Printf(TEXT("    Codec: %s\n"), *TrackInfo.CodecDescription);
			OutInfo += FString::Printf(TEXT("    Name: %s\n"), *TrackInfo.Name);
			OutInfo += FString::Printf(TEXT("    Channels: %u\n"), TrackInfo.NumChannels);
			OutInfo += FString::Printf(TEXT("    Sample Rate: %u Hz\n"), TrackInfo.SampleRate);
			OutInfo += FString::Printf(TEXT("    Bitrate: %u kbps\n"), Track->i_bitrate / 1000);
			OutInfo += TEXT("\n");

			++StreamCount;
		}
		else if (Track->i_type == libvlc_track_video)
		{
			FTrackInfo TrackInfo;
			{
				TrackInfo.Id = Track->i_id;
				TrackInfo.Name = Track->psz_description ? ANSI_TO_TCHAR(Track->psz_description) : TEXT("");
				TrackInfo.DisplayName = TrackInfo.Name.IsEmpty()
					? FText::FromString(FString::Format(TEXT("Video Track {0}"), {VideoTracks.Num()}))
					: FText::FromString(TrackInfo.Name);

				// 视频格式详情
				TrackInfo.CodecDescription = CodecDescription;
				TrackInfo.BitsPerSample = 0;
				TrackInfo.NumChannels = 0;
				TrackInfo.SampleRate = 0;
				TrackInfo.VideoDim = Track->video
					? FIntPoint(Track->video->i_width, Track->video->i_height)
					: FIntPoint::ZeroValue;
				TrackInfo.FrameRate = Track->video
					? (Track->video->i_frame_rate_num > 0 && Track->video->i_frame_rate_den > 0
						? static_cast<float>(Track->video->i_frame_rate_num) / static_cast<float>(Track->video->i_frame_rate_den)
						: 0.0f)
					: 0.0f;
				TrackInfo.Language = Track->psz_language ? ANSI_TO_TCHAR(Track->psz_language) : TEXT("");
			}

			VideoTracks.Add(TrackInfo);

			OutInfo += FString::Printf(TEXT("Stream %i\n"), StreamCount);
			OutInfo += TEXT("    Type: Video\n");
			OutInfo += FString::Printf(TEXT("    Codec: %s\n"), *TrackInfo.CodecDescription);
			OutInfo += FString::Printf(TEXT("    Name: %s\n"), *TrackInfo.Name);
			OutInfo += FString::Printf(TEXT("    Resolution: %ix%i\n"), TrackInfo.VideoDim.X, TrackInfo.VideoDim.Y);
			OutInfo += FString::Printf(TEXT("    Frame Rate: %.2f fps\n"), TrackInfo.FrameRate);
			OutInfo += FString::Printf(TEXT("    Bitrate: %u kbps\n"), Track->i_bitrate / 1000);
			OutInfo += TEXT("\n");

			++StreamCount;
		}
		// 跳过字幕/文本轨道（当前不处理）
	}

	libvlc_media_tracks_release(Tracks, TrackCount);

	UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks %p: Found %i streams"), this, StreamCount);
}

void FVlcMediaTracks::Shutdown()
{
	if (Player != nullptr)
	{
		AudioTracks.Reset();
		VideoTracks.Reset();
		Player = nullptr;
	}
}

void FVlcMediaTracks::AppendStats(FString& OutStats) const
{
	// Audio Tracks
	OutStats += TEXT("Audio Tracks\n");

	if (AudioTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		const int32 SelectedTrack = GetSelectedTrack(EMediaTrackType::Audio);

		for (int32 i = 0; i < AudioTracks.Num(); ++i)
		{
			const FTrackInfo& Track = AudioTracks[i];
			const bool bSelected = (i == SelectedTrack);

			OutStats += FString::Printf(TEXT("    Track %i: %s%s\n"),
				i, *Track.DisplayName.ToString(), bSelected ? TEXT(" (selected)") : TEXT(""));
			OutStats += FString::Printf(TEXT("        Codec: %s\n"), *Track.CodecDescription);
			OutStats += FString::Printf(TEXT("        Channels: %u, Sample Rate: %u Hz\n"),
				Track.NumChannels, Track.SampleRate);
			if (!Track.Language.IsEmpty())
			{
				OutStats += FString::Printf(TEXT("        Language: %s\n"), *Track.Language);
			}
		}
	}

	OutStats += TEXT("\n");

	// Video Tracks
	OutStats += TEXT("Video Tracks\n");

	if (VideoTracks.Num() == 0)
	{
		OutStats += TEXT("    none\n");
	}
	else
	{
		const int32 SelectedTrack = GetSelectedTrack(EMediaTrackType::Video);

		for (int32 i = 0; i < VideoTracks.Num(); ++i)
		{
			const FTrackInfo& Track = VideoTracks[i];
			const bool bSelected = (i == SelectedTrack);

			OutStats += FString::Printf(TEXT("    Track %i: %s%s\n"),
				i, *Track.DisplayName.ToString(), bSelected ? TEXT(" (selected)") : TEXT(""));
			OutStats += FString::Printf(TEXT("        Codec: %s\n"), *Track.CodecDescription);
			OutStats += FString::Printf(TEXT("        Resolution: %ix%i, Frame Rate: %.2f fps\n"),
				Track.VideoDim.X, Track.VideoDim.Y, Track.FrameRate);
		}
	}
}

bool FVlcMediaTracks::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (!AudioTracks.IsValidIndex(TrackIndex) || (FormatIndex != 0))
	{
		return false;
	}

	const FTrackInfo& Track = AudioTracks[TrackIndex];

	OutFormat.BitsPerSample = Track.BitsPerSample;
	OutFormat.NumChannels = Track.NumChannels;
	OutFormat.SampleRate = Track.SampleRate;
	OutFormat.TypeName = Track.CodecDescription.IsEmpty() ? TEXT("Audio") : Track.CodecDescription;

	return true;
}

int32 FVlcMediaTracks::GetNumTracks(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.Num();
	case EMediaTrackType::Video:
		return VideoTracks.Num();
	default:
		return 0;
	}
}

int32 FVlcMediaTracks::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TrackIndex >= 0 && GetNumTracks(TrackType) > TrackIndex ? 1 : 0;
}

int32 FVlcMediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if (Player == nullptr)
	{
		return INDEX_NONE;
	}

	const TArray<FTrackInfo>* CurrentTrack = nullptr;
	int32 TrackId = INDEX_NONE;

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		CurrentTrack = &AudioTracks;
		TrackId = libvlc_audio_get_track(Player);
		break;
	case EMediaTrackType::Video:
		CurrentTrack = &VideoTracks;
		TrackId = libvlc_video_get_track(Player);
		break;
	}

	if (TrackId != INDEX_NONE)
	{
		for (int32 Index = 0; Index < CurrentTrack->Num(); ++Index)
		{
			if ((*CurrentTrack)[Index].Id == TrackId)
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

FText FVlcMediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	auto GetDisplayName = [](const TArray<FTrackInfo>& Track, int32 TrackIndex)
	{
		if (Track.IsValidIndex(TrackIndex))
		{
			return Track[TrackIndex].DisplayName;
		}

		return FText::GetEmpty();
	};

	FText DisplayName = FText::GetEmpty();
	
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		DisplayName = GetDisplayName(AudioTracks, TrackIndex);
		break;

	case EMediaTrackType::Video:
		DisplayName = GetDisplayName(VideoTracks, TrackIndex);
		break;
	}

	return DisplayName;
}

int32 FVlcMediaTracks::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return 0;
}

FString FVlcMediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	auto GetLanguage = [](const TArray<FTrackInfo>& Track, int32 TrackIndex)
	{
		if (Track.IsValidIndex(TrackIndex))
		{
			return Track[TrackIndex].Language;
		}

		return FString();
	};

	FString Language;

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		Language = GetLanguage(AudioTracks, TrackIndex);
		break;

	case EMediaTrackType::Video:
		Language = GetLanguage(VideoTracks, TrackIndex);
		break;
	}

	return Language.IsEmpty() ? TEXT("und") : Language;
}

FString FVlcMediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	auto GetName = [](const TArray<FTrackInfo>& Track, int32 TrackIndex)
	{
		if (Track.IsValidIndex(TrackIndex))
		{
			return Track[TrackIndex].Name;
		}

		return FString();
	};

	FString TrackName;
	
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		TrackName = GetName(AudioTracks, TrackIndex);
		break;

	case EMediaTrackType::Video:
		TrackName = GetName(VideoTracks, TrackIndex);
		break;
	}

	return TrackName;
}

bool FVlcMediaTracks::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (!VideoTracks.IsValidIndex(TrackIndex) || (FormatIndex != 0))
	{
		return false;
	}

	const FTrackInfo& Track = VideoTracks[TrackIndex];

	OutFormat.Dim = Track.VideoDim;
	OutFormat.FrameRate = Track.FrameRate;
	OutFormat.FrameRates = TRange<float>(OutFormat.FrameRate);
	OutFormat.TypeName = Track.CodecDescription.IsEmpty() ? TEXT("Video") : Track.CodecDescription;

	return true;
}

bool FVlcMediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (Player == nullptr)
	{
		return false; // not initialized
	}

	UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks %p: Selecting %s track %i"), this, *MediaUtils::TrackTypeToString(TrackType), TrackIndex);

	int32 TrackId = INDEX_NONE;

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			TrackId = AudioTracks[TrackIndex].Id;
		}
		else if (TrackIndex == INDEX_NONE)
		{
			TrackId = -1;
		}
		else
		{
			return false; // invalid track
		}

		if (libvlc_audio_set_track(Player, TrackId) != 0)
		{
			UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks %p: Failed to %s audio track %i (id %i)"), this, (TrackId == -1) ? TEXT("disable") : TEXT("enable"), TrackIndex, TrackId);
			return false;
		}
		break;

	case EMediaTrackType::Video:
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			TrackId = VideoTracks[TrackIndex].Id;
		}
		else if (TrackIndex == INDEX_NONE)
		{
			TrackId = -1;
		}
		else
		{
			return false; // invalid track
		}

		if ((TrackId != -1) && (libvlc_video_set_track(Player, -1) != 0))
		{
			UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks %p: Failed to disable video decoding"), this);
			return false;
		}

		if (libvlc_video_set_track(Player, TrackId) != 0)
		{
			UE_LOG(LogVlcMedia, Verbose, TEXT("Tracks %p: Failed to %s video track %i (id %i)"), this, (TrackId == -1) ? TEXT("disable") : TEXT("enable"), TrackIndex, TrackId);
			return false;
		}
		break;
		
	default:
		return false; // unsupported track type
	}

	return true;
}

bool FVlcMediaTracks::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	if (Player == nullptr || FormatIndex != 0)
	{
		return false;
	}

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.IsValidIndex(TrackIndex);
	
	case EMediaTrackType::Video:
		return VideoTracks.IsValidIndex(TrackIndex);

	default:
		return false;
	}
}


