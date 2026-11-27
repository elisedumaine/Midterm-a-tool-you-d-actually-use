#include "ForestGameInstance.h"

#include "UObject/UObjectGlobals.h"
#include "ForestCreator.h"
#include "ForestIntroDirector.h"
#include "ForestSky.h"
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
	if (!bMapHooked)
	{
		bMapHooked = true;
		FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UForestGameInstance::HandleMapLoaded);
	}
}

void UForestGameInstance::ReturnToTitle()
{
	bBackToTitle = true;
}

void UForestGameInstance::HandleMapLoaded(UWorld* LoadedWorld)
{
	if (!bBackToTitle || !LoadedWorld || LoadedWorld->GetGameInstance() != this)
	{
		return;
	}
	bBackToTitle = false;
	bMenuShown = false;
	bIntroStarted = false;
	StartMenu = nullptr;
	Creator = nullptr;
	Director = nullptr;
	LoadedWorld->GetTimerManager().SetTimer(MenuRetryTimer, this, &UForestGameInstance::TryShowStartMenu, 0.15f, true);
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

	// The character and world chosen last time.
	const FForestSetup Setup = FForestSetup::Load();
	Setup.ApplyCharacter(World);
	Setup.ApplyWorld(World, false);
	// The sun stays at golden hour while the player chooses.
	if (AForestSky* Sky = AForestSky::Get(World))
	{
		Sky->DayMinutes = 100000.0f;
	}

	Director = World->SpawnActor<AForestIntroDirector>();
	if (Director.IsValid())
	{
		Director->StartTitle(PlayerController);
	}

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
	if (Creator)
	{
		Creator->RemoveFromParent();
		Creator = nullptr;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!World || !PlayerController)
	{
		return;
	}

	FForestSetup::Load().ApplyDayLength(World);
	if (!Director.IsValid())
	{
		Director = World->SpawnActor<AForestIntroDirector>();
	}
	if (Director.IsValid())
	{
		Director->StartIntro(PlayerController);
	}
}

void UForestGameInstance::OpenCreator()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (bIntroStarted || !PlayerController)
	{
		return;
	}
	if (StartMenu)
	{
		StartMenu->RemoveFromParent();
		StartMenu = nullptr;
	}
	if (!Creator)
	{
		Creator = CreateWidget<UForestCreator>(PlayerController, UForestCreator::StaticClass());
	}
	if (!Creator)
	{
		StartForestIntro();
		return;
	}
	Creator->AddToViewport(100);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Creator->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;
}

void UForestGameInstance::BackToTitle()
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (bIntroStarted || !PlayerController)
	{
		return;
	}
	if (Creator)
	{
		Creator->RemoveFromParent();
		Creator = nullptr;
	}
	ShowCreatorView(1);
	StartMenu = CreateWidget<UForestStartMenu>(PlayerController, UForestStartMenu::StaticClass());
	if (StartMenu)
	{
		StartMenu->AddToViewport(100);
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(StartMenu->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

void UForestGameInstance::ShowCreatorView(int32 Page)
{
	if (Director.IsValid())
	{
		if (Page == 0)
		{
			Director->ShowPortrait();
		}
		else
		{
			Director->ShowForest();
		}
	}
}
