// Fill out your copyright notice in the Description page of Project Settings.


#include "Possessables/SMPlayerCharacter.h"

#include "EnhancedInputSubsystems.h"
#include "GameFramework/SpringArmComponent.h"

ASMPlayerCharacter::ASMPlayerCharacter(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FirstPersonHandsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonHandsMesh"));
	FirstPersonHandsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonHandsMesh->SetOnlyOwnerSee(true);
	FirstPersonHandsMesh->SetCastShadow(false);
	FirstPersonHandsMesh->SetupAttachment(RootComponent);

	FirstPersonLegsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonLegsMesh"));
	FirstPersonLegsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonLegsMesh->SetOnlyOwnerSee(true);
	FirstPersonLegsMesh->SetCastShadow(false);
	FirstPersonLegsMesh->SetupAttachment(RootComponent);

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCastHiddenShadow(true);
	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	FirstPersonCamera->SetupAttachment(FirstPersonHandsMesh, TEXT("head"));

	CameraController = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraController"));
	CameraController->bUsePawnControlRotation = true;
	CameraController->TargetArmLength = 0.0f;
	CameraController->bDoCollisionTest = false;
	CameraController->SetupAttachment(FirstPersonHandsMesh, TEXT("head"));

	ThirdPersonSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonSpringArm"));
	ThirdPersonSpringArm->bEnableCameraLag = true;
	ThirdPersonSpringArm->CameraLagSpeed = 15.f;
	ThirdPersonSpringArm->TargetArmLength = 200.f;
	ThirdPersonSpringArm->TargetOffset = FVector(0.f, 0.f, 0.f);
	ThirdPersonSpringArm->SetupAttachment(RootComponent);

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonSpringArm);
}

void ASMPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CameraController->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	FirstPersonHandsMesh->AttachToComponent(CameraController, FAttachmentTransformRules::KeepWorldTransform);
	CameraController->SetRelativeLocation(FVector(0.0f,0.0f,FirstPersonCameraHeight));
}

void ASMPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Make sure that we have a valid PlayerController.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// Get the Enhanced Input Local Player Subsystem from the Local Player related to our Player Controller.
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// PawnClientRestart can run more than once in an Actor's lifetime, so start by clearing out any leftover mappings.
			Subsystem->ClearAllMappings();

			for (int32 Idx = 0; Idx < DefaultInputMappingContexts.Num(); Idx++)
			{
				Subsystem->AddMappingContext(DefaultInputMappingContexts[Idx], Idx);
			}
		}
	}
}

void ASMPlayerCharacter::PerformDeath(AActor* OwningActor)
{
	Super::PerformDeath(OwningActor);

	SetThirdPerson(true);
}

void ASMPlayerCharacter::SetThirdPerson(bool bThirdPerson)
{
	if (bThirdPerson && bIsFirstPerson)
	{
		FirstPersonHandsMesh->SetHiddenInGame(true);
		FirstPersonLegsMesh->SetHiddenInGame(true);
		GetMesh()->SetOwnerNoSee(false);

		ThirdPersonCamera->SetActive(true);
		FirstPersonCamera->SetActive(false);

		bIsFirstPerson = false;
	}
	else if (!bThirdPerson && !bIsFirstPerson)
	{
		FirstPersonHandsMesh->SetHiddenInGame(false);
		FirstPersonLegsMesh->SetHiddenInGame(false);
		GetMesh()->SetOwnerNoSee(true);

		ThirdPersonCamera->SetActive(false);
		FirstPersonCamera->SetActive(true);

		bIsFirstPerson = true;
	}
}

void ASMPlayerCharacter::ToggleThirdPerson()
{
	if (bIsFirstPerson)
	{
		SetThirdPerson(true);
	}
	else
	{
		SetThirdPerson(false);
	}
}

USkeletalMeshComponent* ASMPlayerCharacter::GetMeshOfType(EMeshType MeshType)
{
	switch (MeshType)
	{
	case EMeshType::FirstPersonHands:
		return FirstPersonHandsMesh;

	case EMeshType::ThirdPersonBody:
		return GetMesh();

	case EMeshType::FirstPersonLegs:
		return FirstPersonLegsMesh;
			
	default:
		return Super::GetMeshOfType(MeshType); // In case in the future the base function does something else funky, we call the super anyways to be sure.
	}
}
