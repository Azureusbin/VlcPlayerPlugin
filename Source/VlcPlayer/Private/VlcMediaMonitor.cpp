#include "VlcMediaMonitor.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include "VlcPlayerModule.h"
#include "VlcHeader.h"
#include "Player/VlcPlayer.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

// ── Static members ──

FDelegateHandle FVlcMediaMonitor::DrawHandle;
bool FVlcMediaMonitor::bEnabled = false;

// ── Player registry (LiveCoding-safe: function-local static) ──

TSet<FVlcPlayer*>& GetVlcPlayerRegistry()
{
	static TSet<FVlcPlayer*> Registry;
	return Registry;
}

// ── Layout constants ──

static constexpr float PanelX          = 20.0f;
static constexpr float PanelY          = 100.0f;
static constexpr float PanelWidth      = 540.0f;
static constexpr float PanelPadX       = 8.0f;
static constexpr float PanelPadY       = 6.0f;
static constexpr float LineHeight      = 16.0f;
static constexpr float ProgressBarH    = 8.0f;
static constexpr float CardPadY        = 4.0f;
static constexpr int32 MaxUrlLen       = 50;

// ── Colors ──

static const FLinearColor ColorBg      (0.02f, 0.02f, 0.02f, 0.75f);
static const FLinearColor ColorHeader  (0.10f, 0.10f, 0.10f, 0.85f);
static const FLinearColor ColorSep     (0.20f, 0.20f, 0.20f, 0.60f);
static const FLinearColor ColorLabel   (0.55f, 0.55f, 0.55f, 1.00f);
static const FLinearColor ColorValue   (0.85f, 0.85f, 0.85f, 1.00f);
static const FLinearColor ColorPlaying (0.30f, 0.85f, 0.35f, 1.00f);
static const FLinearColor ColorPaused  (0.65f, 0.65f, 0.65f, 1.00f);
static const FLinearColor ColorError   (0.90f, 0.30f, 0.25f, 1.00f);
static const FLinearColor ColorPreparing(0.90f, 0.75f, 0.20f, 1.00f);
static const FLinearColor ColorBarBg   (0.08f, 0.08f, 0.08f, 0.80f);
static const FLinearColor ColorBarFill (0.22f, 0.55f, 0.85f, 1.00f);
static const FLinearColor ColorAccent  (0.22f, 0.55f, 0.85f, 1.00f);

// ── Helpers ──

static const TCHAR* StateToString(EMediaState State)
{
	switch (State)
	{
	case EMediaState::Closed:    return TEXT("Closed");
	case EMediaState::Preparing: return TEXT("Preparing");
	case EMediaState::Playing:   return TEXT("Playing");
	case EMediaState::Paused:    return TEXT("Paused");
	case EMediaState::Stopped:   return TEXT("Stopped");
	case EMediaState::Error:     return TEXT("Error");
	default:                     return TEXT("Unknown");
	}
}

static const FLinearColor& StateToColor(EMediaState State)
{
	switch (State)
	{
	case EMediaState::Playing:   return ColorPlaying;
	case EMediaState::Paused:    return ColorPaused;
	case EMediaState::Stopped:   return ColorPaused;
	case EMediaState::Error:     return ColorError;
	case EMediaState::Preparing: return ColorPreparing;
	default:                     return ColorLabel;
	}
}

/** Format URL for compact display:
 *   - Project-relative:  Game/Movies/demo.mp4
 *   - External local:    D:/Videos/demo.mp4 (strip file://)
 *   - Network:           keep as-is, cap length
 */
