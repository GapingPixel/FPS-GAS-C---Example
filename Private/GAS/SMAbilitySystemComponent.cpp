// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/SMAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "EnhancedInputComponent.h"
#include "GameplayCueManager.h"
#include "GameFramework/PlayerState.h"
#include "GAS/SMGameplayAbility.h"
#include "Net/UnrealNetwork.h"

static TAutoConsoleVariable<float> CVarReplayMontageErrorThreshold(
	TEXT("GS.replay.MontageErrorThreshold"),
	0.5f,
	TEXT("Tolerance level for when montage playback position correction occurs in replays")
);

float USMAbilitySystemComponent::PlayMontageForMesh(USkeletalMeshComponent* InMesh, USMGameplayAbility* InAnimatingAbility,
                                                    FGameplayAbilityActivationInfo ActivationInfo,
                                                    UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName,
                                                    float StartTimeSeconds, bool bReplicateMontage)
{
	float Duration = -1.f;

	UAnimInstance* AnimInstance = InMesh != nullptr ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate, EMontagePlayReturnType::MontageLength, StartTimeSeconds);
		if (Duration > 0.f)
		{

			FGameplayAbilityLocalAnimMontageForMesh& LocalAnimMontageForMeshInfo = GetLocalAnimMontageInfoForMesh(InMesh);

			FGameplayAbilityLocalAnimMontage& LocalMontageInfo = LocalAnimMontageForMeshInfo.LocalMontageInfo;
			
			if (LocalMontageInfo.AnimatingAbility && LocalMontageInfo.AnimatingAbility != InAnimatingAbility)
			{
				// The ability that was previously animating will have already gotten the 'interrupted' callback.
				// It may be a good idea to make this a global policy and 'cancel' the ability.
				// 
				// For now, we expect it to end itself when this happens.
			}

			if (NewAnimMontage->HasRootMotion() && AnimInstance->GetOwningActor())
			{
				UE_LOG(LogRootMotion, Log, TEXT("UAbilitySystemComponent::PlayMontage %s, Role: %s")
					, *GetNameSafe(NewAnimMontage)
					, *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), AnimInstance->GetOwningActor()->GetLocalRole())
					);
			}

			LocalMontageInfo.AnimMontage = NewAnimMontage;
			LocalMontageInfo.AnimatingAbility = InAnimatingAbility;
			LocalMontageInfo.PlayInstanceId = (LocalMontageInfo.PlayInstanceId < UINT8_MAX ? LocalMontageInfo.PlayInstanceId + 1 : 0);
			
			if (InAnimatingAbility)
			{
				InAnimatingAbility->SetCurrentMontageForMesh(InMesh, NewAnimMontage);
			}
			
			// Start at a given Section.
			if (StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, NewAnimMontage);
			}

			// Replicate to non owners
			if (IsOwnerActorAuthoritative())
			{
				if (bReplicateMontage)
				{
					FGameplayAbilityRepAnimMontageForMesh& MutableRepAnimMontageInfo = GetGameplayAbilityRepAnimMontageForMesh(InMesh);

					// Those are static parameters, they are only set when the montage is played. They are not changed after that.
					MutableRepAnimMontageInfo.RepMontageInfo.AnimMontage = NewAnimMontage;
					MutableRepAnimMontageInfo.RepMontageInfo.PlayInstanceId = (MutableRepAnimMontageInfo.RepMontageInfo.PlayInstanceId < UINT8_MAX ? MutableRepAnimMontageInfo.RepMontageInfo.PlayInstanceId + 1 : 0);

					MutableRepAnimMontageInfo.RepMontageInfo.SectionIdToPlay = 0;
					if (MutableRepAnimMontageInfo.RepMontageInfo.AnimMontage && StartSectionName != NAME_None)
					{
						// we add one so INDEX_NONE can be used in the on rep
						MutableRepAnimMontageInfo.RepMontageInfo.SectionIdToPlay = MutableRepAnimMontageInfo.RepMontageInfo.AnimMontage->GetSectionIndex(StartSectionName) + 1;
					}

					// Update parameters that change during Montage life time.
					AnimMontage_UpdateReplicatedDataForMesh(InMesh);

					// Force net update on our avatar actor
					if (AbilityActorInfo->AvatarActor != nullptr)
					{
						AbilityActorInfo->AvatarActor->ForceNetUpdate();
					}
				}
			}
			else
			{
				// If this prediction key is rejected, we need to end the preview
				FPredictionKey PredictionKey = GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &USMAbilitySystemComponent::OnPredictiveMontageRejectedForMesh, InMesh, NewAnimMontage);
				}
			}
		}
	}

	return Duration;
}

