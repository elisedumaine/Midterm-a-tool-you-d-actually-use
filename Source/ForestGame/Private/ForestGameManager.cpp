#include "ForestGameManager.h"
#include "ForestSpirit.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestBlocks.h"
#include "ForestBuilding.h"
#include "ForestCrafting.h"
#include "ForestGame.h"
#include "ForestHUD.h"
#include "ForestLanguage.h"
#include "ForestPickup.h"
#include "ForestSaveGame.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FString Capitalized(FString Text)
	{
		if (!Text.IsEmpty())
		{
			Text[0] = FChar::ToUpper(Text[0]);
		}
		return Text;
	}

	FString AnimalName(const AForestAnimal* Animal, bool bTo = false)
	{
		return ForestLanguage::Animal(Animal->DisplayName.ToString(), bTo);
	}

	/** "stone_smallA" -> "stone", "plant_bushLarge" -> "plant", "lantern" -> "lantern". */
	FString MeshFamily(const UStaticMesh* Mesh)
	{
		FString Name = Mesh ? Mesh->GetName() : FString();
		int32 Underscore = INDEX_NONE;
		if (Name.FindChar(TEXT('_'), Underscore))
		{
			Name.LeftInline(Underscore);
		}
		return Name;
	}

	FString RecipeName(const ForestCrafting::FRecipe& Recipe)
	{
		return ForestLanguage::Pick(Recipe.English, Recipe.French);
	}

	const FKey NumberKeys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
}

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

// --- Start and end ------------------------------------------------------------

void AForestGameManager::BeginPlay()
{
	Super::BeginPlay();

	WorldCenter = GetActorLocation();
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		if (Animals.IsEmpty())
		{
			// The World Builder gives every animal the playable circle.
			WorldCenter = It->WorldCenter;
			WorldRadius = It->WorldRadius;
		}
		Animals.Add(*It);
	}
	AnimalsTotal = Animals.Num();
	for (TActorIterator<AForestPickup> It(GetWorld()); It; ++It)
	{
		Pickups.Add(*It);
	}

	// Where the trees and rocks of the generated forest stand, and the colours it paints its pieces with.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (const UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			if (!Mesh)
			{
				continue;
			}
			UMaterialInterface* Look = Component->GetMaterial(0);
			if (Look && Look->GetName().Contains(TEXT("M_IWB_Tint")) && Mesh->GetPathName().Contains(TEXT("/Imported/")))
			{
				WorldLooks.FindOrAdd(MeshFamily(Mesh), Look);
			}
			const FString Path = Mesh->GetPathName();
			const bool bTree = Path.Contains(TEXT("SM_LPS_tree")) || Path.Contains(TEXT("Kenney/tree_"));
			const bool bRock = Path.Contains(TEXT("SM_LPS_rock")) || Path.Contains(TEXT("Kenney/rock_"));
			if (!bTree && !bRock)
			{
				continue;
			}
			for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
			{
				FTransform Transform;
				if (Component->GetInstanceTransform(Index, Transform, true)
					&& FVector::Dist2D(Transform.GetLocation(), WorldCenter) < WorldRadius)
				{
					(bTree ? TreeSpots : RockSpots).Add(Transform.GetLocation());
				}
			}
		}
	}

	ScatterResources();
	LoadCamp();
	CountTamed();

	ShowMessage(FText::FromString(ForestLanguage::Pick(
		TEXT("Gather wood, stones and berries. Feed the animals until they trust you, and build your camp (B)."),
		TEXT("Ramasse du bois, des pierres et des baies. Nourris les animaux jusqu'à ce qu'ils te fassent confiance, et construis ton camp (B)."))), 7.0f);
	UE_LOG(LogForestGame, Log, TEXT("Forest game started with %d animals, %d trees, %d rocks, %d pickups."), AnimalsTotal, TreeSpots.Num(), RockSpots.Num(), Pickups.Num());
}

void AForestGameManager::EndPlay(const EEndPlayReason::Type Reason)
{
	SaveCamp();
	Super::EndPlay(Reason);
}

