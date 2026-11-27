#include "ForestBonds.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestBlocks.h"
#include "ForestBuilding.h"
#include "ForestCrafting.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestLife.h"
#include "ForestNightBeast.h"
#include "ForestShop.h"
#include "ForestSky.h"
#include "ForestSpirit.h"
#include "ForestSurvival.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** A tame helper finds something after this many seconds by the player's side. */
	constexpr float HelperNear = 1500.0f;
	/** The fox hears a beast this far from the player. */
	constexpr float FoxHearing = 2600.0f;
	/** A tame boar or fox chases off a beast that comes this close to it. */
	constexpr float GuardReach = 520.0f;
	/** One home per house: roof spots closer than this are the same house. */
	constexpr float SameHouse = 700.0f;
	/** A shelter stands this long before someone moves in. */
	constexpr float ShelterSettle = 25.0f;
	constexpr int32 MaxArrivals = 6;

	struct FBondsHelper
	{
		const TCHAR* Species;
		FName Item;
		int32 Amount;
		float Seconds;
		const TCHAR* English;
		const TCHAR* French;
	};

	const FBondsHelper BondsHelpers[] = {
		{TEXT("Bunny"), TEXT("Berry"), 2, 70.0f, TEXT("%s sniffed out hidden berries for you! (+%d %s)"), TEXT("%s a flairé des baies cachées pour toi ! (+%d %s)")},
		{TEXT("Deer"), TEXT("Wood"), 2, 90.0f, TEXT("%s brings you wood on its back. (+%d %s)"), TEXT("%s t'apporte du bois sur son dos. (+%d %s)")},
		{TEXT("Boar"), TEXT("Mushroom"), 2, 80.0f, TEXT("%s dug up mushrooms for you! (+%d %s)"), TEXT("%s a déterré des champignons pour toi ! (+%d %s)")},
		{TEXT("Beaver"), TEXT("Plank"), 2, 100.0f, TEXT("%s gnawed planks for you. (+%d %s)"), TEXT("%s a rongé des planches pour toi. (+%d %s)")},
	};

	/** What a resident leaves at the door at dawn. */
	struct FBondsGift
	{
		const TCHAR* Species;
		FName Item;
		int32 Amount;
	};

	const FBondsGift BondsGifts[] = {
		{TEXT("Bunny"), TEXT("Berry"), 2},
		{TEXT("Deer"), TEXT("Wood"), 2},
		{TEXT("Fox"), TEXT("Resin"), 1},
		{TEXT("Boar"), TEXT("Mushroom"), 2},
		{TEXT("Beaver"), TEXT("Plank"), 2},
	};

	/** The rewards of the dawn, one after the other. */
	const FBondsGift BondsDawnRewards[] = {
		{TEXT(""), TEXT("Resin"), 2},
		{TEXT(""), TEXT("Plank"), 3},
		{TEXT(""), TEXT("Fiber"), 3},
		{TEXT(""), TEXT("Stone"), 4},
	};

	/** The species that come to the forest at dawn, in turn. */
	const TCHAR* BondsArrivalOrder[] = {TEXT("Bunny"), TEXT("Fox"), TEXT("Deer"), TEXT("Beaver"), TEXT("Bunny"), TEXT("Boar")};

	/** More newcomers in a thriving forest. */
	int32 BondsMaxArrivalsFor(const UForestSpirit* Spirit)
	{
		return Spirit && Spirit->IsThriving() ? MaxArrivals + 4 : MaxArrivals;
	}

	/** BP_Fox_C -> "Fox". */
	FString BondsSpeciesOf(const AForestAnimal* Animal)
	{
		FString Name = Animal ? Animal->GetClass()->GetName() : FString();
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	FString BondsCapitalized(FString Text)
	{
		if (!Text.IsEmpty())
		{
			Text[0] = FChar::ToUpper(Text[0]);
		}
		return Text;
	}

	FString BondsNameOf(const AForestAnimal* Animal)
	{
		return BondsCapitalized(ForestLanguage::Animal(Animal->DisplayName.ToString()));
	}

	/** Where something is, seen by the player: ahead, behind, left, right. */
	FString BondsDirection(const APawn* Player, const FVector& Where)
	{
		const APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
		const float ViewYaw = Controller ? Controller->GetControlRotation().Yaw : Player->GetActorRotation().Yaw;
		const float Angle = FRotator::NormalizeAxis((Where - Player->GetActorLocation()).Rotation().Yaw - ViewYaw);
		if (FMath::Abs(Angle) < 45.0f)
		{
			return ForestLanguage::Pick(TEXT("ahead of you"), TEXT("devant toi"));
		}
		if (FMath::Abs(Angle) > 135.0f)
		{
			return ForestLanguage::Pick(TEXT("behind you"), TEXT("derrière toi"));
		}
		return Angle > 0.0f ? ForestLanguage::Pick(TEXT("on your right"), TEXT("à ta droite")) : ForestLanguage::Pick(TEXT("on your left"), TEXT("à ta gauche"));
	}

	/** Forest.Dawn: pretend a night was just survived (for tests and the presentation). */
	FAutoConsoleCommandWithWorld GDawnCommand(
		TEXT("Forest.Dawn"),
		TEXT("Pretend a night was survived: the counter, the dawn reward, the residents' gifts."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UForestBonds* Bonds = UForestBonds::Get(World))
			{
				Bonds->Dawn(true);
			}
		}));

	/** Forest.MoveIn: an animal moves into the nearest shelter now. */
	FAutoConsoleCommandWithWorld GMoveInCommand(
		TEXT("Forest.MoveIn"),
		TEXT("An animal moves into the nearest free shelter now."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UForestBonds* Bonds = UForestBonds::Get(World))
			{
				Bonds->MoveSomeoneIn();
			}
		}));

	/** "Nights survived: 3", under the season label, top right. */
	class SForestNightsLabel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestNightsLabel) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestBonds>, Bonds)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			TWeakObjectPtr<UForestBonds> Weak = Args._Bonds;
			ChildSlot
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 124.0f, 24.0f, 0.0f))
				[
					SNew(SBorder)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsCounter() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.55f))
					.Padding(FMargin(18.0f, 6.0f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.Text_Lambda([Weak]()
						{
							const int32 Nights = Weak.IsValid() ? Weak->GetNightsSurvived() : 0;
							return FText::FromString(FOREST_PRINTF(TEXT("Nights survived: %d"), TEXT("Nuits survécues : %d"), Nights));
						})
						.ColorAndOpacity(FLinearColor(0.78f, 0.8f, 1.0f))
					]
				]
			];
		}
	};
}

