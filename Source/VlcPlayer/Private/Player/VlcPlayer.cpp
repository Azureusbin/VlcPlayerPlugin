#include "VlcPlayer.h"

#include "IMediaOptions.h"
#include "IMediaSamples.h"
#include "VlcCallbackHandler.h"
#include "VlcMediaMonitor.h"
#include "Misc/FileHelper.h"
#include "Serialization/ArrayReader.h"

FVlcPlayer::FVlcPlayer(IMediaEventSink& InEventSink, libvlc_instance_t* InVlcInstance)
	  : MediaSource(InVlcInstance)
	  , EventSink(InEventSink)
	  , CurrentRate(0)
	  , CurrentTime(0)
	  , bLooping(false)
		  , DesiredRate(1.0f)
	  , VlcInstance(InVlcInstance)
	  , Player(nullptr)
{
	GetVlcPlayerRegistry().Add(this);
}

FVlcPlayer::~FVlcPlayer()
{
	GetVlcPlayerRegistry().Remove(this);
	Close();
}

void FVlcPlayer::Close()
{
	if (Player == nullptr)
	{
		return;
	}

	CallbackHandler.Shutdown();
	MediaTracks.Shutdown();

	libvlc_media_player_stop(Player);
	libvlc_media_player_release(Player);
	Player = nullptr;

	CurrentRate = 0.0f;
	DesiredRate = 0.0f;
	CurrentTime = FTimespan::Zero();
	MediaSource.Close();
	Info.Empty();

	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}

IMediaCache& FVlcPlayer::GetCache()
{
	return *this;
}

IMediaControls& FVlcPlayer::GetControls()
{
	return *this;
}

FString FVlcPlayer::GetInfo() const
{
	return Info;
}

FGuid FVlcPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(TEXT("Vlc Plugin - Azb"));
	return PlayerPluginGUID;
}

IMediaSamples& FVlcPlayer::GetSamples()
{
	return CallbackHandler.GetSamples();
}

FString FVlcPlayer::GetStats() const
{
	FString StatsString;

	// === General ===
	{
		StatsString += TEXT("General\n");
		StatsString += FString::Printf(TEXT("    URL: %s\n"), *GetUrl());

		const EMediaState State = GetState();
		const TCHAR* StateStr = TEXT("Unknown");
		switch (State)
		{
		case EMediaState::Closed:    StateStr = TEXT("Closed"); break;
		case EMediaState::Preparing: StateStr = TEXT("Preparing"); break;
		case EMediaState::Playing:   StateStr = TEXT("Playing"); break;
		case EMediaState::Paused:    StateStr = TEXT("Paused"); break;
		case EMediaState::Stopped:   StateStr = TEXT("Stopped"); break;
		case EMediaState::Error:     StateStr = TEXT("Error"); break;
		}
		StatsString += FString::Printf(TEXT("    State: %s\n"), StateStr);
		StatsString += FString::Printf(TEXT("    Time: %s / %s\n"), *GetTime().ToString(), *GetDuration().ToString());
		StatsString += FString::Printf(TEXT("    Rate: %.2fx (Desired: %.2fx)\n"), GetRate(), DesiredRate);
		StatsString += FString::Printf(TEXT("    Looping: %s\n"), IsLooping() ? TEXT("Yes") : TEXT("No"));
		StatsString += FString::Printf(TEXT("    VLC: %s\n"), ANSI_TO_TCHAR(libvlc_get_version()));
		StatsString += TEXT("\n");
	}

	// === Tracks ===
	MediaTracks.AppendStats(StatsString);

	// === Performance (VLC Internal) ===
	libvlc_media_t* Media = MediaSource.GetMedia();
	if (Media)
	{
		libvlc_media_stats_t Stats;
		if (libvlc_media_get_stats(Media, &Stats))
		{
			StatsString += TEXT("\n");
			StatsString += TEXT("Performance (VLC Internal)\n");
			StatsString += FString::Printf(TEXT("    Decoded Video: %i\n"), Stats.i_decoded_video);
			StatsString += FString::Printf(TEXT("    Displayed Pictures: %i\n"), Stats.i_displayed_pictures);
			StatsString += FString::Printf(TEXT("    Lost Pictures: %i\n"), Stats.i_lost_pictures);
			StatsString += FString::Printf(TEXT("    Decoded Audio: %i\n"), Stats.i_decoded_audio);
			StatsString += FString::Printf(TEXT("    Played A-Buffers: %i\n"), Stats.i_played_abuffers);
			StatsString += FString::Printf(TEXT("    Lost A-Buffers: %i\n"), Stats.i_lost_abuffers);

			StatsString += FString::Printf(TEXT("    Input Bitrate: %.2f kbps\n"), Stats.f_input_bitrate * 0.001f);
			StatsString += FString::Printf(TEXT("    Input Bytes Read: %.2f MB\n"), Stats.i_read_bytes / (1024.0f * 1024.0f));

			StatsString += FString::Printf(TEXT("    Demux Bitrate: %.2f kbps\n"), Stats.f_demux_bitrate * 0.001f);
			StatsString += FString::Printf(TEXT("    Demux Corrupted: %i\n"), Stats.i_demux_corrupted);
			StatsString += FString::Printf(TEXT("    Demux Discontinuity: %i\n"), Stats.i_demux_discontinuity);

			// Stream output only shown when data was actually sent
			if (Stats.i_sent_bytes > 0 || Stats.i_sent_packets > 0)
			{
				StatsString += TEXT("\n");
				StatsString += TEXT("    Stream Output\n");
				StatsString += FString::Printf(TEXT("    Sent Packets: %i\n"), Stats.i_sent_packets);
				StatsString += FString::Printf(TEXT("    Sent Bytes: %.2f MB\n"), Stats.i_sent_bytes / (1024.0f * 1024.0f));
				StatsString += FString::Printf(TEXT("    Send Bitrate: %.2f kbps\n"), Stats.f_send_bitrate * 0.001f);
			}
		}
		else
		{
			StatsString += TEXT("\n");
			StatsString += TEXT("Performance (VLC Internal): Not available\n");
		}
	}

	return StatsString;
}