void AForestGameManager::DressLikeWorld(UStaticMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}
	if (const TObjectPtr<UMaterialInterface>* Look = WorldLooks.Find(MeshFamily(Component->GetStaticMesh())))
	{
		for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
		{
			Component->SetMaterial(Slot, *Look);
		}
	}
}

void AForestGameManager::ShowMessage(const FText& NewMessage, float Seconds)
{
	Message = NewMessage;
	MessageTimeLeft = Seconds;
}

// --- The forest's resources -------------------------------------------------

bool AForestGameManager::GroundAt(float X, float Y, float& OutZ) const
{
	FHitResult Hit;
	const FVector Start(X, Y, WorldCenter.Z + 20000.0f);
	const FVector End(X, Y, WorldCenter.Z - 20000.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestGround), false, this);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	if (Ghost)
	{
		Params.AddIgnoredActor(Ghost);
	}
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	// On top of a tree, a rock or a building, or in the river: not ground.
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component || Component->IsA<UInstancedStaticMeshComponent>() || Cast<AForestBuilding>(Hit.GetActor()))
	{
		return false;
	}
	if (const UMaterialInterface* Material = Component->GetMaterial(0))
	{
		if (Material->GetName().Contains(TEXT("Water")))
		{
			return false;
		}
	}
	if (Hit.ImpactNormal.Z < 0.8f)
	{
		return false;
	}
	OutZ = Hit.ImpactPoint.Z;
	return true;
}

FVector AForestGameManager::RandomSpotNear(const FVector& Around, float MinDistance, float MaxDistance, bool& bFound) const
{
	bFound = false;
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
		const FVector Spot = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		if (FVector::Dist2D(Spot, WorldCenter) > WorldRadius * 0.95f)
		{
			continue;
		}
		float Z = 0.0f;
		if (!GroundAt(Spot.X, Spot.Y, Z))
		{
			continue;
		}
		bool bCrowded = false;
		for (const AForestPickup* Other : Pickups)
		{
			if (Other && FVector::Dist2D(Other->GetActorLocation(), Spot) < 220.0f)
			{
				bCrowded = true;
				break;
			}
		}
		if (!bCrowded)
		{
			bFound = true;
			return FVector(Spot.X, Spot.Y, Z);
		}
	}
	return Around;
}

void AForestGameManager::ScatterResources()
{
	struct FKind
	{
		FName Item;
		int32 Count;
		const TArray<FVector>* Near;
		float MinDistance;
		float MaxDistance;
	};
	// Wood under the trees, stones by the rocks, berry bushes at the forest's edge.
	const FKind Kinds[] = {
		{ForestCrafting::Wood, WoodPiles, &TreeSpots, 140.0f, 380.0f},
		{ForestCrafting::Stone, StonePiles, RockSpots.IsEmpty() ? &TreeSpots : &RockSpots, 120.0f, 420.0f},
		{ForestCrafting::Berry, BerryBushes, &TreeSpots, 380.0f, 900.0f},
	};
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FKind& Kind : Kinds)
	{
		if (Kind.Near->IsEmpty())
		{
			continue;
		}
		int32 Placed = 0;
		for (int32 Try = 0; Try < Kind.Count * 6 && Placed < Kind.Count; ++Try)
		{
			const FVector& Around = (*Kind.Near)[FMath::RandRange(0, Kind.Near->Num() - 1)];
			bool bFound = false;
			const FVector Spot = RandomSpotNear(Around, Kind.MinDistance, Kind.MaxDistance, bFound);
			if (!bFound)
			{
				continue;
			}
			AForestPickup* Pickup = GetWorld()->SpawnActor<AForestPickup>(AForestPickup::StaticClass(),
				FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Spot + FVector(0.0f, 0.0f, 2.0f)), Params);
			if (Pickup)
			{
				Pickup->Configure(Kind.Item);
				DressLikeWorld(Pickup->Mesh);
				Pickups.Add(Pickup);
				++Placed;
			}
		}
	}
}

