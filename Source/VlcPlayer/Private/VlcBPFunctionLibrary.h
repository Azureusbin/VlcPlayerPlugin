#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VlcBPFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class VLCPLAYER_API UVlcBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category=LibVlc, meta=(WorldContext="WorldContextObject"))
	static void TestCaptureViewport(UObject* WorldContextObject);
};