IMediaTracks& FVlcPlayer::GetTracks()
{
	return MediaTracks;
}

FString FVlcPlayer::GetUrl() const
{
	return MediaSource.GetUrl();
}

IMediaView& FVlcPlayer::GetView()
{
	return *this;
}

bool FVlcPlayer::InitializePlayer()
{
	Player = libvlc_media_player_new_from_media(MediaSource.GetMedia());
	if (!Player)
	{
		const FString Reason(libvlc_errmsg());
		UE_LOG(LogVlcMedia, Error, TEXT("无法解析文件：%s, 原因:%s"), *MediaSource.GetUrl(), *Reason);
		return false;
	}

	// 绑定事件
	libvlc_event_manager_t* MediaEventManager = libvlc_media_event_manager(MediaSource.GetMedia());
	libvlc_event_manager_t* PlayerEventManager = libvlc_media_player_event_manager(Player);

	if ((MediaEventManager == nullptr) || (PlayerEventManager == nullptr))
	{
		libvlc_media_player_release(Player);
		Player = nullptr;

		UE_LOG(LogVlcMedia, Error, TEXT("无法播放文件：%s, 原因：无法创建事件管理器"), *MediaSource.GetUrl());

		return false;
	}

	libvlc_event_attach(MediaEventManager, libvlc_MediaParsedChanged, &FVlcPlayer::StaticEventCallback, this);
	libvlc_event_attach(PlayerEventManager, libvlc_MediaPlayerEndReached, &FVlcPlayer::StaticEventCallback, this);
	libvlc_event_attach(PlayerEventManager, libvlc_MediaPlayerPlaying, &FVlcPlayer::StaticEventCallback, this);
	libvlc_event_attach(PlayerEventManager, libvlc_MediaPlayerPositionChanged, &FVlcPlayer::StaticEventCallback, this);
	libvlc_event_attach(PlayerEventManager, libvlc_MediaPlayerStopped, &FVlcPlayer::StaticEventCallback, this);

	CurrentRate = 0.0f;
	DesiredRate = 0.0f;
	CurrentTime = FTimespan::Zero();

	EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);

	UE_LOG(LogVlcMedia, Display, TEXT("准备播放：%s"), *MediaSource.GetUrl());

	CallbackHandler.Initialize(Player);

	return true;
}

