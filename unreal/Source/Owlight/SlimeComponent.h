#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeComponent.generated.h"

struct owl_slime;
struct owl_catalog;

/** The ten hidden properties, mirrored from owlight_c.h so Blueprints can name them. */
UENUM(BlueprintType)
enum class ESlimeProperty : uint8
{
	Density, Heat, Acidity, Clarity, Appetite, Boldness, Memory, Affinity, Light, Bond
};

/** What the player can notice: the type and the visible signs. Numbers stay hidden (design.md §5). */
USTRUCT(BlueprintType)
struct FSlimeLook
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName TypeId;
	UPROPERTY(BlueprintReadOnly) bool bLocked = false;
	UPROPERTY(BlueprintReadOnly) TArray<FText> Signs;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlimeTypeChanged, FName, OldType, FName, NewType);

/**
 * One simulated slime, driven by the core in sim/ through its C interface. Ticks on real time
 * (scaled by TimeScale for development). Where it is kept (HabitatId) and what it does (JobId)
 * are set by whoever owns the slime; the component only integrates.
 */
UCLASS(ClassGroup = (Owlight), meta = (BlueprintSpawnableComponent))
class OWLIGHT_API USlimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlimeComponent();

	/** Catalog habitat the slime currently lives in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime")
	FName HabitatId = TEXT("pen");

	/** Catalog job, or None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime")
	FName JobId;

	/** Deterministic per-slime variance (0 = the resting slime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime")
	int32 Seed = 1;

	/** Display name; the player names slimes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime")
	FString SlimeName = TEXT("slime");

	/** Real seconds per simulated second. 1 = real time (design.md §1). Development only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Dev")
	float TimeScale = 1.f;

	/** Within this distance of the player the slime counts as "near you" (boldness, bond). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime")
	float NearPlayerDistance = 300.f;

	/** The simulation is integrated at most this often, in simulated hours per step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime|Dev")
	float MaxStepHours = 0.05f;

	UPROPERTY(BlueprintAssignable, Category = "Slime")
	FSlimeTypeChanged OnTypeChanged;

	UFUNCTION(BlueprintCallable, Category = "Slime")
	bool Feed(FName FoodId);

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void PickUp();

	UFUNCTION(BlueprintCallable, Category = "Slime")
	void GiveName(const FString& NewName);

	/** Advance the simulation by this many real hours (used at load, for time away). */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	void Advance(float Hours);

	UFUNCTION(BlueprintPure, Category = "Slime")
	FName GetTypeId() const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	bool IsLocked() const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	float GetAgeHours() const;

	UFUNCTION(BlueprintPure, Category = "Slime")
	FSlimeLook GetLook() const;

	/** The journal, oldest first, as "kind: text" lines. */
	UFUNCTION(BlueprintPure, Category = "Slime")
	TArray<FString> GetJournal() const;

	/** Hidden value in 0..1. For rendering and tuning only; never show it to the player. */
	UFUNCTION(BlueprintPure, Category = "Slime|Dev")
	float GetHiddenValue(ESlimeProperty Property) const;

	/** Save as JSON (owlight_c: exact, by id). */
	UFUNCTION(BlueprintCallable, Category = "Slime")
	FString SaveToJson() const;

	UFUNCTION(BlueprintCallable, Category = "Slime")
	bool LoadFromJson(const FString& Json);

	/** Daytime is decided by whoever owns the world clock; default is the real clock, 6:00-20:00. */
	UFUNCTION(BlueprintNativeEvent, Category = "Slime")
	bool IsDaytime() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Step(double Hours);
	bool IsNearPlayer() const;

	owl_slime* Slime = nullptr;
	const owl_catalog* Catalog = nullptr;
	double PendingHours = 0.0;
	FName LastType;
};
