#include "ForestSeasons.h"

#include "GameFramework/PlayerController.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestSky.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Real seconds for the colours to turn from one season to the next. */
	constexpr float TurnSeconds = 60.0f;

	struct FSeasonLook
	{
		float Autumn;	// green leaves and grass turned gold and red
		float Bare;		// leaves dull and brown
		float Hunger;	// animals get hungry this much faster
	};

	FSeasonLook LookOf(EForestSeason Season)
	{
		switch (Season)
		{
		case EForestSeason::Autumn: return {1.0f, 0.0f, 1.15f};
		case EForestSeason::Winter: return {0.0f, 1.0f, 1.5f};
		default: return {0.0f, 0.0f, 1.0f};
		}
	}

	/** The season label under the clock, top right. */
	class SForestSeasonLabel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestSeasonLabel) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestSeasons>, Seasons)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			TWeakObjectPtr<UForestSeasons> Weak = Args._Seasons;
			ChildSlot
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 84.0f, 24.0f, 0.0f))
				[
					SNew(SBorder)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsLabel() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.02f, 0.55f))
					.Padding(FMargin(18.0f, 6.0f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->SeasonName() : FString()); })
						.ColorAndOpacity_Lambda([Weak]()
						{
							const EForestSeason Season = Weak.IsValid() ? Weak->GetSeason() : EForestSeason::Summer;
							switch (Season)
							{
							case EForestSeason::Spring: return FSlateColor(FLinearColor(0.7f, 0.95f, 0.6f));
							case EForestSeason::Autumn: return FSlateColor(FLinearColor(1.0f, 0.62f, 0.25f));
							case EForestSeason::Winter: return FSlateColor(FLinearColor(0.78f, 0.88f, 1.0f));
							default: return FSlateColor(FLinearColor(1.0f, 0.9f, 0.45f));
							}
						})
					]
				]
			];
		}
	};

	/** Forest.Season Spring|Summer|Autumn|Winter: jump to a season (for demos and tests). */
	FAutoConsoleCommandWithWorldAndArgs GSeasonCommand(
		TEXT("Forest.Season"),
		TEXT("Jump to a season: Forest.Season Spring | Summer | Autumn | Winter"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UForestSeasons* Seasons = UForestSeasons::Get(World);
			if (!Seasons || Args.IsEmpty())
			{
				return;
			}
			const FString Name = Args[0];
			if (Name.StartsWith(TEXT("Sp")) || Name.StartsWith(TEXT("Pr")))
			{
				Seasons->SetSeason(EForestSeason::Spring);
			}
			else if (Name.StartsWith(TEXT("Su")) || Name.StartsWith(TEXT("Et")) || Name.StartsWith(TEXT("Ét")))
			{
				Seasons->SetSeason(EForestSeason::Summer);
			}
			else if (Name.StartsWith(TEXT("Au")) || Name.StartsWith(TEXT("Fa")))
			{
				Seasons->SetSeason(EForestSeason::Autumn);
			}
			else if (Name.StartsWith(TEXT("Wi")) || Name.StartsWith(TEXT("Hi")))
			{
				Seasons->SetSeason(EForestSeason::Winter);
			}
		}));
}

bool UForestSeasons::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UForestSeasons::Deinitialize()
{
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

TStatId UForestSeasons::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestSeasons, STATGROUP_Tickables);
}

UForestSeasons* UForestSeasons::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestSeasons>() : nullptr;
}

FString UForestSeasons::SeasonName() const
{
	switch (Season)
	{
	case EForestSeason::Spring: return ForestLanguage::Pick(TEXT("Spring"), TEXT("Printemps"));
	case EForestSeason::Autumn: return ForestLanguage::Pick(TEXT("Autumn"), TEXT("Automne"));
	case EForestSeason::Winter: return ForestLanguage::Pick(TEXT("Winter"), TEXT("Hiver"));
	default: return ForestLanguage::Pick(TEXT("Summer"), TEXT("Été"));
	}
}

bool UForestSeasons::ShowsLabel() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return bStarted && Controller && !Controller->IsMoveInputIgnored();
}

void UForestSeasons::EnsureOverlay()
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
	Overlay = SNew(SForestSeasonLabel).Seasons(this);
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 4);
	Viewport = Client;
}

