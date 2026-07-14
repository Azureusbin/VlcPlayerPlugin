// Fill out your copyright notice in the Description page of Project Settings.


#include "VlcMediaSettings.h"

UVlcSettings::UVlcSettings()
	: DiscCaching(FTimespan::FromMilliseconds(300))
	, FileCaching(FTimespan::FromMilliseconds(300))
	, LiveCaching(FTimespan::FromMilliseconds(300))
	, NetworkCaching(FTimespan::FromMilliseconds(1000))
{ }
