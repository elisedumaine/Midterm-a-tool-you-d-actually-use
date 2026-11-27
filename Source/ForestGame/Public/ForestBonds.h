#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestBonds.generated.h"

class AForestAnimal;
class SWidget;
class UGameViewportClient;

/** What UForestBonds remembers between sessions (slot ForestBonds_<level>). */
UCLASS()
class FORESTGAME_API UForestBondsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 NightsSurvived = 0;
	/** Animals that came to the forest at dawn; they come back when the game loads. */
	UPROPERTY() TArray<FName> Arrivals;
};

/**
 * What makes you want to play one more night: the bonds with the forest.
 *
 * 1. Tame animals help: the bunny sniffs out hidden berries, the deer brings
 *    wood, the boar digs up mushrooms, the beaver gnaws planks; at night the
 *    fox growls when a beast comes near (and from where), and a tame boar or
 *    fox chases off a beast that comes too close.
 * 2. Every night survived counts (shown under the season) and dawn leaves a
 *    reward; every second night a new animal comes to live in the forest.
 * 3. Animals move into what you build: a roof of blocks or a shelter becomes
 *    a home (a lit home attracts the fox, the others the bunny or whoever is
 *    near), and at dawn the residents leave a gift at the door.
 *
 * Console: Forest.Dawn (pretend a night was survived), Forest.MoveIn (an
 * animal moves into the nearest shelter now).
 */
UCLASS()
class FORESTGAME_API UForestBonds : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestBonds* Get(const UObject* WorldContext);

	int32 GetNightsSurvived() const { return NightsSurvived; }
	bool ShowsCounter() const;

	/** Dawn after a night survived: the counter, the reward, maybe a new animal. */
	void Dawn(bool bSurvived);
	/** The nearest free shelter gets a resident now. Returns false if there is none. */
	bool MoveSomeoneIn();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Start();
	void EnsureOverlay();
	void UpdateHelpers(float DeltaSeconds, APawn* Player, bool bNight);
	void UpdateFoxWarning(float DeltaSeconds, APawn* Player);
	void UpdateGuardians(float DeltaSeconds, APawn* Player);
	void UpdateResidents(float DeltaSeconds);
	/** Free spots to live in: one per house of blocks, plus the camp's shelters. */
	TArray<FVector> FindShelters() const;
	AForestAnimal* SpawnArrival(FName Species, bool bAnnounce);
	void SaveBonds() const;
	FString SaveSlot() const;
	void Say(const FString& Text, float Seconds) const;

	bool bStarted = false;
	bool bWasNight = false;
	bool bFaintedTonight = false;
	int32 NightsSurvived = 0;
	TArray<FName> Arrivals;
	float ResidentTimer = 0.0f;
	float WarnCooldown = 0.0f;
	/** Seconds each tame helper has been with the player since its last find. */
	TMap<TWeakObjectPtr<AForestAnimal>, float> HelpTime;
	TMap<TWeakObjectPtr<AForestAnimal>, float> GuardCooldown;
	/** Shelters seen for a while: a home needs to stand a little before someone moves in. */
	TArray<TPair<FVector, float>> ShelterAge;

	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