bool FVlcPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if ((Url.IsEmpty()))
	{
		return false;
	}

	if (Url.StartsWith(TEXT("file://")))
	{
		TSharedPtr<FArchive, ESPMode::ThreadSafe> Archive;
		const TCHAR* FilePath = &Url[7];

		if ((Options != nullptr) && Options->GetMediaOption("PrecacheFile", false))
		{
			FArrayReader* Reader = new FArrayReader;

			if (FFileHelper::LoadFileToArray(*Reader, FilePath))
			{
				Archive = MakeShareable(Reader);
			}
			else
			{
				delete Reader;
			}
		}
		else
		{
			Archive = MakeShareable(IFileManager::Get().CreateFileReader(FilePath));
		}

		if (Archive.IsValid())
		{
			MediaSource.OpenArchive(Archive.ToSharedRef(), Url);
		}
	}
	else
	{
		MediaSource.OpenUrl(Url);
	}

	if (!MediaSource.GetMedia())
	{
		const FString Reason(libvlc_errmsg());
		UE_LOG(LogVlcMedia, Error, TEXT("无法打开文件：%s, 原因%s"), *Url, *Reason);
		return false;
	}

	return InitializePlayer();
}

bool FVlcPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl,
	const IMediaOptions* Options)
{
	Close();

	if (OriginalUrl.IsEmpty() || !MediaSource.OpenArchive(Archive, OriginalUrl))
	{
		return false;
	}

	return InitializePlayer();
}

void FVlcPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	if (Player == nullptr)
	{
		return;
	}

	libvlc_event_e Event;

	while (Events.Dequeue(Event))
	{
		switch (Event)
		{
		case libvlc_MediaParsedChanged:
			MediaTracks.Initialize(Player, Info);
			EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
			break;

		case libvlc_MediaPlayerEndReached:
			libvlc_media_player_stop(Player);

			CallbackHandler.ClearStartTime();
			CallbackHandler.GetSamples().FlushSamples();
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);

			if (bLooping && !FMath::IsNearlyZero(DesiredRate))
			{
				CurrentTime = FTimespan::Zero();
				SetRate(DesiredRate);
			}
			else
			{
				EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
			}
			break;

		case libvlc_MediaPlayerPaused:
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
			break;

		case libvlc_MediaPlayerPlaying:
			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
			break;
		}
	}

	const libvlc_state_t State = libvlc_media_player_get_state(Player);

	// update current time & rate
	if (State == libvlc_Playing)
	{
		CurrentRate = libvlc_media_player_get_rate(Player);
		// 编辑器"暂停"(SetRate(0))时不真正暂停 VLC，而是在此冻结时间
		if (!FMath::IsNearlyZero(DesiredRate))
		{
			CurrentTime += DeltaTime * CurrentRate;
		}
	}
	else
	{
		CurrentRate = 0.0f;
	}

	CallbackHandler.ServiceAudioQueue();
	CallbackHandler.SetCurrentTime(CurrentTime);
}

bool FVlcPlayer::GetPlayerFeatureFlag(EFeatureFlag FeatureFlag) const
{
	switch(FeatureFlag)
	{
	case EFeatureFlag::PlayerSelectsDefaultTracks:
		return true;
	default:
		break;
	}
	return IMediaPlayer::GetPlayerFeatureFlag(FeatureFlag);
}

bool FVlcPlayer::CanControl(EMediaControl Control) const
{
	if (Player == nullptr)
	{
		return false;
	}

	if (Control == EMediaControl::Pause)
	{
		// 模拟暂停：DesiredRate 为 0 表示已暂停，不可再暂停
		if (FMath::IsNearlyZero(DesiredRate))
		{
			return false;
		}
		return libvlc_media_player_can_pause(Player) != 0;
	}

	if (Control == EMediaControl::Resume)
	{
		// 模拟暂停：DesiredRate 为 0 时可以恢复
		if (FMath::IsNearlyZero(DesiredRate))
		{
			return true;
		}
		return (libvlc_media_player_get_state(Player) != libvlc_Playing);
	}

	if (Control == EMediaControl::Seek || Control == EMediaControl::Scrub)
	{
		return true;
	}

	return false;
}

FTimespan FVlcPlayer::GetDuration() const
{
	return MediaSource.GetDuration();
}

float FVlcPlayer::GetRate() const
{
	// 模拟暂停：DesiredRate 为 0 时对外报告速率为 0
	if (FMath::IsNearlyZero(DesiredRate))
	{
		return 0.0f;
	}
	return CurrentRate;
}