float USMAbilitySystemComponent::PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* NewAnimMontage,
	float InPlayRate, FName StartSectionName)
{
	float Duration = -1.f;
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
			AnimMontageInfo.LocalMontageInfo.AnimMontage = NewAnimMontage;
		}
	}

	return Duration;
}

void USMAbilitySystemComponent::CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);
	UAnimMontage* MontageToStop = AnimMontageInfo.LocalMontageInfo.AnimMontage;
	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		const float BlendOutTime = (OverrideBlendOutTime >= 0.0f ? OverrideBlendOutTime : MontageToStop->BlendOut.GetBlendTime());

		AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);

		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedDataForMesh(InMesh);
		}
	}
}

UGameplayAbility* USMAbilitySystemComponent::GetAnimatingAbilityFromAnyMesh()
{
	// Only one ability can be animating for all meshes
	for (FGameplayAbilityLocalAnimMontageForMesh& GameplayAbilityLocalAnimMontageForMesh : LocalAnimMontageInfoForMeshes)
	{
		if (GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility)
		{
			return GameplayAbilityLocalAnimMontageForMesh.LocalMontageInfo.AnimatingAbility;
		}
	}

	return nullptr;
}

UAnimMontage* USMAbilitySystemComponent::GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh)
{
	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	FGameplayAbilityLocalAnimMontageForMesh& AnimMontageInfo = GetLocalAnimMontageInfoForMesh(InMesh);

	if (AnimMontageInfo.LocalMontageInfo.AnimMontage && AnimInstance
		&& AnimInstance->Montage_IsActive(AnimMontageInfo.LocalMontageInfo.AnimMontage))
	{
		return AnimMontageInfo.LocalMontageInfo.AnimMontage;
	}

	return nullptr;
}

FGameplayAbilityLocalAnimMontageForMesh& USMAbilitySystemComponent::GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* Mesh)
{
	for (FGameplayAbilityLocalAnimMontageForMesh& LocalAnimMontage : LocalAnimMontageInfoForMeshes)
	{
		if (LocalAnimMontage.Mesh == Mesh)
		{
			return LocalAnimMontage;
		}
	}

	const FGameplayAbilityLocalAnimMontageForMesh NewAbilityLocalAnimMontageForMesh = FGameplayAbilityLocalAnimMontageForMesh(Mesh);
	LocalAnimMontageInfoForMeshes.Add(NewAbilityLocalAnimMontageForMesh);

	return LocalAnimMontageInfoForMeshes.Last();
}

FGameplayAbilityRepAnimMontageForMesh& USMAbilitySystemComponent::GetGameplayAbilityRepAnimMontageForMesh(
	USkeletalMeshComponent* InMesh)
{
	for (FGameplayAbilityRepAnimMontageForMesh& RepMontageInfo : RepAnimMontageInfoForMeshes)
	{
		if (RepMontageInfo.Mesh == InMesh)
		{
			return RepMontageInfo;
		}
	}

	FGameplayAbilityRepAnimMontageForMesh RepMontageInfo = FGameplayAbilityRepAnimMontageForMesh(InMesh);
	RepAnimMontageInfoForMeshes.Add(RepMontageInfo);
	return RepAnimMontageInfoForMeshes.Last();
}

void USMAbilitySystemComponent::AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh)
{
	check(IsOwnerActorAuthoritative());

	AnimMontage_UpdateReplicatedDataForMesh(GetGameplayAbilityRepAnimMontageForMesh(InMesh));
}

