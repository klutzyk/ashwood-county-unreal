// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "AshwoodCountyGameMode.generated.h"

class AAshwoodCountyCharacter;
class AController;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AAshwoodCountyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AAshwoodCountyGameMode();

	void HandlePlayerDeath(AAshwoodCountyCharacter* DeadCharacter);
	virtual void Logout(AController* Exiting) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 3.0f;

private:
	void RespawnPlayer(TWeakObjectPtr<AController> Controller);
	TMap<TWeakObjectPtr<AController>, FTimerHandle> PendingRespawns;
};



