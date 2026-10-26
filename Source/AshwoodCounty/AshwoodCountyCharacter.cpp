// Copyright Epic Games, Inc. All Rights Reserved.

#include "AshwoodCountyCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AshwoodCounty.h"
#include "Interaction/ACInteractionComponent.h"
#include "Health/ACHealthComponent.h"
#include "AshwoodCountyGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AAshwoodCountyCharacter::AAshwoodCountyCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	InteractionComponent = CreateDefaultSubobject<UACInteractionComponent>(TEXT("InteractionComponent"));
	HealthComponent = CreateDefaultSubobject<UACHealthComponent>(TEXT("HealthComponent"));

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AAshwoodCountyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AAshwoodCountyCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AAshwoodCountyCharacter::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAshwoodCountyCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AAshwoodCountyCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAshwoodCountyCharacter::Look);

		if (InteractionAction)
		{
			EnhancedInputComponent->BindAction(InteractionAction, ETriggerEvent::Started, this, &AAshwoodCountyCharacter::Interact);
		}
		if (MeleeAttackAction)
		{
			EnhancedInputComponent->BindAction(MeleeAttackAction, ETriggerEvent::Started, this, &AAshwoodCountyCharacter::MeleeAttackInput);
		}
	}
	else
	{
		UE_LOG(LogAshwoodCounty, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AAshwoodCountyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AAshwoodCountyCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &AAshwoodCountyCharacter::HandleDeath);
	}
}

void AAshwoodCountyCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (InteractionComponent)
	{
		InteractionComponent->StartLocalTargeting();
	}
}

void AAshwoodCountyCharacter::Interact()
{
	if (IsAlive() && InteractionComponent)
	{
		InteractionComponent->RequestInteraction();
	}
}

void AAshwoodCountyCharacter::MeleeAttackInput()
{
	RequestMeleeAttack();
}

void AAshwoodCountyCharacter::RequestMeleeAttack()
{
	if (!IsAlive() || !IsLocallyControlled())
	{
		return;
	}
	if (HasAuthority())
	{
		PerformServerMeleeAttack();
	}
	else
	{
		ServerRequestMeleeAttack();
	}
}

void AAshwoodCountyCharacter::ServerRequestMeleeAttack_Implementation()
{
	PerformServerMeleeAttack();
}

void AAshwoodCountyCharacter::PerformServerMeleeAttack()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || !IsAlive())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastMeleeAttackServerTime < FMath::Max(MeleeAttackCooldown, 0.05f))
	{
		return;
	}
	LastMeleeAttackServerTime = Now;
	MulticastMeleeAttackStarted();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetActorEyesViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * FMath::Max(MeleeAttackRange, 0.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerMeleeAttack), false, this);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, ViewLocation, TraceEnd, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(FMath::Max(MeleeAttackRadius, 0.0f)), QueryParams);
	Hits.Sort([&ViewLocation](const FHitResult& A, const FHitResult& B)
	{
		return FVector::DistSquared(ViewLocation, A.ImpactPoint) < FVector::DistSquared(ViewLocation, B.ImpactPoint);
	});

	AActor* HitActor = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		UACHealthComponent* CandidateHealth = Candidate ? Candidate->FindComponentByClass<UACHealthComponent>() : nullptr;
		if (Candidate != this && CandidateHealth && CandidateHealth->IsAlive())
		{
			HitActor = Candidate;
			break;
		}
	}

	if (HitActor)
	{
		UGameplayStatics::ApplyDamage(HitActor, FMath::Max(MeleeAttackDamage, 0.0f), GetController(), this, MeleeDamageType);
	}
	MulticastMeleeAttackImpact(HitActor);
}

void AAshwoodCountyCharacter::MulticastMeleeAttackStarted_Implementation()
{
	BP_OnMeleeAttackStarted();
}

void AAshwoodCountyCharacter::MulticastMeleeAttackImpact_Implementation(AActor* HitActor)
{
	BP_OnMeleeAttackImpact(HitActor);
}

void AAshwoodCountyCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AAshwoodCountyCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AAshwoodCountyCharacter::DoMove(float Right, float Forward)
{
	if (IsAlive() && GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AAshwoodCountyCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AAshwoodCountyCharacter::DoJumpStart()
{
	if (IsAlive())
	{
		Jump();
	}
}

void AAshwoodCountyCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

bool AAshwoodCountyCharacter::IsAlive() const
{
	return !HealthComponent || HealthComponent->IsAlive();
}

void AAshwoodCountyCharacter::HandleDeath(UACHealthComponent* ChangedHealthComponent, AController* InstigatedBy, AActor* DamageCauser, UDamageType* DamageType, float FinalDamage)
{
	if (bDeathHandled || ChangedHealthComponent != HealthComponent)
	{
		return;
	}
	bDeathHandled = true;

	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (InteractionComponent)
	{
		InteractionComponent->SetInteractionEnabled(false);
	}
	BP_OnPlayerDied(InstigatedBy, DamageCauser);

	if (HasAuthority())
	{
		if (AAshwoodCountyGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAshwoodCountyGameMode>() : nullptr)
		{
			GameMode->HandlePlayerDeath(this);
		}
	}
}
