// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Interactables/ACDoor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "ACDoor"

AACDoor::AACDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorFrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrameMesh"));
	DoorFrameMesh->SetupAttachment(SceneRoot);
	DoorFrameMesh->SetRelativeLocation(FVector(0.0f, 50.0f, 210.0f));
	DoorFrameMesh->SetRelativeScale3D(FVector(0.15f, 1.2f, 0.12f));
	DoorFrameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(SceneRoot);

	DoorLeafMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeafMesh"));
	DoorLeafMesh->SetupAttachment(DoorHinge);
	DoorLeafMesh->SetRelativeLocation(FVector(0.0f, 50.0f, 100.0f));
	DoorLeafMesh->SetRelativeScale3D(FVector(0.08f, 1.0f, 2.0f));
	DoorLeafMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DoorLeafCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorLeafCollision"));
	DoorLeafCollision->SetupAttachment(DoorHinge);
	DoorLeafCollision->SetRelativeLocation(FVector(0.0f, 50.0f, 100.0f));
	DoorLeafCollision->SetBoxExtent(FVector(5.0f, 50.0f, 100.0f));
	DoorLeafCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetRelativeLocation(FVector(0.0f, 50.0f, 100.0f));
	InteractionCollision->SetBoxExtent(FVector(75.0f, 80.0f, 115.0f));
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DoorFrameMesh->SetStaticMesh(CubeMesh.Object);
		DoorLeafMesh->SetStaticMesh(CubeMesh.Object);
	}

	OpenPrompt = LOCTEXT("OpenPrompt", "[E] Open Door");
	ClosePrompt = LOCTEXT("ClosePrompt", "[E] Close Door");
}

void AACDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedHingeRotation = DoorHinge->GetRelativeRotation();
	ApplyDoorStateAtTime(GetSynchronizedServerTime());
}

void AACDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyDoorStateAtTime(GetSynchronizedServerTime());
}

void AACDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AACDoor, DoorReplicatedState);
	DOREPLIFETIME(AACDoor, bInteractionEnabled);
}

bool AACDoor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return bInteractionEnabled && IsValid(InteractingPawn)
		&& (DoorReplicatedState.State == EACDoorState::Closed || DoorReplicatedState.State == EACDoorState::Open);
}

FText AACDoor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return DoorReplicatedState.State == EACDoorState::Open ? ClosePrompt : OpenPrompt;
}

void AACDoor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	BeginTransition(DoorReplicatedState.State == EACDoorState::Closed
		? EACDoorState::Opening
		: EACDoorState::Closing);
}

void AACDoor::SetInteractionEnabled(bool bEnabled)
{
	if (!HasAuthority() || bInteractionEnabled == bEnabled)
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	OnRep_InteractionEnabled();
	ForceNetUpdate();
}

void AACDoor::BeginTransition(EACDoorState TransitionState)
{
	DoorReplicatedState.State = TransitionState;
	DoorReplicatedState.TransitionStartServerTime = GetSynchronizedServerTime();
	SetActorTickEnabled(true);
	BP_OnDoorStateVisualsChanged(TransitionState);
	ForceNetUpdate();
}

void AACDoor::ApplyDoorStateAtTime(float ServerTime)
{
	const EACDoorState State = DoorReplicatedState.State;
	if (State == EACDoorState::Closed || State == EACDoorState::Open)
	{
		ApplyDoorAlpha(State == EACDoorState::Open ? 1.0f : 0.0f);
		SetActorTickEnabled(false);
		return;
	}

	const float Duration = FMath::Max(OpeningDuration, 0.05f);
	const float TransitionAlpha = FMath::Clamp((ServerTime - DoorReplicatedState.TransitionStartServerTime) / Duration, 0.0f, 1.0f);
	ApplyDoorAlpha(State == EACDoorState::Opening ? TransitionAlpha : 1.0f - TransitionAlpha);

	if (TransitionAlpha < 1.0f)
	{
		SetActorTickEnabled(true);
		return;
	}

	SetActorTickEnabled(false);
	if (HasAuthority())
	{
		DoorReplicatedState.State = State == EACDoorState::Opening ? EACDoorState::Open : EACDoorState::Closed;
		DoorReplicatedState.TransitionStartServerTime = ServerTime;
		BP_OnDoorStateVisualsChanged(DoorReplicatedState.State);
		ForceNetUpdate();
	}
}

void AACDoor::ApplyDoorAlpha(float OpenAlpha)
{
	FRotator AppliedRotation = ClosedHingeRotation;
	AppliedRotation.Yaw += OpeningAngle * FMath::Clamp(OpenAlpha, 0.0f, 1.0f);
	DoorHinge->SetRelativeRotation(AppliedRotation);
}

float AACDoor::GetSynchronizedServerTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.0f;
}

void AACDoor::OnRep_DoorReplicatedState()
{
	ApplyDoorStateAtTime(GetSynchronizedServerTime());
	BP_OnDoorStateVisualsChanged(DoorReplicatedState.State);
}

void AACDoor::OnRep_InteractionEnabled()
{
	// Local interaction components re-evaluate availability on their next timer pass.
}

#undef LOCTEXT_NAMESPACE