bool UForestBonds::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestBonds::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestBonds, STATGROUP_Tickables);
}

UForestBonds* UForestBonds::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestBonds>() : nullptr;
}

void UForestBonds::Deinitialize()
{
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

void UForestBonds::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

bool UForestBonds::ShowsCounter() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return bStarted && Controller && !Controller->IsMoveInputIgnored();
}

void UForestBonds::EnsureOverlay()
{
	if (Overlay.IsValid())
	{
		return;
	}
	UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Client)
	{
		return;
	}
	Overlay = SNew(SForestNightsLabel).Bonds(this);
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 4);
	Viewport = Client;
}

// --- Save -----------------------------------------------------------------------------

FString UForestBonds::SaveSlot() const
{
	return FString::Printf(TEXT("ForestBonds_%s"), *UGameplayStatics::GetCurrentLevelName(this, true));
}

void UForestBonds::SaveBonds() const
{
	UForestBondsSave* Save = Cast<UForestBondsSave>(UGameplayStatics::CreateSaveGameObject(UForestBondsSave::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->NightsSurvived = NightsSurvived;
	Save->Arrivals = Arrivals;
	UGameplayStatics::SaveGameToSlot(Save, SaveSlot(), 0);
}

void UForestBonds::Start()
{
	bStarted = true;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0))
	{
		if (const UForestBondsSave* Save = Cast<UForestBondsSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0)))
		{
			NightsSurvived = Save->NightsSurvived;
			// The animals that came at dawn live in the forest again.
			for (const FName& Species : Save->Arrivals)
			{
				if (SpawnArrival(Species, false))
				{
					Arrivals.Add(Species);
				}
			}
		}
	}
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	bWasNight = Sky && Sky->IsNight();
	UE_LOG(LogForestGame, Log, TEXT("Bonds: %d nights survived, %d animals came to the forest."), NightsSurvived, Arrivals.Num());
}

