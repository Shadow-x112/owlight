#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OwlightHUD.generated.h"

/**
 * Development HUD drawn straight to the canvas: pockets, the interaction prompt, and what the
 * slime in front of you is showing (its signs and the last journal lines). No numbers, ever.
 */
UCLASS()
class OWLIGHT_API AOwlightHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void Line(float X, float Y, const FString& S, const FLinearColor& C, float Scale = 1.f);
};
