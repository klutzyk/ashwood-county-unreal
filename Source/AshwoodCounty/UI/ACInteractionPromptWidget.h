// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ACInteractionPromptWidget.generated.h"

class UTextBlock;

/** Minimal native bridge for a designer-owned interaction prompt Widget Blueprint. */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API UACInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionPrompt(const FText& Prompt, bool bVisible);

protected:
	/** Optional so child widgets may provide their own presentation through the event instead. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction", meta = (DisplayName = "Interaction Prompt Changed"))
	void BP_OnInteractionPromptChanged(const FText& Prompt, bool bVisible);
};
