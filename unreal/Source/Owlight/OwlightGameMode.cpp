#include "OwlightGameMode.h"
#include "OwlightCharacter.h"

AOwlightGameMode::AOwlightGameMode()
{
	DefaultPawnClass = AOwlightCharacter::StaticClass();
}
