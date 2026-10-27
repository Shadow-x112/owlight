#include "FoodStockActor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AFoodStockActor::AFoodStockActor()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	if (Sphere.Succeeded()) Mesh->SetStaticMesh(Sphere.Object);
	Mesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.14f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 7.f));
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetRenderCustomDepth(true);
	Mesh->SetCustomDepthStencilValue(1);
}

int32 AFoodStockActor::Take()
{
	const int32 N = Count;
	Count = 0;
	Destroy();
	return N;
}

FText AFoodStockActor::Label() const
{
	return FText::FromString(FString::Printf(TEXT("%s x%d"), *FoodId.ToString().Replace(TEXT("_"), TEXT(" ")), Count));
}
