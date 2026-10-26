// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACInteractionComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FACInteractionTargetChangedSignature, AActor*, Target, const FText&, Prompt);

/**
 * Reusable, locally responsive interaction targeting with an authoritative RPC.
 * The component never contains object-specific behavior.
 */
UCLASS(ClassGroup = (AshwoodCounty), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class ASHWOODCOUNTY_API UACInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACInteractionComponent();

	/** Request an interaction with the current locally selected target. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RequestInteraction();

	/** Immediately refresh local targeting. Useful after a camera mode change. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshInteractionTarget();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void StartLocalTargeting();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetCurrentPrompt() const { return CurrentPrompt; }

	/** Local-only presentation event. It is deliberately not replicated. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FACInteractionTargetChangedSignature OnInteractionTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Maximum distance from the owning pawn to the target's collision bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDistance = 350.0f;

	/** Local trace cadence. Ten traces per second is responsive without per-frame work. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "0.02", Units = "s"))
	float TraceInterval = 0.1f;

	/** Small sweep radius makes targeting forgiving while preserving occlusion checks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 8.0f;

	/** Radius used to find nearby fallback candidates when the view trace misses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float ProximityRadius = 300.0f;

	/** Minimum camera alignment for proximity candidates. Zero excludes the rear hemisphere. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinimumFacingDot = 0.0f;

	/** Keeps the current target until another candidate is meaningfully better. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting", meta = (ClampMin = "0.0"))
	float TargetSwitchScoreAdvantage = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Targeting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Minimum interval accepted between authoritative requests from this pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Networking", meta = (ClampMin = "0.0", Units = "s"))
	float ServerRequestCooldown = 0.1f;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestInteraction(AActor* RequestedTarget);

	APawn* GetOwnerPawn() const;
	AActor* FindLocalTarget(FText& OutPrompt) const;
	bool IsValidCandidate(const APawn* InteractingPawn, AActor* Candidate) const;
	float ScoreProximityCandidate(const APawn* InteractingPawn, const AActor* Candidate, const FVector& ViewLocation, const FVector& ViewForward) const;
	FVector GetClosestTargetPoint(const FVector& SourceLocation, const AActor* Target) const;
	bool ValidateServerInteraction(APawn* InteractingPawn, AActor* RequestedTarget) const;
	bool IsTargetInRange(const APawn* InteractingPawn, const AActor* Target) const;
	bool HasLineOfSightToTarget(const APawn* InteractingPawn, const AActor* Target) const;
	void SetCurrentTarget(AActor* NewTarget, const FText& NewPrompt);

	FTimerHandle TargetingTimerHandle;
	TWeakObjectPtr<AActor> CurrentTarget;
	FText CurrentPrompt;
	double LastServerRequestTime = -DBL_MAX;
	bool bInteractionEnabled = true;
};
