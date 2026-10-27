#include "OwlightCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "FoodStockActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Owlight.h"
#include "SlimeActor.h"
#include "SlimeComponent.h"
#include "UObject/ConstructorHelpers.h"

AOwlightCharacter::AOwlightCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	ApplyDevTimeScale();
}

void AOwlightCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFocus();
}

// ---- input --------------------------------------------------------------------------------

void AOwlightCharacter::EnsureInputAssets()
{
	if (MappingContext) return;
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	FeedAction = NewObject<UInputAction>(this, TEXT("IA_Feed"));
	CycleFoodAction = NewObject<UInputAction>(this, TEXT("IA_CycleFood"));
	DevTimeAction = NewObject<UInputAction>(this, TEXT("IA_DevTime"));

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Owlight"));
	auto MapMove = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swz = NewObject<UInputModifierSwizzleAxis>(this);
			Swz->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swz);
		}
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	};
	// Action value: X = forward/back, Y = right/left. Swizzle moves a key onto Y.
	MapMove(EKeys::W, false, false);  MapMove(EKeys::S, false, true);
	MapMove(EKeys::Up, false, false); MapMove(EKeys::Down, false, true);
	MapMove(EKeys::D, true, false);   MapMove(EKeys::A, true, true);
	MapMove(EKeys::Right, true, false); MapMove(EKeys::Left, true, true);
	MapMove(EKeys::Gamepad_LeftY, false, false);
	MapMove(EKeys::Gamepad_LeftX, true, false);

	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
	MappingContext->MapKey(FeedAction, EKeys::F);
	MappingContext->MapKey(FeedAction, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(CycleFoodAction, EKeys::Q);
	MappingContext->MapKey(CycleFoodAction, EKeys::Gamepad_FaceButton_Top);
	MappingContext->MapKey(DevTimeAction, EKeys::T);
	MappingContext->MapKey(DevTimeAction, EKeys::Gamepad_Special_Right);
}

void AOwlightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogOwlight, Warning, TEXT("Owlight needs the Enhanced Input component as the default input component."));
		return;
	}
	EnsureInputAssets();
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOwlightCharacter::Move);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AOwlightCharacter::Interact);
	EIC->BindAction(FeedAction, ETriggerEvent::Started, this, &AOwlightCharacter::FeedSelected);
	EIC->BindAction(CycleFoodAction, ETriggerEvent::Started, this, &AOwlightCharacter::CycleFood);
	EIC->BindAction(DevTimeAction, ETriggerEvent::Started, this, &AOwlightCharacter::CycleDevTime);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(MappingContext, 0);
		}
	}
}

void AOwlightCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();   // X = forward, Y = right, relative to the camera's fixed yaw
	const FRotator YawRot(0.f, CameraYaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.X);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.Y);
}

// ---- hands --------------------------------------------------------------------------------

void AOwlightCharacter::UpdateFocus()
{
	FocusSlime = nullptr; FocusStock = nullptr;
	if (Carried) return;   // hands full: the prompt is "put down"
	const FVector Me = GetActorLocation();
	const FVector Fwd = GetActorForwardVector();
	float Best = Reach;
	for (TActorIterator<ASlimeActor> It(GetWorld()); It; ++It)
	{
		const FVector D = It->GetActorLocation() - Me;
		const float Dist = D.Size2D();
		// in reach and roughly in front (or very close)
		if (Dist < Best && (Dist < 60.f || FVector::DotProduct(D.GetSafeNormal2D(), Fwd) > 0.2f)) { Best = Dist; FocusSlime = *It; }
	}
	for (TActorIterator<AFoodStockActor> It(GetWorld()); It; ++It)
	{
		const FVector D = It->GetActorLocation() - Me;
		const float Dist = D.Size2D();
		if (Dist < Best && (Dist < 60.f || FVector::DotProduct(D.GetSafeNormal2D(), Fwd) > 0.2f)) { Best = Dist; FocusStock = *It; FocusSlime = nullptr; }
	}
}

void AOwlightCharacter::Interact()
{
	if (Carried)
	{
		// set it down just ahead, on the ground
		const FVector Start = GetActorLocation() + GetActorForwardVector() * 70.f + FVector(0, 0, 50.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PutDown), false, this);
		Params.AddIgnoredActor(Carried);
		FVector Ground = Start - FVector(0, 0, 200.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Ground, ECC_Visibility, Params)) Ground = Hit.Location;
		Carried->PutDown(Ground);
		Carried = nullptr;
		return;
	}
	if (FocusStock)
	{
		const FName Food = FocusStock->FoodId;
		AddFood(Food, FocusStock->Take());
		return;
	}
	if (FocusSlime)
	{
		Carried = FocusSlime;
		Carried->PickUp(this);
		FocusSlime = nullptr;
	}
}

void AOwlightCharacter::FeedSelected()
{
	ASlimeActor* Target = Carried ? Carried.Get() : FocusSlime.Get();
	if (!Target || SelectedFood.IsNone()) return;
	int32* Count = Pockets.Find(SelectedFood);
	if (!Count || *Count <= 0) return;
	if (Target->Slime->Feed(SelectedFood))
	{
		if (--(*Count) <= 0) { Pockets.Remove(SelectedFood); CycleFood(); }
	}
}

void AOwlightCharacter::CycleFood()
{
	TArray<FName> Keys; Pockets.GetKeys(Keys);
	if (Keys.Num() == 0) { SelectedFood = NAME_None; return; }
	Keys.Sort(FNameLexicalLess());
	const int32 i = Keys.IndexOfByKey(SelectedFood);
	SelectedFood = Keys[(i + 1) % Keys.Num()];
}

void AOwlightCharacter::AddFood(FName FoodId, int32 Count)
{
	if (Count <= 0) return;
	Pockets.FindOrAdd(FoodId) += Count;
	if (SelectedFood.IsNone()) SelectedFood = FoodId;
}

void AOwlightCharacter::CycleDevTime()
{
	DevTimeScale = DevTimeScale < 30.f ? 60.f : (DevTimeScale < 1000.f ? 3600.f : 1.f);
	ApplyDevTimeScale();
}

void AOwlightCharacter::ApplyDevTimeScale()
{
	for (TActorIterator<ASlimeActor> It(GetWorld()); It; ++It) It->Slime->TimeScale = DevTimeScale;
}

FText AOwlightCharacter::PromptText() const
{
	if (Carried) return FText::FromString(FString::Printf(TEXT("E  put %s down"), *Carried->Slime->SlimeName));
	if (FocusStock) return FText::FromString(FString::Printf(TEXT("E  take %s"), *FocusStock->Label().ToString()));
	if (FocusSlime)
	{
		FString S = FString::Printf(TEXT("E  pick up %s"), *FocusSlime->Slime->SlimeName);
		if (!SelectedFood.IsNone()) S += FString::Printf(TEXT("     F  feed %s"), *SelectedFood.ToString().Replace(TEXT("_"), TEXT(" ")));
		return FText::FromString(S);
	}
	return FText::GetEmpty();
}
