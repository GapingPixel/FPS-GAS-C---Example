// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/AttributeSets/SMCharacterAttributeSet.h"
#include "SMHealthComponent.generated.h"

/*
 * Initial development goals for the health component:
 *
 * 1. Classes - In order for AI to detect who they actually need to attack, we need a "classes" concept up and running.
 * This will also help for in the future when we have the ability to play as a zombie.
 *
 * The Spawn Master will not be a class in itself, as that will be a separate pawn entirely. The class concept
 * only applies to characters.
 */

class USMHealthAttributeSet;
class USMAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSMHealth_AttributeChanged, USMHealthComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMHealth_DeathEvent, AActor*, OwningActor);

// one way to possibly do it is by using GameplayTags, so for example you could have "Teams.Survivor" and "Teams.Zombie".
// Then in code you could have global const variables such as TEAM_SURVIVOR. 
UENUM()
enum class ETeamID : uint8
{
	// @TODO: look into being able to add and remove teams instead of having a plain old enum
	NoTeam = 0,
	Survivor,
	Zombie
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamChanged, ETeamID, OldTeam, ETeamID, NewTeam);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class USMHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	USMHealthComponent();

	// Delegate fired when the health value has changed.
	UPROPERTY(BlueprintAssignable)
	FSMHealth_AttributeChanged OnHealthChanged;

	// Delegate fired when the max health value has changed.
	UPROPERTY(BlueprintAssignable)
	FSMHealth_AttributeChanged OnMaxHealthChanged;

	// Delegate fired when the character dies.
	UPROPERTY(BlueprintAssignable)
	FSMHealth_DeathEvent OnDeath;

	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "SpawnMaster|Health")
	static USMHealthComponent* FindHealthComponent(const AActor* Actor) { return Actor ? Actor->FindComponentByClass<USMHealthComponent>() : nullptr;  }

#pragma region GAS
	
	/* GAS
	***********************************************************************************/

public:
	
	void InitializeWithAbilitySystemComponent(USMAbilitySystemComponent* InASC);
	void UnInitializeFromAbilitySystemComponent();

	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "SpawnMaster|Health")
	float GetHealth() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "SpawnMaster|Health")
	float GetMaxHealth() const;


protected:

	virtual void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec& DamageEffectSpec, float DamageMagnitude);

	// The ASC that owns this Health Component.
	UPROPERTY()
	USMAbilitySystemComponent* AbilitySystemComponent;

	// The Health Attribute Set that the owning ASC has.
	UPROPERTY()
	const USMHealthAttributeSet* HealthSet;

#pragma endregion GAS

#pragma region Team
	
	/* Team
	***********************************************************************************/

public:
	
	// Set the team of the pawn that this Health Component lives on.
	UFUNCTION(BlueprintCallable, Category = HealthComponent)
	void SetTeam(ETeamID newTeam);

	// Get the Team ID of this Health Component.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = HealthComponent)
	ETeamID GetTeam();
	
	// Find the team of a certain Actor if it has a Health Component. Can return NoTeam.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = HealthComponent)
	static ETeamID GetTeamFromActor(const AActor* Actor);

	// Returns if two Actors are on the same team.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = HealthComponent)
	static bool IsFriendly(const AActor* ActorA, const AActor* ActorB);

	// @TODO: can we change this type of delegate to something more performant?
	// Called after this component owner team changes.
	UPROPERTY(BlueprintAssignable, Category = HealthComponent)
	FOnTeamChanged OnTeamChange;

private:
	// The current team. Should not edit directly, please use the SetTeam function instead.
	UPROPERTY(EditDefaultsOnly, Category = HealthComponent, ReplicatedUsing=OnRep_TeamID, meta=(AllowPrivateAccess=true))
	ETeamID TeamID = ETeamID::NoTeam;

protected:
	UFUNCTION()
	virtual void OnRep_TeamID(ETeamID OldTeam);

#pragma endregion Team

public:

	UFUNCTION()
	void OnRep_IsDead(bool bOldIsDead);
	
private:

	UPROPERTY(ReplicatedUsing=OnRep_IsDead)
	bool bIsDead = false;
};
