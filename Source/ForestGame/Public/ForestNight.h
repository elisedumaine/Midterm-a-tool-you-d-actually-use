#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestNight.generated.h"

class AForestNightBeast;

/**
 * The horror side of the forest: the days are gentle, the nights are not.
 *
 * When the living sky turns to night, creatures with red eyes come out of the
 * dark (AForestNightBeast), more as the night goes on.  They hunt the player
 * and bite (hearts, see UForestSurvival), but they never step into the light
 * of a campfire, or close to a lantern: to survive the night, stay by the fire.
 * At dawn they run off and fade away.
 *
 * Like the survival, it lives in every game world by itself (a world subsystem).
 */
UCLASS()
class FORESTGAME_API UForestNight : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestNight* Get(const UObject* WorldContext);

	/** Is the player standing in the light of a fire or a lantern? */
	bool IsInLight(const FVector& Where) const;

	/** How many beasts roam the night at most. */
	UPROPERTY(EditAnywhere, Category = "Forest|Night")
	int32 MaxBeasts = 6;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SpawnBeast(const FVector& Around);
	void Say(const FString& Text, float Seconds) const;

	bool bNight = false;
	bool bStarted = false;
	float NightTime = 0.0f;
	float SpawnTimer = 0.0f;
	float WarnCooldown = 0.0f;
	int32 Spawned = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AForestNightBeast>> Beasts;
};
