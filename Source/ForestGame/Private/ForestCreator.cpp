#include "ForestCreator.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ForestGameInstance.h"
#include "ForestLanguage.h"
#include "ForestNight.h"
#include "ForestSeasons.h"
#include "ForestSky.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* CreatorSection = TEXT("ForestGame");

	const FLinearColor CreatorGold(0.98f, 0.84f, 0.55f, 1.0f);
	const FLinearColor CreatorCream(0.97f, 0.95f, 0.88f, 1.0f);
	const FLinearColor CreatorNight(0.01f, 0.015f, 0.02f, 1.0f);

	// Round world: the share of the World Builder's strongest curve (iwb_curved_world.py, MAX_CURVATURE).
	const float CreatorMaxCurvature = 1.0f / 30000.0f;
	const float CreatorRoundnesses[] = {0.0f, 0.2f, 0.4f, 0.7f, 1.0f};
	const int32 CreatorNightBeasts[] = {2, 6, 10};
	/** A day of the living sky lasts 12 minutes (the clock stands still on the title and the pages). */
	const float CreatorDayMinutes = 12.0f;

	FLinearColor CreatorAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	TSharedRef<SWidget> CreatorShade(float Alpha)
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CreatorAlpha(CreatorNight, Alpha))
			.Padding(0.0f);
	}

	int32 CreatorWrap(int32 Value, int32 Count)
	{
		return Count > 0 ? ((Value % Count) + Count) % Count : 0;
	}

	FString CreatorSeasonLabel(int32 Season)
	{
		switch (Season)
		{
		case 0: return ForestLanguage::Pick(TEXT("Spring"), TEXT("Printemps"));
		case 2: return ForestLanguage::Pick(TEXT("Autumn"), TEXT("Automne"));
		case 3: return ForestLanguage::Pick(TEXT("Winter"), TEXT("Hiver"));
		default: return ForestLanguage::Pick(TEXT("Summer"), TEXT("Été"));
		}
	}

	FString CreatorRoundnessLabel(int32 Index)
	{
		switch (Index)
		{
		case 0: return ForestLanguage::Pick(TEXT("Flat"), TEXT("Plat"));
		case 1: return ForestLanguage::Pick(TEXT("Gentle curve"), TEXT("Légèrement rond"));
		case 3: return ForestLanguage::Pick(TEXT("Very round"), TEXT("Très rond"));
		case 4: return ForestLanguage::Pick(TEXT("Tiny planet"), TEXT("Petite planète"));
		default: return ForestLanguage::Pick(TEXT("Round"), TEXT("Rond"));
		}
	}

	FString CreatorNightsLabel(int32 Index)
	{
		switch (Index)
		{
		case 0: return ForestLanguage::Pick(TEXT("Gentle"), TEXT("Douces"));
		case 2: return ForestLanguage::Pick(TEXT("Terrifying"), TEXT("Terrifiantes"));
		default: return ForestLanguage::Pick(TEXT("Normal"), TEXT("Normales"));
		}
	}

}

namespace
{
	/** Forest.CreatorPage 0|1|2: turns the creative pages without the mouse (tests); 2 = wake up. */
	FAutoConsoleCommandWithWorldAndArgs GCreatorPageCommand(
		TEXT("Forest.CreatorPage"),
		TEXT("Creative pages: 0 = character, 1 = world, 2 = wake up"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Page = Args.IsEmpty() ? 1 : FCString::Atoi(*Args[0]);
			for (TObjectIterator<UForestCreator> It; It; ++It)
			{
				if (It->GetWorld() == World)
				{
					It->GoToPage(Page);
				}
			}
		}));
}

// --- The choices --------------------------------------------------------------

FForestSetup FForestSetup::Load()
{
	FForestSetup Setup;
	if (GConfig)
	{
		GConfig->GetInt(CreatorSection, TEXT("Season"), Setup.Season, GGameUserSettingsIni);
		GConfig->GetInt(CreatorSection, TEXT("Weather"), Setup.Weather, GGameUserSettingsIni);
		GConfig->GetInt(CreatorSection, TEXT("Roundness"), Setup.Roundness, GGameUserSettingsIni);
		GConfig->GetInt(CreatorSection, TEXT("Nights"), Setup.Nights, GGameUserSettingsIni);
	}
	Setup.Season = CreatorWrap(Setup.Season, 4);
	Setup.Weather = CreatorWrap(Setup.Weather, Weathers().Num());
	Setup.Roundness = CreatorWrap(Setup.Roundness, UE_ARRAY_COUNT(CreatorRoundnesses));
	Setup.Nights = CreatorWrap(Setup.Nights, UE_ARRAY_COUNT(CreatorNightBeasts));
	return Setup;
}

