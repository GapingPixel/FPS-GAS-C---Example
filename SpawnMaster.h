// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputMappingContext.h"

#define COLLISION_SMCHARACTERBASE ECC_GameTraceChannel1
#define TRACECHANNEL_BULLET ECC_GameTraceChannel2

DECLARE_LOG_CATEGORY_EXTERN(LogSpawnMaster, Log, All);

#define SM_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogSpawnMaster, Verbosity, Format, ##__VA_ARGS__); \
}

DECLARE_STATS_GROUP(TEXT("SpawnMaster_Game"), STATGROUP_SpawnMaster, STATCAT_Advanced);