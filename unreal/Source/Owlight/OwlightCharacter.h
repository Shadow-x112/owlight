#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OwlightCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * The player: a pulled-back third-person character with a fixed-yaw, tilted camera
 * (design.md §3). Movement is relative to the camera's fixed yaw, so "up" on the stick is
 * always away from the camera. The body is a placeholder until there is a real character.
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void EnsureInputAssets();

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Body")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Owlight|Body")
	TObjectPtr<UStaticMeshComponent> Head;

	/** Built in code at BeginPlay so the project needs no input assets yet. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;
};
