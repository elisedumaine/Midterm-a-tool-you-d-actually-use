#include "ForestCrafting.h"

#include "ForestLanguage.h"

const FName ForestCrafting::Mushroom(TEXT("Mushroom"));
const FName ForestCrafting::Berry(TEXT("Berry"));
const FName ForestCrafting::Wood(TEXT("Wood"));
const FName ForestCrafting::Stone(TEXT("Stone"));
const FName ForestCrafting::Plank(TEXT("Plank"));
const FName ForestCrafting::Fiber(TEXT("Fiber"));
const FName ForestCrafting::Resin(TEXT("Resin"));

const TArray<FName>& ForestCrafting::Items()
{
	static const TArray<FName> List = {Mushroom, Berry, Wood, Stone, Plank, Fiber, Resin};
	return List;
}

bool ForestCrafting::IsFood(FName Item)
{
	return Item == Mushroom || Item == Berry;
}

FString ForestCrafting::ItemName(FName Item, int32 Count)
{
	const bool bMany = Count != 1;
	FString Name;
	if (Item == Mushroom)
	{
		Name = ForestLanguage::Pick(bMany ? TEXT("mushrooms") : TEXT("mushroom"), bMany ? TEXT("champignons") : TEXT("champignon"));
	}
	else if (Item == Berry)
	{
		Name = ForestLanguage::Pick(bMany ? TEXT("berries") : TEXT("berry"), bMany ? TEXT("baies") : TEXT("baie"));
	}
	else if (Item == Wood)
	{
		Name = ForestLanguage::Pick(TEXT("wood"), TEXT("bois"));
	}
	else if (Item == Stone)
	{
		Name = ForestLanguage::Pick(bMany ? TEXT("stones") : TEXT("stone"), bMany ? TEXT("pierres") : TEXT("pierre"));
	}
	else if (Item == Plank)
	{
		Name = ForestLanguage::Pick(bMany ? TEXT("planks") : TEXT("plank"), bMany ? TEXT("planches") : TEXT("planche"));
	}
	else if (Item == Fiber)
	{
		Name = ForestLanguage::Pick(bMany ? TEXT("fibers") : TEXT("fiber"), bMany ? TEXT("fibres") : TEXT("fibre"));
	}
	else if (Item == Resin)
	{
		Name = ForestLanguage::Pick(TEXT("resin"), TEXT("résine"));
	}
	else
	{
		Name = Item.ToString();
	}
	return Count >= 0 ? FString::Printf(TEXT("%d %s"), Count, *Name) : Name;
}

const TArray<ForestCrafting::FRecipe>& ForestCrafting::Recipes()
{
	static const TArray<FRecipe> List = {
		{TEXT("Campfire"), TEXT("Campfire"), TEXT("Feu de camp"),
			TEXT("Light and warmth. Tame animals sleep by it at night."), TEXT("Lumière et chaleur. Les animaux apprivoisés y dorment la nuit."),
			{{Wood, 3}, {Stone, 2}}},
		{TEXT("Feeder"), TEXT("Feeder"), TEXT("Mangeoire"),
			TEXT("Holds 6 berries. Hungry animals come to eat and learn to trust you."), TEXT("Contient 6 baies. Les animaux affamés viennent manger et t'apprivoisent."),
			{{Wood, 2}, {Berry, 3}}},
		{TEXT("Shelter"), TEXT("Shelter"), TEXT("Abri"),
			TEXT("A log roof. Animals hide under it from the rain."), TEXT("Un toit de rondins. Les animaux s'y cachent de la pluie."),
			{{Wood, 6}, {Stone, 1}}},
		{TEXT("Fence"), TEXT("Fence"), TEXT("Clôture"),
			TEXT("A piece of fence for your camp."), TEXT("Un morceau de clôture pour ton camp."),
			{{Wood, 2}}},
		{TEXT("Lantern"), TEXT("Lantern"), TEXT("Lanterne"),
			TEXT("Lights up by itself at night."), TEXT("S'allume toute seule la nuit."),
			{{Wood, 1}, {Stone, 2}}},
		{TEXT("Workbench"), TEXT("Workbench"), TEXT("Établi"),
			TEXT("Craft here (T): planks, an axe, glow stones."), TEXT("Pour fabriquer (T) : planches, hache, pierres lumineuses."),
			{{Wood, 4}, {Stone, 2}}},
		{TEXT("Bench"), TEXT("Log bench"), TEXT("Banc en rondin"),
			TEXT("A place to sit and watch the sky."), TEXT("Pour s'asseoir et regarder le ciel."),
			{{Wood, 3}}},
	};
	return List;
}

const ForestCrafting::FRecipe* ForestCrafting::FindRecipe(FName Id)
{
	for (const FRecipe& Recipe : Recipes())
	{
		if (Recipe.Id == Id)
		{
			return &Recipe;
		}
	}
	return nullptr;
}

FString ForestCrafting::CostText(const FRecipe& Recipe)
{
	TArray<FString> Parts;
	for (const FCost& Cost : Recipe.Cost)
	{
		Parts.Add(ItemName(Cost.Item, Cost.Amount));
	}
	return FString::Join(Parts, TEXT(" + "));
}
