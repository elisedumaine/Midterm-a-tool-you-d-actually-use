#include "ForestSpirit.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestBonds.h"
#include "ForestCrop.h"
#include "ForestEvents.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestLife.h"
#include "ForestNight.h"
#include "ForestShop.h"
#include "ForestSky.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor SpiritMoonBlue(0.55f, 0.65f, 1.0f);
	const FLinearColor SpiritMoonRed(1.0f, 0.12f, 0.06f);
	constexpr int32 SpiritMaxFlowers = 140;
	const TCHAR* SpiritFlowerMeshes[] = {
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_purpleA.flower_purpleA"),
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_purpleB.flower_purpleB"),
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_redA.flower_redA"),
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_redB.flower_redB"),
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_yellowA.flower_yellowA"),
		TEXT("/Game/WorldBuilder/Imported/Kenney/flower_yellowB.flower_yellowB"),
	};

	/** 0 = new moon, 0.5 = full moon (a mean lunar month from a known new moon). */
	float SpiritMoonPhase(const FDateTime& Date)
	{
		const FDateTime KnownNewMoon(2000, 1, 6, 18, 14);
		const double Days = (Date - KnownNewMoon).GetTotalDays();
		const double Phase = FMath::Fmod(Days / 29.530588853, 1.0);
		return float(Phase < 0.0 ? Phase + 1.0 : Phase);
	}

	FAutoConsoleCommandWithWorldAndArgs GNightThemeCommand(
		TEXT("Forest.NightTheme"),
		TEXT("The coming night: Forest.NightTheme Calm | Fog | RedMoon | Silent | Storm"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UForestSpirit* Spirit = UForestSpirit::Get(World);
			if (!Spirit || Args.IsEmpty())
			{
				return;
			}
			const FString Name = Args[0];
			EForestNightTheme Theme = EForestNightTheme::Calm;
			if (Name.StartsWith(TEXT("F")) || Name.StartsWith(TEXT("B"))) Theme = EForestNightTheme::Fog;
			else if (Name.StartsWith(TEXT("R")) || Name.StartsWith(TEXT("L"))) Theme = EForestNightTheme::RedMoon;
			else if (Name.StartsWith(TEXT("Si"))) Theme = EForestNightTheme::Silent;
			else if (Name.StartsWith(TEXT("St")) || Name.StartsWith(TEXT("O"))) Theme = EForestNightTheme::Storm;
			Spirit->ForceTheme(Theme);
		}));

	FAutoConsoleCommandWithWorldAndArgs GHealthCommand(
		TEXT("Forest.Health"),
		TEXT("Sets the life of the forest: Forest.Health 90"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UForestSpirit* Spirit = UForestSpirit::Get(World))
			{
				Spirit->ChangeHealth((Args.IsEmpty() ? 90.0f : FCString::Atof(*Args[0])) - Spirit->GetHealth());
			}
		}));

	/** "Life of the forest: 62 %" and the fireflies, under the nights counter. */
	class SForestSpiritLabel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestSpiritLabel) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestSpirit>, Spirit)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			TWeakObjectPtr<UForestSpirit> Weak = Args._Spirit;
			ChildSlot
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 164.0f, 24.0f, 0.0f))
				[
					SNew(SBorder)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsHud() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.02f, 0.55f))
					.Padding(FMargin(18.0f, 6.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.Text_Lambda([Weak]()
							{
								const float Health = Weak.IsValid() ? Weak->GetHealth() : 0.0f;
								return FText::FromString(FOREST_PRINTF(TEXT("Life of the forest: %d %%"), TEXT("Vie de la forêt : %d %%"), FMath::RoundToInt(Health)));
							})
							.ColorAndOpacity_Lambda([Weak]()
							{
								const float Health = Weak.IsValid() ? Weak->GetHealth() / 100.0f : 0.5f;
								return FSlateColor(FMath::Lerp(FLinearColor(1.0f, 0.45f, 0.3f), FLinearColor(0.55f, 1.0f, 0.5f), Health));
							})
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.Text_Lambda([]() { return FText::FromString(FOREST_PRINTF(TEXT("Fireflies: %d   [U] shop"), TEXT("Lucioles : %d   [U] boutique"), ForestWallet::Fireflies())); })
							.ColorAndOpacity(FLinearColor(0.85f, 1.0f, 0.55f))
						]
					]
				]
			];
		}
	};
}