// --- The basket -------------------------------------------------------------

int32 AForestGameManager::Count(FName Item) const
{
	const int32* Found = Items.Find(Item);
	return Found ? *Found : 0;
}

void AForestGameManager::AddItem(FName Item, int32 Amount)
{
	int32& Stock = Items.FindOrAdd(Item);
	Stock = FMath::Max(0, Stock + Amount);
}

FName AForestGameManager::BestFoodFor(const AForestAnimal* Animal) const
{
	if (!Animal)
	{
		return NAME_None;
	}
	if (Count(Animal->FavoriteFood) > 0)
	{
		return Animal->FavoriteFood;
	}
	for (const FName Item : ForestCrafting::Items())
	{
		if (ForestCrafting::IsFood(Item) && Animal->Eats(Item) && Count(Item) > 0)
		{
			return Item;
		}
	}
	return NAME_None;
}

// --- Asked by the animals -------------------------------------------------

AActor* AForestGameManager::FindMeal(const AForestAnimal* Animal, const FVector& From, float Range) const
{
	AActor* Best = nullptr;
	float BestDistance = Range;
	for (AForestPickup* Pickup : Pickups)
	{
		if (!Pickup || Pickup->IsCollected() || !Animal->Eats(Pickup->Item) || !ForestCrafting::IsFood(Pickup->Item))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(From, Pickup->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Pickup;
		}
	}
	// A filled feeder is worth walking a bit further for.
	if (AForestBuilding* Feeder = NearestFeeder(From, Range * 1.5f))
	{
		if (FVector::Dist2D(From, Feeder->GetActorLocation()) < BestDistance * 1.5f)
		{
			Best = Feeder;
		}
	}
	return Best;
}

AForestBuilding* AForestGameManager::NearestFeeder(const FVector& From, float Range) const
{
	AForestBuilding* Best = nullptr;
	float BestDistance = Range;
	for (AForestBuilding* Building : Buildings)
	{
		if (!Building || Building->GetBuildingType() != TEXT("Feeder") || Building->GetFood() <= 0)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(From, Building->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Building;
		}
	}
	return Best;
}

bool AForestGameManager::FindBuilding(FName Type, const FVector& From, float Range, FVector& OutSpot) const
{
	float BestDistance = Range;
	bool bFound = false;
	for (const AForestBuilding* Building : Buildings)
	{
		if (!Building || Building->GetBuildingType() != Type)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(From, Building->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			OutSpot = Building->GetActorLocation();
			bFound = true;
		}
	}
	return bFound;
}

bool AForestGameManager::FindShelter(const FVector& From, FVector& OutSpot) const
{
	if (FindBuilding(TEXT("Shelter"), From, 3500.0f, OutSpot))
	{
		// Spread out a little under the roof.
		OutSpot += FVector(FMath::FRandRange(-60.0f, 40.0f), FMath::FRandRange(-90.0f, 90.0f), 0.0f);
		return true;
	}
	// Under a roof the player built with blocks.
	if (const UForestBlocks* Blocks = UForestBlocks::Get(this))
	{
		if (Blocks->FindRoofSpot(From, 3500.0f, OutSpot))
		{
			return true;
		}
	}
	float BestDistance = 5000.0f;
	const FVector* Best = nullptr;
	for (const FVector& Tree : TreeSpots)
	{
		const float Distance = FVector::Dist2D(From, Tree);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = &Tree;
		}
	}
	if (!Best)
	{
		return false;
	}
	// Right next to the trunk, on the side it comes from.
	FVector Side = From - *Best;
	Side.Z = 0.0f;
	OutSpot = *Best + Side.GetSafeNormal() * 110.0f;
	return true;
}

// --- Every frame -------------------------------------------------------------

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

	SaveTimer += DeltaSeconds;
	if (SaveTimer > 30.0f)
	{
		SaveCamp();
	}

	const APawn* Player = Controller->GetPawn();
	// The title screen and the wake-up intro hold the player still: no game keys yet.
	if (!Player || Controller->IsMoveInputIgnored())
	{
		Prompt = FText::GetEmpty();
		return;
	}
	ReadKeys(Controller);
	UpdateGhost(Player);
	UpdatePrompt(Player);
}