void UForestSeasons::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	AForestSky* Sky = AForestSky::Get(World);
	if (!World || !Sky)
	{
		return;
	}
	EnsureOverlay();

	if (!bStarted)
	{
		// The World Builder's season choice: a winter world starts in winter.
		StartSeason = Sky->bWinter ? EForestSeason::Winter : EForestSeason::Summer;
		FirstDay = Sky->GetDay();
		Season = StartSeason;
		bStarted = true;
		Apply(DeltaSeconds, true);
		UE_LOG(LogForestGame, Log, TEXT("Seasons: the year starts in %s."), *SeasonName());
		return;
	}

	const int32 Turned = FMath::Max(0, Sky->GetDay() - FirstDay) / FMath::Max(1, DaysPerSeason);
	const EForestSeason Now = static_cast<EForestSeason>((static_cast<int32>(StartSeason) + Turned) % 4);
	if (Now != Season)
	{
		Season = Now;
		Announce();
	}
	Apply(DeltaSeconds, false);
}

void UForestSeasons::SetSeason(EForestSeason NewSeason, bool bQuiet)
{
	AForestSky* Sky = AForestSky::Get(GetWorld());
	StartSeason = NewSeason;
	FirstDay = Sky ? Sky->GetDay() : 1;
	bStarted = true;
	if (NewSeason != Season)
	{
		Season = NewSeason;
		if (!bQuiet)
		{
			Announce();
		}
	}
	// Quietly (the world page): no message, the colours change at once.
	if (bQuiet)
	{
		Apply(0.0f, true);
	}
}

void UForestSeasons::Announce() const
{
	FString Text;
	switch (Season)
	{
	case EForestSeason::Spring:
		Text = ForestLanguage::Pick(TEXT("Spring is here: the forest turns green again."), TEXT("Le printemps est là : la forêt reverdit."));
		break;
	case EForestSeason::Summer:
		Text = ForestLanguage::Pick(TEXT("Summer: long warm days."), TEXT("L'été : de longues journées chaudes."));
		break;
	case EForestSeason::Autumn:
		Text = ForestLanguage::Pick(TEXT("Autumn: the leaves turn gold and red, the animals fill up before winter."),
			TEXT("L'automne : les feuilles deviennent dorées et rouges, les animaux font des réserves."));
		break;
	case EForestSeason::Winter:
		Text = ForestLanguage::Pick(TEXT("Winter: snow is coming and food is scarce. Hungry animals are dangerous!"),
			TEXT("L'hiver : la neige arrive et la nourriture se fait rare. Les animaux affamés sont dangereux !"));
		break;
	}
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), 6.0f);
	}
	UE_LOG(LogForestGame, Log, TEXT("Seasons: %s."), *SeasonName());
}

void UForestSeasons::Apply(float DeltaSeconds, bool bInstant)
{
	UWorld* World = GetWorld();
	AForestSky* Sky = AForestSky::Get(World);
	if (!Sky)
	{
		return;
	}
	const FSeasonLook Look = LookOf(Season);

	// The colours turn slowly, over about a minute.
	const float Step = bInstant ? 1.0f : DeltaSeconds / TurnSeconds;
	Autumn = FMath::FInterpConstantTo(Autumn, Look.Autumn, 1.0f, Step);
	Bare = FMath::FInterpConstantTo(Bare, Look.Bare, 1.0f, Step);
	if (UMaterialParameterCollection* Collection = Sky->WeatherParameters)
	{
		// Older worlds whose collection has no season values simply keep their colours.
		if (Collection->GetScalarParameterByName(TEXT("Autumn")))
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, TEXT("Autumn"), Autumn);
		}
		if (Collection->GetScalarParameterByName(TEXT("Bare")))
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, TEXT("Bare"), Bare);
		}
	}

	// In winter the sky sends snow instead of rain.
	Sky->bWinter = Season == EForestSeason::Winter;

	// Scarce food: the animals get hungry faster in autumn and winter.
	for (TActorIterator<AForestAnimal> It(World); It; ++It)
	{
		const float* Base = BaseHunger.Find(*It);
		if (!Base)
		{
			Base = &BaseHunger.Add(*It, It->HungerSeconds);
		}
		It->HungerSeconds = *Base / Look.Hunger;
	}
}
