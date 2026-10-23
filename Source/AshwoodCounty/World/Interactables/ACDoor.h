// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ACInteractableInterface.h"
#include "ACDoor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EACDoorState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

USTRUCT()
struct FACDoorReplicatedState
{
	GENERATED_BODY()

	UPROPERTY()
	EACDoorState State = EACDoorState::Closed;

	UPROPERTY()
	float TransitionStartServerTime = 0.0f;
};

/** Replicated, asset-swappable hinged door using the shared interaction contract. */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API AACDoor : public AActor, public IACInteractableInterface
{
	GENERATED_BODY()

public:
	AACDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintPure, Category = "Door")
	EACDoorState GetDoorState() const { return DoorReplicatedState.State; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Door")
	void SetInteractionEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Cosmetic frame; replace its mesh, material, and transform in a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorFrameMesh;

	/** Place this component at the desired pivot, then align the leaf relative to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> DoorHinge;

	/** Cosmetic leaf; gameplay collision is kept on the separate box below. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorLeafMesh;

	/** Moving, asset-independent collision. Align it with the replacement leaf. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DoorLeafCollision;

	/** Broad query shape used for reliable view and proximity targeting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionCollision;

	/** Signed yaw from closed to open; use the sign to choose opening direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Animation", meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
	float OpeningAngle = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Animation", meta = (ClampMin = "0.05", Units = "s"))
	float OpeningDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Interaction")
	FText OpenPrompt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Interaction")
	FText ClosePrompt;

	/** Cosmetic notification only; authoritative behavior remains native. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Door|Visuals", meta = (DisplayName = "Door State Visuals Changed"))
	void BP_OnDoorStateVisualsChanged(EACDoorState NewState);

private:
	UFUNCTION()
	void OnRep_DoorReplicatedState();

	UFUNCTION()
	void OnRep_InteractionEnabled();

	void BeginTransition(EACDoorState TransitionState);
	void ApplyDoorStateAtTime(float ServerTime);
	void ApplyDoorAlpha(float OpenAlpha);
	float GetSynchronizedServerTime() const;

	UPROPERTY(ReplicatedUsing = OnRep_DoorReplicatedState, VisibleInstanceOnly, Category = "Door")
	FACDoorReplicatedState DoorReplicatedState;

	UPROPERTY(ReplicatedUsing = OnRep_InteractionEnabled, EditAnywhere, Category = "Door|Interaction")
	bool bInteractionEnabled = true;

	FRotator ClosedHingeRotation = FRotator::ZeroRotator;
};