static FString FormatDisplayUrl(const FString& Url, int32 MaxLen = MaxUrlLen)
{
	// Network / streaming — keep as-is
	if (Url.StartsWith(TEXT("http://")) || Url.StartsWith(TEXT("https://"))
		|| Url.StartsWith(TEXT("rtsp://")) || Url.StartsWith(TEXT("rtmp://")))
	{
		if (Url.Len() > MaxLen)
		{
			return Url.Left(MaxLen - 3) + TEXT("...");
		}
		return Url;
	}

	// Strip file:// protocol
	FString Clean = Url;
	Clean.RemoveFromStart(TEXT("file:///"));
	Clean.RemoveFromStart(TEXT("file://"));

	// Try to make project-relative
	const FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	if (Clean.StartsWith(ContentDir))
	{
		Clean.RemoveFromStart(ContentDir);
		// Ensure leading separator is removed for clean display
		if (Clean.StartsWith(TEXT("/")) || Clean.StartsWith(TEXT("\\")))
		{
			Clean.RemoveAt(0);
		}
		Clean = FString::Printf(TEXT("Game/%s"), *Clean);
	}

	if (Clean.Len() > MaxLen)
	{
		Clean = Clean.Left(MaxLen - 3) + TEXT("...");
	}
	return Clean;
}

// ── Public API ──

void FVlcMediaMonitor::Start()
{
	if (DrawHandle.IsValid())
	{
		return;
	}

	DrawHandle = UDebugDrawService::Register(TEXT("Game"),
		FDebugDrawDelegate::CreateStatic(&FVlcMediaMonitor::Draw));
}

void FVlcMediaMonitor::Stop()
{
	if (!DrawHandle.IsValid())
	{
		return;
	}

	UDebugDrawService::Unregister(DrawHandle);
	DrawHandle.Reset();
	bEnabled = false;
}

void FVlcMediaMonitor::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	if (!bInEnabled)
	{
		return;
	}

	// UDebugDrawService only fires callbacks when the corresponding show flag
	// is enabled on the current viewport. The "Game" flag is OFF by default in
	// editor viewports (see FEngineShowFlags::Init), so we must enable it explicitly.
	int32 ViewportsEnabled = 0;

	auto EnableGameFlag = [&ViewportsEnabled](FEngineShowFlags& Flags)
	{
		const int32 GameIndex = FEngineShowFlags::FindIndexByName(TEXT("Game"));
		if (GameIndex != INDEX_NONE)
		{
			Flags.SetSingleFlag(GameIndex, true);
			++ViewportsEnabled;
		}
	};

	// Standalone game / PIE viewport
	if (GEngine && GEngine->GameViewport)
	{
		EnableGameFlag(GEngine->GameViewport->EngineShowFlags);
	}

#if WITH_EDITOR
	// All editor viewports (level, blueprint, etc.)
	if (GEditor)
	{
		for (FEditorViewportClient* Client : GEditor->GetAllViewportClients())
		{
			if (Client)
			{
				EnableGameFlag(Client->EngineShowFlags);
			}
		}
	}
#endif

	UE_LOG(LogVlcMedia, Log, TEXT("VLC Monitor: Game show flag enabled on %i viewport(s)"), ViewportsEnabled);
}

// ── Console command ──

