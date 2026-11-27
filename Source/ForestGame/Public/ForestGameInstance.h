#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "ForestGameInstance.generated.h"

class AForestIntroDirector;
class UForestCreator;
class UForestStartMenu;

/** Starts the native title menu, then the creative pages (character, world), then the forest intro. */
UCLASS()
class FORESTGAME_API UForestGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void OnStart() override;
	/** Leaves the title screen and plays the wake-up (the PLAY button; callable from Python for tests). */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void StartForestIntro();

	/** PLAY: the creative pages, your character then your world. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void OpenCreator();
	/** From the character page, back to the title screen. */
	void BackToTitle();
	/** Moves the camera for a creative page: 0 = in front of the player, 1 = above the forest. */
	void ShowCreatorView(int32 Page);
	/** The pause menu's "Title screen": the title shows again once the level has loaded. */
	void ReturnToTitle();

private:
	void TryShowStartMenu();
	void HandleMapLoaded(UWorld* LoadedWorld);

	UPROPERTY(Transient) TObjectPtr<UForestStartMenu> StartMenu;
	UPROPERTY(Transient) TObjectPtr<UForestCreator> Creator;
	/** Films the forest behind the title, then plays the falling asleep and the waking. */
	TWeakObjectPtr<AForestIntroDirector> Director;
	FTimerHandle MenuRetryTimer;
	bool bMenuShown = false;
	bool bIntroStarted = false;
	bool bBackToTitle = false;
	bool bMapHooked = false;
};
