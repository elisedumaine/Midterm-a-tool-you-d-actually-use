#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "ForestLook.h"
#include "ForestCreator.generated.h"

class SVerticalBox;
class USkeletalMesh;

/**
 * Everything the player chooses before the forest wakes up, remembered
 * between sessions (GameUserSettings.ini, [ForestGame]).
 */
struct FORESTGAME_API FForestSetup
{
	int32 Season = 1;		// EForestSeason: Spring, Summer, Autumn, Winter
	int32 Weather = 0;		// index in ForestSetup::Weathers()
	int32 Roundness = 2;	// index in ForestSetup::Roundnesses()
	int32 Nights = 1;		// gentle, normal, terrifying

	static FForestSetup Load();
	void Save() const;

	/** The weather names of the sky (Clear, Mist...). */
	static const TArray<FName>& Weathers();
	FName WeatherName() const;

	/** Puts the world choices on the world right now (season, curve, nights; the weather only if bWeather). */
	void ApplyWorld(UWorld* World, bool bWeather) const;
	/** Starts the clock of the living sky (it stands still during the title and the pages). */
	void ApplyDayLength(UWorld* World) const;
	/** Dresses the player in the look chosen on the character page (FForestLook). */
	void ApplyCharacter(UWorld* World) const;
};

/**
 * The creative pages between the title and the wake-up, in the same
 * see-through style as the title:
 *
 * 1. Your character: body, head, skin, hair, beard and clothes with their
 *    colours (FForestLook), the camera in front of you.  Accessories come
 *    later in the game.
 * 2. Your world: season, weather, round world, nights, day length.  Each
 *    change is seen at once on the forest behind the page.
 *
 * Start: falls asleep, wakes up in the world you made.
 */
UCLASS()
class FORESTGAME_API UForestCreator : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	int32 GetPage() const { return Page; }
	/** 0 = character, 1 = world, 2 = wake up. */
	void GoToPage(int32 NewPage);

private:
	void ShowPage(int32 NewPage);
	void StepLook(EForestLookPart Part, int32 Step);
	void SurpriseLook();
	void StepWorld(int32 Row, int32 Step);
	FString WorldValue(int32 Row) const;

	TSharedRef<SWidget> Row(int32 Index, TFunction<FString()> Label, TFunction<FString()> Value, TFunction<void(int32)> Step,
		TFunction<FString()> Value2 = nullptr, TFunction<void(int32)> Step2 = nullptr);
	TSharedRef<SWidget> Button(TFunction<FString()> Label, TFunction<void()> Click, bool bMain);
	float Appear(float Delay = 0.0f) const;

	FForestSetup Setup;
	int32 Page = 0;
	double ShownAt = 0.0;
	FForestLook Look;
	TSharedPtr<SVerticalBox> Body;
};
