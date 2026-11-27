#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestCrop.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Something the player planted: berry seeds grow into a berry bush, mushroom
 * spores into a cluster of mushrooms.  A tiny sprout first, then it grows (twice
 * as fast in the rain), and when it is ripe the fruit appears and E harvests
 * it.  Then it starts again from a sprout: a garden keeps feeding the camp.
 */
UCLASS()
class FORESTGAME_API AForestCrop : public AActor
{
	GENERATED_BODY()

public:
	AForestCrop();

	virtual void Tick(float DeltaSeconds) override;

	/** Plants it: "BerrySeeds" or "Spores", Growth 0..1 (saved games restore it). */
	void Plant(FName NewSeed, float StartGrowth);

	FName GetSeed() const { return Seed; }
	float GetGrowth() const { return Growth; }
	bool IsRipe() const { return Growth >= 1.0f; }

	/** Picks the fruit: returns what goes into the basket and how many. */
	FName Harvest(int32& OutAmount);

	/** Real seconds from seed to ripe (in fine weather). */
	float GrowSeconds = 150.0f;

private:
	void ShowGrowth();

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Soil;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlantMesh;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Fruit;

	FName Seed;
	float Growth = 0.0f;
	float Time = 0.0f;
};