void FForestSetup::Save() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetInt(CreatorSection, TEXT("Season"), Season, GGameUserSettingsIni);
	GConfig->SetInt(CreatorSection, TEXT("Weather"), Weather, GGameUserSettingsIni);
	GConfig->SetInt(CreatorSection, TEXT("Roundness"), Roundness, GGameUserSettingsIni);
	GConfig->SetInt(CreatorSection, TEXT("Nights"), Nights, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

const TArray<FName>& FForestSetup::Weathers()
{
	static const TArray<FName> Names = {TEXT("Clear"), TEXT("Mist"), TEXT("Cloudy"), TEXT("SunShower"), TEXT("Rain"), TEXT("Snow"), TEXT("Storm")};
	return Names;
}

FName FForestSetup::WeatherName() const
{
	return Weathers()[CreatorWrap(Weather, Weathers().Num())];
}

void FForestSetup::ApplyWorld(UWorld* World, bool bWeather) const
{
	if (!World)
	{
		return;
	}
	if (UForestSeasons* Seasons = UForestSeasons::Get(World))
	{
		Seasons->SetSeason(static_cast<EForestSeason>(Season), true);
	}
	if (UForestNight* Nights_ = UForestNight::Get(World))
	{
		Nights_->MaxBeasts = CreatorNightBeasts[Nights];
	}
	AForestSky* Sky = AForestSky::Get(World);
	if (!Sky)
	{
		return;
	}
	if (UMaterialParameterCollection* Collection = Sky->WeatherParameters)
	{
		if (Collection->GetScalarParameterByName(TEXT("Curvature")))
		{
			UKismetMaterialLibrary::SetScalarParameterValue(World, Collection, TEXT("Curvature"), CreatorMaxCurvature * CreatorRoundnesses[Roundness]);
		}
	}
	if (bWeather)
	{
		Sky->SetMoment(Sky->GetHour(), WeatherName(), false);
	}
}

void FForestSetup::ApplyDayLength(UWorld* World) const
{
	if (AForestSky* Sky = AForestSky::Get(World))
	{
		Sky->DayMinutes = CreatorDayMinutes;
	}
}

void FForestSetup::ApplyCharacter(UWorld* World) const
{
	FForestLook::Load().Apply(World ? Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr);
}

// --- The pages ------------------------------------------------------------------

float UForestCreator::Appear(float Delay) const
{
	const float Age = float(FPlatformTime::Seconds() - ShownAt) - Delay;
	const float Alpha = FMath::Clamp(Age / 0.8f, 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}

void UForestCreator::StepLook(EForestLookPart Part, int32 Step)
{
	Look.Step(Part, Step);
	Look.Save();
	Look.Apply(Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

void UForestCreator::SurpriseLook()
{
	Look = FForestLook::Random();
	Look.Save();
	Look.Apply(Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

FString UForestCreator::WorldValue(int32 Row) const
{
	switch (Row)
	{
	case 0: return CreatorSeasonLabel(Setup.Season);
	case 1: return ForestLanguage::Weather(Setup.WeatherName());
	case 2: return CreatorRoundnessLabel(Setup.Roundness);
	default: return CreatorNightsLabel(Setup.Nights);
	}
}

void UForestCreator::StepWorld(int32 Row, int32 Step)
{
	switch (Row)
	{
	case 0: Setup.Season = CreatorWrap(Setup.Season + Step, 4); break;
	case 1: Setup.Weather = CreatorWrap(Setup.Weather + Step, FForestSetup::Weathers().Num()); break;
	case 2: Setup.Roundness = CreatorWrap(Setup.Roundness + Step, UE_ARRAY_COUNT(CreatorRoundnesses)); break;
	default: Setup.Nights = CreatorWrap(Setup.Nights + Step, UE_ARRAY_COUNT(CreatorNightBeasts)); break;
	}
	// Seen at once on the forest behind the page.
	Setup.ApplyWorld(GetWorld(), Row == 1);
	Setup.Save();
}

TSharedRef<SWidget> UForestCreator::Button(TFunction<FString()> Label, TFunction<void()> Click, bool bMain)
{
	TWeakObjectPtr<UForestCreator> Self = this;
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", bMain ? 22 : 17);
	Font.LetterSpacing = bMain ? 260 : 120;
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ContentPadding(bMain ? FMargin(48.0f, 13.0f) : FMargin(26.0f, 11.0f))
		.ButtonColorAndOpacity_Lambda([Self, bMain]()
		{
			const float Alpha = Self.IsValid() ? Self->Appear(0.4f) : 0.0f;
			return bMain ? FLinearColor(0.95f, 0.78f, 0.45f, 0.34f * Alpha) : FLinearColor(0.2f, 0.2f, 0.2f, 0.22f * Alpha);
		})
		.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text_Lambda([Label]() { return FText::FromString(Label()); })
			.Font(Font)
			.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorCream, Self.IsValid() ? Self->Appear(0.4f) : 0.0f)); })
			.ShadowOffset(FVector2D(0.0f, 2.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
		];
}

TSharedRef<SWidget> UForestCreator::Row(int32 Index, TFunction<FString()> Label, TFunction<FString()> Value, TFunction<void(int32)> Step,
	TFunction<FString()> Value2, TFunction<void(int32)> Step2)
{
	TWeakObjectPtr<UForestCreator> Self = this;
	const float Delay = 0.15f + 0.08f * Index;
	auto Fade = [Self, Delay](FLinearColor Color) { return FSlateColor(CreatorAlpha(Color, Self.IsValid() ? Self->Appear(Delay) : 0.0f)); };
	auto Arrow = [Self, Delay, Fade](TFunction<void(int32)> Step, const TCHAR* Glyph, int32 Direction) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ContentPadding(FMargin(14.0f, 4.0f))
			.ButtonColorAndOpacity_Lambda([Self, Delay]() { return FLinearColor(0.95f, 0.78f, 0.45f, 0.22f * (Self.IsValid() ? Self->Appear(Delay) : 0.0f)); })
			.OnClicked_Lambda([Step, Direction]() { Step(Direction); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Glyph))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity_Lambda([Fade]() { return Fade(CreatorGold); })
			];
	};

	// One value, or two side by side (a haircut and its colour...).
	const bool bPair = (bool)Value2;
	auto ValueBox = [Fade](TFunction<FString()> Text, float Width) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Text]() { return FText::FromString(Text()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
				.ColorAndOpacity_Lambda([Fade]() { return Fade(CreatorCream); })
				.ShadowOffset(FVector2D(0.0f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
				.Justification(ETextJustify::Center)
			];
	};
	TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(170.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Label]() { return FText::FromString(Label()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 17))
				.ColorAndOpacity_Lambda([Fade]() { return Fade(CreatorAlpha(CreatorCream, 0.8f)); })
				.ShadowOffset(FVector2D(0.0f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(Step, TEXT("<"), -1)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ValueBox(Value, bPair ? 190.0f : 250.0f)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(Step, TEXT(">"), 1)];
	if (bPair)
	{
		Line->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(18.0f, 0.0f, 0.0f, 0.0f))[Arrow(Step2, TEXT("<"), -1)];
		Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[ValueBox(Value2, 150.0f)];
		Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[Arrow(Step2, TEXT(">"), 1)];
	}
	return Line;
}

void UForestCreator::GoToPage(int32 NewPage)
{
	if (NewPage >= 2)
	{
		Setup.Save();
		if (UForestGameInstance* GameInstance = GetGameInstance<UForestGameInstance>())
		{
			GameInstance->StartForestIntro();
		}
		return;
	}
	ShowPage(FMath::Max(0, NewPage));
}

void UForestCreator::ShowPage(int32 NewPage)
{
	Page = NewPage;
	ShownAt = FPlatformTime::Seconds();
	if (UForestGameInstance* GameInstance = GetGameInstance<UForestGameInstance>())
	{
		GameInstance->ShowCreatorView(Page);
	}
	if (Page == 1)
	{
		Setup.ApplyWorld(GetWorld(), true);
	}
	if (!Body.IsValid())
	{
		return;
	}
	Body->ClearChildren();
	TWeakObjectPtr<UForestCreator> Self = this;

	FSlateFontInfo StepFont = FCoreStyle::GetDefaultFontStyle("Regular", 14);
	StepFont.LetterSpacing = 300;
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Light", 46);
	TitleFont.LetterSpacing = 260;

	const bool bCharacter = Page == 0;
	const FString Step = FString::Printf(TEXT("%d / 2"), Page + 1);
	const FString Title = bCharacter ? ForestLanguage::Pick(TEXT("YOUR CHARACTER"), TEXT("TON PERSONNAGE")) : ForestLanguage::Pick(TEXT("YOUR WORLD"), TEXT("TON MONDE"));
	const FString Subtitle = bCharacter
		? ForestLanguage::Pick(TEXT("Who will wake up in the forest?"), TEXT("Qui va se réveiller dans la forêt ?"))
		: ForestLanguage::Pick(TEXT("Shape the forest you will wake up in."), TEXT("Façonne la forêt où tu vas te réveiller."));

	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(Step))
		.Font(StepFont)
		.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorGold, 0.8f * (Self.IsValid() ? Self->Appear() : 0.0f))); })
	];
	Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(Title))
		.Font(TitleFont)
		.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorGold, Self.IsValid() ? Self->Appear() : 0.0f)); })
		.ShadowOffset(FVector2D(0.0f, 3.0f))
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
	];
	Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 34.0f))
	[
		SNew(STextBlock)
		.Text(FText::FromString(Subtitle))
		.Font(FCoreStyle::GetDefaultFontStyle("Italic", 19))
		.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorCream, 0.9f * (Self.IsValid() ? Self->Appear(0.1f) : 0.0f))); })
		.ShadowOffset(FVector2D(0.0f, 2.0f))
		.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
	];

	if (bCharacter)
	{
		struct FLookRow
		{
			const TCHAR* English;
			const TCHAR* French;
			EForestLookPart Part;
			int32 Color;	// the part's colour on the same line, or -1
		};
		const FLookRow Rows[] = {
			{TEXT("Body"), TEXT("Corps"), EForestLookPart::Body, -1},
			{TEXT("Head"), TEXT("Tête"), EForestLookPart::Head, -1},
			{TEXT("Skin"), TEXT("Peau"), EForestLookPart::Skin, -1},
			{TEXT("Hair"), TEXT("Cheveux"), EForestLookPart::Hair, (int32)EForestLookPart::HairColor},
			{TEXT("Face"), TEXT("Visage"), EForestLookPart::Face, -1},
			{TEXT("Top"), TEXT("Haut"), EForestLookPart::Top, (int32)EForestLookPart::TopColor},
			{TEXT("Bottom"), TEXT("Bas"), EForestLookPart::Bottom, (int32)EForestLookPart::BottomColor},
			{TEXT("Shoes"), TEXT("Chaussures"), EForestLookPart::Shoes, (int32)EForestLookPart::ShoeColor},
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Rows); ++Index)
		{
			const FLookRow& Each = Rows[Index];
			const TCHAR* English = Each.English;
			const TCHAR* French = Each.French;
			const EForestLookPart Part = Each.Part;
			TFunction<FString()> Value2 = nullptr;
			TFunction<void(int32)> Step2 = nullptr;
			if (Each.Color >= 0)
			{
				const EForestLookPart Color = (EForestLookPart)Each.Color;
				Value2 = [Self, Color]() { return Self.IsValid() ? Self->Look.ValueName(Color) : FString(); };
				Step2 = [Self, Color](int32 Direction) { if (Self.IsValid()) { Self->StepLook(Color, Direction); } };
			}
			Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
			[
				Row(Index, [English, French]() { return FString(ForestLanguage::Pick(English, French)); },
					[Self, Part]() { return Self.IsValid() ? Self->Look.ValueName(Part) : FString(); },
					[Self, Part](int32 Direction) { if (Self.IsValid()) { Self->StepLook(Part, Direction); } },
					Value2, Step2)
			];
		}
		Body->AddSlot().AutoHeight().Padding(FMargin(170.0f, 14.0f, 0.0f, 0.0f))
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			[
				Button([]() { return FString(ForestLanguage::Pick(TEXT("SURPRISE ME"), TEXT("AU HASARD"))); },
					[Self]() { if (Self.IsValid()) { Self->SurpriseLook(); } }, false)
			]
		];
		Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 18.0f, 0.0f, 0.0f))
		[
			SNew(SBox)
			.WidthOverride(620.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ForestLanguage::Pick(
					TEXT("Hats, glasses, backpacks... you will find them later in the forest."),
					TEXT("Chapeaux, lunettes, sacs à dos... tu les trouveras plus tard dans la forêt."))))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 14))
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorCream, 0.6f * (Self.IsValid() ? Self->Appear(0.3f) : 0.0f))); })
				.ShadowOffset(FVector2D(0.0f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
			]
		];
	}
	else
	{
		const TCHAR* Labels[][2] = {
			{TEXT("Season"), TEXT("Saison")},
			{TEXT("Weather"), TEXT("Météo")},
			{TEXT("Round world"), TEXT("Monde rond")},
			{TEXT("Nights"), TEXT("Nuits")},
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
		{
			const TCHAR* English = Labels[Index][0];
			const TCHAR* French = Labels[Index][1];
			Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 7.0f))
			[
				Row(Index, [English, French]() { return FString(ForestLanguage::Pick(English, French)); },
					[Self, Index]() { return Self.IsValid() ? Self->WorldValue(Index) : FString(); },
					[Self, Index](int32 Direction) { if (Self.IsValid()) { Self->StepWorld(Index, Direction); } })
			];
		}
		Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))
		[
			SNew(SBox)
			.WidthOverride(560.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Self]()
				{
					const int32 Nights = Self.IsValid() ? Self->Setup.Nights : 1;
					return FText::FromString(Nights == 0
						? ForestLanguage::Pick(TEXT("Few creatures come out at night. Ideal to explore."), TEXT("Peu de créatures sortent la nuit. Idéal pour explorer."))
						: Nights == 2
						? ForestLanguage::Pick(TEXT("Many red eyes in the dark. Build fast, stay in the light."), TEXT("Beaucoup d'yeux rouges dans le noir. Construis vite, reste dans la lumière."))
						: ForestLanguage::Pick(TEXT("Gentle days, dangerous nights."), TEXT("Des jours doux, des nuits dangereuses.")));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 14))
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([Self]() { return FSlateColor(CreatorAlpha(CreatorCream, 0.6f * (Self.IsValid() ? Self->Appear(0.5f) : 0.0f))); })
				.ShadowOffset(FVector2D(0.0f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
			]
		];
	}

	Body->AddSlot().FillHeight(1.0f)[SNew(SSpacer)];
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bCharacter)
	{
		Buttons->AddSlot().AutoWidth()
		[
			Button([]() { return FString(ForestLanguage::Pick(TEXT("BACK"), TEXT("RETOUR"))); },
				[Self]() { if (Self.IsValid()) { if (UForestGameInstance* GameInstance = Self->GetGameInstance<UForestGameInstance>()) { GameInstance->BackToTitle(); } } }, false)
		];
		Buttons->AddSlot().AutoWidth().Padding(FMargin(18.0f, 0.0f, 0.0f, 0.0f))
		[
			Button([]() { return FString(ForestLanguage::Pick(TEXT("NEXT  >"), TEXT("SUIVANT  >"))); },
				[Self]() { if (Self.IsValid()) { Self->ShowPage(1); } }, true)
		];
	}
	else
	{
		Buttons->AddSlot().AutoWidth()
		[
			Button([]() { return FString(ForestLanguage::Pick(TEXT("<  CHARACTER"), TEXT("<  PERSONNAGE"))); },
				[Self]() { if (Self.IsValid()) { Self->ShowPage(0); } }, false)
		];
		Buttons->AddSlot().AutoWidth().Padding(FMargin(18.0f, 0.0f, 0.0f, 0.0f))
		[
			Button([]() { return FString(ForestLanguage::Pick(TEXT("WAKE UP"), TEXT("SE RÉVEILLER"))); },
				[Self]()
				{
					if (Self.IsValid())
					{
						Self->Setup.Save();
						if (UForestGameInstance* GameInstance = Self->GetGameInstance<UForestGameInstance>())
						{
							GameInstance->StartForestIntro();
						}
					}
				}, true)
		];
	}
	Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 70.0f))[Buttons];
}

