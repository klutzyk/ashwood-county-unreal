// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ACInteractionPromptWidget.h"

#include "Components/TextBlock.h"

void UACInteractionPromptWidget::SetInteractionPrompt(const FText& Prompt, bool bVisible)
{
	if (PromptText)
	{
		PromptText->SetText(Prompt);
	}

	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	BP_OnInteractionPromptChanged(Prompt, bVisible);
}
