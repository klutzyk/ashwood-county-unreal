// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ACDamageVolume.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UDamageType;

/** Timer-driven authoritative test hazard; no per-frame actor scanning. */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API AACDamageVolume : public AActor
{
	GENERATED_BODY()

public:
	AACDamageVolume();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> DamageBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HazardMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> HazardLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageAmount = 25.0f;

	/** Zero means overlap-entry damage only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", Units = "s"))
	float DamageInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bDamageImmediately = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bOneShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	void ApplyDamageToActor(TWeakObjectPtr<AActor> TargetActor);
	void StopDamagingActor(TWeakObjectPtr<AActor> TargetActor);

	TMap<TWeakObjectPtr<AActor>, FTimerHandle> ActiveDamageTimers;
};
