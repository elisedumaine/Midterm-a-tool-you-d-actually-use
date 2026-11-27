#pragma once

#include "CoreMinimal.h"

/**
 * What the player can carry and what they can build with it.
 *
 * Items: Mushroom and Berry are food for the animals; Wood and Stone are for
 * building; Plank, Fiber and Resin are for blocks and crafting (ForestBlocks).  Recipes are the camp pieces of the build menu (B): each one costs
 * a few items and is placed in the world as an AForestBuilding.
 */
namespace ForestCrafting
{
	struct FCost
	{
		FName Item;
		int32 Amount;
	};

	struct FRecipe
	{
		FName Id;
		const TCHAR* English;
		const TCHAR* French;
		const TCHAR* EnglishHint;
		const TCHAR* FrenchHint;
		TArray<FCost> Cost;
	};

	extern const FName Mushroom;
	extern const FName Berry;
	extern const FName Wood;
	extern const FName Stone;
	/** Cut from wood at the workbench; for plank blocks and doors. */
	extern const FName Plank;
	/** Pulled from berry bushes; for leaf blocks, tools and rope. */
	extern const FName Fiber;
	/** Found in wood piles and on trees hit with the axe; for torches and glow blocks. */
	extern const FName Resin;

	/** Every item, in the order the HUD lists them. */
	FORESTGAME_API const TArray<FName>& Items();

	FORESTGAME_API bool IsFood(FName Item);

	/** "Wood" / "Bois", or "3 wood" / "3 bois" when Count is given. */
	FORESTGAME_API FString ItemName(FName Item, int32 Count = -1);

	/** The build menu, in the order of the keys 1, 2, 3... */
	FORESTGAME_API const TArray<FRecipe>& Recipes();

	FORESTGAME_API const FRecipe* FindRecipe(FName Id);

	/** "3 wood + 2 stone" */
	FORESTGAME_API FString CostText(const FRecipe& Recipe);
}