void USMAbilitySystemComponent::AnimMontage_UpdateReplicatedDataForMesh(FGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo)
{
	UAnimInstance* AnimInstance = IsValid(OutRepAnimMontageInfo.Mesh) ? OutRepAnimMontageInfo.Mesh->GetAnimInstance() : nullptr;
	if (AnimInstance && LocalAnimMontageInfo.AnimMontage)
	{
		FGameplayAbilityRepAnimMontage& RepMontageInfo = OutRepAnimMontageInfo.RepMontageInfo;
		
		RepMontageInfo.AnimMontage = LocalAnimMontageInfo.AnimMontage;

		// Compressed Flags
		bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);

		if (!bIsStopped)
		{
			RepMontageInfo.PlayRate = AnimInstance->Montage_GetPlayRate(LocalAnimMontageInfo.AnimMontage);
			RepMontageInfo.Position = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
			RepMontageInfo.BlendTime = AnimInstance->Montage_GetBlendTime(LocalAnimMontageInfo.AnimMontage);
		}

		if (RepMontageInfo.IsStopped != bIsStopped)
		{
			// Set this prior to calling UpdateShouldTick, so we start ticking if we are playing a Montage
			RepMontageInfo.IsStopped = bIsStopped;

			// When we start or stop an animation, update the clients right away for the Avatar Actor
			if (AbilityActorInfo->AvatarActor != nullptr)
			{
				AbilityActorInfo->AvatarActor->ForceNetUpdate();
			}

			// When this changes, we should update whether or not we should be ticking
			UpdateShouldTick();
		}

		// Replicate NextSectionID to keep it in sync.
		// We actually replicate NextSectionID+1 on a BYTE to put INDEX_NONE in there.
		int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(RepMontageInfo.Position);
		if (CurrentSectionID != INDEX_NONE)
		{
			int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalAnimMontageInfo.AnimMontage, CurrentSectionID);
			if (NextSectionID >= (256 - 1))
			{
				UE_LOG(LogTemp,  Error, TEXT("AnimMontage_UpdateReplicatedData. NextSectionID = %d.  RepAnimMontageInfo.Position: %.2f, CurrentSectionID: %d. LocalAnimMontageInfo.AnimMontage %s"), 
					NextSectionID, RepMontageInfo.Position, CurrentSectionID, *GetNameSafe(LocalAnimMontageInfo.AnimMontage) );
				ensure(NextSectionID < (256 - 1));
			}
			RepMontageInfo.NextSectionID = uint8(NextSectionID + 1);
		}
		else
		{
			RepMontageInfo.NextSectionID = 0;
		}
	}
}

void USMAbilitySystemComponent::OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage)
{
	static const float MONTAGE_PREDICTION_REJECT_FADETIME = 0.25f;

	UAnimInstance* AnimInstance = IsValid(InMesh) && InMesh->GetOwner() == AbilityActorInfo->AvatarActor ? InMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && PredictiveMontage)
	{
		// If this montage is still playing: kill it
		if (AnimInstance->Montage_IsPlaying(PredictiveMontage))
		{
			AnimInstance->Montage_Stop(MONTAGE_PREDICTION_REJECT_FADETIME, PredictiveMontage);
		}
	}
}

bool USMAbilitySystemComponent::IsReadyForReplicatedMontageForMesh(USkeletalMeshComponent* InMesh)
{
	/** Children may want to override this for additional checks (e.g, "has skin been applied") */
	return true;
}

void USMAbilitySystemComponent::ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters & GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Executed, GameplayCueParameters);
}

void USMAbilitySystemComponent::AddGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters & GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::OnActive, GameplayCueParameters);
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::WhileActive, GameplayCueParameters);
}

void USMAbilitySystemComponent::RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters & GameplayCueParameters)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(GetOwner(), GameplayCueTag, EGameplayCueEvent::Type::Removed, GameplayCueParameters);
}

void USMAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	USMGameplayAbility* ability = Cast<USMGameplayAbility>(AbilitySpec.Ability);
	if (ability && GetNetMode() != NM_DedicatedServer) // @TODO: stop this code running on listen servers for simulated proxies
	{
		if (!ability->InputAction)
		{
			UE_LOG(LogTemp, Log, TEXT("Input Action in ability %s does not have an Input Action bound to it."), *GetNameSafe(ability))
			Super::OnGiveAbility(AbilitySpec);
			return;
		}

		// Bind the Input Action to the AbilityLocalInputX functions with the correct InputIDs
		OwnerPlayerInputComponent = GetInputComponent();
		if (OwnerPlayerInputComponent)
		{
			FAbilityInput inputStruct;

			AbilitySpec.InputID = GetNextInputID();
			inputStruct.InputID = AbilitySpec.InputID;

			inputStruct.PressedHandle = OwnerPlayerInputComponent->BindAction(ability->InputAction, ETriggerEvent::Started, this, &UAbilitySystemComponent::AbilityLocalInputPressed, AbilitySpec.InputID).GetHandle();
			inputStruct.ReleasedHandle = OwnerPlayerInputComponent->BindAction(ability->InputAction, ETriggerEvent::Completed,this, &UAbilitySystemComponent::AbilityLocalInputReleased, AbilitySpec.InputID).GetHandle();
			// We may not need to bind for ETriggerEvent::Cancelled, but this is an attempt to fix a strange buug.
			inputStruct.CancelledHandle = OwnerPlayerInputComponent->BindAction(ability->InputAction, ETriggerEvent::Canceled,this, &UAbilitySystemComponent::AbilityLocalInputReleased, AbilitySpec.InputID).GetHandle();

			AbilityInputArray.AddUnique(inputStruct);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InputComponent not found in OnGiveAbility in %s"), *GetNameSafe(this))
		}
	}

	Super::OnGiveAbility(AbilitySpec);
}

void USMAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnRemoveAbility(AbilitySpec);
	
	if (bCachedIsNetSimulated && GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// @TODO: how the fuck is this nullptr, the debugger shows that the cast is successful?????? HELLO???
	// (game will still work for now, but the input component will never unbind old abilities).
	
	UEnhancedInputComponent* inputComponent = GetInputComponent();
	if (inputComponent == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("inputComponent in OnRemoveAbility returned nullptr in %s. Has the owner changed or something somehow? Server/Client: %i"), *GetNameSafe(this), (int32)bCachedIsNetSimulated)
		return;
	}

	for (const FAbilityInput abilityInput : AbilityInputArray)
	{
		FGameplayAbilitySpec* spec = FindAbilitySpecFromHandle(abilityInput.Handle);
		if (spec)
		{
			if (spec->InputID == abilityInput.InputID)
			{
				inputComponent->RemoveBindingByHandle(abilityInput.PressedHandle);
				inputComponent->RemoveBindingByHandle(abilityInput.ReleasedHandle);
				inputComponent->RemoveBindingByHandle(abilityInput.CancelledHandle);

				AbilityInputArray.Remove(abilityInput);
				break;
			}
		}
	}
}

