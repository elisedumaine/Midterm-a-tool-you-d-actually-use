#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestMusic.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Music that follows the forest, one track per mood, fading into each other:
 *
 *   Title   the title screen, the creative pages and the wake-up
 *   Day     daytime
 *   Dusk    evening, as the sun goes down
 *   Night   at night
 *   Danger  a creature with red eyes is near and the player is out of the light
 *
 * The tracks are any sounds in /Game/Music whose name contains the mood
 * (MUS_Day, Forest_Night_Theme...).  A missing mood falls back to a quieter
 * one (Danger -> Night, Dusk -> Day, Title -> Day), or to silence.  The volume
 * follows the pause menu's Music setting, and the music keeps playing (more
 * softly) in the pause menu.
 */
UCLASS()
class FORESTGAME_API UForestMusic : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual void Deinitialize() override;

	static UForestMusic* Get(const UObject* WorldContext);

	enum EMood : uint8 { Title, Day, Dusk, Night, Danger, Count };

	/** The mood playing now (for the log and tests). */
	EMood GetMood() const { return Mood; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void FindTracks();
	EMood WantedMood() const;
	/** The mood that really plays: the wanted one, or its fallback if it has no track. */
	EMood Playable(EMood Wanted) const;

	bool bFound = false;
	EMood Mood = Title;
	/** Seconds the danger has been gone: the danger music lingers a little. */
	float CalmFor = 100.0f;
	float Volume[Count] = {};

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Tracks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Players;
};
