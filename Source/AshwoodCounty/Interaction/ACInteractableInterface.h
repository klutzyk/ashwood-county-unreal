// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ACInteractableInterface.generated.h"

class APawn;

/**
 * Focused contract for actors that can be targeted by an interaction component.
 * Detection and network validation remain the caller's responsibility; the
 * implementation owns only its availability, prompt, and authoritative action.
 */
UINTERFACE(BlueprintType)
class ASHWOODCOUNTY_API UACInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class ASHWOODCOUNTY_API IACInteractableInterface
{
	GENERATED_BODY()

public:
	/** Return whether this pawn may interact with the object right now. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	bool CanInteract(APawn* InteractingPawn) const;

	/** Context-sensitive local prompt, for example "[E] Open Supply Crate". */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	FText GetInteractionPrompt(APawn* InteractingPawn) const;

	/** Execute gameplay behavior. Called only after server-side validation. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Interaction")
	void Interact(APawn* InteractingPawn);
};
