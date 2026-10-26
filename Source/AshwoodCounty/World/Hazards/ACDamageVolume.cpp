// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/Hazards/ACDamageVolume.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Health/ACHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AACDamageVolume::AACDamageVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DamageBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBounds"));
	DamageBounds->SetupAttachment(SceneRoot);
	DamageBounds->SetBoxExtent(FVector(100.0f, 100.0f, 25.0f));
	DamageBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	DamageBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageBounds->SetCollisionObjectType(ECC_WorldDynamic);
	DamageBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DamageBounds->SetGenerateOverlapEvents(true);
	DamageBounds->OnComponentBeginOverlap.AddUniqueDynamic(this, &AACDamageVolume::HandleBeginOverlap);
	DamageBounds->OnComponentEndOverlap.AddUniqueDynamic(this, &AACDamageVolume::HandleEndOverlap);

	HazardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HazardMesh"));
	HazardMesh->SetupAttachment(SceneRoot);
	HazardMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f));
	HazardMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 0.5f));
	HazardMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		HazardMesh->SetStaticMesh(CubeMesh.Object);
	}

	HazardLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HazardLabel"));
	HazardLabel->SetupAttachment(SceneRoot);
	HazardLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	HazardLabel->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	HazardLabel->SetHorizontalAlignment(EHTA_Center);
	HazardLabel->SetWorldSize(32.0f);
	HazardLabel->SetTextRenderColor(FColor::Red);
	HazardLabel->SetText(FText::FromString(TEXT("DANGER - DAMAGE")));
}

void AACDamageVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<TWeakObjectPtr<AActor>, FTimerHandle>& Entry : ActiveDamageTimers)
		{
			World->GetTimerManager().ClearTimer(Entry.Value);
		}
	}
	ActiveDamageTimers.Empty();
	Super::EndPlay(EndPlayReason);
}

void AACDamageVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == this || ActiveDamageTimers.Contains(OtherActor))
	{
		return;
	}

	UACHealthComponent* HealthComponent = OtherActor->FindComponentByClass<UACHealthComponent>();
	if (!HealthComponent || !HealthComponent->IsAlive())
	{
		return;
	}

	if (bDamageImmediately)
	{
		ApplyDamageToActor(OtherActor);
	}

	if (!bOneShot && DamageInterval > 0.0f && HealthComponent->IsAlive())
	{
		FTimerHandle& Timer = ActiveDamageTimers.FindOrAdd(OtherActor);
		FTimerDelegate DamageDelegate;
		DamageDelegate.BindUObject(this, &AACDamageVolume::ApplyDamageToActor, TWeakObjectPtr<AActor>(OtherActor));
		GetWorldTimerManager().SetTimer(Timer, DamageDelegate, FMath::Max(DamageInterval, 0.01f), true);
	}
}

void AACDamageVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	if (HasAuthority())
	{
		StopDamagingActor(TWeakObjectPtr<AActor>(OtherActor));
	}
}

void AACDamageVolume::ApplyDamageToActor(TWeakObjectPtr<AActor> TargetActor)
{
	AActor* Target = TargetActor.Get();
	if (!HasAuthority() || !IsValid(Target) || !DamageBounds->IsOverlappingActor(Target))
	{
		StopDamagingActor(TargetActor);
		return;
	}

	UACHealthComponent* HealthComponent = Target->FindComponentByClass<UACHealthComponent>();
	if (!HealthComponent || !HealthComponent->IsAlive())
	{
		StopDamagingActor(TargetActor);
		return;
	}

	UGameplayStatics::ApplyDamage(Target, FMath::Max(DamageAmount, 0.0f), GetInstigatorController(), this, DamageTypeClass);
	if (!HealthComponent->IsAlive())
	{
		StopDamagingActor(TargetActor);
	}
}

void AACDamageVolume::StopDamagingActor(TWeakObjectPtr<AActor> TargetActor)
{
	if (FTimerHandle* Timer = ActiveDamageTimers.Find(TargetActor))
	{
		GetWorldTimerManager().ClearTimer(*Timer);
		ActiveDamageTimers.Remove(TargetActor);
	}
}
