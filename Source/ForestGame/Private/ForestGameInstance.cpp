#include "ForestGameInstance.h"

#include "ForestIntroDirector.h"
#include "ForestStartMenu.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UForestGameInstance::OnStart()
{
	Super::OnStart();
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	World->GetTimerManager().SetTimer(MenuRetryTimer, this, &UForestGameInstance::TryShowStartMenu, 0.15f, true);
}

void UForestGameInstance::TryShowStartMenu()
{
	if (bMenuShown || bIntroStarted)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!PlayerController || !PlayerController->GetPawn())
	{
		return;
	}

	bMenuShown = true;
	World->GetTimerManager().ClearTimer(MenuRetryTimer);
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerController->bShowMouseCursor = true;

	StartMenu = CreateWidget<UForestStartMenu>(PlayerController, UForestStartMenu::StaticClass());
	if (!StartMenu)
	{
		return;
	}
	StartMenu->AddToViewport(100);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(StartMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}

void UForestGameInstance::StartForestIntro()
{
	if (bIntroStarted)
	{
		return;
	}
	bIntroStarted = true;
	if (StartMenu)
	{
		StartMenu->RemoveFromParent();
		StartMenu = nullptr;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!World || !PlayerController)
	{
		return;
	}

	AForestIntroDirector* Director = World->SpawnActor<AForestIntroDirector>();
	if (Director)
	{
		Director->StartIntro(PlayerController);
	}
}
