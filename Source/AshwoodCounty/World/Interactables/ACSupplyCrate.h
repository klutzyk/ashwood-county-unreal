// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ACInteractableInterface.h"
#include "ACSupplyCrate.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/** First authoritative interactable: a replicated, asset-swappable supply crate. */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API AACSupplyCrate : public AActor, public IACInteractableInterface
{
	GENERATED_BODY()

public:
	AACSupplyCrate();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintPure, Category = "Supply Crate")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Supply Crate")
	void SetInteractionEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Replace the mesh, materials, and relative transform in a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	/** Hinge/presentation pivot. Asset-specific open motion can be overridden in Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LidPivot;

	/** Replace the mesh, materials, and relative transform in a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LidMesh;

	/** Authoritative collision/range bounds; kept independent from cosmetic mesh geometry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crate|Interaction")
	FText OpenPrompt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Crate|Interaction")
	FText ClosePrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Crate|Visuals")
	FRotator ClosedLidRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Crate|Visuals")
	FRotator OpenLidRotation = FRotator(-95.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Crate|Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float SharedInteractionCooldown = 0.25f;

	/** Cosmetic hook; never make authoritative gameplay changes here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Crate|Visuals", meta = (DisplayName = "Open State Visuals Changed"))
	void BP_OnOpenStateVisualsChanged(bool bNowOpen);

private:
	UFUNCTION()
	void OnRep_IsOpen();

	UFUNCTION()
	void OnRep_InteractionEnabled();

	void ApplyOpenStateVisuals();

	UPROPERTY(ReplicatedUsing = OnRep_IsOpen, VisibleInstanceOnly, Category = "Supply Crate")
	bool bIsOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_InteractionEnabled, EditAnywhere, Category = "Supply Crate|Interaction")
	bool bInteractionEnabled = true;

	double LastInteractionServerTime = -DBL_MAX;
};
