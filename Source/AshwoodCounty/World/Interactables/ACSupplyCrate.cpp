// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Interactables/ACSupplyCrate.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "ACSupplyCrate"

AACSupplyCrate::AACSupplyCrate()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetBoxExtent(FVector(62.0f, 42.0f, 36.0f));
	InteractionCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 36.0f));
	InteractionCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	BaseMesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.5f));
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LidPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LidPivot"));
	LidPivot->SetupAttachment(SceneRoot);
	LidPivot->SetRelativeLocation(FVector(-60.0f, 0.0f, 56.0f));

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(LidPivot);
	LidMesh->SetRelativeLocation(FVector(60.0f, 0.0f, 0.0f));
	LidMesh->SetRelativeScale3D(FVector(1.2f, 0.8f, 0.12f));
	LidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A visible engine primitive is a safe fallback. Blueprint children can replace
	// either mesh, material, and transform without affecting collision or networking.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CubeMesh.Object);
		LidMesh->SetStaticMesh(CubeMesh.Object);
	}

	OpenPrompt = LOCTEXT("OpenPrompt", "[E] Open Supply Crate");
	ClosePrompt = LOCTEXT("ClosePrompt", "[E] Close Supply Crate");
}

void AACSupplyCrate::BeginPlay()
{
	Super::BeginPlay();
	ApplyOpenStateVisuals();
}

void AACSupplyCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AACSupplyCrate, bIsOpen);
	DOREPLIFETIME(AACSupplyCrate, bInteractionEnabled);
}

bool AACSupplyCrate::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return bInteractionEnabled && IsValid(InteractingPawn);
}

FText AACSupplyCrate::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return bIsOpen ? ClosePrompt : OpenPrompt;
}

void AACSupplyCrate::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastInteractionServerTime < SharedInteractionCooldown)
	{
		return;
	}
	LastInteractionServerTime = Now;

	bIsOpen = !bIsOpen;
	ApplyOpenStateVisuals();
	ForceNetUpdate();
}

void AACSupplyCrate::SetInteractionEnabled(bool bEnabled)
{
	if (!HasAuthority() || bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	OnRep_InteractionEnabled();
	ForceNetUpdate();
}

void AACSupplyCrate::OnRep_IsOpen()
{
	ApplyOpenStateVisuals();
}

void AACSupplyCrate::OnRep_InteractionEnabled()
{
	// Local interaction components re-evaluate CanInteract on their next timer pass.
}

void AACSupplyCrate::ApplyOpenStateVisuals()
{
	if (LidPivot)
	{
		LidPivot->SetRelativeRotation(bIsOpen ? OpenLidRotation : ClosedLidRotation);
	}
	BP_OnOpenStateVisualsChanged(bIsOpen);
}

#undef LOCTEXT_NAMESPACE
