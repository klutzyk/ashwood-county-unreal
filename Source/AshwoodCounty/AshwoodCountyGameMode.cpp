// Copyright Epic Games, Inc. All Rights Reserved.

#include "AshwoodCountyGameMode.h"

#include "AshwoodCountyCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

AAshwoodCountyGameMode::AAshwoodCountyGameMode()
{
	// stub
}

void AAshwoodCountyGameMode::HandlePlayerDeath(AAshwoodCountyCharacter* DeadCharacter)
{
	if (!HasAuthority() || !IsValid(DeadCharacter))
	{
		return;
	}

	AController* Controller = DeadCharacter->GetController();
	UWorld* World = GetWorld();
	if (!IsValid(Controller) || !World)
	{
		return;
	}

	const TWeakObjectPtr<AController> ControllerKey(Controller);
	if (FTimerHandle* ExistingTimer = PendingRespawns.Find(ControllerKey))
	{
		World->GetTimerManager().ClearTimer(*ExistingTimer);
	}

	FTimerHandle& RespawnTimer = PendingRespawns.FindOrAdd(ControllerKey);
	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindUObject(this, &AAshwoodCountyGameMode::RespawnPlayer, ControllerKey);
	World->GetTimerManager().SetTimer(RespawnTimer, RespawnDelegate, FMath::Max(RespawnDelay, 0.01f), false);
}

void AAshwoodCountyGameMode::RespawnPlayer(TWeakObjectPtr<AController> Controller)
{
	AController* ValidController = Controller.Get();
	PendingRespawns.Remove(Controller);
	if (!IsValid(ValidController) || ValidController->IsPendingKillPending())
	{
		return;
	}

	APawn* DeadPawn = ValidController->GetPawn();
	ValidController->UnPossess();
	if (IsValid(DeadPawn))
	{
		DeadPawn->Destroy();
	}
	RestartPlayer(ValidController);
}

void AAshwoodCountyGameMode::Logout(AController* Exiting)
{
	const TWeakObjectPtr<AController> ControllerKey(Exiting);
	if (FTimerHandle* RespawnTimer = PendingRespawns.Find(ControllerKey))
	{
		GetWorldTimerManager().ClearTimer(*RespawnTimer);
		PendingRespawns.Remove(ControllerKey);
	}
	Super::Logout(Exiting);
}