// --- Every frame ------------------------------------------------------------------------

void UForestBonds::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const AForestSky* Sky = World ? AForestSky::Get(World) : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Sky || !Player || !AForestGameManager::Get(World))
	{
		return;
	}
	EnsureOverlay();

	const UForestSurvival* Survival = UForestSurvival::Get(World);
	if (bWasNight && Survival && Survival->IsFainted())
	{
		bFaintedTonight = true;
	}
	// Nothing happens during the title, the creative pages and the wake-up intro
	// (sleeping at home blocks the input too: its dawn is counted when the player gets up).
	if (Controller->IsMoveInputIgnored())
	{
		return;
	}
	if (!bStarted)
	{
		Start();
	}

	const bool bNight = Sky->IsNight();
	if (bNight && !bWasNight)
	{
		bFaintedTonight = false;
	}
	else if (!bNight && bWasNight)
	{
		Dawn(!bFaintedTonight);
	}
	bWasNight = bNight;

	UpdateHelpers(DeltaSeconds, Player, bNight);
	if (bNight)
	{
		UpdateFoxWarning(DeltaSeconds, Player);
		UpdateGuardians(DeltaSeconds, Player);
	}
	UpdateResidents(DeltaSeconds);
}

// --- 1. Tame animals help ------------------------------------------------------------------

void UForestBonds::UpdateHelpers(float DeltaSeconds, APawn* Player, bool bNight)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (bNight || !Manager)
	{
		return;
	}
	for (AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (!IsValid(Animal) || !Animal->IsTame() || FVector::Dist2D(Animal->GetActorLocation(), Player->GetActorLocation()) > HelperNear)
		{
			continue;
		}
		const FString Species = BondsSpeciesOf(Animal);
		const FBondsHelper* Helper = nullptr;
		for (const FBondsHelper& Each : BondsHelpers)
		{
			if (Species == Each.Species)
			{
				Helper = &Each;
			}
		}
		if (!Helper)
		{
			continue;
		}
		float& Time = HelpTime.FindOrAdd(Animal, FMath::FRandRange(0.0f, 20.0f));
		Time += DeltaSeconds;
		if (Time < Helper->Seconds)
		{
			continue;
		}
		Time = 0.0f;
		Manager->AddItem(Helper->Item, Helper->Amount);
		Animal->Pet();
		Say(ForestLanguage::Format(ForestLanguage::Pick(Helper->English, Helper->French), *BondsNameOf(Animal), Helper->Amount,
			*ForestCrafting::ItemName(Helper->Item, Helper->Amount)), 4.0f);
	}
}

