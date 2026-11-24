#include "ForestGameManager.h"

#include "ForestLanguage.h"

#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestGame.h"
#include "ForestHUD.h"
#include "ForestPickup.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AForestGameManager::AForestGameManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AForestGameManager* AForestGameManager::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AForestGameManager> It(World);
	return It ? *It : nullptr;
}

void AForestGameManager::BeginPlay()
{
	Super::BeginPlay();
	AnimalsTotal = 0;
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		++AnimalsTotal;
	}
	ShowMessage(FText::FromString(AnimalsTotal > 0
		? ForestLanguage::Pick(TEXT("The forest animals are hungry! Pick up mushrooms and approach them gently."),
			TEXT("Les animaux de la forêt ont faim ! Ramasse des champignons et approche-toi doucement."))
		: ForestLanguage::Pick(TEXT("Pick up the mushrooms of the forest."), TEXT("Ramasse les champignons de la forêt."))), 6.0f);
	UE_LOG(LogForestGame, Log, TEXT("Forest game started with %d animals."), AnimalsTotal);
}

void AForestGameManager::ShowMessage(const FText& NewMessage, float Seconds)
{
	Message = NewMessage;
	MessageTimeLeft = Seconds;
}

AForestPickup* AForestGameManager::NearestPickup(const FVector& From) const
{
	AForestPickup* Best = nullptr;
	float BestDistance = PickupRange;
	for (TActorIterator<AForestPickup> It(GetWorld()); It; ++It)
	{
		if (It->IsCollected())
		{
			continue;
		}
		const float Distance = FVector::Dist(From, It->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

AForestAnimal* AForestGameManager::NearestAnimal(const FVector& From) const
{
	AForestAnimal* Best = nullptr;
	float BestDistance = FeedRange;
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		if (It->IsFed())
		{
			continue;
		}
		const float Distance = FVector::Dist(From, It->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

void AForestGameManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	MessageTimeLeft = FMath::Max(0.0f, MessageTimeLeft - DeltaSeconds);

	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (!Controller)
	{
		return;
	}
	if (!bHudInstalled)
	{
		Controller->ClientSetHUD(AForestHUD::StaticClass());
		bHudInstalled = true;
	}

	const APawn* Player = Controller->GetPawn();
	Prompt = FText::GetEmpty();
	if (Player)
	{
		const FVector From = Player->GetActorLocation();
		if (NearestPickup(From))
		{
			Prompt = FText::FromString(ForestLanguage::Pick(TEXT("[E]  Pick up the mushroom"), TEXT("[E]  Ramasser le champignon")));
		}
		else if (const AForestAnimal* Animal = NearestAnimal(From))
		{
			const FString Name = Animal->DisplayName.ToString();
			Prompt = FText::FromString(Mushrooms > 0
				? FOREST_PRINTF(TEXT("[E]  Give a mushroom %s"), TEXT("[E]  Donner un champignon %s"), *ForestLanguage::Animal(Name, true))
				: FOREST_PRINTF(TEXT("You need a mushroom to feed %s"), TEXT("Il te faut un champignon pour nourrir %s"), *ForestLanguage::Animal(Name)));
		}
	}

	if (Controller->WasInputKeyJustPressed(InteractKey) || Controller->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left))
	{
		Interact();
	}
}

void AForestGameManager::Interact()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}
	const FVector From = Player->GetActorLocation();

	if (AForestPickup* Pickup = NearestPickup(From))
	{
		Pickup->Collect();
		++Mushrooms;
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("+1 mushroom  (%d in your basket)"), TEXT("+1 champignon  (%d dans ton panier)"), Mushrooms)), 2.0f);
		return;
	}

	AForestAnimal* Animal = NearestAnimal(From);
	if (!Animal)
	{
		return;
	}
	FString Name = Animal->DisplayName.ToString();
	if (Mushrooms <= 0)
	{
		Animal->Refuse();
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("You have nothing to give %s... Go and find a mushroom!"), TEXT("Tu n'as rien à donner %s... Va trouver un champignon !"), *ForestLanguage::Animal(Name, true))), 2.5f);
		return;
	}
	if (!Animal->Feed(Player))
	{
		return;
	}
	--Mushrooms;
	++AnimalsFed;
	if (AnimalsTotal > 0 && AnimalsFed >= AnimalsTotal)
	{
		bWon = true;
		for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
		{
			It->Dance();
		}
		ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("All the animals are fed! The forest thanks you."), TEXT("Tous les animaux sont nourris ! La forêt te remercie."))), 10.0f);
	}
	else
	{
		Name = ForestLanguage::Animal(Name);
		if (!Name.IsEmpty())
		{
			Name[0] = FChar::ToUpper(Name[0]);
		}
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("Yum! %s is fed (%d / %d) and follows you now."), TEXT("Miam ! %s est nourri (%d / %d) et te suit maintenant."),
			*Name, AnimalsFed, AnimalsTotal)), 3.0f);
	}
}
