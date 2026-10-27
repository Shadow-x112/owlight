#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FoodStockActor.generated.h"

class UStaticMeshComponent;

/** A little pile of one catalog food lying in the world. Interacting takes it into your pockets. */
UCLASS()
class OWLIGHT_API AFoodStockActor : public AActor
{
	GENERATED_BODY()

public:
	AFoodStockActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food")
	FName FoodId = TEXT("grain");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Food")
	int32 Count = 5;

	UPROPERTY(VisibleAnywhere, Category = "Food")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Takes everything; returns how many were taken and destroys the pile. */
	UFUNCTION(BlueprintCallable, Category = "Food")
	int32 Take();

	FText Label() const;
};
