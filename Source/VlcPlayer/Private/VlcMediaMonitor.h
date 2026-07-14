#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Font.h"

class FVlcPlayer;

/**
 * Thread-safe registry of active VLC player instances.
 * Players self-register in constructor / self-remove in destructor.
 * Lives in a function-local static — safe across LiveCoding hot-reloads.
 */
TSet<FVlcPlayer*>& GetVlcPlayerRegistry();

/**
 * Real-time HUD overlay for VLC Media Player monitoring.
 *
 * Renders an on-screen panel similar to "stat engine" showing:
 *   - Number of active player instances
 *   - Per-player state, URL, time/duration, rate
 *   - VLC internal frame statistics (decoded/displayed/lost)
 *
 * Toggle with console command:  VlcMedia.StatsOverlay [0|1]
 */
class FVlcMediaMonitor
{
public:
	/** Register the debug-draw delegate. Call from module Startup. */
	static void Start();

	/** Unregister the debug-draw delegate. Call from module Shutdown. */
	static void Stop();

	/** Returns true if the overlay is currently enabled. */
	static bool IsEnabled() { return bEnabled; }

	/** Enable or disable the overlay. */
	static void SetEnabled(bool bInEnabled);
	
	/** Console command handler: VlcMedia.StatsOverlay [0|1] */
	static void HandleToggleCommand(const TArray<FString>& Args);
	
private:

	/** Per-frame draw callback registered with UDebugDrawService. */
	static void Draw(UCanvas* Canvas, APlayerController* PlayerController);

	/** Delegate handle returned by UDebugDrawService::Register. */
	static FDelegateHandle DrawHandle;

	/** Whether the overlay is currently active. */
	static bool bEnabled;

	// ── Drawing helpers ──

	struct FDrawContext
	{
		FCanvas* Canvas = nullptr;
		UFont* Font = nullptr;
		float X = 0;
		float Y = 0;
		float PanelW = 0;
		float LineH = 0;
	};

	static void DrawPanelBackground(const FDrawContext& Ctx, float PanelH, const FLinearColor& Color);
	static void DrawHeader(const FDrawContext& Ctx, int32 PlayerCount);
	static void DrawPlayerCard(const FDrawContext& Ctx, int32 Index, const class FVlcPlayer& Player);
	static void DrawProgressBar(const FDrawContext& Ctx, float Fraction, const FLinearColor& FillColor);
	static void DrawTextRow(const FDrawContext& Ctx, const FString& Text, const FLinearColor& Color);
	static void DrawTextRow(const FDrawContext& Ctx, const FString& Text);
};