bool UForestSpirit::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestSpirit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestSpirit, STATGROUP_Tickables);
}

UForestSpirit* UForestSpirit::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestSpirit>() : nullptr;
}

void UForestSpirit::Deinitialize()
{
	if (bStarted)
	{
		SaveSpirit();
	}
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

void UForestSpirit::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

void UForestSpirit::Log(const FString& Line) const
{
	if (UForestLife* Life = UForestLife::Get(GetWorld()))
	{
		Life->Log(Line);
	}
}

bool UForestSpirit::ShowsHud() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return bStarted && Controller && !Controller->IsMoveInputIgnored();
}

void UForestSpirit::EnsureOverlay()
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
	Overlay = SNew(SForestSpiritLabel).Spirit(this);
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 4);
	Viewport = Client;
}

// --- Save --------------------------------------------------------------------------------------

FString UForestSpirit::SaveSlot() const
{
	return FString::Printf(TEXT("ForestSpirit_%s"), *UGameplayStatics::GetCurrentLevelName(this, true));
}

void UForestSpirit::SaveSpirit() const
{
	if (UForestSpiritSave* Save = Cast<UForestSpiritSave>(UGameplayStatics::CreateSaveGameObject(UForestSpiritSave::StaticClass())))
	{
		Save->Health = Health;
		Save->Flowers = Flowers;
		UGameplayStatics::SaveGameToSlot(Save, SaveSlot(), 0);
	}
}

void UForestSpirit::Start()
{
	bStarted = true;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0))
	{
		if (const UForestSpiritSave* Save = Cast<UForestSpiritSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0)))
		{
			Health = FMath::Clamp(Save->Health, 0.0f, 100.0f);
			for (const FVector& Spot : Save->Flowers)
			{
				SpawnFlower(Spot);
			}
		}
	}
	// The animals already known (tame ones loaded from the save are not new friends).
	if (const AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		for (AForestAnimal* Animal : Manager->GetAnimals())
		{
			if (IsValid(Animal))
			{
				KnownTrust.Add(Animal, Animal->GetTrust());
				if (Animal->IsTame())
				{
					KnownTame.Add(Animal);
				}
			}
		}
	}
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	bNight = Sky && Sky->IsNight();
	bAnnounced = bNight || (Sky && Sky->GetHour() >= 19.0f);
	UE_LOG(LogForestGame, Log, TEXT("Spirit: life of the forest %.0f, %d flowers."), Health, Flowers.Num());
}

// --- The life of the forest --------------------------------------------------------------

void UForestSpirit::ChangeHealth(float Delta)
{
	const float Before = Health;
	Health = FMath::Clamp(Health + Delta, 0.0f, 100.0f);
	// Crossing a threshold is told once.
	if (Before >= 30.0f && Health < 30.0f)
	{
		Say(ForestLanguage::Pick(TEXT("The forest is suffering... Too much has been taken. Plant seeds, feed the animals."),
			TEXT("La forêt souffre... On lui a trop pris. Plante des graines, nourris les animaux.")), 6.0f);
		Log(ForestLanguage::Pick(TEXT("The forest is suffering."), TEXT("La forêt souffre.")));
	}
	else if (Before < 75.0f && Health >= 75.0f)
	{
		Say(ForestLanguage::Pick(TEXT("The forest is thriving! It thanks you: more fireflies, more friends."),
			TEXT("La forêt rayonne ! Elle te remercie : plus de lucioles, plus d'amis.")), 6.0f);
		Log(ForestLanguage::Pick(TEXT("The forest is thriving."), TEXT("La forêt rayonne.")));
	}
}

void UForestSpirit::WatchAnimals()
{
	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}
	for (AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (!IsValid(Animal))
		{
			continue;
		}
		// A meal raised its trust: the forest is grateful.
		float& Known = KnownTrust.FindOrAdd(Animal, Animal->GetTrust());
		if (Animal->GetTrust() > Known + 0.01f)
		{
			ChangeHealth((Animal->GetTrust() - Known) * 8.0f);
		}
		Known = Animal->GetTrust();
		if (Animal->IsTame() && !KnownTame.Contains(Animal))
		{
			// A new friend.
			KnownTame.Add(Animal);
			ChangeHealth(5.0f);
			ForestWallet::Add(this, 5);
		}
	}
}

