#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "ForestGameManager.generated.h"

class AForestAnimal;
class AForestBuilding;
class AForestPickup;
class APlayerController;
class UMaterialInterface;

/**
 * Runs the forest sandbox.  The World Builder places one in every generated world.
 *
 * - Gathering: E picks up mushrooms, berries, wood and stones.  When Play
 *   starts it scatters wood under the trees, stones by the rocks and berry
 *   bushes at the forest's edge; they all grow back after a while.
 * - Animals: E gives food to an animal (its favourite counts double) until it
 *   trusts you and follows you; E pets a tame one, F tells it to stay at the
 *   camp or to follow again.
 * - Building: B opens the build menu, a number picks a piece, a glowing ghost
 *   shows where it goes, R turns it, E places it.
 * - The camp is saved (basket, buildings, trust); F9 starts a fresh one.
 *
 * It also keeps the on-screen messages and installs the Forest HUD.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestGameManager : public AActor
{
	GENERATED_BODY()

public:
	AForestGameManager();

	virtual void Tick(float DeltaSeconds) override;

	/** The manager of the current world, if any. */
	static AForestGameManager* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Interact();

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void ShowMessage(const FText& Message, float Seconds = 3.0f);

	// --- The basket -----------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Forest")
	int32 Count(FName Item) const;

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void AddItem(FName Item, int32 Amount);

	/** What the player would give this animal: its favourite if carried, else any food it eats. */
	FName BestFoodFor(const AForestAnimal* Animal) const;

	// --- Building -------------------------------------------------------------
	/** Opens the build menu, or closes it (and cancels a placement). */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void ToggleBuildMenu();

	/** Picks a recipe of the build menu and starts placing it. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void ChooseRecipe(int32 Index);

	bool CanAfford(FName Recipe) const;

	bool IsBuildMenuOpen() const { return bBuildMenu; }
	bool IsPlacing() const { return Ghost != nullptr; }
	FName GetPlacingRecipe() const { return PlacingRecipe; }
	bool IsPlacementValid() const { return bPlacementValid; }

	// --- What the animals ask the world -------------------------------------
	/** The nearest food this animal can walk to: a bush with berries, a mushroom, a feeder with food. */
	AActor* FindMeal(const AForestAnimal* Animal, const FVector& From, float Range) const;

	/** A dry spot: under the camp's shelter if one is near, else under the nearest tree. */
	bool FindShelter(const FVector& From, FVector& OutSpot) const;

	bool FindBuilding(FName Type, const FVector& From, float Range, FVector& OutSpot) const;
	bool FindCampfire(const FVector& From, float Range, FVector& OutSpot) const { return FindBuilding(TEXT("Campfire"), From, Range, OutSpot); }

	const TArray<TObjectPtr<AForestAnimal>>& GetAnimals() const { return Animals; }
	/** An animal that came to the forest during the game (UForestBonds): it can be fed and tamed like the others. */
	void AddAnimal(AForestAnimal* Animal);

	/** Paints a Kenney piece (log, stone, bush...) with the colours the World Builder gave the same piece in this world. */
	void DressLikeWorld(class UStaticMeshComponent* Component) const;

	// --- Settings and what the HUD reads ----------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FKey InteractKey = EKeys::E;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float PickupRange = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float FeedRange = 420.0f;

	/** How many wood piles, stones and berry bushes to scatter when Play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	int32 WoodPiles = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	int32 StonePiles = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	int32 BerryBushes = 26;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TMap<FName, int32> Items;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	int32 AnimalsTamed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	int32 AnimalsTotal = 0;

	/** What pressing E would do right now; empty when nothing is in reach. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	FText Prompt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	FText Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	float MessageTimeLeft = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	bool bWon = false;

	/** The key help in the corner (H hides it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	bool bShowHelp = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void ReadKeys(APlayerController* Controller);
	void UpdatePrompt(const APawn* Player);
	void UpdateGhost(const APawn* Player);
	void PlaceBuilding();
	void CancelPlacing();
	AForestBuilding* SpawnBuilding(FName Type, const FTransform& Transform, bool bGhost);

	void ScatterResources();
	FVector RandomSpotNear(const FVector& Around, float MinDistance, float MaxDistance, bool& bFound) const;
	bool GroundAt(float X, float Y, float& OutZ) const;

	AForestPickup* NearestPickup(const FVector& From) const;
	AForestAnimal* NearestAnimal(const FVector& From) const;
	AForestBuilding* NearestFeeder(const FVector& From, float Range) const;
	void CountTamed();

	FString SaveSlot() const;
	void SaveCamp();
	void LoadCamp();
	void ResetCamp();

	bool bHudInstalled = false;
	bool bBuildMenu = false;
	bool bPlacementValid = false;
	FName PlacingRecipe;
	float GhostYaw = 0.0f;
	float SaveTimer = 0.0f;

	FVector WorldCenter = FVector::ZeroVector;
	float WorldRadius = 8000.0f;
	TArray<FVector> TreeSpots;
	/** The World Builder's tinted materials, by Kenney family ("log", "stone", "plant"...). */
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UMaterialInterface>> WorldLooks;
	TArray<FVector> RockSpots;

	UPROPERTY(Transient) TObjectPtr<AForestBuilding> Ghost;
	UPROPERTY(Transient) TArray<TObjectPtr<AForestBuilding>> Buildings;
	UPROPERTY(Transient) TArray<TObjectPtr<AForestAnimal>> Animals;
	UPROPERTY(Transient) TArray<TObjectPtr<AForestPickup>> Pickups;
};
