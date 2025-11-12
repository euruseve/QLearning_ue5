#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ENeedType : uint8
{
	Hunger      UMETA(DisplayName = "Hunger"),
	Bladder     UMETA(DisplayName = "Bladder"),
	Energy      UMETA(DisplayName = "Energy"),
	Social      UMETA(DisplayName = "Social"),
	Hygiene     UMETA(DisplayName = "Hygiene"),
	Fun         UMETA(DisplayName = "Fun"),
    
	MAX         UMETA(Hidden)
};