void AForestGameManager::ReadKeys(APlayerController* Controller)
{
	auto Pressed = [Controller](const FKey& Key) { return Controller->WasInputKeyJustPressed(Key); };

	if (Pressed(EKeys::B) || Pressed(EKeys::Gamepad_FaceButton_Top))
	{
		ToggleBuildMenu();
		return;
	}
	if (Pressed(EKeys::H))
	{
		bShowHelp = !bShowHelp;
	}
	if (Pressed(EKeys::F9))
	{
		ResetCamp();
		return;
	}
	if (bBuildMenu && !IsPlacing())
	{
		for (int32 Index = 0; Index < ForestCrafting::Recipes().Num() && Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
		{
			if (Pressed(NumberKeys[Index]))
			{
				ChooseRecipe(Index);
				return;
			}
		}
	}
	if (IsPlacing())
	{
		if (Pressed(EKeys::R) || Pressed(EKeys::MouseScrollUp))
		{
			GhostYaw += 45.0f;
		}
		if (Pressed(EKeys::MouseScrollDown))
		{
			GhostYaw -= 45.0f;
		}
		if (Pressed(EKeys::RightMouseButton) || Pressed(EKeys::Gamepad_FaceButton_Right))
		{
			CancelPlacing();
			return;
		}
		if (Pressed(EKeys::LeftMouseButton))
		{
			PlaceBuilding();
			return;
		}
	}
	if (Pressed(InteractKey) || Pressed(EKeys::Gamepad_FaceButton_Left))
	{
		Interact();
	}
	if (Pressed(EKeys::F))
	{
		const APawn* Player = Controller->GetPawn();
		AForestAnimal* Animal = Player ? NearestAnimal(Player->GetActorLocation()) : nullptr;
		if (Animal && Animal->IsTame())
		{
			Animal->ToggleStay();
			const FString Name = Capitalized(AnimalName(Animal));
			ShowMessage(FText::FromString(Animal->IsStaying()
				? FOREST_PRINTF(TEXT("%s stays here at the camp."), TEXT("%s reste ici, au camp."), *Name)
				: FOREST_PRINTF(TEXT("%s follows you again."), TEXT("%s te suit de nouveau."), *Name)), 2.5f);
			SaveCamp();
		}
	}
}

AForestPickup* AForestGameManager::NearestPickup(const FVector& From) const
{
	AForestPickup* Best = nullptr;
	float BestDistance = PickupRange;
	for (AForestPickup* Pickup : Pickups)
	{
		if (!Pickup || Pickup->IsCollected())
		{
			continue;
		}
		const float Distance = FVector::Dist(From, Pickup->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Pickup;
		}
	}
	return Best;
}

AForestAnimal* AForestGameManager::NearestAnimal(const FVector& From) const
{
	AForestAnimal* Best = nullptr;
	float BestDistance = FeedRange;
	for (AForestAnimal* Animal : Animals)
	{
		if (!Animal || Animal->GetAnimalState() == EAnimalState::Dancing)
		{
			continue;
		}
		const float Distance = FVector::Dist(From, Animal->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Animal;
		}
	}
	return Best;
}

void AForestGameManager::UpdatePrompt(const APawn* Player)
{
	Prompt = FText::GetEmpty();
	if (IsPlacing())
	{
		Prompt = FText::FromString(bPlacementValid
			? ForestLanguage::Pick(TEXT("[E] Build here   [R] Turn   [Right click] Cancel"), TEXT("[E] Construire ici   [R] Tourner   [Clic droit] Annuler"))
			: ForestLanguage::Pick(TEXT("Not here: find a flat, open spot   [Right click] Cancel"), TEXT("Pas ici : trouve un endroit plat et dégagé   [Clic droit] Annuler")));
		return;
	}
	if (bBuildMenu)
	{
		return;
	}
	const FVector From = Player->GetActorLocation();
	if (const AForestPickup* Pickup = NearestPickup(From))
	{
		Prompt = FText::FromString(FOREST_PRINTF(TEXT("[E]  Pick up %s"), TEXT("[E]  Ramasser : %s"), *ForestCrafting::ItemName(Pickup->Item, Pickup->Amount)));
		return;
	}
	if (const AForestAnimal* Animal = NearestAnimal(From))
	{
		const FString To = AnimalName(Animal, true);
		const FName Food = BestFoodFor(Animal);
		if (Animal->IsTame())
		{
			Prompt = FText::FromString(Animal->IsHungry() && Food != NAME_None
				? FOREST_PRINTF(TEXT("[E]  Give %s %s   [F] Stay / follow"), TEXT("[E]  Donner : %s %s   [F] Reste / suis-moi"), *ForestCrafting::ItemName(Food, 1), *To)
				: FOREST_PRINTF(TEXT("[E]  Pet %s   [F] Stay / follow"), TEXT("[E]  Caresser %s   [F] Reste / suis-moi"), *AnimalName(Animal)));
		}
		else if (Food == NAME_None)
		{
			Prompt = FText::FromString(FOREST_PRINTF(TEXT("%s would like %s"), TEXT("%s aimerait : %s"),
				*Capitalized(AnimalName(Animal)), *ForestCrafting::ItemName(Animal->FavoriteFood, 2)));
		}
		else
		{
			const bool bFavorite = Food == Animal->FavoriteFood;
			Prompt = FText::FromString(FOREST_PRINTF(TEXT("[E]  Give %s %s%s"), TEXT("[E]  Donner : %s %s%s"),
				*ForestCrafting::ItemName(Food, 1), *To, bFavorite ? ForestLanguage::Pick(TEXT("  (its favourite!)"), TEXT("  (son préféré !)")) : TEXT("")));
		}
		return;
	}
	for (const AForestBuilding* Building : Buildings)
	{
		if (Building && Building->GetBuildingType() == TEXT("Feeder") && FVector::Dist2D(From, Building->GetActorLocation()) < 300.0f
			&& Building->GetFood() < AForestBuilding::FeederCapacity)
		{
			Prompt = FText::FromString(Count(ForestCrafting::Berry) > 0
				? FOREST_PRINTF(TEXT("[E]  Fill the feeder with berries (%d / %d)"), TEXT("[E]  Remplir la mangeoire de baies (%d / %d)"), Building->GetFood(), AForestBuilding::FeederCapacity)
				: FOREST_PRINTF(TEXT("The feeder needs berries (%d / %d)"), TEXT("La mangeoire a besoin de baies (%d / %d)"), Building->GetFood(), AForestBuilding::FeederCapacity));
			return;
		}
	}
}

void AForestGameManager::Interact()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}
	if (IsPlacing())
	{
		PlaceBuilding();
		return;
	}
	if (bBuildMenu)
	{
		return;
	}
	const FVector From = Player->GetActorLocation();

	if (AForestPickup* Pickup = NearestPickup(From))
	{
		Pickup->Collect();
		AddItem(Pickup->Item, Pickup->Amount);
		// The forest gives back what it is given: wood and stone are dear to it, berries grow back.
		if (UForestSpirit* Spirit = UForestSpirit::Get(this))
		{
			Spirit->ChangeHealth(Pickup->Item == ForestCrafting::Wood || Pickup->Item == ForestCrafting::Stone ? -1.5f : -0.4f);
		}
		// Bushes give fibre with their berries; old wood sometimes holds resin.
		FString Extra;
		if (Pickup->Item == ForestCrafting::Berry)
		{
			AddItem(ForestCrafting::Fiber, 2);
			Extra = TEXT("   +") + ForestCrafting::ItemName(ForestCrafting::Fiber, 2);
		}
		else if (Pickup->Item == ForestCrafting::Wood && FMath::FRand() < 0.35f)
		{
			AddItem(ForestCrafting::Resin, 1);
			Extra = TEXT("   +") + ForestCrafting::ItemName(ForestCrafting::Resin, 1);
		}
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("+%s  (%d in your basket)"), TEXT("+%s  (%d dans ton panier)"),
			*ForestCrafting::ItemName(Pickup->Item, Pickup->Amount), Count(Pickup->Item)) + Extra), 2.0f);
		return;
	}

	if (AForestAnimal* Animal = NearestAnimal(From))
	{
		const FString Name = Capitalized(AnimalName(Animal));
		const FName Food = BestFoodFor(Animal);
		if (Animal->IsTame() && !(Animal->IsHungry() && Food != NAME_None))
		{
			Animal->Pet();
			ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s loves that."), TEXT("%s adore ça."), *Name)), 2.0f);
			return;
		}
		if (Food == NAME_None)
		{
			Animal->Refuse();
			const FString Favorite = ForestCrafting::ItemName(Animal->FavoriteFood, 2);
			// English says "nothing the deer eats", the others "nothing to give to the deer".
			ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("You have nothing %s eats... It loves %s."), TEXT("Tu n'as rien à donner %s... Il adore : %s."),
				*AnimalName(Animal, !ForestLanguage::IsEnglish()), *Favorite)), 3.0f);
			return;
		}
		const bool bWasTame = Animal->IsTame();
		if (!Animal->Feed(Player, Food))
		{
			return;
		}
		AddItem(Food, -1);
		if (Animal->IsTame() && !bWasTame)
		{
			CountTamed();
			SaveCamp();
			if (!bWon)
			{
				ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s trusts you now and follows you! (%d / %d tame)"), TEXT("%s te fait confiance et te suit ! (%d / %d apprivoisés)"),
					*Name, AnimalsTamed, AnimalsTotal)), 3.5f);
			}
		}
		else if (!Animal->IsTame())
		{
			const int32 Percent = FMath::RoundToInt(Animal->GetTrust() * 100.0f);
			ShowMessage(FText::FromString(Food == Animal->FavoriteFood
				? FOREST_PRINTF(TEXT("Yum, its favourite! %s trusts you more (%d%%)."), TEXT("Miam, son préféré ! %s te fait plus confiance (%d%%)."), *Name, Percent)
				: FOREST_PRINTF(TEXT("Yum! %s trusts you a little more (%d%%)."), TEXT("Miam ! %s te fait un peu plus confiance (%d%%)."), *Name, Percent)), 3.0f);
		}
		else
		{
			ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s eats happily."), TEXT("%s mange avec plaisir."), *Name)), 2.0f);
		}
		return;
	}

	for (AForestBuilding* Building : Buildings)
	{
		if (Building && Building->GetBuildingType() == TEXT("Feeder") && FVector::Dist2D(From, Building->GetActorLocation()) < 300.0f)
		{
			const int32 Added = Building->AddFood(Count(ForestCrafting::Berry));
			if (Added > 0)
			{
				AddItem(ForestCrafting::Berry, -Added);
				ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("You put %s in the feeder."), TEXT("Tu mets %s dans la mangeoire."), *ForestCrafting::ItemName(ForestCrafting::Berry, Added))), 2.5f);
				SaveCamp();
			}
			return;
		}
	}
}