void UForestBonds::UpdateFoxWarning(float DeltaSeconds, APawn* Player)
{
	WarnCooldown -= DeltaSeconds;
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (WarnCooldown > 0.0f || !Manager)
	{
		return;
	}
	const AForestAnimal* Fox = nullptr;
	for (const AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (IsValid(Animal) && Animal->IsTame() && BondsSpeciesOf(Animal) == TEXT("Fox")
			&& FVector::Dist2D(Animal->GetActorLocation(), Player->GetActorLocation()) < HelperNear * 1.5f)
		{
			Fox = Animal;
			break;
		}
	}
	if (!Fox)
	{
		return;
	}
	const AForestNightBeast* Nearest = nullptr;
	float NearestDistance = FoxHearing;
	for (TActorIterator<AForestNightBeast> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation());
		if (!It->IsFleeing() && Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	if (!Nearest)
	{
		return;
	}
	WarnCooldown = 18.0f;
	Say(FOREST_PRINTF(TEXT("%s growls: %s is coming, %s!"), TEXT("%s grogne : %s arrive, %s !"),
		*BondsNameOf(Fox), *Nearest->GetBeastName(), *BondsDirection(Player, Nearest->GetActorLocation())), 4.0f);
}

void UForestBonds::UpdateGuardians(float DeltaSeconds, APawn* Player)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}
	for (AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (!IsValid(Animal) || !Animal->IsTame())
		{
			continue;
		}
		const FString Species = BondsSpeciesOf(Animal);
		if (Species != TEXT("Boar") && Species != TEXT("Fox"))
		{
			continue;
		}
		float& Cooldown = GuardCooldown.FindOrAdd(Animal, 0.0f);
		Cooldown -= DeltaSeconds;
		if (Cooldown > 0.0f || FVector::Dist2D(Animal->GetActorLocation(), Player->GetActorLocation()) > HelperNear)
		{
			continue;
		}
		for (TActorIterator<AForestNightBeast> It(GetWorld()); It; ++It)
		{
			if (!It->IsFleeing() && FVector::Dist2D(It->GetActorLocation(), Animal->GetActorLocation()) < GuardReach)
			{
				It->Flee();
				Animal->Pet();
				Cooldown = 45.0f;
				Say(FOREST_PRINTF(TEXT("%s chased off %s! You are not alone in the dark."), TEXT("%s a fait fuir %s ! Tu n'es pas seul dans le noir."),
					*BondsNameOf(Animal), *It->GetBeastName()), 4.0f);
				break;
			}
		}
	}
}

// --- 2. Nights survived ----------------------------------------------------------------------

void UForestBonds::Dawn(bool bSurvived)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}
	UForestSpirit* Spirit = UForestSpirit::Get(GetWorld());
	if (!bSurvived)
	{
		if (Spirit)
		{
			Spirit->OnDawn(false);
		}
		Say(ForestLanguage::Pick(TEXT("The night was too hard this time. No reward... but the forest is waiting for you."),
			TEXT("La nuit a été trop dure cette fois. Pas de récompense... mais la forêt t'attend.")), 6.0f);
		return;
	}
	++NightsSurvived;

	// The dawn's reward: berries, and something rarer each night.
	const FBondsGift& Rare = BondsDawnRewards[(NightsSurvived - 1) % UE_ARRAY_COUNT(BondsDawnRewards)];
	Manager->AddItem(TEXT("Berry"), 2);
	Manager->AddItem(Rare.Item, Rare.Amount);
	FString Text = FOREST_PRINTF(TEXT("Night %d survived! Dawn leaves you %d %s and 2 berries."), TEXT("Nuit %d survécue ! L'aube te laisse %d %s et 2 baies."),
		NightsSurvived, Rare.Amount, *ForestCrafting::ItemName(Rare.Item, Rare.Amount));

	// The residents leave a gift at the door.
	int32 Gifts_ = 0;
	for (AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (!IsValid(Animal) || !Animal->HasMovedIn())
		{
			continue;
		}
		const FString Species = BondsSpeciesOf(Animal);
		for (const FBondsGift& Gift : BondsGifts)
		{
			if (Species == Gift.Species)
			{
				Manager->AddItem(Gift.Item, Gift.Amount);
				++Gifts_;
			}
		}
	}
	if (Gifts_ > 0)
	{
		Text += TEXT("\n") + FOREST_PRINTF(TEXT("Your residents left %d gifts at the door."), TEXT("Tes habitants ont laissé %d cadeaux devant la porte."), Gifts_);
	}

	// The forest spirit: fireflies for the night, flowers where the player cared for the forest.
	if (Spirit)
	{
		const FString SpiritText = Spirit->OnDawn(true);
		if (!SpiritText.IsEmpty())
		{
			Text += TEXT("\n") + SpiritText;
		}
	}

	// Every second night (every night in a thriving forest, never in a suffering one), a new animal comes to live in the forest.
	const bool bArrival = Spirit ? (!Spirit->IsSuffering() && (Spirit->IsThriving() || NightsSurvived % 2 == 0)) : NightsSurvived % 2 == 0;
	if (bArrival && Arrivals.Num() < BondsMaxArrivalsFor(Spirit))
	{
		const FName Species = BondsArrivalOrder[Arrivals.Num() % UE_ARRAY_COUNT(BondsArrivalOrder)];
		if (const AForestAnimal* Newcomer = SpawnArrival(Species, true))
		{
			Arrivals.Add(Species);
			Text += TEXT("\n") + FOREST_PRINTF(TEXT("%s came to live in the forest, drawn by your camp."), TEXT("%s est venu vivre dans la forêt, attiré par ton camp."), *BondsNameOf(Newcomer));
		}
	}
	Say(Text, 8.0f);
	SaveBonds();
	UE_LOG(LogForestGame, Log, TEXT("Bonds: dawn, night %d survived (%d gifts, %d arrivals)."), NightsSurvived, Gifts_, Arrivals.Num());
}

