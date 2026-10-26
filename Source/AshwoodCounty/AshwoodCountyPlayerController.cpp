// Copyright Epic Games, Inc. All Rights Reserved.


#include "AshwoodCountyPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "AshwoodCounty.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Interaction/ACInteractionComponent.h"
#include "UI/ACInteractionPromptWidget.h"
#include "Health/ACHealthComponent.h"
#include "UI/ACHealthWidget.h"

void AAshwoodCountyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && InteractionPromptWidgetClass)
	{
		InteractionPromptWidget = CreateWidget<UACInteractionPromptWidget>(this, InteractionPromptWidgetClass);
		if (InteractionPromptWidget)
		{
			InteractionPromptWidget->AddToPlayerScreen(10);
			InteractionPromptWidget->SetInteractionPrompt(FText::GetEmpty(), false);
		}
	}

	if (IsLocalPlayerController() && HealthWidgetClass && !HealthWidget)
	{
		HealthWidget = CreateWidget<UACHealthWidget>(this, HealthWidgetClass);
		if (HealthWidget)
		{
			HealthWidget->AddToPlayerScreen(5);
		}
	}

	BindInteractionSource(GetPawn());
	BindHealthSource(GetPawn());

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogAshwoodCounty, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AAshwoodCountyPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	BindInteractionSource(InPawn);
	BindHealthSource(InPawn);
}

void AAshwoodCountyPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	BindInteractionSource(P);
	BindHealthSource(P);
}

void AAshwoodCountyPlayerController::BindInteractionSource(APawn* SourcePawn)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UACInteractionComponent* PreviousComponent = BoundInteractionComponent.Get())
	{
		PreviousComponent->OnInteractionTargetChanged.RemoveDynamic(this, &AAshwoodCountyPlayerController::HandleInteractionTargetChanged);
	}
	BoundInteractionComponent.Reset();

	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->SetInteractionPrompt(FText::GetEmpty(), false);
	}

	if (!SourcePawn)
	{
		return;
	}

	if (UACInteractionComponent* InteractionComponent = SourcePawn->FindComponentByClass<UACInteractionComponent>())
	{
		InteractionComponent->StartLocalTargeting();
		BoundInteractionComponent = InteractionComponent;
		InteractionComponent->OnInteractionTargetChanged.AddUniqueDynamic(this, &AAshwoodCountyPlayerController::HandleInteractionTargetChanged);
		HandleInteractionTargetChanged(InteractionComponent->GetCurrentTarget(), InteractionComponent->GetCurrentPrompt());
	}
}

void AAshwoodCountyPlayerController::BindHealthSource(APawn* SourcePawn)
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	BoundHealthComponent.Reset();
	UACHealthComponent* NewHealthComponent = SourcePawn ? SourcePawn->FindComponentByClass<UACHealthComponent>() : nullptr;
	BoundHealthComponent = NewHealthComponent;
	if (HealthWidget)
	{
		HealthWidget->BindToHealthComponent(NewHealthComponent);
	}
}

void AAshwoodCountyPlayerController::HandleInteractionTargetChanged(AActor* Target, const FText& Prompt)
{
	if (InteractionPromptWidget)
	{
		InteractionPromptWidget->SetInteractionPrompt(Prompt, IsValid(Target) && !Prompt.IsEmpty());
	}
}

void AAshwoodCountyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AAshwoodCountyPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
