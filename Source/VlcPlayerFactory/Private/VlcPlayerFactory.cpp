#include "VlcPlayerFactory.h"

#include "IMediaModule.h"
#include "VlcPlayerModule.h"

#define LOCTEXT_NAMESPACE "FVlcPlayerFactoryModule"

void FVlcPlayerFactoryModule::StartupModule()
{
	// supported file extensions
	SupportedFileExtensions.Add(TEXT("mp4"));
	SupportedFileExtensions.Add(TEXT("avi"));
	SupportedFileExtensions.Add(TEXT("bik"));
	SupportedFileExtensions.Add(TEXT("flv"));
	SupportedFileExtensions.Add(TEXT("f4v"));
	SupportedFileExtensions.Add(TEXT("m4v"));
	SupportedFileExtensions.Add(TEXT("mkv"));
	SupportedFileExtensions.Add(TEXT("mov"));
	SupportedFileExtensions.Add(TEXT("wmv"));
	SupportedFileExtensions.Add(TEXT("webm"));

	// supported platforms
	SupportedPlatforms.Add(TEXT("Linux"));
	SupportedPlatforms.Add(TEXT("Mac"));
	SupportedPlatforms.Add(TEXT("Windows"));

	// supported schemes
	SupportedUriSchemes.Add(TEXT("file"));
	SupportedUriSchemes.Add(TEXT("ftp"));
	SupportedUriSchemes.Add(TEXT("http"));
	SupportedUriSchemes.Add(TEXT("https"));
	SupportedUriSchemes.Add(TEXT("rtmp"));
	SupportedUriSchemes.Add(TEXT("rtp"));
	SupportedUriSchemes.Add(TEXT("rtsp"));
	
	// register media player info
	auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->RegisterPlayerFactory(*this);
	}
}

void FVlcPlayerFactoryModule::ShutdownModule()
{
	// unregister player factory
	auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->UnregisterPlayerFactory(*this);
	}
}

bool FVlcPlayerFactoryModule::CanPlayUrl(const FString& Url, const IMediaOptions* Options, TArray<FText>* OutWarnings,
	TArray<FText>* OutErrors) const
{
	return GetPlayabilityConfidenceScore(Url, Options, OutWarnings, OutErrors) > 0;
}

TSharedPtr<IMediaPlayer> FVlcPlayerFactoryModule::CreatePlayer(IMediaEventSink& EventSink)
{
	FVlcPlayerModule* VlcPlayerModule = FModuleManager::LoadModulePtr<FVlcPlayerModule>("VlcPlayer");
	return (VlcPlayerModule != nullptr) ? VlcPlayerModule->CreatePlayer(EventSink) : nullptr;
}

FText FVlcPlayerFactoryModule::GetDisplayName() const
{
	return FText::FromString(TEXT("Vlc Media Player"));
}

FName FVlcPlayerFactoryModule::GetPlayerName() const
{
	static FName PlayerName(TEXT("VlcMediaPlayer"));
	return PlayerName;
}

FGuid FVlcPlayerFactoryModule::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(TEXT("Vlc Plugin - Azb"));
	return PlayerPluginGUID;
}

const TArray<FString>& FVlcPlayerFactoryModule::GetSupportedPlatforms() const
{
	return SupportedPlatforms;
}

bool FVlcPlayerFactoryModule::SupportsFeature(EMediaFeature Feature) const
{
	return ((Feature == EMediaFeature::AudioSamples) ||
			(Feature == EMediaFeature::AudioTracks) ||
			(Feature == EMediaFeature::VideoSamples) ||
			(Feature == EMediaFeature::VideoTracks));
}

int32 FVlcPlayerFactoryModule::GetPlayabilityConfidenceScore(const FString& Url, const IMediaOptions* Options,
	TArray<FText>* OutWarnings, TArray<FText>* OutErrors) const
{
	FString Scheme;
	FString Location;

	// check scheme
	if (!Url.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
	{
		if (OutErrors != nullptr)
		{
			OutErrors->Add(LOCTEXT("NoSchemeFound", "No URI scheme found"));
		}

		return 0;
	}

	if (!SupportedUriSchemes.Contains(Scheme))
	{
		if (OutErrors != nullptr)
		{
			OutErrors->Add(FText::Format(LOCTEXT("SchemeNotSupported", "The URI scheme '{0}' is not supported"), FText::FromString(Scheme)));
		}

		return 0;
	}

	// check file extension
	if (Scheme == TEXT("file"))
	{
		const FString Extension = FPaths::GetExtension(Location, false);

		if (!SupportedFileExtensions.Contains(Extension))
		{
			if (OutErrors != nullptr)
			{
				OutErrors->Add(FText::Format(LOCTEXT("ExtensionNotSupported", "The file extension '{0}' is not supported"), FText::FromString(Extension)));
			}

			return 0;
		}
	}

	return 120;
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FVlcPlayerFactoryModule, VlcPlayerFactory)