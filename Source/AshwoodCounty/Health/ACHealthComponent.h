// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ACHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FACHealthChangedSignature, UACHealthComponent*, HealthComponent, float, OldHealth, float, NewHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FACDamageReceivedSignature, UACHealthComponent*, HealthComponent, float, Damage, AController*, InstigatedBy, AActor*, DamageCauser, UDamageType*, DamageType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FACHealingReceivedSignature, UACHealthComponent*, HealthComponent, float, Healing, AActor*, HealingSource);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FACDeathSignature, UACHealthComponent*, HealthComponent, AController*, InstigatedBy, AActor*, DamageCauser, UDamageType*, DamageType, float, FinalDamage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FACAliveStateChangedSignature, UACHealthComponent*, HealthComponent, bool, bIsAlive);
DECLARE_MULTICAST_DELEGATE_FourParams(FACHealthChangedNativeSignature, UACHealthComponent*, float, float, float);

/**
 * Reusable server-authoritative health state for players, NPCs, animals, and props.
 * Damage enters through Unreal's standard AActor damage pipeline; replicated state
 * drives presentation on clients and supports late joiners.
 */
UCLASS(ClassGroup = (AshwoodCounty), BlueprintType, meta = (BlueprintSpawnableComponent))
class ASHWOODCOUNTY_API UACHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UACHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return !bIsDead; }

	/** Server-only healing. Returns the amount actually restored. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	float ApplyHealing(float Amount, AActor* HealingSource = nullptr);

	/** Server-only designer/runtime maximum-health change. Current health is clamped. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void SetMaxHealth(float NewMaxHealth, bool bRestoreToFull = false);

	/** Server-only reset for pooled/reused actors. New player pawns begin full automatically. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void ResetToMaxHealth();

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FACHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FACDamageReceivedSignature OnDamageReceived;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FACHealingReceivedSignature OnHealingReceived;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FACDeathSignature OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health|Events")
	FACAliveStateChangedSignature OnAliveStateChanged;

	/** Native presentation hook. Blueprint consumers should use OnHealthChanged. */
	FACHealthChangedNativeSignature OnHealthChangedNative;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_MaxHealth, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

private:
	UFUNCTION()
	void HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void OnRep_MaxHealth(float OldMaxHealth);

	UFUNCTION()
	void OnRep_CurrentHealth(float OldHealth);

	UFUNCTION()
	void OnRep_IsDead(bool bWasDead);

	void SetHealthInternal(float NewHealth, AController* InstigatedBy, AActor* ChangeCauser, UDamageType* DamageType, float RequestedDamage);
	void BroadcastHealthDelta(float OldHealth, AController* InstigatedBy, AActor* ChangeCauser, UDamageType* DamageType, float RequestedDamage);

	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth, VisibleInstanceOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, VisibleInstanceOnly, Category = "Health")
	bool bIsDead = false;
};
