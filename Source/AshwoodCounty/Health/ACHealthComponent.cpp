// Copyright Epic Games, Inc. All Rights Reserved.

#include "Health/ACHealthComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Net/UnrealNetwork.h"

UACHealthComponent::UACHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UACHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		const float OldHealth = CurrentHealth;
		CurrentHealth = FMath::Max(MaxHealth, 1.0f);
		MaxHealth = CurrentHealth;
		bIsDead = false;
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UACHealthComponent::HandleOwnerTakeAnyDamage);

		if (!FMath::IsNearlyEqual(OldHealth, CurrentHealth))
		{
			OnHealthChanged.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
			OnHealthChangedNative.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
		}
	}
}

void UACHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UACHealthComponent, MaxHealth);
	DOREPLIFETIME(UACHealthComponent, CurrentHealth);
	DOREPLIFETIME(UACHealthComponent, bIsDead);
}

float UACHealthComponent::GetHealthNormalized() const
{
	return MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

float UACHealthComponent::ApplyHealing(float Amount, AActor* HealingSource)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsDead || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = CurrentHealth;
	SetHealthInternal(CurrentHealth + Amount, nullptr, HealingSource, nullptr, 0.0f);
	return CurrentHealth - OldHealth;
}

void UACHealthComponent::SetMaxHealth(float NewMaxHealth, bool bRestoreToFull)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float OldMaxHealth = MaxHealth;
	const float OldHealth = CurrentHealth;
	MaxHealth = FMath::Max(NewMaxHealth, 1.0f);
	CurrentHealth = bRestoreToFull ? MaxHealth : FMath::Clamp(CurrentHealth, 0.0f, MaxHealth);

	if (!FMath::IsNearlyEqual(OldMaxHealth, MaxHealth) || !FMath::IsNearlyEqual(OldHealth, CurrentHealth))
	{
		OnHealthChanged.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
		OnHealthChangedNative.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
		Owner->ForceNetUpdate();
	}
}

void UACHealthComponent::ResetToMaxHealth()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	const bool bWasDead = bIsDead;
	CurrentHealth = MaxHealth;
	bIsDead = false;
	OnHealthChanged.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
	OnHealthChangedNative.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
	if (bWasDead)
	{
		OnAliveStateChanged.Broadcast(this, true);
	}
	Owner->ForceNetUpdate();
}

void UACHealthComponent::HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (DamagedActor != GetOwner() || !DamagedActor || !DamagedActor->HasAuthority() || bIsDead || Damage <= 0.0f)
	{
		return;
	}

	SetHealthInternal(CurrentHealth - Damage, InstigatedBy, DamageCauser, const_cast<UDamageType*>(DamageType), Damage);
}

void UACHealthComponent::SetHealthInternal(float NewHealth, AController* InstigatedBy, AActor* ChangeCauser, UDamageType* DamageType, float RequestedDamage)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	if (FMath::IsNearlyEqual(OldHealth, CurrentHealth))
	{
		return;
	}

	BroadcastHealthDelta(OldHealth, InstigatedBy, ChangeCauser, DamageType, RequestedDamage);

	if (!bIsDead && CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		OnAliveStateChanged.Broadcast(this, false);
		OnDeath.Broadcast(this, InstigatedBy, ChangeCauser, DamageType, FMath::Min(RequestedDamage, OldHealth));
	}

	Owner->ForceNetUpdate();
}

void UACHealthComponent::BroadcastHealthDelta(float OldHealth, AController* InstigatedBy, AActor* ChangeCauser, UDamageType* DamageType, float RequestedDamage)
{
	OnHealthChanged.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
	OnHealthChangedNative.Broadcast(this, OldHealth, CurrentHealth, MaxHealth);
	const float Delta = CurrentHealth - OldHealth;
	if (Delta < 0.0f)
	{
		OnDamageReceived.Broadcast(this, -Delta, InstigatedBy, ChangeCauser, DamageType);
	}
	else if (Delta > 0.0f)
	{
		OnHealingReceived.Broadcast(this, Delta, ChangeCauser);
	}
}

void UACHealthComponent::OnRep_MaxHealth(float OldMaxHealth)
{
	if (!FMath::IsNearlyEqual(OldMaxHealth, MaxHealth))
	{
		OnHealthChanged.Broadcast(this, CurrentHealth, CurrentHealth, MaxHealth);
		OnHealthChangedNative.Broadcast(this, CurrentHealth, CurrentHealth, MaxHealth);
	}
}

void UACHealthComponent::OnRep_CurrentHealth(float OldHealth)
{
	BroadcastHealthDelta(OldHealth, nullptr, nullptr, nullptr, FMath::Max(OldHealth - CurrentHealth, 0.0f));
}

void UACHealthComponent::OnRep_IsDead(bool bWasDead)
{
	if (bWasDead == bIsDead)
	{
		return;
	}

	OnAliveStateChanged.Broadcast(this, !bIsDead);
	if (bIsDead)
	{
		OnDeath.Broadcast(this, nullptr, nullptr, nullptr, 0.0f);
	}
}