AForestAnimal* UForestBonds::SpawnArrival(FName Species, bool bAnnounce)
{
	UWorld* World = GetWorld();
	AForestGameManager* Manager = AForestGameManager::Get(World);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	const FString Path = FString::Printf(TEXT("/Game/WorldBuilder/Gameplay/BP_%s.BP_%s_C"), *Species.ToString(), *Species.ToString());
	UClass* Class = LoadClass<AForestAnimal>(nullptr, *Path);
	if (!Manager || !Player || !Class)
	{
		return nullptr;
	}
	// The playable circle, the same as the animals the World Builder placed.
	const AForestAnimal* Model = nullptr;
	for (const AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (IsValid(Animal))
		{
			Model = Animal;
			break;
		}
	}
	for (int32 Try = 0; Try < 16; ++Try)
	{
		// At the edge of what the player sees, so it walks in from the trees.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		FVector Spot = Player->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(1800.0f, 2800.0f);
		if (Model && FVector::Dist2D(Spot, Model->WorldCenter) > Model->WorldRadius * 0.9f)
		{
			continue;
		}
		FHitResult Hit;
		const FVector Start(Spot.X, Spot.Y, Spot.Z + 6000.0f);
		const FVector End(Spot.X, Spot.Y, Spot.Z - 6000.0f);
		if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic)) || Hit.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		AForestAnimal* Animal = World->SpawnActorDeferred<AForestAnimal>(Class, FTransform(FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Hit.ImpactPoint + FVector(0.0f, 0.0f, 60.0f)),
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Animal)
		{
			return nullptr;
		}
		if (Model)
		{
			Animal->WorldCenter = Model->WorldCenter;
			Animal->WorldRadius = Model->WorldRadius;
		}
		UGameplayStatics::FinishSpawningActor(Animal, Animal->GetActorTransform());
		Manager->AddAnimal(Animal);
		if (bAnnounce)
		{
			UE_LOG(LogForestGame, Log, TEXT("Bonds: a new %s came to the forest."), *Species.ToString());
		}
		return Animal;
	}
	return nullptr;
}

// --- 3. Animals move into your homes -----------------------------------------------------

TArray<FVector> UForestBonds::FindShelters() const
{
	TArray<FVector> Shelters;
	if (const UForestBlocks* Blocks = UForestBlocks::Get(GetWorld()))
	{
		for (const FVector& Spot : Blocks->GetRoofSpots())
		{
			if (!Shelters.ContainsByPredicate([&Spot](const FVector& Other) { return FVector::Dist2D(Spot, Other) < SameHouse; }))
			{
				Shelters.Add(Spot);
			}
		}
	}
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		if (It->GetBuildingType() == TEXT("Shelter"))
		{
			Shelters.Add(It->GetActorLocation());
		}
	}
	return Shelters;
}

