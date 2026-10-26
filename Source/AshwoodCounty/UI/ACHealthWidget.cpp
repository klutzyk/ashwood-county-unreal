// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ACHealthWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Health/ACHealthComponent.h"

#define LOCTEXT_NAMESPACE "ACHealthWidget"

void UACHealthWidget::BindToHealthComponent(UACHealthComponent* HealthComponent)
{
	if (BoundHealthComponent.Get() == HealthComponent)
	{
		RefreshDisplay();
		return;
	}

	UnbindHealthComponent();
	BoundHealthComponent = HealthComponent;
	if (HealthComponent)
	{
		HealthChangedHandle = HealthComponent->OnHealthChangedNative.AddUObject(this, &UACHealthWidget::HandleHealthChanged);
	}
	RefreshDisplay();
}

void UACHealthWidget::NativeDestruct()
{
	UnbindHealthComponent();
	Super::NativeDestruct();
}

void UACHealthWidget::HandleHealthChanged(UACHealthComponent* HealthComponent, float OldHealth, float NewHealth, float MaxHealth)
{
	RefreshDisplay();
}

void UACHealthWidget::RefreshDisplay()
{
	UACHealthComponent* HealthComponent = BoundHealthComponent.Get();
	const float Current = HealthComponent ? HealthComponent->GetCurrentHealth() : 0.0f;
	const float Maximum = HealthComponent ? HealthComponent->GetMaxHealth() : 0.0f;
	const float Normalized = HealthComponent ? HealthComponent->GetHealthNormalized() : 0.0f;

	if (HealthBar)
	{
		HealthBar->SetPercent(Normalized);
	}
	if (HealthText)
	{
		HealthText->SetText(FText::Format(LOCTEXT("HealthFormat", "{0} / {1}"), FMath::RoundToInt(Current), FMath::RoundToInt(Maximum)));
	}

	BP_OnHealthDisplayChanged(Current, Maximum, Normalized, HealthComponent && HealthComponent->IsAlive());
}

void UACHealthWidget::UnbindHealthComponent()
{
	if (UACHealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthComponent->OnHealthChangedNative.Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();
	BoundHealthComponent.Reset();
}

#undef LOCTEXT_NAMESPACE