void AForestGameManager::CountTamed()
{
	AnimalsTamed = 0;
	for (const AForestAnimal* Animal : Animals)
	{
		if (Animal && Animal->IsTame())
		{
			++AnimalsTamed;
		}
	}
	if (!bWon && AnimalsTotal > 0 && AnimalsTamed >= AnimalsTotal)
	{
		bWon = true;
		for (AForestAnimal* Animal : Animals)
		{
			if (Animal)
			{
				Animal->Dance();
			}
		}
		ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("Every animal of the forest trusts you! The forest thanks you."), TEXT("Tous les animaux de la forêt te font confiance ! La forêt te remercie."))), 10.0f);
	}
}

// --- Building -------------------------------------------------------------

bool AForestGameManager::CanAfford(FName RecipeId) const
{
	const ForestCrafting::FRecipe* Recipe = ForestCrafting::FindRecipe(RecipeId);
	if (!Recipe)
	{
		return false;
	}
	for (const ForestCrafting::FCost& Cost : Recipe->Cost)
	{
		if (Count(Cost.Item) < Cost.Amount)
		{
			return false;
		}
	}
	return true;
}

void AForestGameManager::ToggleBuildMenu()
{
	if (IsPlacing() || bBuildMenu)
	{
		CancelPlacing();
		bBuildMenu = false;
		return;
	}
	bBuildMenu = true;
}

