// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zombie/ACZombieCharacter.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Health/ACHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

AACZombieCharacter::AACZombieCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;

	HealthComponent = CreateDefaultSubobject<UACHealthComponent>(TEXT("HealthComponent"));
}

void AACZombieCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(MovementSpeed, 0.0f);

	if (HealthComponent)
	{
		HealthComponent->OnDamageReceived.AddUniqueDynamic(this, &AACZombieCharacter::HandleDamageReceived);
		HealthComponent->OnDeath.AddUniqueDynamic(this, &AACZombieCharacter::HandleDeath);
	}

	if (HasAuthority() && HealthComponent && HealthComponent->IsAlive())
	{
		GetWorldTimerManager().SetTimer(AIUpdateTimer, this, &AACZombieCharacter::UpdateAI, FMath::Max(AIUpdateInterval, 0.1f), true, 0.05f);
	}
}

void AACZombieCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AIUpdateTimer);
	GetWorldTimerManager().ClearTimer(AttackImpactTimer);
	GetWorldTimerManager().ClearTimer(DirectMovementTimer);
	Super::EndPlay(EndPlayReason);
}

void AACZombieCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AACZombieCharacter, CurrentTarget);
	DOREPLIFETIME(AACZombieCharacter, bZombieDead);
}

bool AACZombieCharacter::IsValidLivingTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || Candidate == this)
	{
		return false;
	}

	const UACHealthComponent* TargetHealth = Candidate->FindComponentByClass<UACHealthComponent>();
	return TargetHealth && TargetHealth->IsAlive();
}

AActor* AACZombieCharacter::FindBestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* BestTarget = nullptr;
	float BestDistanceSquared = FMath::Square(FMath::Max(DetectionRange, 0.0f));
	const AAIController* ZombieController = Cast<AAIController>(GetController());
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		APawn* Candidate = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!IsValidLivingTarget(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared > BestDistanceSquared)
		{
			continue;
		}

		if (bRequireLineOfSightToAcquire && ZombieController && !ZombieController->LineOfSightTo(Candidate))
		{
			continue;
		}

		BestTarget = Candidate;
		BestDistanceSquared = DistanceSquared;
	}
	return BestTarget;
}

void AACZombieCharacter::SetCurrentTarget(AActor* NewTarget)
{
	if (!HasAuthority() || CurrentTarget == NewTarget)
	{
		return;
	}

	AActor* PreviousTarget = CurrentTarget;
	CurrentTarget = NewTarget;
	ConsecutiveStalledUpdates = 0;
	LastMovementCheckLocation = GetActorLocation();
	GetWorldTimerManager().ClearTimer(DirectMovementTimer);
	if (PreviousTarget)
	{
		BP_OnTargetLost(PreviousTarget);
	}
	if (CurrentTarget)
	{
		BP_OnTargetAcquired(CurrentTarget);
	}
	ForceNetUpdate();
}

void AACZombieCharacter::UpdateAI()
{
	if (!HasAuthority() || bZombieDead || !HealthComponent || !HealthComponent->IsAlive())
	{
		return;
	}

	if (CurrentTarget)
	{
		const float LoseRange = DetectionRange * FMath::Max(LoseTargetRangeMultiplier, 1.0f);
		if (!IsValidLivingTarget(CurrentTarget) || FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation()) > FMath::Square(LoseRange))
		{
			SetCurrentTarget(nullptr);
		}
	}
	if (!CurrentTarget)
	{
		SetCurrentTarget(FindBestTarget());
	}

	AAIController* ZombieController = Cast<AAIController>(GetController());
	if (!CurrentTarget || !ZombieController)
	{
		GetWorldTimerManager().ClearTimer(DirectMovementTimer);
		SetAttackFacingEnabled(false);
		if (ZombieController)
		{
			ZombieController->StopMovement();
		}
		return;
	}

	const float DistanceSquared = FVector::DistSquared(GetActorLocation(), CurrentTarget->GetActorLocation());
	if (DistanceSquared <= FMath::Square(FMath::Max(AttackRange, 0.0f)))
	{
		GetWorldTimerManager().ClearTimer(DirectMovementTimer);
		ZombieController->StopMovement();
		SetAttackFacingEnabled(true);
		TryStartAttack();
	}
	else if (!bAttackInProgress)
	{
		SetAttackFacingEnabled(false);

		// Once path following has proven unusable, let the fallback own movement;
		// repeated MoveTo requests would overwrite its requested velocity.
		if (GetWorldTimerManager().IsTimerActive(DirectMovementTimer))
		{
			return;
		}

		// Ignore small path-following jitter; a walking zombie should cover far more
		// than five centimetres during one AI update interval.
		if (FVector::DistSquared2D(GetActorLocation(), LastMovementCheckLocation) < FMath::Square(5.0f))
		{
			++ConsecutiveStalledUpdates;
		}
		else
		{
			ConsecutiveStalledUpdates = 0;
		}
		LastMovementCheckLocation = GetActorLocation();

		const EPathFollowingRequestResult::Type MoveResult = ZombieController->MoveToActor(
			CurrentTarget, FMath::Max(AttackRange * 0.8f, 25.0f), false, true, true, nullptr, true);
		const bool bUseDirectFallback = MoveResult == EPathFollowingRequestResult::Failed
			|| ZombieController->GetMoveStatus() != EPathFollowingStatus::Moving
			|| ConsecutiveStalledUpdates >= 3;
		if (bUseDirectFallback)
		{
			if (!GetWorldTimerManager().IsTimerActive(DirectMovementTimer))
			{
				ZombieController->StopMovement();
				GetWorldTimerManager().SetTimer(DirectMovementTimer, this, &AACZombieCharacter::UpdateDirectMovement, 0.05f, true);
			}
		}
	}
}

