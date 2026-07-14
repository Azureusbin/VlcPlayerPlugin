#pragma once

#include "CoreMinimal.h"
#include "IMediaPlayerFactory.h"
#include "Modules/ModuleManager.h"

class FVlcPlayerFactoryModule : public IMediaPlayerFactory, public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // ~IMediaPlayerFactory
    virtual bool CanPlayUrl(const FString& Url, const IMediaOptions* Options, TArray<FText>* OutWarnings,
        TArray<FText>* OutErrors) const override;
    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override;
    virtual FText GetDisplayName() const override;
    virtual FName GetPlayerName() const override;
    virtual FGuid GetPlayerPluginGUID() const override;
    virtual const TArray<FString>& GetSupportedPlatforms() const override;
    virtual bool SupportsFeature(EMediaFeature Feature) const override;
    virtual int32 GetPlayabilityConfidenceScore(const FString& Url, const IMediaOptions* Options,
        TArray<FText>* OutWarnings, TArray<FText>* OutErrors) const override;

private:
    /** List of supported media file types. */
    TArray<FString> SupportedFileExtensions;

    /** List of platforms that the media player support. */
    TArray<FString> SupportedPlatforms;

    /** List of supported URI schemes. */
    TArray<FString> SupportedUriSchemes;
};