void AForestGameManager::ChooseRecipe(int32 Index)
{
	const TArray<ForestCrafting::FRecipe>& Recipes = ForestCrafting::Recipes();
	if (!Recipes.IsValidIndex(Index))
	{
		return;
	}
	const ForestCrafting::FRecipe& Recipe = Recipes[Index];
	if (!CanAfford(Recipe.Id))
	{
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s needs %s."), TEXT("%s : il faut %s."), *RecipeName(Recipe), *ForestCrafting::CostText(Recipe))), 3.0f);
		return;
	}
	CancelPlacing();
	PlacingRecipe = Recipe.Id;
	Ghost = SpawnBuilding(Recipe.Id, GetActorTransform(), true);
	bBuildMenu = false;
}

AForestBuilding* AForestGameManager::SpawnBuilding(FName Type, const FTransform& Transform, bool bGhost)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AForestBuilding* Building = GetWorld()->SpawnActor<AForestBuilding>(AForestBuilding::StaticClass(), Transform, Params);
	if (Building)
	{
		Building->Build(Type, bGhost);
	}
	return Building;
}

void AForestGameManager::UpdateGhost(const APawn* Player)
{
	if (!Ghost)
	{
		return;
	}
	// A few steps in front of the player, on the ground.
	const APlayerController* Controller = Cast<APlayerController>(Player->GetController());
	const float Yaw = Controller ? Controller->GetControlRotation().Yaw : Player->GetActorRotation().Yaw;
	const FVector Forward = FRotator(0.0f, Yaw, 0.0f).Vector();
	const FVector Spot = Player->GetActorLocation() + Forward * 380.0f;
	float Z = Player->GetActorLocation().Z - 90.0f;
	const bool bGround = GroundAt(Spot.X, Spot.Y, Z);
	Ghost->SetActorLocationAndRotation(FVector(Spot.X, Spot.Y, Z), FRotator(0.0f, Yaw + GhostYaw, 0.0f));

	// Free if nothing solid (tree, rock, another building) stands where it goes.
	bool bFree = bGround;
	if (bFree)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestPlace), false, Ghost);
		Params.AddIgnoredActor(Player);
		const float Radius = FMath::Min(Ghost->GetRadius() * 0.55f, 90.0f);
		bFree = !GetWorld()->OverlapAnyTestByObjectType(FVector(Spot.X, Spot.Y, Z + Radius + 25.0f), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params);
	}
	bPlacementValid = bFree && FVector::Dist2D(Spot, WorldCenter) < WorldRadius;
	Ghost->SetGhostValid(bPlacementValid);
}

