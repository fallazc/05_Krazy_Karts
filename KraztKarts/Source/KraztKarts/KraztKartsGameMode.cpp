// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "KraztKartsGameMode.h"
#include "KraztKartsPawn.h"
#include "KraztKartsHud.h"

AKraztKartsGameMode::AKraztKartsGameMode()
{
	DefaultPawnClass = AKraztKartsPawn::StaticClass();
	HUDClass = AKraztKartsHud::StaticClass();
}
