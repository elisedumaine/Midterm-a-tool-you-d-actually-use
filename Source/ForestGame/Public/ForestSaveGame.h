#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ForestSaveGame.generated.h"

USTRUCT()
struct FForestSavedBuilding
{
	GENERATED_BODY()

	UPROPERTY() FName Type;
	UPROPERTY() FTransform Transform;
	UPROPERTY() int32 Food = 0;
};

USTRUCT()
struct FForestSavedAnimal
{
	GENERATED_BODY()

	/** The animal's actor name in the level (BP_Deer_C_3...). */
	UPROPERTY() FName Name;
	UPROPERTY() float Trust = 0.0f;
	UPROPERTY() bool bTame = false;
	UPROPERTY() bool bStaying = false;
	UPROPERTY() FVector Location = FVector::ZeroVector;
};

/**
 * The player's camp, kept between two Play sessions: the basket, what they
 * built, and how much each animal trusts them.  One save per level; F9 in the
 * game starts a fresh camp.
 */
UCLASS()
class FORESTGAME_API UForestSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FString Level;
	UPROPERTY() TMap<FName, int32> Items;
	UPROPERTY() TArray<FForestSavedBuilding> Buildings;
	UPROPERTY() TArray<FForestSavedAnimal> Animals;
};