void AForestGameManager::PlaceBuilding()
{
	const ForestCrafting::FRecipe* Recipe = ForestCrafting::FindRecipe(PlacingRecipe);
	if (!Ghost || !Recipe)
	{
		return;
	}
	if (!bPlacementValid)
	{
		ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("Not here: find a flat, open spot."), TEXT("Pas ici : trouve un endroit plat et dégagé."))), 2.0f);
		return;
	}
	if (!CanAfford(Recipe->Id))
	{
		ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s needs %s."), TEXT("%s : il faut %s."), *RecipeName(*Recipe), *ForestCrafting::CostText(*Recipe))), 3.0f);
		CancelPlacing();
		return;
	}
	for (const ForestCrafting::FCost& Cost : Recipe->Cost)
	{
		AddItem(Cost.Item, -Cost.Amount);
	}
	if (AForestBuilding* Building = SpawnBuilding(Recipe->Id, Ghost->GetActorTransform(), false))
	{
		Buildings.Add(Building);
	}
	ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("%s built!"), TEXT("%s construit !"), *RecipeName(*Recipe))), 2.5f);
	CancelPlacing();
	SaveCamp();
}

void AForestGameManager::CancelPlacing()
{
	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}
	PlacingRecipe = NAME_None;
	bPlacementValid = false;
}

