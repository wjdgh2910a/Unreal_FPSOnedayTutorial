// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unreal_FPSTutorialCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AUnreal_FPSTutorialCharacter

AUnreal_FPSTutorialCharacter::AUnreal_FPSTutorialCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom,FName(TEXT("HeadCamera"))); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(GetMesh(), FName(TEXT("WeaponSocket")));
}

void AUnreal_FPSTutorialCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
	Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(TEXT("WeaponSocket")));
}

//////////////////////////////////////////////////////////////////////////
// Input

void AUnreal_FPSTutorialCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AUnreal_FPSTutorialCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AUnreal_FPSTutorialCharacter::Look);

		// Fire
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AUnreal_FPSTutorialCharacter::Fire);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AUnreal_FPSTutorialCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AUnreal_FPSTutorialCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AUnreal_FPSTutorialCharacter::Fire(const FInputActionValue& Value)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance != nullptr && AnimInstance->IsAnyMontagePlaying() == false)
	{
		AnimInstance->Montage_Play(FireMontage);
		FVector2D ViewPortSize;
		GEngine->GameViewport->GetViewportSize(ViewPortSize);
		ViewPortSize /= 2; // 화면 중앙 좌표 계산

		FVector WorldLocation;
		FVector WorldDirection;
		APlayerController* PlayerController = Cast<APlayerController>(Controller);

		if (PlayerController)
		{
			// 화면 중앙의 스크린 좌표를 월드 좌표와 방향으로 변환
			PlayerController->DeprojectScreenPositionToWorld(ViewPortSize.X, ViewPortSize.Y, WorldLocation, WorldDirection);

			FCollisionQueryParams TraceParams(FName(TEXT("TraceParam")), true, this);
			FHitResult HitResult;
			bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, WorldLocation, WorldLocation + WorldDirection * 5000.0f, ECollisionChannel::ECC_Visibility, TraceParams);

			if (bHit)
			{
				FVector FireLocation = Weapon->GetSocketLocation(FName(TEXT("Muzzle")));

				// 디버그용으로 라인과 점을 그리기
				DrawDebugLine(GetWorld(), FireLocation, HitResult.ImpactPoint, FColor::Green, false, 2.0f);
				DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.0f, FColor::Red, false, 2.0f);

				// 힘을 가할 방향을 계산
				FVector ForceDirection = HitResult.ImpactPoint - FireLocation;
				ForceDirection.Normalize(); // 방향 벡터 정규화

				// 충돌된 물체의 컴포넌트를 가져옴
				UPrimitiveComponent* HitComponent = HitResult.GetComponent();

				if (HitComponent && HitComponent->IsSimulatingPhysics())
				{
					// 물리적인 힘을 가함
					float ForceStrength = 50000.0f;
					HitComponent->AddForce(ForceDirection * ForceStrength, NAME_None, true);
				}
			}
		}
	}
}

void AUnreal_FPSTutorialCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector force = GetActorTransform().InverseTransformVector(GetCharacterMovement()->Velocity);
	MoveForce.X = force.X;
	MoveForce.Y = force.Y;
}
