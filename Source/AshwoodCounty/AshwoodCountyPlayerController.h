// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AshwoodCountyPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UACInteractionComponent;
class UACInteractionPromptWidget;
class UACHealthComponent;
class UACHealthWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AAshwoodCountyPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Local-only contextual prompt presentation. Set to a designer Widget Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|UI")
	TSubclassOf<UACInteractionPromptWidget> InteractionPromptWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UACInteractionPromptWidget> InteractionPromptWidget;

	UPROPERTY(EditDefaultsOnly, Category = "Health|UI")
	TSubclassOf<UACHealthWidget> HealthWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UACHealthWidget> HealthWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	virtual void SetPawn(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
	void BindInteractionSource(APawn* SourcePawn);
	void BindHealthSource(APawn* SourcePawn);

	UFUNCTION()
	void HandleInteractionTargetChanged(AActor* Target, const FText& Prompt);

	TWeakObjectPtr<UACInteractionComponent> BoundInteractionComponent;
	TWeakObjectPtr<UACHealthComponent> BoundHealthComponent;

};
