#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeActor.generated.h"

class USlimeComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * A slime in the world: the sculpted translucent shell and its core (Scripts/slime_build.py),
 * with the look read from the simulation every frame and eased so changes feel like the slime
 * changing, not a switch. Density squashes it, Clarity clears it, Heat warms it, lantern types glow.
 */
UCLASS()
class OWLIGHT_API ASlimeActor : public AActor
{
	GENERATED_BODY()

public:
	ASlimeActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<USlimeComponent> Slime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slime")
	TObjectPtr<UStaticMeshComponent> Core;

	/** Emissive is in scene units; this maps "1" to roughly scene white (see unreal/README.md). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Look")
	float EmissiveScale = 0.03f;

	/** How fast the look eases toward the simulation (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Look")
	float EaseSpeed = 0.8f;

	/** Base colour per type id; anything missing falls back to Plain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Look")
	TMap<FName, FLinearColor> TypeColors;

	UFUNCTION(BlueprintPure, Category = "Slime|Look")
	FLinearColor ColorForType(FName TypeId) const;

	/** Picked up by hand: rides in front of the carrier; tells the sim (bond, boldness). */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void PickUp(AActor* Carrier);

	/** Set down on the ground at this spot. */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void PutDown(const FVector& GroundLocation);

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsCarried() const { return Carrier != nullptr; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ApplyLook(bool bSnap);

	UPROPERTY(Transient) TObjectPtr<AActor> Carrier;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShellMID;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMID;

	// eased state
	FLinearColor CurColor = FLinearColor(0.06f, 0.5f, 0.16f);
	float CurClarity = 0.5f, CurHeat = 0.f, CurGlow = 0.f, CurDensity = 0.5f;
};
