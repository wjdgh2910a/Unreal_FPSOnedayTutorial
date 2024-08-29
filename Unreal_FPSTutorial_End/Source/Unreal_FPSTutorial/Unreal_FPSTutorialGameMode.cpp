// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unreal_FPSTutorialGameMode.h"
#include "Unreal_FPSTutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/HUD.h"

AUnreal_FPSTutorialGameMode::AUnreal_FPSTutorialGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
	static ConstructorHelpers::FClassFinder<AHUD> BPHUDClass(TEXT("/Game/UI/BP_AimHUD.BP_AimHUD_C"));
	if (BPHUDClass.Class != NULL)
	{
		HUDClass = BPHUDClass.Class;
	}
}
