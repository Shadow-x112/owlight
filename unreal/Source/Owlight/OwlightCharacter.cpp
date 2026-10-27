#include "OwlightCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Owlight.h"
#include "UObject/ConstructorHelpers.h"

AOwlightCharacter::AOwlightCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(28.f, 70.f);

	// The character turns to face where it walks; the controller's rotation never turns the body.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = 320.f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->MaxAcceleration = 2000.f;
	Move->GravityScale = 1.4f;
	Move->JumpZVelocity = 0.f;

	// Camera: fixed yaw, tilted down, pulled back, with a little lag so it glides.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 6.f;
	CameraBoom->SetUsingAbsoluteRotation(true);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Placeholder body: a cylinder and a sphere, on the fine pixel grid like the slimes.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -22.f));
	Body->SetRelativeScale3D(FVector(0.44f, 0.44f, 0.92f));
	Body->SetRenderCustomDepth(true);
	Body->SetCustomDepthStencilValue(1);
	if (CylinderMesh.Succeeded()) Body->SetStaticMesh(CylinderMesh.Object);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(RootComponent);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 44.f));
	Head->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.34f));
	Head->SetRenderCustomDepth(true);
	Head->SetCustomDepthStencilValue(1);
	if (SphereMesh.Succeeded()) Head->SetStaticMesh(SphereMesh.Object);
}

void AOwlightCharacter::BeginPlay()
{
	Super::BeginPlay();

	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SetWorldRotation(FRotator(CameraPitch, CameraYaw, 0.f));
	Camera->SetFieldOfView(CameraFOV);

}

void AOwlightCharacter::EnsureInputAssets()
{
	if (MappingContext) return;
	// Input assets built in code: WASD / arrows / left stick -> 2D move axis.
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Owlight"));
	auto MapKey = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swz = NewObject<UInputModifierSwizzleAxis>(this);
			Swz->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swz);
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		}
	};
	// Action value: X = forward/back, Y = right/left. Swizzle moves a key onto Y.
	MapKey(EKeys::W, false, false);
	MapKey(EKeys::S, false, true);
	MapKey(EKeys::Up, false, false);
	MapKey(EKeys::Down, false, true);
	MapKey(EKeys::D, true, false);
	MapKey(EKeys::A, true, true);
	MapKey(EKeys::Right, true, false);
	MapKey(EKeys::Left, true, true);
	// Gamepad: stick Y is forward (X of our value), stick X is right (Y of our value).
	MapKey(EKeys::Gamepad_LeftY, false, false);
	MapKey(EKeys::Gamepad_LeftX, true, false);

}

void AOwlightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnsureInputAssets();
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOwlightCharacter::Move);
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Sub->AddMappingContext(MappingContext, 0);
			}
		}
	}
	else
	{
		UE_LOG(LogOwlight, Warning, TEXT("Owlight needs the Enhanced Input component as the default input component."));
	}
}

void AOwlightCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();   // X = forward, Y = right, relative to the camera's fixed yaw
	const FRotator YawRot(0.f, CameraYaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddMovementInput(Forward, Axis.X);
	AddMovementInput(Right, Axis.Y);
}