void USMAbilitySystemComponent::OnRep_ReplicatedAnimMontageForMesh()
{
	for (FGameplayAbilityRepAnimMontageForMesh& NewRepMontageInfoForMesh : RepAnimMontageInfoForMeshes)
	{
		UWorld* World = GetWorld();

		FGameplayAbilityRepAnimMontageForMesh& AnimMontageInfo = GetGameplayAbilityRepAnimMontageForMesh(NewRepMontageInfoForMesh.Mesh);
		FGameplayAbilityRepAnimMontage& RepAnimMontage = AnimMontageInfo.RepMontageInfo;
		
		if (RepAnimMontage.bSkipPlayRate)
		{
			GetGameplayAbilityRepAnimMontageForMesh(NewRepMontageInfoForMesh.Mesh).RepMontageInfo.PlayRate = 1.f;
		}

		const bool bIsPlayingReplay = World && World->IsPlayingReplay();

		const float MONTAGE_REP_POS_ERR_THRESH = bIsPlayingReplay ? CVarReplayMontageErrorThreshold.GetValueOnGameThread() : 0.1f;

		UAnimInstance* AnimInstance = IsValid(NewRepMontageInfoForMesh.Mesh) && NewRepMontageInfoForMesh.Mesh->GetOwner()
			== AbilityActorInfo->AvatarActor ? NewRepMontageInfoForMesh.Mesh->GetAnimInstance() : nullptr;
		if (AnimInstance == nullptr || !IsReadyForReplicatedMontageForMesh(NewRepMontageInfoForMesh.Mesh))
		{
			// We can't handle this yet
			bPendingMontageRep = true;
			return;
		}
		bPendingMontageRep = false;

		if (!AbilityActorInfo->IsLocallyControlled())
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.Montage.Debug"));
			bool DebugMontage = (CVar && CVar->GetValueOnGameThread() == 1);
			if (DebugMontage)
			{
				UE_LOG(LogTemp,  Warning, TEXT("\n\nOnRep_ReplicatedAnimMontageForMesh, %s"), *GetNameSafe(this));
				UE_LOG(LogTemp,  Warning, TEXT("\tAnimMontage: %s\n\tPlayRate: %f\n\tPosition: %f\n\tBlendTime: %f\n\tNextSectionID: %d\n\tIsStopped: %d\n\tPlayInstanceId: %d"),
					*GetNameSafe(RepAnimMontage.AnimMontage),
					RepAnimMontage.PlayRate,
					RepAnimMontage.Position,
					RepAnimMontage.BlendTime,
					RepAnimMontage.NextSectionID,
					RepAnimMontage.IsStopped,
					RepAnimMontage.PlayInstanceId);
				UE_LOG(LogTemp, Warning, TEXT("\tLocalAnimMontageInfo.AnimMontage: %s\n\tPosition: %f"),
					*GetNameSafe(RepAnimMontage.AnimMontage), AnimInstance->Montage_GetPosition(RepAnimMontage.AnimMontage));
			}

			if (NewRepMontageInfoForMesh.RepMontageInfo.AnimMontage)
			{
				// New Montage to play
				if ((RepAnimMontage.AnimMontage != NewRepMontageInfoForMesh.RepMontageInfo.AnimMontage) || 
				    (RepAnimMontage.PlayInstanceId != NewRepMontageInfoForMesh.RepMontageInfo.PlayInstanceId))
				{
					RepAnimMontage.PlayInstanceId = NewRepMontageInfoForMesh.RepMontageInfo.PlayInstanceId;
					PlayMontageSimulatedForMesh(NewRepMontageInfoForMesh.Mesh, NewRepMontageInfoForMesh.RepMontageInfo.AnimMontage, NewRepMontageInfoForMesh.RepMontageInfo.PlayRate);
				}

				if (RepAnimMontage.AnimMontage == nullptr)
				{ 
					UE_LOG(LogTemp, Warning, TEXT("OnRep_ReplicatedAnimMontage: PlayMontageSimulated failed. Name: %s, AnimMontage: %s"), *GetNameSafe(this), *GetNameSafe(NewRepMontageInfoForMesh.RepMontageInfo.AnimMontage));
					return;
				}

				// Play Rate has changed
				if (AnimInstance->Montage_GetPlayRate(RepAnimMontage.AnimMontage) != NewRepMontageInfoForMesh.RepMontageInfo.PlayRate)
				{
					AnimInstance->Montage_SetPlayRate(RepAnimMontage.AnimMontage, NewRepMontageInfoForMesh.RepMontageInfo.PlayRate);
				}

				const int32 SectionIdToPlay = (static_cast<int32>(NewRepMontageInfoForMesh.RepMontageInfo.SectionIdToPlay) - 1);
				if (SectionIdToPlay != INDEX_NONE)
				{
					FName SectionNameToJumpTo = RepAnimMontage.AnimMontage->GetSectionName(SectionIdToPlay);
					if (SectionNameToJumpTo != NAME_None)
					{
						AnimInstance->Montage_JumpToSection(SectionNameToJumpTo);
					}
				}

				// Compressed Flags
				const bool bIsStopped = AnimInstance->Montage_GetIsStopped(RepAnimMontage.AnimMontage);
				const bool bReplicatedIsStopped = bool(NewRepMontageInfoForMesh.RepMontageInfo.IsStopped);

				// Process stopping first, so we don't change sections and cause blending to pop.
				if (bReplicatedIsStopped)
				{
					if (!bIsStopped)
					{
						CurrentMontageStopForMesh(NewRepMontageInfoForMesh.Mesh, NewRepMontageInfoForMesh.RepMontageInfo.BlendTime);
					}
				}
				else if (!NewRepMontageInfoForMesh.RepMontageInfo.SkipPositionCorrection)
				{
					const int32 RepSectionID = RepAnimMontage.AnimMontage->GetSectionIndexFromPosition(NewRepMontageInfoForMesh.RepMontageInfo.Position);
					const int32 RepNextSectionID = int32(NewRepMontageInfoForMesh.RepMontageInfo.NextSectionID) - 1;
			
					// And NextSectionID for the replicated SectionID.
					if( RepSectionID != INDEX_NONE )
					{
						const int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(RepAnimMontage.AnimMontage, RepSectionID);

						// If NextSectionID is different than the replicated one, then set it.
						if( NextSectionID != RepNextSectionID )
						{
							AnimInstance->Montage_SetNextSection(RepAnimMontage.AnimMontage->GetSectionName(RepSectionID), RepAnimMontage.AnimMontage->GetSectionName(RepNextSectionID), RepAnimMontage.AnimMontage);
						}

						// Make sure we haven't received that update too late and the client hasn't already jumped to another section. 
						const int32 CurrentSectionID = RepAnimMontage.AnimMontage->GetSectionIndexFromPosition(AnimInstance->Montage_GetPosition(RepAnimMontage.AnimMontage));
						if ((CurrentSectionID != RepSectionID) && (CurrentSectionID != RepNextSectionID))
						{
							// Client is in a wrong section, teleport him into the begining of the right section
							const float SectionStartTime = RepAnimMontage.AnimMontage->GetAnimCompositeSection(RepSectionID).GetTime();
							AnimInstance->Montage_SetPosition(RepAnimMontage.AnimMontage, SectionStartTime);
						}
					}

					// Update Position. If error is too great, jump to replicated position.
					const float CurrentPosition = AnimInstance->Montage_GetPosition(RepAnimMontage.AnimMontage);
					const int32 CurrentSectionID = RepAnimMontage.AnimMontage->GetSectionIndexFromPosition(CurrentPosition);
					const float DeltaPosition = NewRepMontageInfoForMesh.RepMontageInfo.Position - CurrentPosition;

					// Only check threshold if we are located in the same section. Different sections require a bit more work as we could be jumping around the timeline.
					// And therefore DeltaPosition is not as trivial to determine.
					if ((CurrentSectionID == RepSectionID) && (FMath::Abs(DeltaPosition) > MONTAGE_REP_POS_ERR_THRESH) && (NewRepMontageInfoForMesh.RepMontageInfo.IsStopped == 0))
					{
						// fast forward to server position and trigger notifies
						if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(NewRepMontageInfoForMesh.RepMontageInfo.AnimMontage))
						{
							// Skip triggering notifies if we're going backwards in time, we've already triggered them.
							const float DeltaTime = !FMath::IsNearlyZero(NewRepMontageInfoForMesh.RepMontageInfo.PlayRate) ? (DeltaPosition / NewRepMontageInfoForMesh.RepMontageInfo.PlayRate) : 0.f;
							if (DeltaTime >= 0.f)
							{
								MontageInstance->UpdateWeight(DeltaTime);
								MontageInstance->HandleEvents(CurrentPosition, NewRepMontageInfoForMesh.RepMontageInfo.Position, nullptr);
								AnimInstance->TriggerAnimNotifies(DeltaTime);
							}
						}
						AnimInstance->Montage_SetPosition(RepAnimMontage.AnimMontage, NewRepMontageInfoForMesh.RepMontageInfo.Position);
					}
				}
			}
		}
	}
}

UEnhancedInputComponent* USMAbilitySystemComponent::GetInputComponent() const
{
	// I don't want to cast each time we want to get the InputComponent, i'd rather sacrifice for an extra nullptr check instead of a cast each time to get it.

	if (OwnerPlayerInputComponent)
	{
		return OwnerPlayerInputComponent;
	}

	// @TODO: i fucking hate all of these casts, luckily we only have to do them all once but still, it's kinda bad :/
	if (GetOwnerActor())
	{
		if (APlayerState* state = Cast<APlayerState>(GetOwnerActor()))
		{
			if (APlayerController* controller = Cast<APlayerController>(state->GetPlayerController()))
			{
				return Cast<UEnhancedInputComponent>(controller->InputComponent.Get());
			}
		}
	}

	return nullptr;
}

void USMAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USMAbilitySystemComponent, RepAnimMontageInfoForMeshes);
}