// --- Themed nights ----------------------------------------------------------------------------

FString UForestSpirit::ThemeName(EForestNightTheme Theme)
{
	switch (Theme)
	{
	case EForestNightTheme::Fog: return ForestLanguage::Pick(TEXT("Night of fog"), TEXT("Nuit de brume"));
	case EForestNightTheme::RedMoon: return ForestLanguage::Pick(TEXT("Red moon"), TEXT("Lune rouge"));
	case EForestNightTheme::Silent: return ForestLanguage::Pick(TEXT("Silent night"), TEXT("Nuit silencieuse"));
	case EForestNightTheme::Storm: return ForestLanguage::Pick(TEXT("Night of storm"), TEXT("Nuit d'orage"));
	default: return ForestLanguage::Pick(TEXT("Quiet night"), TEXT("Nuit calme"));
	}
}

void UForestSpirit::ForceTheme(EForestNightTheme NewTheme)
{
	Theme = NewTheme;
	bForced = true;
	Say(FOREST_PRINTF(TEXT("The next night will be: %s."), TEXT("La prochaine nuit sera : %s."), *ThemeName(Theme)), 4.0f);
}

EForestNightTheme UForestSpirit::PickTheme(FString& OutReason) const
{
	// The real calendar first.
	const FDateTime Now = FDateTime::Now();
	if (Now.GetMonth() == 10 && Now.GetDay() == 31)
	{
		OutReason = ForestLanguage::Pick(TEXT("It is Halloween tonight, in your world too."), TEXT("C'est Halloween ce soir, dans ton monde aussi."));
		return EForestNightTheme::RedMoon;
	}
	if ((Now.GetMonth() == 12 && (Now.GetDay() == 24 || Now.GetDay() == 25 || Now.GetDay() == 31)) || (Now.GetMonth() == 1 && Now.GetDay() == 1))
	{
		OutReason = ForestLanguage::Pick(TEXT("A day of celebration in your world: the forest is at peace."), TEXT("Jour de fête dans ton monde : la forêt est en paix."));
		return EForestNightTheme::Silent;
	}
	const float Moon = SpiritMoonPhase(Now);
	if (FMath::Abs(Moon - 0.5f) < 0.035f)
	{
		OutReason = ForestLanguage::Pick(TEXT("The moon is full tonight, in your sky too."), TEXT("La lune est pleine ce soir, dans ton ciel aussi."));
		return EForestNightTheme::RedMoon;
	}

	// Then the forest's own mood: calm at first, bolder as the nights go on.
	const UForestBonds* Bonds = UForestBonds::Get(GetWorld());
	const int32 Nights = Bonds ? Bonds->GetNightsSurvived() : 0;
	if (Nights == 0)
	{
		return EForestNightTheme::Calm;
	}
	if (FMath::FRand() < 0.15f)
	{
		return EForestNightTheme::Silent;
	}
	const float Danger = FMath::Clamp(0.1f + 0.05f * Nights + (Health < 30.0f ? 0.3f : 0.0f), 0.0f, 0.7f);
	const float Roll = FMath::FRand();
	if (Roll < Danger * 0.5f)
	{
		return EForestNightTheme::RedMoon;
	}
	if (Roll < 0.3f + Danger * 0.5f)
	{
		return EForestNightTheme::Fog;
	}
	if (Roll < 0.5f + Danger * 0.5f)
	{
		return EForestNightTheme::Storm;
	}
	return EForestNightTheme::Calm;
}