void UForestBonds::UpdateResidents(float DeltaSeconds)
{
	ResidentTimer += DeltaSeconds;
	if (ResidentTimer < 5.0f)
	{
		return;
	}
	const float Elapsed = ResidentTimer;
	ResidentTimer = 0.0f;

	// A home needs to stand for a little while before someone dares to move in.
	const TArray<FVector> Shelters = FindShelters();
	TArray<TPair<FVector, float>> Aged;
	for (const FVector& Shelter : Shelters)
	{
		float Age = 0.0f;
		for (const TPair<FVector, float>& Old : ShelterAge)
		{
			if (FVector::Dist2D(Old.Key, Shelter) < SameHouse)
			{
				Age = Old.Value;
				break;
			}
		}
		Aged.Emplace(Shelter, Age + Elapsed);
	}
	ShelterAge = MoveTemp(Aged);

	for (const TPair<FVector, float>& Shelter : ShelterAge)
	{
		if (Shelter.Value >= ShelterSettle)
		{
			MoveSomeoneIn();
			return;
		}
	}
}

bool UForestBonds::MoveSomeoneIn()
{
	UWorld* World = GetWorld();
	AForestGameManager* Manager = AForestGameManager::Get(World);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Manager || !Player)
	{
		return false;
	}
	const UForestBlocks* Blocks = UForestBlocks::Get(World);
	for (const FVector& Shelter : FindShelters())
	{
		// Already lived in?
		bool bTaken = false;
		for (const AForestAnimal* Animal : Manager->GetAnimals())
		{
			if (IsValid(Animal) && Animal->HasMovedIn() && FVector::Dist2D(Animal->GetHome(), Shelter) < SameHouse)
			{
				bTaken = true;
				break;
			}
		}
		if (bTaken)
		{
			continue;
		}
		// A lit home attracts the fox; any other home the bunny; otherwise whoever lives nearest.
		bool bLit = false;
		if (Blocks)
		{
			for (const FVector& Glow : Blocks->GetGlowSpots())
			{
				bLit |= FVector::Dist2D(Glow, Shelter) < SameHouse;
			}
		}
		const TCHAR* Wanted = bLit ? TEXT("Fox") : TEXT("Bunny");
		AForestAnimal* Best = nullptr;
		float BestScore = BIG_NUMBER;
		for (AForestAnimal* Animal : Manager->GetAnimals())
		{
			// Animals that follow the player or stay at the camp already have their place.
			if (!IsValid(Animal) || Animal->HasMovedIn() || (Animal->IsTame() && !Animal->IsStaying()))
			{
				continue;
			}
			const float Score = FVector::Dist2D(Animal->GetActorLocation(), Shelter) * (BondsSpeciesOf(Animal) == Wanted ? 0.25f : 1.0f);
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Animal;
			}
		}
		if (!Best)
		{
			return false;
		}
		Best->MoveIn(Shelter);
		ForestWallet::Add(this, 3, false);
		if (UForestSpirit* Spirit = UForestSpirit::Get(World))
		{
			Spirit->ChangeHealth(4.0f);
		}
		if (UForestLife* Life = UForestLife::Get(World))
		{
			Life->Log(FOREST_PRINTF(TEXT("%s moved into one of your shelters."), TEXT("%s a emménagé dans un de tes abris."), *BondsNameOf(Best)));
		}
		Say(bLit
			? FOREST_PRINTF(TEXT("%s moved into your lit home! It will leave you gifts at dawn."), TEXT("%s a emménagé dans ta maison éclairée ! Il te laissera des cadeaux à l'aube."), *BondsNameOf(Best))
			: FOREST_PRINTF(TEXT("%s moved in under your roof! It will leave you gifts at dawn."), TEXT("%s a emménagé sous ton toit ! Il te laissera des cadeaux à l'aube."), *BondsNameOf(Best)), 5.0f);
		UE_LOG(LogForestGame, Log, TEXT("Bonds: %s moved into a shelter."), *Best->GetName());
		return true;
	}
	return false;
}
