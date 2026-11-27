#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestLife.generated.h"

class AForestChest;
class AForestCrop;
class APawn;
class APlayerController;
class SWidget;
class UGameViewportClient;
class USceneComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/** A text of the survival life in the four languages (English, French, Spanish, Italian). */
FORESTGAME_API const TCHAR* ForestText(const TCHAR* English, const TCHAR* French, const TCHAR* Spanish, const TCHAR* Italian);

USTRUCT()
struct FForestSavedChest
{
	GENERATED_BODY()

	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.0f;
	UPROPERTY() bool bOpened = false;
	UPROPERTY() bool bStar = false;
};

USTRUCT()
struct FForestSavedCrop
{
	GENERATED_BODY()

	UPROPERTY() FName Seed;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() float Growth = 0.0f;
};

/** The survivor's life between two Play sessions: the date, the gear, the garden. */
UCLASS()
class FORESTGAME_API UForestLifeSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() FDateTime StartDate;
	/** Days fully lived before this session. */
	UPROPERTY() int32 DaysBefore = 0;
	UPROPERTY() float Food = 100.0f;
	/** Tools owned, with how much use is left in each (seconds for the torch). */
	UPROPERTY() TMap<FName, float> Gear;
	UPROPERTY() FName Held;
	UPROPERTY() TArray<FForestSavedChest> Chests;
	UPROPERTY() TArray<FForestSavedCrop> Crops;
	UPROPERTY() TArray<FString> Journal;
};

/**
 * The survival life of the player, on top of the hearts (UForestSurvival):
 *
 * - Hunger: a food bar that empties with time.  C eats from the basket
 *   (berries, mushrooms); by a campfire the food is cooked and feeds more.
 *   Starving costs hearts.
 * - Hidden chests: wooden chests hidden behind rocks and at the foot of trees.
 *   They wink when the player is close; E opens them.  Inside: tools,
 *   weapons, seeds, food.
 * - Held things: what the player found is held in the right hand.  Q (or 1-7)
 *   switches, left click uses it: a branch, a spear or an axe hits night
 *   creatures and angry animals (the axe also chops wood from trees); a torch
 *   lights the night and scares creatures away; seeds are planted.
 * - Garden: planted seeds grow into berry bushes or mushrooms (faster in the
 *   rain); E harvests them when ripe, and they grow again.
 * - Calendar: the game counts the days from the moment the player started it,
 *   with real dates (the first day is the real day they began), and keeps a
 *   dated journal of what happened (J).
 *
 * It lives in every game world by itself and draws its own bars, hotbar,
 * date and journal.  Everything is saved between Play sessions; F9 starts over.
 */
UCLASS()
class FORESTGAME_API UForestLife : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestLife* Get(const UObject* WorldContext);

	/** Adds a dated line to the journal. */
	void Log(const FString& Line);

	/** A chest somewhere hidden around a point; returns false if no spot was found. */
	bool SpawnChestNear(const FVector& Around, float MinDistance, float MaxDistance, bool bStar);
	AForestChest* SpawnChestAt(const FVector& Location, bool bStar, float Yaw = 0.0f, bool bOpened = false);

	/** For tests and the presentation: gives a tool or seeds. */
	UFUNCTION(BlueprintCallable, Category = "Forest|Life")
	void Give(FName Thing, int32 Amount = 1);

	// --- Read by the overlay ----------------------------------------------------
	bool IsActive() const { return bActive; }
	float GetFood() const { return Food; }
	int32 GetSurvivalDay() const;
	FString DateText() const;
	FString DayText() const;
	const FString& GetPrompt() const { return Prompt; }
	bool IsJournalOpen() const { return bJournal; }
	const TArray<FString>& GetJournal() const { return Journal; }
	/** The hotbar: what the player has, in order (Hands first). */
	TArray<FName> Hotbar() const;
	FName GetHeld() const { return Held; }
	FString ThingName(FName Thing) const;
	/** 0..1 use left in a tool, or -1 for things that do not wear out. */
	float WearLeft(FName Thing) const;
	int32 SeedCount(FName Seeds) const;
	FString HelpLine() const;

	static constexpr float MaxFood = 100.0f;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Start(APawn* Player);
	void EnsureOverlay();
	void ReadKeys(APlayerController* Controller, APawn* Player);
	void UpdateHunger(float DeltaSeconds, APawn* Player);
	void UpdatePrompt(APawn* Player);
	void UpdateTorch(float DeltaSeconds, APawn* Player);
	void UpdateSwing(float DeltaSeconds);
	void UpdateDay();

	void Eat(APawn* Player);
	void Use(APawn* Player);
	void Interact(APawn* Player);
	void OpenChest(AForestChest* Chest);
	void GiveLoot(bool bRare, TArray<FString>& OutFound);
	void Select(FName Thing);
	void Cycle();
	void Wear(FName Tool, float Amount);
	void ShowHeld();
	USceneComponent* HandAnchor(APawn* Player, FName& OutSocket) const;

	void GatherSpots();
	void HideChests(int32 Count);
	bool GroundAt(float X, float Y, float& OutZ) const;
	AForestChest* NearestChest(const FVector& From, float Range) const;
	AForestCrop* NearestCrop(const FVector& From, float Range) const;

	FString SaveSlot() const;
	void SaveLife();
	void LoadLife();
	void ResetLife(APawn* Player);
	void Say(const FString& Text, float Seconds) const;

	bool bStarted = false;
	bool bActive = false;
	bool bJournal = false;
	float Food = MaxFood;
	float StarveTimer = 0.0f;
	float SaveTimer = 0.0f;
	float TorchCheck = 0.0f;
	float SwingTime = -1.0f;
	float UseCooldown = 0.0f;
	int32 LastSkyDay = 0;
	int32 DaysBefore = 0;
	FDateTime StartDate;
	FString Prompt;

	FName Held = TEXT("Hands");
	TMap<FName, float> Gear;
	TArray<FString> Journal;
	/** Health of the night creatures hit by the player. */
	TMap<TWeakObjectPtr<AActor>, int32> Wounds;

	TArray<FVector> TreeSpots;
	TArray<FVector> RockSpots;
	FVector WorldCenter = FVector::ZeroVector;
	float WorldRadius = 8000.0f;

	UPROPERTY(Transient) TArray<TObjectPtr<AForestChest>> Chests;
	UPROPERTY(Transient) TArray<TObjectPtr<AForestCrop>> Crops;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> HeldRoot;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> TorchLight;

	TWeakObjectPtr<UGameViewportClient> Viewport;
	TSharedPtr<SWidget> Overlay;
};