void UForestSpirit::Dusk()
{
	bAnnounced = true;
	FString Reason;
	if (!bForced)
	{
		Theme = PickTheme(Reason);
	}
	bForced = false;
	FString Text;
	switch (Theme)
	{
	case EForestNightTheme::Fog:
		Text = ForestLanguage::Pick(TEXT("The spirit of the forest whispers: tonight, a thick fog. The red eyes will come out of nowhere."),
			TEXT("L'esprit de la forêt murmure : cette nuit, un brouillard épais. Les yeux rouges surgiront de nulle part."));
		break;
	case EForestNightTheme::RedMoon:
		Text = ForestLanguage::Pick(TEXT("The spirit of the forest trembles: a RED MOON is rising. Twice as many creatures... and twice the reward."),
			TEXT("L'esprit de la forêt tremble : une LUNE ROUGE se lève. Deux fois plus de créatures... et deux fois plus de récompense."));
		break;
	case EForestNightTheme::Silent:
		Text = ForestLanguage::Pick(TEXT("The spirit of the forest smiles: a silent night. No creature tonight. Look up at the sky..."),
			TEXT("L'esprit de la forêt sourit : une nuit silencieuse. Aucune créature ce soir. Regarde le ciel..."));
		break;
	case EForestNightTheme::Storm:
		Text = ForestLanguage::Pick(TEXT("The spirit of the forest warns: a storm is coming tonight. Stay away from the lightning."),
			TEXT("L'esprit de la forêt prévient : un orage arrive cette nuit. Méfie-toi des éclairs."));
		break;
	default:
		Text = ForestLanguage::Pick(TEXT("The sun goes down. A quiet night is coming... probably."), TEXT("Le soleil se couche. Une nuit calme s'annonce... sans doute."));
		break;
	}
	if (!Reason.IsEmpty())
	{
		Text = Reason + TEXT("\n") + Text;
	}
	Say(Text, 8.0f);
	Log(FOREST_PRINTF(TEXT("Dusk: %s."), TEXT("Crépuscule : %s."), *ThemeName(Theme)));
}

void UForestSpirit::BeginNight()
{
	bNight = true;
	UWorld* World = GetWorld();
	AForestSky* Sky = AForestSky::Get(World);
	UForestNight* Night = UForestNight::Get(World);
	if (Night)
	{
		BaseBeasts = Night->MaxBeasts;
		int32 Beasts = BaseBeasts;
		if (Theme == EForestNightTheme::RedMoon) Beasts *= 2;
		if (Theme == EForestNightTheme::Silent) Beasts = 0;
		if (Theme == EForestNightTheme::Fog) Beasts += 1;
		// A suffering forest sends more of them.
		if (IsSuffering() && Theme != EForestNightTheme::Silent) Beasts += 3;
		Night->MaxBeasts = Beasts;
	}
	if (Sky)
	{
		if (Theme == EForestNightTheme::Fog)
		{
			Sky->SetMoment(Sky->GetHour(), TEXT("Mist"), false);
		}
		else if (Theme == EForestNightTheme::Storm)
		{
			Sky->SetMoment(Sky->GetHour(), TEXT("Storm"), false);
		}
		else if (Theme == EForestNightTheme::Silent || Theme == EForestNightTheme::RedMoon)
		{
			Sky->SetMoment(Sky->GetHour(), TEXT("Clear"), false);
		}
		if (Sky->MoonLight)
		{
			Sky->MoonLight->SetLightColor(Theme == EForestNightTheme::RedMoon ? SpiritMoonRed : SpiritMoonBlue);
		}
	}
	// On a silent night a star falls into the forest a little after nightfall.
	StarIn = Theme == EForestNightTheme::Silent ? 25.0f : -1.0f;
}

void UForestSpirit::EndNight()
{
	bNight = false;
	bAnnounced = false;
	UWorld* World = GetWorld();
	if (UForestNight* Night = UForestNight::Get(World))
	{
		Night->MaxBeasts = BaseBeasts;
	}
	if (AForestSky* Sky = AForestSky::Get(World))
	{
		if (Sky->MoonLight)
		{
			Sky->MoonLight->SetLightColor(SpiritMoonBlue);
		}
	}
}

