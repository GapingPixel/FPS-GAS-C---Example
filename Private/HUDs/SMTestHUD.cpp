// Fill out your copyright notice in the Description page of Project Settings.


#include "HUDs/SMTestHUD.h"

#include "Components/SMHealthComponent.h"
#include "Possessables/SMPlayerCharacter.h"
#include "SpawnMaster/SpawnMaster.h"

void ASMTestHUD::BeginPlay()
{
	Super::BeginPlay();
	
	OwningCharacter = Cast<ASMPlayerCharacter>(GetOwningPawn());
	if (OwningCharacter)
	{
		HealthComp = OwningCharacter->FindComponentByClass<USMHealthComponent>();
		if (HealthComp)
		{
			UE_LOG(LogSpawnMaster, Log, TEXT("Creation of debug HUD success."))
		}
	}
}
