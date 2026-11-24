#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "ForestGameInstance.generated.h"

class UForestStartMenu;

/** Starts the native title menu and hands Play over to the forest intro. */
UCLASS()
class FORESTGAME_API UForestGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void OnStart() override;
	void StartForestIntro();

private:
	void TryShowStartMenu();

	TObjectPtr<UForestStartMenu> StartMenu;
	FTimerHandle MenuRetryTimer;
	bool bMenuShown = false;
	bool bIntroStarted = false;
};