FString UForestSpirit::OnDawn(bool bSurvived)
{
	FString Text;
	if (!bSurvived)
	{
		ChangeHealth(-3.0f);
		return Text;
	}
	// Fireflies: more after a hard night, more from a thriving forest.
	int32 Fireflies = 3;
	switch (Theme)
	{
	case EForestNightTheme::Fog: Fireflies += 2; break;
	case EForestNightTheme::Storm: Fireflies += 3; break;
	case EForestNightTheme::RedMoon: Fireflies += 8; break;
	default: break;
	}
	if (IsThriving())
	{
		Fireflies += 3;
	}
	ForestWallet::Add(this, Fireflies, false);
	Text += FOREST_PRINTF(TEXT("%s survived: +%d fireflies."), TEXT("%s survécue : +%d lucioles."), *ThemeName(Theme), Fireflies);

	// The forest remembers: flowers where the player cared for it.
	int32 Bloomed = 0;
	const int32 PerSpot = 1 + FMath::FloorToInt(Health / 35.0f);
	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (Manager)
	{
		for (const AForestAnimal* Animal : Manager->GetAnimals())
		{
			if (IsValid(Animal) && (Animal->HasMovedIn() || Animal->IsStaying()))
			{
				Bloom(Animal->GetHome(), PerSpot);
				Bloomed += PerSpot;
				ChangeHealth(1.0f);
			}
		}
	}
	for (TActorIterator<AForestCrop> It(GetWorld()); It; ++It)
	{
		Bloom(It->GetActorLocation(), 1);
		++Bloomed;
	}
	if (Bloomed > 0)
	{
		Text += TEXT("\n") + FOREST_PRINTF(TEXT("Flowers grew where you cared for the forest (%d)."), TEXT("Des fleurs ont poussé là où tu as pris soin de la forêt (%d)."), Bloomed);
	}
	ChangeHealth(1.0f);
	SaveSpirit();
	return Text;
}

// --- The forest remembers ---------------------------------------------------------------------

void UForestSpirit::SpawnFlower(const FVector& Spot)
{
	UWorld* World = GetWorld();
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, SpiritFlowerMeshes[FMath::RandHelper(UE_ARRAY_COUNT(SpiritFlowerMeshes))]);
	if (!World || !Mesh)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Flower = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Spot, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f), Params);
	if (!Flower)
	{
		return;
	}
	Flower->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* Component = Flower->GetStaticMeshComponent();
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flower->SetActorScale3D(FVector(FMath::FRandRange(1.4f, 2.2f)));
	FlowerActors.Add(Flower);
}

void UForestSpirit::Bloom(const FVector& Around, int32 Count)
{
	UWorld* World = GetWorld();
	for (int32 Index = 0; Index < Count && Flowers.Num() < SpiritMaxFlowers; ++Index)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Probe = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(90.0f, 320.0f);
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, Probe + FVector(0.0f, 0.0f, 600.0f), Probe - FVector(0.0f, 0.0f, 1200.0f), FCollisionObjectQueryParams(ECC_WorldStatic))
			&& Hit.ImpactNormal.Z > 0.7f)
		{
			Flowers.Add(Hit.ImpactPoint);
			SpawnFlower(Hit.ImpactPoint);
		}
	}
}

// --- Every frame --------------------------------------------------------------------------------

void UForestSpirit::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	AForestSky* Sky = World ? AForestSky::Get(World) : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Sky || !Controller || !Controller->GetPawn() || !AForestGameManager::Get(World))
	{
		return;
	}
	EnsureOverlay();
	// Nothing during the title, the creative pages and the wake-up.
	if (Controller->IsMoveInputIgnored() && !bStarted)
	{
		return;
	}
	if (!bStarted)
	{
		Start();
	}

	WatchAnimals();
	const bool bIsNight = Sky->IsNight();
	if (!bAnnounced && !bIsNight && Sky->GetHour() >= 19.0f)
	{
		Dusk();
	}
	if (bIsNight && !bNight)
	{
		if (!bAnnounced)
		{
			Dusk();
		}
		BeginNight();
	}
	else if (!bIsNight && bNight)
	{
		EndNight();
	}
	if (StarIn > 0.0f)
	{
		StarIn -= DeltaSeconds;
		if (StarIn <= 0.0f)
		{
			if (UForestEvents* Events = UForestEvents::Get(World))
			{
				Events->Trigger(TEXT("Star"));
			}
		}
	}
	SaveTimer += DeltaSeconds;
	if (SaveTimer > 30.0f)
	{
		SaveTimer = 0.0f;
		SaveSpirit();
	}
}
