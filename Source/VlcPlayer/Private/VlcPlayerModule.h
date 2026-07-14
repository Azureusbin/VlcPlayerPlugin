// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class IMediaEventSink;
class IMediaPlayer;
struct libvlc_instance_t;

/**
 * Interface for the AvfMedia module.
 */
class IVlcMediaModule
	: public IModuleInterface
{
public:

	/**
	 * Create a media player.
	 *
	 * @param EventSink The object that receives media events from the player.
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) = 0;

public:

	/** Virtual destructor. */
	virtual ~IVlcMediaModule() { }
};

class FVlcPlayerModule : public IVlcMediaModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override;

	/**
	 * Get all currently active players.
	 * Dead references are pruned automatically.
	 */
	TArray<TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>> GetActivePlayers();

	libvlc_instance_t* VlcInstance;

private:
	TArray<TWeakPtr<IMediaPlayer, ESPMode::ThreadSafe>> ActivePlayers;
};