static FAutoConsoleCommand CmdVlcStatsOverlay(
	TEXT("VlcMedia.StatsOverlay"),
	TEXT("Toggle VLC Media Player stats overlay on screen. Usage: VlcMedia.StatsOverlay [0|1]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FVlcMediaMonitor::HandleToggleCommand)
);

void FVlcMediaMonitor::HandleToggleCommand(const TArray<FString>& Args)
{
	if (Args.Num() > 0)
	{
		FVlcMediaMonitor::SetEnabled(FCString::Atoi(*Args[0]) != 0);
	}
	else
	{
		FVlcMediaMonitor::SetEnabled(!FVlcMediaMonitor::IsEnabled());
	}

	UE_LOG(LogVlcMedia, Log, TEXT("VLC Stats Overlay: %s"),
		FVlcMediaMonitor::IsEnabled() ? TEXT("ON") : TEXT("OFF"));
}

// ── Drawing helpers ──

void FVlcMediaMonitor::DrawPanelBackground(const FDrawContext& Ctx, float PanelH, const FLinearColor& Color)
{
	FCanvasTileItem Tile(FVector2D(Ctx.X, Ctx.Y), FVector2D(Ctx.PanelW, PanelH), Color);
	Tile.BlendMode = SE_BLEND_Translucent;
	Ctx.Canvas->DrawItem(Tile);
}

void FVlcMediaMonitor::DrawTextRow(const FDrawContext& Ctx, const FString& Text, const FLinearColor& Color)
{
	FCanvasTextItem TextItem(FVector2D(Ctx.X + PanelPadX, Ctx.Y), FText::FromString(Text), Ctx.Font, Color);
	TextItem.EnableShadow(FLinearColor::Black);
	Ctx.Canvas->DrawItem(TextItem);
}

void FVlcMediaMonitor::DrawTextRow(const FDrawContext& Ctx, const FString& Text)
{
	DrawTextRow(Ctx, Text, ColorValue);
}

void FVlcMediaMonitor::DrawProgressBar(const FDrawContext& Ctx, float Fraction, const FLinearColor& FillColor)
{
	const float BarY = Ctx.Y;
	const float BarW = Ctx.PanelW - PanelPadX * 2.0f;

	// Background
	FCanvasTileItem Bg(FVector2D(Ctx.X + PanelPadX, BarY), FVector2D(BarW, ProgressBarH), ColorBarBg);
	Bg.BlendMode = SE_BLEND_Translucent;
	Ctx.Canvas->DrawItem(Bg);

	// Fill
	const float ClampedFrac = FMath::Clamp(Fraction, 0.0f, 1.0f);
	if (ClampedFrac > 0.0f)
	{
		FCanvasTileItem Fill(FVector2D(Ctx.X + PanelPadX, BarY), FVector2D(BarW * ClampedFrac, ProgressBarH), FillColor);
		Fill.BlendMode = SE_BLEND_Translucent;
		Ctx.Canvas->DrawItem(Fill);
	}
}

void FVlcMediaMonitor::DrawHeader(const FDrawContext& Ctx, int32 PlayerCount)
{
	// Header background
	FCanvasTileItem HeaderBg(FVector2D(Ctx.X, Ctx.Y), FVector2D(Ctx.PanelW, Ctx.LineH), ColorHeader);
	HeaderBg.BlendMode = SE_BLEND_Translucent;
	Ctx.Canvas->DrawItem(HeaderBg);

	const FString HeaderText = FString::Printf(TEXT("VLC Player  |  %i Player%s  |  %s"),
		PlayerCount,
		PlayerCount == 1 ? TEXT("") : TEXT("s"),
		ANSI_TO_TCHAR(libvlc_get_version()));

	FCanvasTextItem TextItem(FVector2D(Ctx.X + PanelPadX, Ctx.Y + 1.0f),
		FText::FromString(HeaderText), Ctx.Font, ColorAccent);
	TextItem.EnableShadow(FLinearColor::Black);
	Ctx.Canvas->DrawItem(TextItem);
}

void FVlcMediaMonitor::DrawPlayerCard(const FDrawContext& Ctx, int32 Index, const FVlcPlayer& Player)
{
	float Y = Ctx.Y;

	const EMediaState State = Player.GetState();
	const FLinearColor& StateColor = StateToColor(State);
	const TCHAR* StateStr = StateToString(State);

	// Index + State badge
	{
		const FString Line = FString::Printf(TEXT("[%i] %s"), Index, StateStr);
		FCanvasTextItem TextItem(FVector2D(Ctx.X + PanelPadX, Y), FText::FromString(Line), Ctx.Font, StateColor);
		TextItem.EnableShadow(FLinearColor::Black);
		Ctx.Canvas->DrawItem(TextItem);
		Y += Ctx.LineH;
	}

	// URL
	{
		const FString Url = FormatDisplayUrl(Player.GetUrl());
		FCanvasTextItem TextItem(FVector2D(Ctx.X + PanelPadX, Y), FText::FromString(Url), Ctx.Font, ColorLabel);
		TextItem.EnableShadow(FLinearColor::Black);
		Ctx.Canvas->DrawItem(TextItem);
		Y += Ctx.LineH;
	}

	// Progress bar + time + rate + looping (merged into one compact line)
	{
		const FTimespan Duration = Player.GetDuration();
		const FTimespan Time = Player.GetTime();
		const float Fraction = (Duration > FTimespan::Zero())
			? static_cast<float>(Time.GetTicks()) / static_cast<float>(Duration.GetTicks())
			: 0.0f;

		DrawProgressBar({Ctx.Canvas, Ctx.Font, Ctx.X, Y, Ctx.PanelW, Ctx.LineH}, Fraction, ColorBarFill);

		const float Rate = Player.GetRate();
		const bool bLooping = Player.IsLooping();

		const FString InfoLine = (Duration > FTimespan::Zero())
			? FString::Printf(TEXT("%s / %s  %.0f%%  %.2fx  loop:%s"),
				*Time.ToString(TEXT("%h:%m:%s")), *Duration.ToString(TEXT("%h:%m:%s")),
				Fraction * 100.0f, Rate, bLooping ? TEXT("on") : TEXT("off"))
			: FString::Printf(TEXT("LIVE  %.2fx  loop:%s"), Rate, bLooping ? TEXT("on") : TEXT("off"));

		FCanvasTextItem InfoItem(FVector2D(Ctx.X + PanelPadX, Y + ProgressBarH + 1.0f),
			FText::FromString(InfoLine), Ctx.Font, ColorValue);
		InfoItem.EnableShadow(FLinearColor::Black);
		Ctx.Canvas->DrawItem(InfoItem);
		Y += Ctx.LineH + ProgressBarH + 2.0f;  // extra space for bar + text
	}

	// VLC internal stats (decoded / displayed / lost)
	{
		const libvlc_media_t* Media = Player.GetMediaSource().GetMedia();
		if (Media)
		{
			libvlc_media_stats_t Stats;
			if (libvlc_media_get_stats(const_cast<libvlc_media_t*>(Media), &Stats))
			{
				const int32 VideoLostPct = (Stats.i_displayed_pictures + Stats.i_lost_pictures) > 0
					? FMath::RoundToInt(100.0f * Stats.i_lost_pictures / static_cast<float>(Stats.i_displayed_pictures + Stats.i_lost_pictures))
					: 0;

				const int32 AudioLostPct = (Stats.i_played_abuffers + Stats.i_lost_abuffers) > 0
					? FMath::RoundToInt(100.0f * Stats.i_lost_abuffers / static_cast<float>(Stats.i_played_abuffers + Stats.i_lost_abuffers))
					: 0;

				const FLinearColor VideoColor = VideoLostPct > 5 ? ColorError : ColorLabel;
				const FLinearColor AudioColor = AudioLostPct > 5 ? ColorError : ColorLabel;

				const FString VLine = FString::Printf(
					TEXT("V: %i dec / %i dsp / %i lost (%i%%)"),
					Stats.i_decoded_video,
					Stats.i_displayed_pictures,
					Stats.i_lost_pictures,
					VideoLostPct);

				const FString ALine = FString::Printf(
					TEXT("A: %i dec / %i buf / %i lost (%i%%)"),
					Stats.i_decoded_audio,
					Stats.i_played_abuffers,
					Stats.i_lost_abuffers,
					AudioLostPct);

				FCanvasTextItem VTextItem(FVector2D(Ctx.X + PanelPadX, Y),
					FText::FromString(VLine), Ctx.Font, VideoColor);
				VTextItem.EnableShadow(FLinearColor::Black);
				Ctx.Canvas->DrawItem(VTextItem);
				Y += Ctx.LineH;

				FCanvasTextItem ATextItem(FVector2D(Ctx.X + PanelPadX, Y),
					FText::FromString(ALine), Ctx.Font, AudioColor);
				ATextItem.EnableShadow(FLinearColor::Black);
				Ctx.Canvas->DrawItem(ATextItem);
				Y += Ctx.LineH;
			}
		}
	}

	// Card separator
	const float SepY = Y + CardPadY;
	FCanvasTileItem Sep(FVector2D(Ctx.X + PanelPadX, SepY), FVector2D(Ctx.PanelW - PanelPadX * 2.0f, 1.0f), ColorSep);
	Sep.BlendMode = SE_BLEND_Translucent;
	Ctx.Canvas->DrawItem(Sep);
}

// ── Main draw callback ──

void FVlcMediaMonitor::Draw(UCanvas* Canvas, APlayerController* /*PlayerController*/)
{
	if (!bEnabled)
	{
		return;
	}

	if (!Canvas || !Canvas->Canvas)
	{
		// Canvas not ready — log once per toggle
		static bool bCanvasWarningLogged = false;
		if (!bCanvasWarningLogged)
		{
			bCanvasWarningLogged = true;
			UE_LOG(LogVlcMedia, Warning, TEXT("VLC Monitor: Draw called but Canvas=%p Canvas->Canvas=%p"),
				Canvas, Canvas ? static_cast<void*>(Canvas->Canvas) : nullptr);
		}
		return;
	}

	// ── Get active players from the registry (ctor/dtor auto-register) ──

	TSet<FVlcPlayer*>& Registry = GetVlcPlayerRegistry();
	TArray<FVlcPlayer*> ActivePlayers = Registry.Array();

	// Filter out closed players
	ActivePlayers.RemoveAll([](FVlcPlayer* P) { return !P || P->GetState() == EMediaState::Closed; });

	// ── Prepare canvas context ──

	UFont* Font = GEngine->GetSmallFont();
	if (!Font)
	{
		return;
	}

	// Periodically log that Draw is being called (every ~60 frames)
	{
		static int32 FrameCounter = 0;
		if (++FrameCounter % 60 == 0)
		{
			UE_LOG(LogVlcMedia, Verbose, TEXT("VLC Monitor: Draw() frame %i, Players=%i"),
				FrameCounter, ActivePlayers.Num());
		}
	}

	FCanvas* FCanv = Canvas->Canvas;
	FString TestText;

	if (ActivePlayers.Num() == 0)
	{
		// Draw a small indicator to confirm the Draw callback is running
		TestText = FString::Printf(TEXT("VLC Monitor: No active players"));
		FCanvasTextItem TextItem(FVector2D(PanelX, PanelY),
			FText::FromString(TestText), Font, ColorAccent);
		TextItem.EnableShadow(FLinearColor::Black);
		FCanv->DrawItem(TextItem);
		return;
	}

	FDrawContext Ctx;
	Ctx.Canvas = FCanv;
	Ctx.Font = Font;
	Ctx.X = PanelX;
	Ctx.Y = PanelY;
	Ctx.PanelW = PanelWidth;
	Ctx.LineH = LineHeight;

	// ── Calculate total panel height ──

	const int32 PlayerCount = ActivePlayers.Num();
	// Each card: state + URL + (progress bar extra) + V-stats + A-stats + separator
	// Lines: state(1) + URL(1) + V-stats(1) + A-stats(1) = 4 text lines, + separator
	// Plus progress bar section: LineHeight + ProgressBarH + 2
	const float CardBodyH = 4.0f * LineHeight + (LineHeight + ProgressBarH + 2.0f) + CardPadY + 1.0f/*sep*/;
	const float TotalH = PanelPadY * 2.0f + LineHeight /*header*/ + CardPadY
		+ static_cast<float>(PlayerCount) * CardBodyH;

	// ── Draw panel background ──

	DrawPanelBackground(Ctx, TotalH, ColorBg);

	// ── Draw header ──

	DrawHeader(Ctx, PlayerCount);
	Ctx.Y += Ctx.LineH + CardPadY;

	// ── Draw each player card ──

	for (int32 i = 0; i < ActivePlayers.Num(); ++i)
	{
		FVlcPlayer* VlcPlayer = ActivePlayers[i];
		if (!VlcPlayer)
		{
			continue;
		}

		DrawPlayerCard(Ctx, i, *VlcPlayer);

		Ctx.Y += CardBodyH;
	}
}
