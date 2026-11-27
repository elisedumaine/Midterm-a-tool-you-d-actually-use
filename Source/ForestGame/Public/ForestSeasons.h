#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestSeasons.generated.h"

class SWidget;
class UGameViewportClient;

UENUM(BlueprintType)
enum class EForestSeason : uint8
{
	Spring,
	Summer,
	Autumn,
	Winter
};

/**
 * The year turns in the forest: spring, summer, autumn, winter, a few days of
 * the living sky each.
 *
 * - Autumn: the green of the leaves and the grass slowly turns gold and red.
 * - Winter: the leaves go dull, rain falls as snow and settles; the animals
 *   get hungry faster (food is scarce), so they are more dangerous.
 * - Spring: everything greens up again.
 *
 * The colours come from two values of the weather parameter collection,
 * Autumn and Bare, that every World Builder material reads (see
 * _weather_shading in interactive_world_builder.py).  The season is shown
 * under the clock.  For a demo, the console command "Forest.Season Autumn"
 * jumps straight to a season.
 *
 * It lives in every game world by itself (a world subsystem).
 */
UCLASS()
class FORESTGAME_API UForestSeasons : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestSeasons* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintPure, Category = "Forest|Seasons")
	EForestSeason GetSeason() const { return Season; }

	/** "Autumn" / "Automne" */
	FString SeasonName() const;

	/** Jumps to a season now (the year then goes on from there). */
	UFUNCTION(BlueprintCallable, Category = "Forest|Seasons")
	void SetSeason(EForestSeason NewSeason, bool bQuiet = false);

	/** Days of the living sky in each season. */
	UPROPERTY(EditAnywhere, Category = "Forest|Seasons")
	int32 DaysPerSeason = 2;

	/** Read by the label under the clock. */
	/** Hidden during the title screen and the wake-up, like the rest of the HUD. */
	bool ShowsLabel() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureOverlay();
	void Apply(float DeltaSeconds, bool bInstant);
	void Announce() const;

	bool bStarted = false;
	EForestSeason Season = EForestSeason::Summer;
	EForestSeason StartSeason = EForestSeason::Summer;
	/** The sky's day when the current year started (SetSeason moves it). */
	int32 FirstDay = 1;
	float Autumn = 0.0f;
	float Bare = 0.0f;

	/** Each animal's own hunger speed, before the season changes it. */
	TMap<TWeakObjectPtr<class AForestAnimal>, float> BaseHunger;

	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
