#include "VlcBPFunctionLibrary.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Async/Async.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateViewportProvider.h"
#include "Widgets/SWindow.h"

struct FTestStr
{
#if ENGINE_MINOR_VERSION < 8
	void OnFrameBufferReady(SWindow& SlateWindow, const FTextureRHIRef& FrameBuffer)
	{
#else
	void OnFrameBufferReady(SWindow& SlateWindow, ISlateViewportProvider& ViewportProvider)
	{
		const FTextureRHIRef& FrameBuffer = ViewportProvider.GetBackBufferResource();
#endif
	
		ENQUEUE_RENDER_COMMAND(CopyFrame)(
				[this, FrameBuffer](FRHICommandListImmediate& RHICmdList)
				{
					SourceRect = FIntRect (0, 0,
						FrameBuffer->GetDesc().Extent.X,
						FrameBuffer->GetDesc().Extent.Y);
					
					ColorData.AddUninitialized(SourceRect.Width() * SourceRect.Height());
					RHICmdList.ReadSurfaceData(FrameBuffer, SourceRect, ColorData, FReadSurfaceDataFlags());

					UE_LOG(LogTemp, Warning, TEXT("Buffer format: %d"), FrameBuffer->GetDesc().Format);
					UE_LOG(LogTemp, Warning, TEXT("ColorData Size: %d"), ColorData.Num());

					/*for (FColor& Pixel : ColorData)
					{
						Pixel.A = 255;
					}*/

					SaveImageFile();
				});
		//FlushRenderingCommands();

		AsyncTask(ENamedThreads::GameThread,[this]
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().RemoveAll(this);
		});
	}

	void SaveImageFile()
	{
		FImageView ImageView(ColorData.GetData(), SourceRect.Width(), SourceRect.Height());
		FImageCore::SetAlphaOpaque(ImageView);
		FString FilePath = FPaths::ProjectContentDir() / TEXT("CurrentFrame.png");
		FImageUtils::SaveImageAutoFormat(*FilePath, ImageView);
	}

	TArray<FColor> ColorData;
	FIntRect SourceRect;
};

void UVlcBPFunctionLibrary::TestCaptureViewport(UObject* WorldContextObject)
{
	FTestStr*TestStr = new FTestStr;
	
	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddRaw(TestStr,
		&FTestStr::OnFrameBufferReady);
}
