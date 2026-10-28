// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ACZombieCharacter.generated.h"

class UACHealthComponent;
class UDamageType;

/**
 * Lightweight server-driven zombie foundation. Blueprints own appearance,
 * animation, audio, and VFX while native code owns targeting and damage.
 */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API AACZombieCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AACZombieCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Zombie|Health")
	UACHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Zombie|AI")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category = "Zombie|State")
	bool IsZombieDead() const { return bZombieDead; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Combat")
	bool IsAttackInProgress() const { return bAttackInProgress; }

	/** Server-only attack hit moment. Intended for a future server animation notify. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Zombie|Combat")
	void CommitAttackImpact();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UACHealthComponent> HealthComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MovementSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|AI", meta = (ClampMin = "100.0", Units = "cm"))
	float DetectionRange = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|AI", meta = (ClampMin = "1.0"))
	float LoseTargetRangeMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|AI", meta = (ClampMin = "0.1", Units = "s"))
	float AIUpdateInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|AI")
	bool bRequireLineOfSightToAcquire = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Combat", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Combat", meta = (ClampMin = "0.05", Units = "s"))
	float AttackCooldown = 1.25f;

	/** Time between Attack Started and the authoritative hit check. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Combat", meta = (ClampMin = "0.0", Units = "s"))
	float AttackWindup = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Combat")
	TSubclassOf<UDamageType> AttackDamageType;

	/** Zero keeps the corpse indefinitely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie|Death", meta = (ClampMin = "0.0", Units = "s"))
	float CorpseCleanupDelay = 10.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|AI", meta = (DisplayName = "Target Acquired"))
	void BP_OnTargetAcquired(AActor* NewTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|AI", meta = (DisplayName = "Target Lost"))
	void BP_OnTargetLost(AActor* PreviousTarget);

	/** Cosmetic only; authoritative damage is committed by native server code. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Combat", meta = (DisplayName = "Attack Started"))
	void BP_OnAttackStarted(AActor* Target);

	/** Cosmetic hit-moment hook. It cannot be called remotely by a client. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Combat", meta = (DisplayName = "Attack Impact"))
	void BP_OnAttackImpact(AActor* Target, bool bHit);

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Health", meta = (DisplayName = "Damage Received"))
	void BP_OnDamageReceived(float Damage, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Zombie|Death", meta = (DisplayName = "Zombie Died"))
	void BP_OnZombieDied(AController* InstigatedBy, AActor* DamageCauser);

private:
	void UpdateAI();
	AActor* FindBestTarget() const;
	bool IsValidLivingTarget(const AActor* Candidate) const;
	void SetCurrentTarget(AActor* NewTarget);
	void TryStartAttack();
	void FinishAttack();
	void UpdateDirectMovement();
	void SetAttackFacingEnabled(bool bEnabled);
	void StopAllBehavior();

	UFUNCTION()
	void HandleDamageReceived(UACHealthComponent* ChangedHealthComponent, float Damage, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType);

	UFUNCTION()
	void HandleDeath(UACHealthComponent* ChangedHealthComponent, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType, float FinalDamage);

	UFUNCTION()
	void OnRep_CurrentTarget(AActor* PreviousTarget);

	UFUNCTION()
	void OnRep_ZombieDead();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttackStarted(AActor* Target);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttackImpact(AActor* Target, bool bHit);

	UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget, BlueprintReadOnly, Category = "Zombie|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieDead, BlueprintReadOnly, Category = "Zombie|State", meta = (AllowPrivateAccess = "true"))
	bool bZombieDead = false;

	FTimerHandle AIUpdateTimer;
	FTimerHandle AttackImpactTimer;
	FTimerHandle DirectMovementTimer;
	double LastAttackServerTime = -DBL_MAX;
	FVector LastMovementCheckLocation = FVector::ZeroVector;
	int32 ConsecutiveStalledUpdates = 0;
	bool bAttackInProgress = false;
};