// --- Saving the camp ----------------------------------------------------

FString AForestGameManager::SaveSlot() const
{
	return TEXT("ForestCamp_") + UGameplayStatics::GetCurrentLevelName(this, true);
}

void AForestGameManager::SaveCamp()
{
	SaveTimer = 0.0f;
	UForestSaveGame* Save = Cast<UForestSaveGame>(UGameplayStatics::CreateSaveGameObject(UForestSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->Level = UGameplayStatics::GetCurrentLevelName(this, true);
	Save->Items = Items;
	for (const AForestBuilding* Building : Buildings)
	{
		if (Building)
		{
			FForestSavedBuilding& Saved = Save->Buildings.AddDefaulted_GetRef();
			Saved.Type = Building->GetBuildingType();
			Saved.Transform = Building->GetActorTransform();
			Saved.Food = Building->GetFood();
		}
	}
	for (const AForestAnimal* Animal : Animals)
	{
		if (Animal && (Animal->GetTrust() > 0.0f || Animal->IsTame()))
		{
			FForestSavedAnimal& Saved = Save->Animals.AddDefaulted_GetRef();
			Saved.Name = Animal->GetFName();
			Saved.Trust = Animal->GetTrust();
			Saved.bTame = Animal->IsTame();
			Saved.bStaying = Animal->IsStaying();
			Saved.Location = Animal->IsStaying() ? Animal->GetHome() : Animal->GetActorLocation();
		}
	}
	UGameplayStatics::SaveGameToSlot(Save, SaveSlot(), 0);
}

void AForestGameManager::LoadCamp()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0))
	{
		return;
	}
	const UForestSaveGame* Save = Cast<UForestSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0));
	if (!Save)
	{
		return;
	}
	Items = Save->Items;
	for (const FForestSavedBuilding& Saved : Save->Buildings)
	{
		if (AForestBuilding* Building = SpawnBuilding(Saved.Type, Saved.Transform, false))
		{
			Building->SetFood(Saved.Food);
			Buildings.Add(Building);
		}
	}
	for (const FForestSavedAnimal& Saved : Save->Animals)
	{
		for (AForestAnimal* Animal : Animals)
		{
			if (Animal && Animal->GetFName() == Saved.Name)
			{
				Animal->Restore(Saved.Trust, Saved.bTame, Saved.bStaying, Saved.Location);
				break;
			}
		}
	}
	UE_LOG(LogForestGame, Log, TEXT("Loaded the camp: %d buildings, %d animals that know the player."), Save->Buildings.Num(), Save->Animals.Num());
}

void AForestGameManager::ResetCamp()
{
	CancelPlacing();
	bBuildMenu = false;
	for (AForestBuilding* Building : Buildings)
	{
		if (Building)
		{
			Building->Destroy();
		}
	}
	Buildings.Reset();
	Items.Reset();
	UGameplayStatics::DeleteGameInSlot(SaveSlot(), 0);
	// The animals forget the player too.
	for (AForestAnimal* Animal : Animals)
	{
		if (Animal)
		{
			Animal->Restore(0.0f, false, false, Animal->GetHome());
		}
	}
	bWon = false;
	CountTamed();
	ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("A fresh camp: basket, buildings and the animals' trust are cleared."),
		TEXT("Nouveau camp : panier, constructions et confiance des animaux effacés."))), 5.0f);
}

void AForestGameManager::AddAnimal(AForestAnimal* Animal)
{
	if (Animal && !Animals.Contains(Animal))
	{
		Animals.Add(Animal);
		AnimalsTotal = Animals.Num();
	}
}
