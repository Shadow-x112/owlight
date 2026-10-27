#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PenActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/**
 * A place slimes are kept. Anything with a USlimeComponent inside its box takes the pen's
 * HabitatId (design.md §5: habitat nudges per hour). The fence is a placeholder: four low walls.
 */
UCLASS()
class OWLIGHT_API APenActor : public AActor
{
	GENERATED_BODY()

public:
	APenActor();

	/** Catalog habitat id this pen counts as. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pen")
	FName HabitatId = TEXT("pen");

	/** Inner size of the pen in cm (x, y). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pen")
	FVector2D Size = FVector2D(260.f, 200.f);

	UPROPERTY(VisibleAnywhere, Category = "Pen")
	TObjectPtr<UBoxComponent> Area;

	UPROPERTY(VisibleAnywhere, Category = "Pen")
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	/** Where a carried slime should be set down inside this pen. */
	UFUNCTION(BlueprintPure, Category = "Pen")
	FVector FreeSpot(int32 Index) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnEnter(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION()
	void OnLeave(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 BodyIndex);
};
