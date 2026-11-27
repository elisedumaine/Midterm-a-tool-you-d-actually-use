#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestSpirit.generated.h"

class AForestAnimal;
class AStaticMeshActor;
class SWidget;
class UGameViewportClient;

/** Each night has its own mood, announced at dusk. */
enum class EForestNightTheme : uint8
{
	Calm,		// an ordinary night
	Fog,		// thick mist: the red eyes come out of nowhere
	RedMoon,	// twice as many creatures, a red moon, a big reward
	Silent,		// no creature at all, and a star falls
	Storm,		// thunder and lightning
	Count
};

/** What the forest spirit remembers (slot ForestSpirit_<level>). */
UCLASS()
class FORESTGAME_API UForestSpiritSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float Health = 60.0f;
	/** The flowers that grew where the player cared for the forest. */
	UPROPERTY() TArray<FVector> Flowers;
};

/**
 * The forest's spirit: the forest gives back what the player gives it.
 *
 * - Life of the forest (0-100, under the season): cutting trees and taking
 *   wood and stone lowers it; feeding and taming animals, planting seeds and
 *   welcoming residents raise it.  A thriving forest (75+) pays more
 *   fireflies and welcomes a new animal every dawn; a suffering forest (under
 *   30) sends more creatures at night and nobody new comes.
 * - Themed nights: at dusk the spirit says what kind of night is coming
 *   (fog, red moon, silent night, storm); they get bolder as the nights go
 *   on, and the real calendar has its say (Halloween is a red moon,
 *   Christmas and New Year are silent nights with a falling star).
 * - The forest remembers: at dawn, flowers grow around the homes of the
 *   residents, the tame animals that stay at the camp and the garden.
 * - Fireflies, the shop's money (ForestWallet): earned at dawn, when an
 *   animal is tamed, when a resident moves in.
 *
 * Console: Forest.NightTheme Fog|RedMoon|Silent|Storm|Calm (the next night),
 * Forest.Health 90.
 */
UCLASS()
class FORESTGAME_API UForestSpirit : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestSpirit* Get(const UObject* WorldContext);

	/** The life of the forest, 0..100. */
	float GetHealth() const { return Health; }
	/** Called when the player takes from the forest (negative) or gives to it (positive). */
	void ChangeHealth(float Delta);
	bool IsThriving() const { return Health >= 75.0f; }
	bool IsSuffering() const { return Health < 30.0f; }

	EForestNightTheme GetTheme() const { return Theme; }
	static FString ThemeName(EForestNightTheme Theme);
	/** The coming night will be this one (console, tests). */
	void ForceTheme(EForestNightTheme NewTheme);

	/** Dawn after a night (UForestBonds): fireflies, flowers; returns the lines to add to the dawn message. */
	FString OnDawn(bool bSurvived);

	bool ShowsHud() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Start();
	void SaveSpirit() const;
	FString SaveSlot() const;
	void EnsureOverlay();
	void Say(const FString& Text, float Seconds) const;
	void Log(const FString& Line) const;

	/** The kind of night that comes, and why (a date of the real calendar, or empty). */
	EForestNightTheme PickTheme(FString& OutReason) const;
	void Dusk();
	void BeginNight();
	void EndNight();
	void WatchAnimals();
	void Bloom(const FVector& Around, int32 Count);
	void SpawnFlower(const FVector& Spot);

	bool bStarted = false;
	bool bAnnounced = false;
	bool bNight = false;
	float Health = 60.0f;
	EForestNightTheme Theme = EForestNightTheme::Calm;
	bool bForced = false;
	int32 BaseBeasts = 6;
	float StarIn = -1.0f;
	float SaveTimer = 0.0f;
	TArray<FVector> Flowers;
	/** Trust of each animal last time it was checked, to see a meal or a taming. */
	TMap<TWeakObjectPtr<AForestAnimal>, float> KnownTrust;
	TSet<TWeakObjectPtr<AForestAnimal>> KnownTame;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AStaticMeshActor>> FlowerActors;

	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
