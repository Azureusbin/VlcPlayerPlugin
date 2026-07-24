
#include "VlcPlayerModule.h"

#include "ISettingsModule.h"
#include "VlcMediaSettings.h"
#include "Player/VlcPlayer.h"
#include "VlcMediaMonitor.h"

#include "VlcHeader.h"

DEFINE_LOG_CATEGORY(LogVlcMedia)

#define LOCTEXT_NAMESPACE "FVlcPlayerPluginsModule"

static void VlcDebugCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args)
{
	TAnsiStringBuilder<256> VlcOutput;
	VlcOutput.AppendV(fmt, args);
	
	if (level == LIBVLC_WARNING)
	{
		UE_LOG(LogVlcMedia, Warning, TEXT("%hs"), *VlcOutput);
	}
	else if (level == LIBVLC_ERROR)
	{
		UE_LOG(LogVlcMedia, Error, TEXT("%hs"), *VlcOutput);
	}
	else if (level == LIBVLC_NOTICE)
	{
		UE_LOG(LogVlcMedia, Verbose, TEXT("%hs"), *VlcOutput);
	}
	else if (level == LIBVLC_DEBUG)
	{
		UE_LOG(LogVlcMedia, Verbose, TEXT("%hs"), *VlcOutput);
	}
	else
	{
		UE_LOG(LogVlcMedia, Log, TEXT("%hs"), *VlcOutput);
	}
}

void FVlcPlayerModule::StartupModule()
{
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		return;
	}
	
	UVlcSettings* Settings = GetMutableDefault<UVlcSettings>();

#if WITH_EDITOR
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "VlcMediaPlayer",
			FText::FromString(TEXT("Vlc Media Player")),
			FText::FromString(TEXT("配置VLC播放器插件")),
			Settings);
	}
#endif

	const FString DiscCaching = FString::Printf(TEXT("--disc-caching=%i"), static_cast<int32>(Settings->DiscCaching.GetTotalMilliseconds()));
	const FString FileCaching = FString::Printf(TEXT("--file-caching=%i"), static_cast<int32>(Settings->FileCaching.GetTotalMilliseconds()));
	const FString LiveCaching = FString::Printf(TEXT("--live-caching=%i"), static_cast<int32>(Settings->LiveCaching.GetTotalMilliseconds()));
	const FString NetworkCaching = FString::Printf(TEXT("--network-caching=%i"), static_cast<int32>(Settings->NetworkCaching.GetTotalMilliseconds()));
	
	const ANSICHAR* Args[] =
	{
		TCHAR_TO_ANSI(*DiscCaching),
		TCHAR_TO_ANSI(*FileCaching),
		TCHAR_TO_ANSI(*LiveCaching),
		TCHAR_TO_ANSI(*NetworkCaching),

		// config
		"--ignore-config",
		
		// output
		"--aout", "amem",
		"--intf", "dummy",
		"--text-renderer", "dummy",
		"--vout", "vmem",

		// performance
		"--drop-late-frames",

		// undesired features
		"--no-disable-screensaver",
		"--no-plugins-cache",
		"--no-snapshot-preview",
		"--no-video-title-show",
	};

	int32 Argc = sizeof(Args) / sizeof(ANSICHAR*);
	VlcInstance = libvlc_new(Argc, Args);

	if (VlcInstance == nullptr)
	{
		UE_LOG(LogVlcMedia, Fatal, TEXT("Failed to create VLC instance (%hs)"), libvlc_errmsg());
	}
	
	libvlc_log_set(VlcInstance, VlcDebugCallback, nullptr);
	libvlc_set_log_verbosity(VlcInstance, LIBVLC_DEBUG);

	// Register real-time stats overlay (Runtime-safe, uses UDebugDrawService)
	FVlcMediaMonitor::Start();
}

void FVlcPlayerModule::ShutdownModule()
{
	FVlcMediaMonitor::Stop();

	if (VlcInstance != nullptr)
	{
		libvlc_log_unset(VlcInstance);
		libvlc_release(VlcInstance);
		VlcInstance = nullptr;
	}

	WSACleanup();
}

TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> FVlcPlayerModule::CreatePlayer(IMediaEventSink& EventSink)
{
	return MakeShared<FVlcPlayer>(EventSink, VlcInstance);
}
#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FVlcPlayerModule, VlcPlayer)