void AACZombieCharacter::UpdateDirectMovement()
{
	if (!HasAuthority() || bZombieDead || !IsValidLivingTarget(CurrentTarget))
	{
		GetWorldTimerManager().ClearTimer(DirectMovementTimer);
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.SizeSquared() <= FMath::Square(FMath::Max(AttackRange, 0.0f)))
	{
		GetWorldTimerManager().ClearTimer(DirectMovementTimer);
		return;
	}

	const FVector Direction = ToTarget.GetSafeNormal();
	GetCharacterMovement()->RequestDirectMove(Direction * FMath::Max(MovementSpeed, 0.0f), true);
}

void AACZombieCharacter::SetAttackFacingEnabled(bool bEnabled)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	AAIController* ZombieController = Cast<AAIController>(GetController());
	if (!MovementComponent || !ZombieController)
	{
		return;
	}

	if (bEnabled && IsValid(CurrentTarget))
	{
		ZombieController->SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = true;
	}
	else
	{
		ZombieController->ClearFocus(EAIFocusPriority::Gameplay);
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->bOrientRotationToMovement = true;
	}
}

void AACZombieCharacter::TryStartAttack()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || bZombieDead || bAttackInProgress || !IsValidLivingTarget(CurrentTarget))
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastAttackServerTime < FMath::Max(AttackCooldown, 0.05f))
	{
		return;
	}

	LastAttackServerTime = Now;
	bAttackInProgress = true;
	MulticastAttackStarted(CurrentTarget);
	if (AttackWindup <= 0.0f)
	{
		CommitAttackImpact();
	}
	else
	{
		World->GetTimerManager().SetTimer(AttackImpactTimer, this, &AACZombieCharacter::CommitAttackImpact, AttackWindup, false);
	}
}

void AACZombieCharacter::CommitAttackImpact()
{
	if (!HasAuthority() || bZombieDead || !bAttackInProgress)
	{
		return;
	}

	AActor* Target = CurrentTarget;
	const bool bCanHit = IsValidLivingTarget(Target)
		&& FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(FMath::Max(AttackRange + 25.0f, 0.0f));
	if (bCanHit)
	{
		UGameplayStatics::ApplyDamage(Target, FMath::Max(AttackDamage, 0.0f), GetController(), this, AttackDamageType);
	}
	MulticastAttackImpact(Target, bCanHit);
	FinishAttack();
}

void AACZombieCharacter::FinishAttack()
{
	bAttackInProgress = false;
	GetWorldTimerManager().ClearTimer(AttackImpactTimer);
}

void AACZombieCharacter::HandleDamageReceived(UACHealthComponent* ChangedHealthComponent, float Damage, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType)
{
	if (ChangedHealthComponent == HealthComponent)
	{
		BP_OnDamageReceived(Damage, InstigatedBy, DamageCauser);
	}
}

void AACZombieCharacter::HandleDeath(UACHealthComponent* ChangedHealthComponent, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType, float FinalDamage)
{
	if (!HasAuthority() || ChangedHealthComponent != HealthComponent || bZombieDead)
	{
		return;
	}

	bZombieDead = true;
	StopAllBehavior();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	BP_OnZombieDied(InstigatedBy, DamageCauser);
	ForceNetUpdate();
	if (CorpseCleanupDelay > 0.0f)
	{
		SetLifeSpan(CorpseCleanupDelay);
	}
}

void AACZombieCharacter::StopAllBehavior()
{
	GetWorldTimerManager().ClearTimer(AIUpdateTimer);
	GetWorldTimerManager().ClearTimer(AttackImpactTimer);
	GetWorldTimerManager().ClearTimer(DirectMovementTimer);
	bAttackInProgress = false;
	SetAttackFacingEnabled(false);
	if (AAIController* ZombieController = Cast<AAIController>(GetController()))
	{
		ZombieController->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetCurrentTarget(nullptr);
}

void AACZombieCharacter::OnRep_CurrentTarget(AActor* PreviousTarget)
{
	if (PreviousTarget)
	{
		BP_OnTargetLost(PreviousTarget);
	}
	if (CurrentTarget)
	{
		BP_OnTargetAcquired(CurrentTarget);
	}
}

void AACZombieCharacter::OnRep_ZombieDead()
{
	if (bZombieDead)
	{
		StopAllBehavior();
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		BP_OnZombieDied(nullptr, nullptr);
	}
}

void AACZombieCharacter::MulticastAttackStarted_Implementation(AActor* Target)
{
	BP_OnAttackStarted(Target);
}

void AACZombieCharacter::MulticastAttackImpact_Implementation(AActor* Target, bool bHit)
{
	BP_OnAttackImpact(Target, bHit);
}
