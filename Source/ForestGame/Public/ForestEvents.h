#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestEvents.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A patch of fire left by lightning: flames that flicker for a while and burn
 * whoever stands in them, then die out.
 */
UCLASS(NotBlueprintable)
class FORESTGAME_API AForestFire : public AActor
{
	GENERATED_BODY()

public:
	AForestFire();
	virtual void Tick(float DeltaSeconds) override;

	void Ignite(float Seconds);

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	float Life = 0.0f;
	float Lifetime = 25.0f;
	float BurnCooldown = 0.0f;
};

/**
 * The forest's natural events, which make the survival game unpredictable.
 *
 * - Lightning: in a storm, bolts strike around the player.  A glowing circle
 *   on the ground warns a couple of seconds before; standing in it hurts, and
 *   the bolt leaves a fire.  Now and then a dry storm rolls in by itself.
 * - Earthquake: the ground shakes for a few seconds, and sometimes uncovers a
 *   hidden chest.
 * - Shooting star: on clear nights, a star falls into the forest and leaves a
 *   shining chest with rare things in it where it lands.
 *
 * Every event goes into the player's journal with its date (UForestLife).
 */
UCLASS()
class FORESTGAME_API UForestEvents : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestEvents* Get(const UObject* WorldContext);

	/** For tests and the presentation: "Lightning", "Earthquake", "Star", "Storm". */
	UFUNCTION(BlueprintCallable, Category = "Forest|Events")
	void Trigger(FName Event);

	/** The big line in the middle of the screen while an event happens. */
	const FString& GetBanner() const { return Banner; }
	float GetBannerAlpha() const;
	/** Seconds left before the warned bolt falls (negative: none). */
	float GetStrikeCountdown() const { return StrikeCountdown; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void ShowBanner(const FString& Text, float Seconds);
	void WarnLightning(const FVector& Around, float MinDistance, float MaxDistance);
	void Strike();
	void StartQuake();
	void TickQuake(float DeltaSeconds);
	void StartShootingStar();
	void TickStar(float DeltaSeconds);
	bool GroundAt(float X, float Y, float& OutZ) const;
	void EnsureVisuals();

	float NextEvent = 150.0f;
	float NextStormBolt = 12.0f;

	// Lightning: where it will fall, and the warning circle.
	float StrikeCountdown = -1.0f;
	FVector StrikeSpot = FVector::ZeroVector;
	float BoltGlow = 0.0f;

	// Earthquake.
	float QuakeLeft = 0.0f;
	float QuakeTime = 0.0f;
	FRotator QuakeOffset = FRotator::ZeroRotator;

	// Shooting star: a glowing ball that crosses the sky and lands.
	float StarTime = -1.0f;
	FVector StarFrom = FVector::ZeroVector;
	FVector StarTo = FVector::ZeroVector;

	FString Banner;
	float BannerTime = 0.0f;
	float BannerLength = 1.0f;

	TWeakObjectPtr<AActor> Visuals;
	TWeakObjectPtr<UStaticMeshComponent> WarningDisc;
	TWeakObjectPtr<UStaticMeshComponent> Bolt;
	TWeakObjectPtr<UStaticMeshComponent> Star;
	TWeakObjectPtr<UPointLightComponent> BoltLight;
	TWeakObjectPtr<UMaterialInstanceDynamic> WarningMaterial;
	TWeakObjectPtr<UMaterialInstanceDynamic> BoltMaterial;
};
