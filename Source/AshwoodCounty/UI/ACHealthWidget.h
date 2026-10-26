// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ACHealthWidget.generated.h"

class UACHealthComponent;
class UProgressBar;
class UTextBlock;

/** Event-driven local health display bridge for a designer-owned Widget Blueprint. */
UCLASS(BlueprintType, Blueprintable)
class ASHWOODCOUNTY_API UACHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Health")
	void BindToHealthComponent(UACHealthComponent* HealthComponent);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UFUNCTION(BlueprintImplementableEvent, Category = "Health", meta = (DisplayName = "Health Display Changed"))
	void BP_OnHealthDisplayChanged(float CurrentHealth, float MaxHealth, float NormalizedHealth, bool bIsAlive);

private:
	void HandleHealthChanged(UACHealthComponent* HealthComponent, float OldHealth, float NewHealth, float MaxHealth);

	void RefreshDisplay();
	void UnbindHealthComponent();

	TWeakObjectPtr<UACHealthComponent> BoundHealthComponent;
	FDelegateHandle HealthChangedHandle;
};
