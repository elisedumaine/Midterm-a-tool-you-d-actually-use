#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestPauseMenu.generated.h"

class SVerticalBox;

/**
 * The player's settings, remembered between sessions (GameUserSettings.ini,
 * [ForestGame]).  Volumes go from 0 to 10.
 */
namespace ForestSettings
{
	FORESTGAME_API int32 MasterVolume();
	FORESTGAME_API int32 MusicVolume();
	FORESTGAME_API int32 NatureVolume();
	/** 0 = low ... 3 = epic (Unreal's scalability levels). */
	FORESTGAME_API int32 Quality();

	FORESTGAME_API void SetMasterVolume(int32 Value);
	FORESTGAME_API void SetMusicVolume(int32 Value);
	FORESTGAME_API void SetNatureVolume(int32 Value);
	FORESTGAME_API void SetQuality(int32 Value);

	/** Rain, wind and birds: a multiplier from 0 to 1 for their volume. */
	FORESTGAME_API float NatureMultiplier();
	/** The music (when there is some): a multiplier from 0 to 1. */
	FORESTGAME_API float MusicMultiplier();

	/** Puts the master volume and the quality on the game. */
	FORESTGAME_API void Apply(UWorld* World);
}

/**
 * The pause menu, in the style of the title screen: Resume, the settings
 * (volumes, language, graphics quality, help), back to the title, quit.
 */
UCLASS()
class FORESTGAME_API UForestPauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> Row(TFunction<FString()> Label, TFunction<FString()> Value, TFunction<void(int32)> Step);
	TSharedRef<SWidget> Button(TFunction<FString()> Label, TFunction<void()> Click, bool bMain);
};

/**
 * Opens and closes the pause menu: P (and Escape outside the editor, where
 * Escape stops Play), or the gamepad's Start button.  The game stands still
 * while it is open.  Lives in every game world by itself.
 */
UCLASS()
class FORESTGAME_API UForestPauseMenu : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	/** Keeps ticking while the game is paused. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	static UForestPauseMenu* Get(const UObject* WorldContext);

	void Open();
	void Close();
	bool IsOpen() const { return Widget != nullptr; }
	/** Leaves the game for the title screen (reloads the level). */
	void BackToTitle();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool bApplied = false;
	bool bMouseWasShown = false;

	UPROPERTY(Transient)
	TObjectPtr<UForestPauseWidget> Widget;
};
