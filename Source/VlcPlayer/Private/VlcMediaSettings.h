// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VlcMediaSettings.generated.h"

/**
 * VLC 播放器插件的设置
 */
UCLASS(Config=engine)
class VLCPLAYER_API UVlcSettings : public UObject
{
	GENERATED_BODY()

public:
	UVlcSettings();

	/** 光盘媒体的缓存时长 (默认 = 300 ms). */
	UPROPERTY(config, EditAnywhere, Category=Caching)
	FTimespan DiscCaching;

	/** 本地文件的缓存时长 (默认 = 300 ms). */
	UPROPERTY(config, EditAnywhere, Category=Caching)
	FTimespan FileCaching;

	/** 实时流媒体的缓存时长 (默认 = 300 ms). */
	UPROPERTY(config, EditAnywhere, Category=Caching)
	FTimespan LiveCaching;

	/** 网络资源的缓存时长 (默认 = 1000 ms). */
	UPROPERTY(config, EditAnywhere, Category=Caching)
	FTimespan NetworkCaching;
};