EMediaState FVlcPlayer::GetState() const
{
	if (Player == nullptr)
	{
		return EMediaState::Closed;
	}

	// 模拟暂停：SetRate(0) 不真正暂停 VLC（以保持画面刷新用于拖动进度条），
	// 但对外需要报告 Paused 状态，否则 UI 无法正确切换暂停/播放按钮。
	if (FMath::IsNearlyZero(DesiredRate))
	{
		return EMediaState::Paused;
	}

	const libvlc_state_t CurrentState = libvlc_media_player_get_state(Player);

	switch (CurrentState)
	{
	case libvlc_Opening:
	case libvlc_Buffering:
		return EMediaState::Preparing;

	case libvlc_Playing:
		return EMediaState::Playing;

	case libvlc_Paused:
		return EMediaState::Paused;

	case libvlc_NothingSpecial:
	case libvlc_Stopped:
	case libvlc_Ended:
		return EMediaState::Stopped;

	case libvlc_Error:
		return EMediaState::Error;
	}

	return EMediaState::Error;
}

EMediaStatus FVlcPlayer::GetStatus() const
{
	return GetState() == EMediaState::Preparing ? EMediaStatus::Buffering : EMediaStatus::None;
}

TRangeSet<float> FVlcPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	TRangeSet<float> Result;

	if (Thinning == EMediaRateThinning::Thinned)
	{
		Result.Add(TRange<float>::Inclusive(0.0f, 10.0f));
	}
	else
	{
		Result.Add(TRange<float>::Inclusive(0.0f, 1.0f));
	}

	return Result;
}

FTimespan FVlcPlayer::GetTime() const
{
	return CurrentTime;
}

bool FVlcPlayer::IsLooping() const
{
	return bLooping;
}

bool FVlcPlayer::Seek(const FTimespan& Time)
{
	if (Player == nullptr)
	{
		return false;
	}

	if (Time != CurrentTime)
	{
		libvlc_media_player_set_time(Player, Time.GetTotalMilliseconds());
		CurrentTime = Time;
	}

	EventSink.ReceiveMediaEvent(EMediaEvent::SeekCompleted);
	return true;
}

bool FVlcPlayer::Seek(const FTimespan& Time, const FMediaSeekParams& SeekParams)
{
	// 返回 false 会导致 Facade 跳过 flush，所以在此自己清理管线
	CallbackHandler.ClearStartTime();
	CallbackHandler.GetSamples().FlushSamples();
	Seek(Time);
	return false;
}

bool FVlcPlayer::SetLooping(bool Looping)
{
	bLooping = Looping;
	return true;
}

bool FVlcPlayer::SetRate(float Rate)
{
	if (Player == nullptr)
		return false;

	const bool bWasPaused = FMath::IsNearlyZero(DesiredRate);
	DesiredRate = Rate;

	if (FMath::IsNearlyZero(Rate))
	{
		// 不暂停 VLC：拖动进度条时需要 VLC 保持活跃以刷新画面。
		// 时间冻结在 TickInput 中通过 DesiredRate 控制。
		libvlc_audio_set_mute(Player, 1);
	}
	else
	{
		libvlc_audio_set_mute(Player, 0);

		// Seek back: VLC kept playing internally; jump to frozen time
		if (bWasPaused)
		{
			CallbackHandler.ClearStartTime();
			CallbackHandler.GetSamples().FlushSamples();
			libvlc_media_player_set_time(Player, CurrentTime.GetTotalMilliseconds());
		}

		if (libvlc_media_player_set_rate(Player, Rate) == -1)
			return false;

		if (libvlc_media_player_get_state(Player) != libvlc_Playing)
		{
			if (libvlc_media_player_play(Player) == -1)
				return false;
		}
	}

	return true;
}

void FVlcPlayer::StaticEventCallback(const libvlc_event_t* Event, void* UserData)
{
	if (Event == nullptr)
	{
		return;
	}

	UE_LOG(LogVlcMedia, Verbose, TEXT("Player %p: Event [%s]"),
		UserData,
		FUTF8ToTCHAR(libvlc_event_type_name(Event->type)).Get());

	if (UserData != nullptr)
	{
		static_cast<FVlcPlayer*>(UserData)->Events.Enqueue(static_cast<libvlc_event_e>(Event->type));
	}
}