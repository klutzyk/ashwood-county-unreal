// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/ACInteractionComponent.h"

#include "Interaction/ACInteractableInterface.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UACInteractionComponent::UACInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UACInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		GetWorld()->GetTimerManager().SetTimer(
			TargetingTimerHandle,
			this,
			&UACInteractionComponent::RefreshInteractionTarget,
			FMath::Max(TraceInterval, 0.02f),
			true,
			0.05f);
	}
}

void UACInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetingTimerHandle);
	}

	SetCurrentTarget(nullptr, FText::GetEmpty());
	Super::EndPlay(EndPlayReason);
}

APawn* UACInteractionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

void UACInteractionComponent::RefreshInteractionTarget()
{
	APawn* OwnerPawn = GetOwnerPawn();
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	FText Prompt;
	AActor* NewTarget = FindLocalTarget(Prompt);
	SetCurrentTarget(NewTarget, Prompt);
}

AActor* UACInteractionComponent::FindLocalTarget(FText& OutPrompt) const
{
	OutPrompt = FText::GetEmpty();
	const APawn* OwnerPawn = GetOwnerPawn();
	const APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	const UWorld* World = GetWorld();
	if (!OwnerPawn || !PlayerController || !World)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	// The third-person camera can sit several metres behind the pawn. Extend the
	// view ray by that offset, then enforce the real pawn-to-target distance below.
	// This keeps aiming responsive without granting extra interaction reach.
	const float CameraOffset = FVector::Distance(ViewLocation, OwnerPawn->GetPawnViewLocation());
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * (InteractionDistance + CameraOffset);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ACInteractionTargeting), false, OwnerPawn);
	FHitResult ViewHit;
	const bool bHit = TraceRadius > 0.0f
		? World->SweepSingleByChannel(ViewHit, ViewLocation, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), QueryParams)
		: World->LineTraceSingleByChannel(ViewHit, ViewLocation, TraceEnd, TraceChannel, QueryParams);

	AActor* Candidate = bHit ? ViewHit.GetActor() : nullptr;
	if (!IsValid(Candidate) || !Candidate->Implements<UACInteractableInterface>())
	{
		return nullptr;
	}

	if (!IsTargetInRange(OwnerPawn, Candidate) || !HasLineOfSightToTarget(OwnerPawn, Candidate))
	{
		return nullptr;
	}

	if (!IACInteractableInterface::Execute_CanInteract(Candidate, const_cast<APawn*>(OwnerPawn)))
	{
		return nullptr;
	}

	OutPrompt = IACInteractableInterface::Execute_GetInteractionPrompt(Candidate, const_cast<APawn*>(OwnerPawn));
	return Candidate;
}

bool UACInteractionComponent::IsTargetInRange(const APawn* InteractingPawn, const AActor* Target) const
{
	if (!InteractingPawn || !Target)
	{
		return false;
	}

	const FBox TargetBounds = Target->GetComponentsBoundingBox(true);
	const float DistanceSquared = TargetBounds.IsValid
		? TargetBounds.ComputeSquaredDistanceToPoint(InteractingPawn->GetActorLocation())
		: FVector::DistSquared(InteractingPawn->GetActorLocation(), Target->GetActorLocation());
	return DistanceSquared <= FMath::Square(InteractionDistance);
}

bool UACInteractionComponent::HasLineOfSightToTarget(const APawn* InteractingPawn, const AActor* Target) const
{
	const UWorld* World = GetWorld();
	if (!World || !InteractingPawn || !Target)
	{
		return false;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	const FVector EyeLocation = InteractingPawn->GetPawnViewLocation();
	const FVector ClosestBoundsPoint(
		FMath::Clamp(EyeLocation.X, BoundsOrigin.X - BoundsExtent.X, BoundsOrigin.X + BoundsExtent.X),
		FMath::Clamp(EyeLocation.Y, BoundsOrigin.Y - BoundsExtent.Y, BoundsOrigin.Y + BoundsExtent.Y),
		FMath::Clamp(EyeLocation.Z, BoundsOrigin.Z - BoundsExtent.Z, BoundsOrigin.Z + BoundsExtent.Z));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ACInteractionOcclusion), false, InteractingPawn);
	FHitResult OcclusionHit;
	const bool bBlocked = World->LineTraceSingleByChannel(OcclusionHit, EyeLocation, ClosestBoundsPoint, TraceChannel, QueryParams);
	return !bBlocked || OcclusionHit.GetActor() == Target;
}

void UACInteractionComponent::SetCurrentTarget(AActor* NewTarget, const FText& NewPrompt)
{
	if (CurrentTarget.Get() == NewTarget && CurrentPrompt.EqualTo(NewPrompt))
	{
		return;
	}

	CurrentTarget = NewTarget;
	CurrentPrompt = NewPrompt;
	OnInteractionTargetChanged.Broadcast(NewTarget, NewPrompt);
}

void UACInteractionComponent::RequestInteraction()
{
	APawn* OwnerPawn = GetOwnerPawn();
	AActor* RequestedTarget = CurrentTarget.Get();
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !IsValid(RequestedTarget))
	{
		return;
	}

	if (OwnerPawn->HasAuthority())
	{
		ServerRequestInteraction_Implementation(RequestedTarget);
	}
	else
	{
		ServerRequestInteraction(RequestedTarget);
	}
}

void UACInteractionComponent::ServerRequestInteraction_Implementation(AActor* RequestedTarget)
{
	APawn* OwnerPawn = GetOwnerPawn();
	UWorld* World = GetWorld();
	if (!OwnerPawn || !World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastServerRequestTime < ServerRequestCooldown)
	{
		return;
	}
	LastServerRequestTime = Now;

	if (ValidateServerInteraction(OwnerPawn, RequestedTarget))
	{
		IACInteractableInterface::Execute_Interact(RequestedTarget, OwnerPawn);
	}
}

bool UACInteractionComponent::ValidateServerInteraction(APawn* InteractingPawn, AActor* RequestedTarget) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(InteractingPawn) || !IsValid(RequestedTarget))
	{
		return false;
	}

	if (RequestedTarget->GetWorld() != GetWorld() || !RequestedTarget->Implements<UACInteractableInterface>())
	{
		return false;
	}

	if (!IsTargetInRange(InteractingPawn, RequestedTarget) || !HasLineOfSightToTarget(InteractingPawn, RequestedTarget))
	{
		return false;
	}

	return IACInteractableInterface::Execute_CanInteract(RequestedTarget, InteractingPawn);
}
