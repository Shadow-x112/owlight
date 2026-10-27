#include "PenActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "SlimeComponent.h"
#include "UObject/ConstructorHelpers.h"

APenActor::APenActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Area->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/PolyHaven/Models/dead_tree_trunk/MI_dead_tree_trunk"));
	static const TCHAR* Names[4] = { TEXT("WallN"), TEXT("WallS"), TEXT("WallE"), TEXT("WallW") };
	for (int i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* W = CreateDefaultSubobject<UStaticMeshComponent>(Names[i]);
		W->SetupAttachment(Area);
		if (Cube.Succeeded()) W->SetStaticMesh(Cube.Object);
		if (Wood.Succeeded()) W->SetMaterial(0, Wood.Object);
		W->SetCollisionProfileName(TEXT("BlockAll"));
		Walls.Add(W);
	}
}

void APenActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const float H = 28.f, T = 10.f;
	Area->SetBoxExtent(FVector(Size.X * 0.5f, Size.Y * 0.5f, 60.f));   // the actor sits on the ground; the box straddles it
	const float hx = Size.X * 0.5f + T * 0.5f, hy = Size.Y * 0.5f + T * 0.5f;
	const FVector Pos[4] = { FVector(hx, 0, H * 0.5f), FVector(-hx, 0, H * 0.5f), FVector(0, hy, H * 0.5f), FVector(0, -hy, H * 0.5f) };
	const FVector Scl[4] = { FVector(T, Size.Y + 2 * T, H) / 100.f, FVector(T, Size.Y + 2 * T, H) / 100.f, FVector(Size.X, T, H) / 100.f, FVector(Size.X, T, H) / 100.f };
	for (int i = 0; i < 4 && i < Walls.Num(); ++i)
	{
		Walls[i]->SetRelativeLocation(Pos[i]);
		Walls[i]->SetRelativeScale3D(Scl[i]);
	}
}

void APenActor::BeginPlay()
{
	Super::BeginPlay();
	Area->OnComponentBeginOverlap.AddDynamic(this, &APenActor::OnEnter);
	Area->OnComponentEndOverlap.AddDynamic(this, &APenActor::OnLeave);
	// slimes placed inside in the editor are already overlapping
	TArray<AActor*> Inside;
	Area->GetOverlappingActors(Inside);
	for (AActor* A : Inside)
	{
		if (USlimeComponent* S = A->FindComponentByClass<USlimeComponent>()) S->HabitatId = HabitatId;
	}
}

void APenActor::OnEnter(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (USlimeComponent* S = Other ? Other->FindComponentByClass<USlimeComponent>() : nullptr) S->HabitatId = HabitatId;
}

void APenActor::OnLeave(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
	if (USlimeComponent* S = Other ? Other->FindComponentByClass<USlimeComponent>() : nullptr)
	{
		if (S->HabitatId == HabitatId) S->HabitatId = TEXT("pen");   // the open clearing counts as a pen for now (§11)
	}
}

FVector APenActor::FreeSpot(int32 Index) const
{
	const int32 Cols = 3;
	const float dx = Size.X / (Cols + 1), dy = Size.Y / 3.f;
	const int32 c = Index % Cols, r = (Index / Cols) % 2;
	const FVector Local(-Size.X * 0.5f + dx * (c + 1), -Size.Y * 0.5f + dy * (r + 1), 0.f);
	return Area->GetComponentTransform().TransformPosition(Local);
}
