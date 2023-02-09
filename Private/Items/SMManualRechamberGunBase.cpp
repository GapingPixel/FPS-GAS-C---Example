// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/SMManualRechamberGunBase.h"

#include "GAS/SMAbilitySystemComponent.h"
#include "Interfaces/SMFirstPersonInterface.h"
#include "Net/UnrealNetwork.h"
#include "SpawnMaster/SpawnMaster.h"

ASMManualRechamberGunBase::ASMManualRechamberGunBase()
{
	
}

void ASMManualRechamberGunBase::BeginPlay()
{
	Super::BeginPlay();
}

void ASMManualRechamberGunBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	bNeedsRechambering = bStartUnChambered;

	OnEquippableIsIdle.AddDynamic(this, &ASMManualRechamberGunBase::CheckForReChamber);
}

void ASMManualRechamberGunBase::CheckForReChamber()
{
	if (ReChamberAbilityTag.IsValid() == false)
	{
		SM_LOG(Warning, TEXT("ReChamberAbilityTag is not valid in CheckForReChamber."))
		return;
	}

	if (bNeedsRechambering == false)
	{
		return;
	}
	
	ISMFirstPersonInterface* interface = GetOwnerFirstPersonInterface();
	check(interface)
	
	USMAbilitySystemComponent* ASC = interface->GetSMAbilitySystemComponent();
	check(ASC)

	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(ReChamberAbilityTag);
	
	ASC->TryActivateAbilitiesByTag(TagContainer);
}

void ASMManualRechamberGunBase::OnExplicitlySpawnedIn()
{
	Super::OnExplicitlySpawnedIn();

	bNeedsRechambering = bSpawnUnChambered;
}

void ASMManualRechamberGunBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASMManualRechamberGunBase, bNeedsRechambering, COND_SkipOwner);
}