TSharedRef<SWidget> UForestCreator::RebuildWidget()
{
	Setup = FForestSetup::Load();
	Look = FForestLook::Load();

	// A soft shade on the left, where the page is; the right stays clear for the view.
	TSharedRef<SHorizontalBox> Shade = SNew(SHorizontalBox);
	for (int32 Band = 12; Band >= 1; --Band)
	{
		Shade->AddSlot().FillWidth(0.6f / 12.0f)[CreatorShade(0.6f * FMath::Pow(Band / 12.0f, 1.3f))];
	}
	Shade->AddSlot().FillWidth(0.4f)[SNew(SSpacer)];

	TWeakObjectPtr<UForestCreator> Self = this;
	TSharedRef<SWidget> Root = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.Padding(0.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
				.Padding(0.0f)
				.ColorAndOpacity_Lambda([Self]() { return FLinearColor(1.0f, 1.0f, 1.0f, Self.IsValid() ? FMath::Min(1.0f, float(FPlatformTime::Seconds() - Self->ShownAt) + 0.3f) : 0.0f); })
				[Shade]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.Padding(FMargin(110.0f, 120.0f, 0.0f, 0.0f))
			[
				SAssignNew(Body, SVerticalBox)
			]
		];
	ShowPage(0);
	return Root;
}
