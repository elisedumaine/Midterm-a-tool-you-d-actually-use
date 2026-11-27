#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestBlocks.generated.h"

class AActor;
class APawn;
class APlayerController;
class SWidget;
class UGameViewportClient;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** The blocks the player placed, between two Play sessions. */
UCLASS()
class FORESTGAME_API UForestBlocksSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() TArray<FIntVector> Cells;
	UPROPERTY() TArray<uint8> Types;
};

/**
 * Free building with blocks, crafting, and a home that keeps the night out.
 *
 * - Blocks (G): a 1 m grid over the forest.  Log, plank, stone, leaf, glow
 *   and door blocks; left click places one against the ground or another
 *   block, right click takes it back (the materials come back too), the wheel
 *   or 1-6 picks the kind.
 * - Crafting (T): planks, tools, torches and seeds.  Some recipes need a
 *   workbench nearby (built from the build menu, B).
 * - What it means in the living forest:
 *   * Glow blocks shine at night and the night beasts keep out of their light,
 *     like a lantern.
 *   * A home is walls on four sides and a roof: inside it the beasts cannot
 *     reach the player, and Z sleeps through the night (the sky rushes to dawn).
 *     Doors count as walls but let the player walk through.
 *   * When it rains, the animals run under the player's roofs.
 *
 * It lives in every game world by itself and draws its own block bar,
 * crafting panel and home badge.  Saved per level; F9 starts over.
 */
UCLASS()
class FORESTGAME_API UForestBlocks : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestBlocks* Get(const UObject* WorldContext);

	static constexpr float CellSize = 100.0f;
	static constexpr int32 NumTypes = 6;

	/** True while the block mode or the crafting panel owns the mouse and the number keys. */
	bool WantsInput() const { return bBlockMode || bCraftMenu; }

	/** Walls on four sides and a roof above this point. */
	bool IsHome(const FVector& Where) const;

	/** The glow blocks that light the night. */
	const TArray<FVector>& GetGlowSpots() const { return GlowSpots; }
	/** Every dry spot under the player's roofs. */
	const TArray<FVector>& GetRoofSpots() const { return RoofSpots; }

	/** The nearest dry spot under one of the player's roofs. */
	bool FindRoofSpot(const FVector& From, float Range, FVector& OutSpot) const;

	// --- For tests and the presentation (console: Forest.House) -------------------
	/** A small log cabin with a plank roof, a door and a glow block, around the player. */
	UFUNCTION(BlueprintCallable, Category = "Forest|Blocks")
	void BuildDemoHouse();

	UFUNCTION(BlueprintCallable, Category = "Forest|Blocks")
	void SetBlockMode(bool bOn);

	/** Crafts recipe Index of the T panel, as if its number was pressed. */
	UFUNCTION(BlueprintCallable, Category = "Forest|Blocks")
	void CraftRecipe(int32 Index) { Craft(Index); }

	UFUNCTION(BlueprintPure, Category = "Forest|Blocks")
	int32 NumBlocks() const { return Blocks.Num(); }

	UFUNCTION(BlueprintPure, Category = "Forest|Blocks")
	bool IsPlayerAtHome() const { return bPlayerHome; }

	/** Places the selected block where the player aims, as a left click would. */
	UFUNCTION(BlueprintCallable, Category = "Forest|Blocks")
	void PlaceAimed();

	/** Starts sleeping (as Z would); returns false if the player cannot sleep now. */
	UFUNCTION(BlueprintCallable, Category = "Forest|Blocks")
	bool TrySleep();

	// --- Read by the overlay ------------------------------------------------------
	bool ShowsHud() const;
	bool IsBlockMode() const { return bBlockMode; }
	bool IsCraftMenuOpen() const { return bCraftMenu; }
	bool IsPlayerHome() const { return bPlayerHome; }
	int32 GetSelected() const { return Selected; }
	float GetSleepDarkness() const { return SleepDarkness; }
	FString BlockName(int32 Type) const;
	FString BlockCostText(int32 Type) const;
	bool CanAffordBlock(int32 Type) const;
	int32 NumRecipes() const;
	FString RecipeText(int32 Index) const;
	bool CanCraft(int32 Index) const;
	bool IsNearWorkbench() const { return bNearWorkbench; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Start();
	void EnsureOverlay();
	void ReadKeys(APlayerController* Controller, APawn* Player);
	void UpdateTarget(APlayerController* Controller, APawn* Player);
	void UpdateSleep(float DeltaSeconds, APlayerController* Controller);
	void Place(APawn* Player);
	void Break();
	void Craft(int32 Index);
	void Sleep(APlayerController* Controller, APawn* Player);
	void Rebuild();
	void SaveBlocks();
	void LoadBlocks();
	void ResetBlocks();
	void Say(const FString& Text, float Seconds) const;
	bool HasBlock(const FIntVector& Cell) const { return Blocks.Contains(Cell); }
	bool NearWorkbench(const FVector& From) const;

	static FIntVector CellOf(const FVector& Where);
	static FVector CenterOf(const FIntVector& Cell);

	bool bStarted = false;
	bool bBlockMode = false;
	bool bCraftMenu = false;
	bool bPlayerHome = false;
	bool bNearWorkbench = false;
	bool bHasTarget = false;
	bool bTargetValid = false;
	bool bHasBreak = false;
	bool bSleeping = false;
	bool bDirty = false;
	int32 Selected = 0;
	float SaveTimer = 0.0f;
	float CheckTimer = 0.0f;
	float SleepDarkness = 0.0f;
	float DayMinutesBeforeSleep = 12.0f;
	FIntVector TargetCell = FIntVector::ZeroValue;
	FIntVector BreakCell = FIntVector::ZeroValue;

	TMap<FIntVector, uint8> Blocks;
	TArray<FVector> GlowSpots;
	TArray<FVector> RoofSpots;

	UPROPERTY(Transient) TObjectPtr<AActor> Holder;
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> Meshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Ghost;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GhostMaterial;
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> Lights;

	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
