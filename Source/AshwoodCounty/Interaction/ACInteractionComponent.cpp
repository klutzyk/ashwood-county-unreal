// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/ACInteractionComponent.h"

#include "Interaction/ACInteractableInterface.h"
#include "Engine/OverlapResult.h"
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

	StartLocalTargeting();
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
	if (!bInteractionEnabled || !IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	FText Prompt;
	AActor* NewTarget = FindLocalTarget(Prompt);
	SetCurrentTarget(NewTarget, Prompt);
}

void UACInteractionComponent::StartLocalTargeting()
{
	APawn* OwnerPawn = GetOwnerPawn();
	UWorld* World = GetWorld();
	if (!bInteractionEnabled || !World || GetNetMode() == NM_DedicatedServer || !IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(TargetingTimerHandle))
	{
		World->GetTimerManager().SetTimer(TargetingTimerHandle, this,
			&UACInteractionComponent::RefreshInteractionTarget,
			FMath::Max(TraceInterval, 0.02f), true, 0.05f);
	}
	RefreshInteractionTarget();
}

void UACInteractionComponent::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	if (!bInteractionEnabled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TargetingTimerHandle);
		}
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}
	StartLocalTargeting();
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

	// A valid direct view hit always wins. The fallback is only used when precise
	// camera targeting does not identify an interactable.
	AActor* DirectCandidate = bHit ? ViewHit.GetActor() : nullptr;
	if (IsValidCandidate(OwnerPawn, DirectCandidate))
	{
		OutPrompt = IACInteractableInterface::Execute_GetInteractionPrompt(DirectCandidate, const_cast<APawn*>(OwnerPawn));
		return DirectCandidate;
	}

	const float SearchRadius = FMath::Min(ProximityRadius, InteractionDistance);
	if (SearchRadius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		OwnerPawn->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams);

	AActor* BestCandidate = nullptr;
	float BestScore = -FLT_MAX;
	float CurrentTargetScore = -FLT_MAX;
	TSet<AActor*> EvaluatedActors;
	const FVector ViewForward = ViewRotation.Vector();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValid(Candidate) || EvaluatedActors.Contains(Candidate))
		{
			continue;
		}
		EvaluatedActors.Add(Candidate);

		if (!IsValidCandidate(OwnerPawn, Candidate))
		{
			continue;
		}

		const float Score = ScoreProximityCandidate(OwnerPawn, Candidate, ViewLocation, ViewForward);
		if (Score == -FLT_MAX)
		{
			continue;
		}

		if (Candidate == CurrentTarget.Get())
		{
			CurrentTargetScore = Score;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	if (CurrentTarget.IsValid() && CurrentTargetScore > -FLT_MAX &&
		CurrentTargetScore + TargetSwitchScoreAdvantage >= BestScore)
	{
		BestCandidate = CurrentTarget.Get();
	}

	if (BestCandidate)
	{
		OutPrompt = IACInteractableInterface::Execute_GetInteractionPrompt(BestCandidate, const_cast<APawn*>(OwnerPawn));
	}
	return BestCandidate;
}

bool UACInteractionComponent::IsValidCandidate(const APawn* InteractingPawn, AActor* Candidate) const
{
	return IsValid(Candidate)
		&& Candidate->Implements<UACInteractableInterface>()
		&& IsTargetInRange(InteractingPawn, Candidate)
		&& HasLineOfSightToTarget(InteractingPawn, Candidate)
		&& IACInteractableInterface::Execute_CanInteract(Candidate, const_cast<APawn*>(InteractingPawn));
}

float UACInteractionComponent::ScoreProximityCandidate(
	const APawn* InteractingPawn,
	const AActor* Candidate,
	const FVector& ViewLocation,
	const FVector& ViewForward) const
{
	if (!InteractingPawn || !Candidate)
	{
		return -FLT_MAX;
	}

	const FVector TargetPoint = GetClosestTargetPoint(ViewLocation, Candidate);
	const FVector DirectionToTarget = (TargetPoint - ViewLocation).GetSafeNormal();
	const float FacingDot = FVector::DotProduct(ViewForward, DirectionToTarget);
	if (FacingDot < MinimumFacingDot)
	{
		return -FLT_MAX;
	}

	const FVector PawnTargetPoint = GetClosestTargetPoint(InteractingPawn->GetActorLocation(), Candidate);
	const float Distance = FVector::Distance(InteractingPawn->GetActorLocation(), PawnTargetPoint);
	const float DistanceScore = 1.0f - FMath::Clamp(Distance / FMath::Max(ProximityRadius, 1.0f), 0.0f, 1.0f);
	const float AlignmentScore = (FacingDot + 1.0f) * 0.5f;
	return AlignmentScore * 0.65f + DistanceScore * 0.35f;
}

FVector UACInteractionComponent::GetClosestTargetPoint(const FVector& SourceLocation, const AActor* Target) const
{
	if (!Target)
	{
		return SourceLocation;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	Target->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	return FVector(
		FMath::Clamp(SourceLocation.X, BoundsOrigin.X - BoundsExtent.X, BoundsOrigin.X + BoundsExtent.X),
		FMath::Clamp(SourceLocation.Y, BoundsOrigin.Y - BoundsExtent.Y, BoundsOrigin.Y + BoundsExtent.Y),
		FMath::Clamp(SourceLocation.Z, BoundsOrigin.Z - BoundsExtent.Z, BoundsOrigin.Z + BoundsExtent.Z));
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

	const FVector EyeLocation = InteractingPawn->GetPawnViewLocation();
	const FVector ClosestBoundsPoint = GetClosestTargetPoint(EyeLocation, Target);

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
	if (!bInteractionEnabled || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !IsValid(RequestedTarget))
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
	if (!bInteractionEnabled || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(InteractingPawn) || !IsValid(RequestedTarget))
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
