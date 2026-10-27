#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OwlightCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UInputAction;
class ASlimeActor;
class AFoodStockActor;
struct FInputActionValue;

/**
 * The player: a pulled-back third-person character with a fixed-yaw, tilted camera
 * (design.md §3). Movement is relative to the camera's fixed yaw, so "up" on the stick is
 * always away from the camera. Slimes are picked up by hand (§1.4) and fed from the pockets.
 * The body is a placeholder until there is a real character.
 */
UCLASS()
class OWLIGHT_API AOwlightCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AOwlightCharacter();

	/** Camera pitch in degrees (negative looks down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owlight|Camera")
	float CameraPitch = -36.f;

	/** Camera yaw in world degrees; it never follows the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owlight|Camera")
	float CameraYaw = 0.f;

	/** Distance from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owlight|Camera")
	float CameraDistance = 1050.f;

	/** Narrow field of view flattens the scene the way the reference does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owlight|Camera")
	float CameraFOV = 32.f;

	/** How far you can reach to pick up or feed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Owlight|Hands")
	float Reach = 140.f;

	/** What you are carrying in your pockets: food id -> count. */
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Hands")
	TMap<FName, int32> Pockets;

	/** Which pocket food F feeds. */
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Hands")
	FName SelectedFood;

	/** The slime in your hands, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Hands")
	TObjectPtr<ASlimeActor> Carried;

	/** The nearest thing in reach, for the prompt. Only one is set. */
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Hands")
	TObjectPtr<ASlimeActor> FocusSlime;
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Hands")
	TObjectPtr<AFoodStockActor> FocusStock;

	/** Development: sim seconds per real second for every slime (T cycles 1 / 60 / 3600). */
	UPROPERTY(BlueprintReadOnly, Category = "Owlight|Dev")
	float DevTimeScale = 1.f;

	UFUNCTION(BlueprintPure, Category = "Owlight|Hands")
	FText PromptText() const;

	UFUNCTION(BlueprintCallable, Category = "Owlight|Hands")
	void AddFood(FName FoodId, int32 Count);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	UFUNCTION(BlueprintCallable, Category = "Owlight|Hands") void Interact();
	UFUNCTION(BlueprintCallable, Category = "Owlight|Hands") void FeedSelected();
	UFUNCTION(BlueprintCallable, Category = "Owlight|Hands") void CycleFood();
	UFUNCTION(BlueprintCallable, Category = "Owlight|Dev") void CycleDevTime();
	void EnsureInputAssets();
	void UpdateFocus();
	void ApplyDevTimeScale();

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Body")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Body")
	TObjectPtr<UStaticMeshComponent> Head;

	/** Built in code so the project needs no input assets yet. */
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FeedAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleFoodAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DevTimeAction;
};
