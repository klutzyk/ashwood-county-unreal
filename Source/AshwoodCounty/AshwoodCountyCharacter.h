// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AshwoodCountyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UACInteractionComponent;
class UACHealthComponent;
class AController;
class UDamageType;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AAshwoodCountyCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Reusable interaction capability; object-specific behavior lives on targets. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UACInteractionComponent* InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UACHealthComponent* HealthComponent;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Contextual world interaction action (E by default in IMC_Default). */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractionAction;

	/** Basic combat action (left mouse by default). */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MeleeAttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "0.0"))
	float MeleeAttackDamage = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeAttackRange = 225.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "0.0", Units = "cm"))
	float MeleeAttackRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "0.05", Units = "s"))
	float MeleeAttackCooldown = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Melee")
	TSubclassOf<UDamageType> MeleeDamageType;

public:

	/** Constructor */
	AAshwoodCountyCharacter();	

	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Route input to the reusable interaction component. */
	void Interact();
	void MeleeAttackInput();

	UFUNCTION()
	void HandleDeath(UACHealthComponent* ChangedHealthComponent, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType, float FinalDamage);

	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "Player Died"))
	void BP_OnPlayerDied(AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Melee", meta = (DisplayName = "Melee Attack Started"))
	void BP_OnMeleeAttackStarted();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Melee", meta = (DisplayName = "Melee Attack Impact"))
	void BP_OnMeleeAttackImpact(AActor* HitActor);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns interaction component subobject. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FORCEINLINE UACInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "Health")
	UACHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const;

	/** Requests the trusted server-side melee trace; safe to call from an owning client Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Melee")
	void RequestMeleeAttack();

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestMeleeAttack();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeAttackStarted();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMeleeAttackImpact(AActor* HitActor);

	void PerformServerMeleeAttack();

	bool bDeathHandled = false;
	double LastMeleeAttackServerTime = -DBL_MAX;
};

