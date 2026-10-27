#include "OwlightGameMode.h"
#include "OwlightCharacter.h"
#include "OwlightHUD.h"

AOwlightGameMode::AOwlightGameMode()
{
	DefaultPawnClass = AOwlightCharacter::StaticClass();
	HUDClass = AOwlightHUD::StaticClass();
}
