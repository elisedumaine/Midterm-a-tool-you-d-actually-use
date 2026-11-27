#include "ForestLife.h"

#include "ForestBlocks.h"
#include "ForestSpirit.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestBuilding.h"
#include "ForestChest.h"
#include "ForestCrafting.h"
#include "ForestCrop.h"
#include "ForestEvents.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestNightBeast.h"
#include "ForestSky.h"
#include "ForestSurvival.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

const TCHAR* ForestText(const TCHAR* English, const TCHAR* French, const TCHAR* Spanish, const TCHAR* Italian)
{
	switch (ForestLanguage::Get())
	{
	case EForestLanguage::English: return English;
	case EForestLanguage::French: return French;
	case EForestLanguage::Spanish: return Spanish;
	case EForestLanguage::Italian: return Italian;
	default:
	{
		// Portuguese, Chinese, Japanese: the shared table, by the English text.
		const TCHAR* Translated = ForestLanguage::Translate(English, ForestLanguage::Get());
		return Translated ? Translated : English;
	}
	}
}

namespace
{
	const FName Hands(TEXT("Hands"));
	const FName Branch(TEXT("Branch"));
	const FName Spear(TEXT("Spear"));
	const FName Axe(TEXT("Axe"));
	const FName Torch(TEXT("Torch"));
	const FName BerrySeeds(TEXT("BerrySeeds"));
	const FName Spores(TEXT("Spores"));

	/** Everything that can sit in the hotbar, in its order. */
	const FName AllThings[] = {Hands, Branch, Spear, Axe, Torch, BerrySeeds, Spores};

	/** How much use a new tool has (seconds of light for the torch). */
	float NewWear(FName Tool)
	{
		if (Tool == Branch) return 20.0f;
		if (Tool == Spear) return 35.0f;
		if (Tool == Axe) return 40.0f;
		if (Tool == Torch) return 240.0f;
		return 0.0f;
	}

	bool IsSeed(FName Thing)
	{
		return Thing == BerrySeeds || Thing == Spores;
	}

	constexpr float HungerSeconds = 540.0f;		// a full stomach lasts 9 minutes
	constexpr int32 HiddenChests = 14;
	constexpr float ChestReach = 260.0f;
	constexpr float CropReach = 220.0f;

	const TCHAR* const Weekdays[(int32)EForestLanguage::Count][7] = {
		{TEXT("Monday"), TEXT("Tuesday"), TEXT("Wednesday"), TEXT("Thursday"), TEXT("Friday"), TEXT("Saturday"), TEXT("Sunday")},
		{TEXT("lundi"), TEXT("mardi"), TEXT("mercredi"), TEXT("jeudi"), TEXT("vendredi"), TEXT("samedi"), TEXT("dimanche")},
		{TEXT("lunes"), TEXT("martes"), TEXT("miércoles"), TEXT("jueves"), TEXT("viernes"), TEXT("sábado"), TEXT("domingo")},
		{TEXT("lunedì"), TEXT("martedì"), TEXT("mercoledì"), TEXT("giovedì"), TEXT("venerdì"), TEXT("sabato"), TEXT("domenica")},
		{TEXT("segunda-feira"), TEXT("terça-feira"), TEXT("quarta-feira"), TEXT("quinta-feira"), TEXT("sexta-feira"), TEXT("sábado"), TEXT("domingo")},
		{TEXT("星期一"), TEXT("星期二"), TEXT("星期三"), TEXT("星期四"), TEXT("星期五"), TEXT("星期六"), TEXT("星期日")},
		{TEXT("月曜日"), TEXT("火曜日"), TEXT("水曜日"), TEXT("木曜日"), TEXT("金曜日"), TEXT("土曜日"), TEXT("日曜日")},
	};
	const TCHAR* const Months[(int32)EForestLanguage::Count][12] = {
		{TEXT("January"), TEXT("February"), TEXT("March"), TEXT("April"), TEXT("May"), TEXT("June"), TEXT("July"), TEXT("August"), TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")},
		{TEXT("janvier"), TEXT("février"), TEXT("mars"), TEXT("avril"), TEXT("mai"), TEXT("juin"), TEXT("juillet"), TEXT("août"), TEXT("septembre"), TEXT("octobre"), TEXT("novembre"), TEXT("décembre")},
		{TEXT("enero"), TEXT("febrero"), TEXT("marzo"), TEXT("abril"), TEXT("mayo"), TEXT("junio"), TEXT("julio"), TEXT("agosto"), TEXT("septiembre"), TEXT("octubre"), TEXT("noviembre"), TEXT("diciembre")},
		{TEXT("gennaio"), TEXT("febbraio"), TEXT("marzo"), TEXT("aprile"), TEXT("maggio"), TEXT("giugno"), TEXT("luglio"), TEXT("agosto"), TEXT("settembre"), TEXT("ottobre"), TEXT("novembre"), TEXT("dicembre")},
		{TEXT("janeiro"), TEXT("fevereiro"), TEXT("março"), TEXT("abril"), TEXT("maio"), TEXT("junho"), TEXT("julho"), TEXT("agosto"), TEXT("setembro"), TEXT("outubro"), TEXT("novembro"), TEXT("dezembro")},
		// Chinese and Japanese write the month as a number (11月); these are unused.
		{TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("11"), TEXT("12")},
		{TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("10"), TEXT("11"), TEXT("12")},
	};

	int32 LanguageIndex()
	{
		return FMath::Clamp((int32)ForestLanguage::Get(), 0, (int32)EForestLanguage::Count - 1);
	}

	FString Capitalized(FString Text)
	{
		if (!Text.IsEmpty())
		{
			Text[0] = FChar::ToUpper(Text[0]);
		}
		return Text;
	}

	UMaterialInstanceDynamic* Paint(UObject* Outer, const TCHAR* Base, const FLinearColor& Color, float Glow = -1.0f)
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Base);
		if (!Material)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Outer);
		Dynamic->SetVectorParameterValue(TEXT("Tint"), Color);
		if (Glow >= 0.0f)
		{
			Dynamic->SetScalarParameterValue(TEXT("Glow"), Glow);
		}
		else
		{
			Dynamic->SetScalarParameterValue(TEXT("Sway"), 0.0f);
		}
		return Dynamic;
	}

	const TCHAR* TintPath = TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint");
	const TCHAR* GlowPath = TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow");

	// --- What the survivor sees on screen ----------------------------------------
	class SForestLifeOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestLifeOverlay) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestLife>, Life)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			TWeakObjectPtr<UForestLife> Weak = Args._Life;
			const FLinearColor Cream(0.98f, 0.94f, 0.84f, 1.0f);
			const FLinearColor Shade(0.02f, 0.03f, 0.02f, 0.55f);
			auto Panel = []() { return FCoreStyle::Get().GetBrush("WhiteBrush"); };

			FSlateFontInfo BannerFont = FCoreStyle::GetDefaultFontStyle("Bold", 30);
			BannerFont.OutlineSettings.OutlineSize = 2;
			BannerFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

			// The hotbar: one slot per thing; only what the player owns shows.
			TSharedRef<SHorizontalBox> Hotbar = SNew(SHorizontalBox);
			for (const FName Thing : AllThings)
			{
				Hotbar->AddSlot()
					.AutoWidth()
					.Padding(FMargin(4.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(104.0f)
						.HeightOverride(64.0f)
						.Visibility_Lambda([Weak, Thing]() { return Weak.IsValid() && Weak->Hotbar().Contains(Thing) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						[
							SNew(SBorder)
							.BorderImage(Panel())
							.BorderBackgroundColor_Lambda([Weak, Thing, Shade]()
							{
								const bool bHeld = Weak.IsValid() && Weak->GetHeld() == Thing;
								return FSlateColor(bHeld ? FLinearColor(0.85f, 0.66f, 0.3f, 0.75f) : Shade);
							})
							.Padding(FMargin(6.0f, 4.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
									.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.8f, 0.8f))
									.Text_Lambda([Weak, Thing]()
									{
										const int32 Index = Weak.IsValid() ? Weak->Hotbar().IndexOfByKey(Thing) : INDEX_NONE;
										return FText::FromString(Index == INDEX_NONE ? FString() : FString::FromInt(Index + 1));
									})
								]
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.ColorAndOpacity(Cream)
									.Justification(ETextJustify::Center)
									.Text_Lambda([Weak, Thing]()
									{
										if (!Weak.IsValid())
										{
											return FText::GetEmpty();
										}
										const int32 Seeds = Weak->SeedCount(Thing);
										return FText::FromString(Seeds >= 0 ? FString::Printf(TEXT("%s ×%d"), *Weak->ThingName(Thing), Seeds) : Weak->ThingName(Thing));
									})
								]
								// How worn the tool is.
								+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
								[
									SNew(SBox)
									.HeightOverride(5.0f)
									.WidthOverride_Lambda([Weak, Thing]() { return 92.0f * FMath::Max(0.0f, Weak.IsValid() ? Weak->WearLeft(Thing) : 0.0f); })
									.HAlign(HAlign_Left)
									.Visibility_Lambda([Weak, Thing]() { return Weak.IsValid() && Weak->WearLeft(Thing) >= 0.0f ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
									[
										SNew(SBorder)
										.BorderImage(Panel())
										.BorderBackgroundColor_Lambda([Weak, Thing]()
										{
											const float Left = Weak.IsValid() ? Weak->WearLeft(Thing) : 0.0f;
											return FSlateColor(Left > 0.3f ? FLinearColor(0.5f, 0.85f, 0.45f) : FLinearColor(0.95f, 0.4f, 0.25f));
										})
									]
								]
							]
						]
					];
			}

			ChildSlot
			[
				SNew(SOverlay)
				.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->IsActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				// Top centre: the date since the adventure began.
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))
				[
					SNew(SBorder)
					.BorderImage(Panel())
					.BorderBackgroundColor(Shade)
					.Padding(FMargin(22.0f, 8.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Italic", 16))
							.ColorAndOpacity(Cream)
							.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->DateText() : FString()); })
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor(0.95f, 0.8f, 0.5f, 1.0f))
							.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->DayText() : FString()); })
						]
					]
				]
				// Events: a big line in the upper middle.
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 260.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Font(BannerFont)
					.Justification(ETextJustify::Center)
					.Text_Lambda([Weak]()
					{
						const UForestEvents* Events = Weak.IsValid() ? UForestEvents::Get(Weak.Get()) : nullptr;
						return FText::FromString(Events ? Events->GetBanner() : FString());
					})
					.ColorAndOpacity_Lambda([Weak]()
					{
						const UForestEvents* Events = Weak.IsValid() ? UForestEvents::Get(Weak.Get()) : nullptr;
						return FSlateColor(FLinearColor(1.0f, 0.9f, 0.7f, Events ? Events->GetBannerAlpha() : 0.0f));
					})
				]
				// Bottom left, above the hearts: hunger.
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(40.0f, 0.0f, 0.0f, 100.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.ColorAndOpacity(Cream)
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.Text_Lambda([Weak]()
						{
							const float Food = Weak.IsValid() ? Weak->GetFood() : 0.0f;
							return FText::FromString(FString::Printf(TEXT("%s  %d%%"),
								Food < 25.0f ? ForestText(TEXT("Starving!"), TEXT("Affamée !"), TEXT("¡Hambrienta!"), TEXT("Affamata!"))
									: ForestText(TEXT("Food"), TEXT("Nourriture"), TEXT("Comida"), TEXT("Cibo")),
								FMath::RoundToInt(Food)));
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
					[
						SNew(SBorder)
						.BorderImage(Panel())
						.BorderBackgroundColor(Shade)
						.Padding(2.0f)
						[
							SNew(SBox)
							.WidthOverride(300.0f)
							.HeightOverride(12.0f)
							.HAlign(HAlign_Left)
							[
								SNew(SBox)
								.WidthOverride_Lambda([Weak]() { return 300.0f * (Weak.IsValid() ? Weak->GetFood() / UForestLife::MaxFood : 0.0f); })
								[
									SNew(SBorder)
									.BorderImage(Panel())
									.BorderBackgroundColor_Lambda([Weak]()
									{
										const float Food = Weak.IsValid() ? Weak->GetFood() : 0.0f;
										return FSlateColor(Food > 50.0f ? FLinearColor(0.85f, 0.65f, 0.25f) : Food > 25.0f ? FLinearColor(0.95f, 0.45f, 0.15f) : FLinearColor(0.9f, 0.12f, 0.1f));
									})
								]
							]
						]
					]
				]
				// Bottom centre: what E would do here (when the game has nothing else to say).
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 140.0f))
				[
					SNew(SBorder)
					.BorderImage(Panel())
					.BorderBackgroundColor(Shade)
					.Padding(FMargin(28.0f, 10.0f))
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && !Weak->GetPrompt().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
						.ColorAndOpacity(Cream)
						.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->GetPrompt() : FString()); })
					]
				]
				// Bottom centre: the hotbar and its keys.
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 22.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.ColorAndOpacity(FLinearColor(0.95f, 0.93f, 0.85f, 0.85f))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.Text_Lambda([Weak]() { return FText::FromString(Weak.IsValid() ? Weak->HelpLine() : FString()); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						Hotbar
					]
				]
				// Right: the journal (J).
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 30.0f, 0.0f))
				[
					SNew(SBox)
					.WidthOverride(520.0f)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->IsJournalOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SBorder)
						.BorderImage(Panel())
						.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.03f, 0.82f))
						.Padding(FMargin(22.0f, 16.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
								.ColorAndOpacity(FLinearColor(0.95f, 0.8f, 0.5f, 1.0f))
								.Text_Lambda([]() { return FText::FromString(ForestText(TEXT("Survival journal   [J]"), TEXT("Carnet de survie   [J]"), TEXT("Diario de supervivencia   [J]"), TEXT("Diario di sopravvivenza   [J]"))); })
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
								.ColorAndOpacity(Cream)
								.AutoWrapText(true)
								.Text_Lambda([Weak]()
								{
									if (!Weak.IsValid())
									{
										return FText::GetEmpty();
									}
									// The newest first.
									const TArray<FString>& Lines = Weak->GetJournal();
									TArray<FString> Shown;
									for (int32 Index = Lines.Num() - 1; Index >= 0 && Shown.Num() < 12; --Index)
									{
										Shown.Add(Lines[Index]);
									}
									return FText::FromString(FString::Join(Shown, TEXT("\n")));
								})
							]
						]
					]
				]
			];
		}
	};
}

// --- Setup ------------------------------------------------------------------

bool UForestLife::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UForestLife::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartDate = FDateTime::Now().GetDate();
}

void UForestLife::Deinitialize()
{
	if (bStarted)
	{
		SaveLife();
	}
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

TStatId UForestLife::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestLife, STATGROUP_Tickables);
}

UForestLife* UForestLife::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestLife>() : nullptr;
}

void UForestLife::EnsureOverlay()
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
	Viewport = Client;
	Overlay = SNew(SForestLifeOverlay).Life(this);
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 40);
}

void UForestLife::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

void UForestLife::Start(APawn* Player)
{
	bStarted = true;
	GatherSpots();
	LoadLife();
	if (Chests.IsEmpty())
	{
		HideChests(HiddenChests);
	}
	if (Journal.IsEmpty())
	{
		Log(ForestText(TEXT("You woke up alone in the forest."), TEXT("Tu t'es réveillée seule dans la forêt."),
			TEXT("Despertaste sola en el bosque."), TEXT("Ti sei svegliata da sola nella foresta.")));
	}
	ShowHeld();
	UE_LOG(LogForestGame, Log, TEXT("Forest life: %d chests, %d crops, day %d."), Chests.Num(), Crops.Num(), GetSurvivalDay());
}

// --- Every frame -------------------------------------------------------------

void UForestLife::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}
	EnsureOverlay();
	if (!bStarted)
	{
		Start(Player);
	}
	// Nothing happens during the title screen and the wake-up.
	bActive = !Controller->IsMoveInputIgnored();
	if (!bActive)
	{
		return;
	}

	UseCooldown -= DeltaSeconds;
	SaveTimer += DeltaSeconds;
	if (SaveTimer > 30.0f)
	{
		SaveLife();
	}
	ReadKeys(Controller, Player);
	UpdateHunger(DeltaSeconds, Player);
	UpdateTorch(DeltaSeconds, Player);
	UpdateSwing(DeltaSeconds);
	UpdatePrompt(Player);
	UpdateDay();
}

void UForestLife::ReadKeys(APlayerController* Controller, APawn* Player)
{
	auto Pressed = [Controller](const FKey& Key) { return Controller->WasInputKeyJustPressed(Key); };
	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	// The build menu owns the number keys, the mouse and E while it is open.
	const UForestBlocks* Blocks = UForestBlocks::Get(GetWorld());
	const bool bBuilding = (Manager && (Manager->IsBuildMenuOpen() || Manager->IsPlacing())) || (Blocks && Blocks->WantsInput());

	if (Pressed(EKeys::F9))
	{
		ResetLife(Player);
		return;
	}
	if (Pressed(EKeys::J))
	{
		bJournal = !bJournal;
	}
	if (Pressed(EKeys::C))
	{
		Eat(Player);
	}
	if (Pressed(EKeys::Q) || Pressed(EKeys::Gamepad_RightShoulder))
	{
		Cycle();
	}
	if (bBuilding)
	{
		return;
	}
	static const FKey Numbers[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven};
	const TArray<FName> Bar = Hotbar();
	for (int32 Index = 0; Index < Bar.Num() && Index < UE_ARRAY_COUNT(Numbers); ++Index)
	{
		if (Pressed(Numbers[Index]))
		{
			Select(Bar[Index]);
		}
	}
	if (Pressed(EKeys::LeftMouseButton) || Pressed(EKeys::Gamepad_RightTrigger))
	{
		Use(Player);
	}
	// E belongs to the game manager first (pickups, animals, the feeder).
	if ((Pressed(EKeys::E) || Pressed(EKeys::Gamepad_FaceButton_Left)) && (!Manager || Manager->Prompt.IsEmpty()))
	{
		Interact(Player);
	}
}

// --- Hunger ---------------------------------------------------------------

void UForestLife::UpdateHunger(float DeltaSeconds, APawn* Player)
{
	const float Before = Food;
	Food = FMath::Max(0.0f, Food - DeltaSeconds * MaxFood / HungerSeconds);
	if (Before >= 25.0f && Food < 25.0f)
	{
		Say(ForestText(TEXT("You are getting very hungry... Eat something (C)!"), TEXT("Tu as très faim... Mange quelque chose (C) !"),
			TEXT("Tienes mucha hambre... ¡Come algo (C)!"), TEXT("Hai molta fame... Mangia qualcosa (C)!")), 4.0f);
	}
	if (Food > 0.0f)
	{
		StarveTimer = 0.0f;
		return;
	}
	// Starving: a half heart every few seconds.
	StarveTimer += DeltaSeconds;
	if (StarveTimer >= 10.0f)
	{
		StarveTimer = 0.0f;
		if (UForestSurvival* Survival = UForestSurvival::Get(GetWorld()))
		{
			Survival->Hurt(1, Player);
		}
		Say(ForestText(TEXT("You are starving! Eat something (C)."), TEXT("Tu meurs de faim ! Mange quelque chose (C)."),
			TEXT("¡Te mueres de hambre! Come algo (C)."), TEXT("Stai morendo di fame! Mangia qualcosa (C).")), 3.0f);
	}
}

void UForestLife::Eat(APawn* Player)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}
	if (Food >= MaxFood - 5.0f)
	{
		Say(ForestText(TEXT("You are not hungry."), TEXT("Tu n'as pas faim."), TEXT("No tienes hambre."), TEXT("Non hai fame.")), 2.0f);
		return;
	}
	// Mushrooms fill more than berries; eat whichever the basket holds.
	FName Meal = NAME_None;
	float Gain = 0.0f;
	if (Manager->Count(ForestCrafting::Mushroom) > 0)
	{
		Meal = ForestCrafting::Mushroom;
		Gain = 22.0f;
	}
	else if (Manager->Count(ForestCrafting::Berry) > 0)
	{
		Meal = ForestCrafting::Berry;
		Gain = 12.0f;
	}
	if (Meal == NAME_None)
	{
		Say(ForestText(TEXT("Your basket has nothing to eat. Find berries or mushrooms, or grow some!"), TEXT("Ton panier est vide. Trouve des baies ou des champignons, ou fais-en pousser !"),
			TEXT("Tu cesta no tiene comida. ¡Busca bayas o setas, o cultívalas!"), TEXT("Il tuo cesto è vuoto. Trova bacche o funghi, o coltivali!")), 3.0f);
		return;
	}
	// By a campfire the food is cooked: it feeds more and warms the heart.
	bool bCooked = false;
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		if (It->GetBuildingType() == TEXT("Campfire") && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 450.0f)
		{
			bCooked = true;
			break;
		}
	}
	Manager->AddItem(Meal, -1);
	Food = FMath::Min(MaxFood, Food + Gain * (bCooked ? 1.6f : 1.0f));
	if (bCooked)
	{
		if (UForestSurvival* Survival = UForestSurvival::Get(GetWorld()))
		{
			Survival->Heal(1);
		}
	}
	const FString What = ForestCrafting::ItemName(Meal, 1);
	Say(bCooked
		? ForestLanguage::Format(ForestText(TEXT("You grill a %s over the fire. Delicious!"), TEXT("Tu fais griller : %s. Délicieux !"), TEXT("Asas: %s. ¡Delicioso!"), TEXT("Arrostisci: %s. Delizioso!")), *What)
		: ForestLanguage::Format(ForestText(TEXT("You eat a %s."), TEXT("Tu manges : %s."), TEXT("Comes: %s."), TEXT("Mangi: %s.")), *What), 2.0f);
}

// --- Held things -------------------------------------------------------------

TArray<FName> UForestLife::Hotbar() const
{
	TArray<FName> Bar;
	for (const FName Thing : AllThings)
	{
		if (Thing == Hands || Gear.Contains(Thing) || SeedCount(Thing) > 0)
		{
			Bar.Add(Thing);
		}
	}
	return Bar;
}

int32 UForestLife::SeedCount(FName Seeds) const
{
	if (!IsSeed(Seeds))
	{
		return -1;
	}
	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	return Manager ? Manager->Count(Seeds) : 0;
}

float UForestLife::WearLeft(FName Thing) const
{
	const float* Left = Gear.Find(Thing);
	return Left ? FMath::Clamp(*Left / NewWear(Thing), 0.0f, 1.0f) : -1.0f;
}

FString UForestLife::ThingName(FName Thing) const
{
	if (Thing == Branch) return ForestText(TEXT("Branch"), TEXT("Bâton"), TEXT("Palo"), TEXT("Bastone"));
	if (Thing == Spear) return ForestText(TEXT("Spear"), TEXT("Lance"), TEXT("Lanza"), TEXT("Lancia"));
	if (Thing == Axe) return ForestText(TEXT("Axe"), TEXT("Hache"), TEXT("Hacha"), TEXT("Ascia"));
	if (Thing == Torch) return ForestText(TEXT("Torch"), TEXT("Torche"), TEXT("Antorcha"), TEXT("Torcia"));
	if (Thing == BerrySeeds) return ForestText(TEXT("Berry seeds"), TEXT("Graines"), TEXT("Semillas"), TEXT("Semi"));
	if (Thing == Spores) return ForestText(TEXT("Spores"), TEXT("Spores"), TEXT("Esporas"), TEXT("Spore"));
	return ForestText(TEXT("Hands"), TEXT("Mains"), TEXT("Manos"), TEXT("Mani"));
}

FString UForestLife::HelpLine() const
{
	return ForestText(TEXT("[Q] Switch   [Left click] Use   [C] Eat   [J] Journal"), TEXT("[Q] Changer   [Clic gauche] Utiliser   [C] Manger   [J] Carnet"),
		TEXT("[Q] Cambiar   [Clic izq.] Usar   [C] Comer   [J] Diario"), TEXT("[Q] Cambia   [Clic sinistro] Usa   [C] Mangia   [J] Diario"));
}

void UForestLife::Select(FName Thing)
{
	if (Held == Thing)
	{
		return;
	}
	Held = Thing;
	ShowHeld();
}

void UForestLife::Cycle()
{
	const TArray<FName> Bar = Hotbar();
	const int32 Index = Bar.IndexOfByKey(Held);
	Select(Bar[(Index + 1) % Bar.Num()]);
}

void UForestLife::Wear(FName Tool, float Amount)
{
	float* Left = Gear.Find(Tool);
	if (!Left)
	{
		return;
	}
	*Left -= Amount;
	if (*Left > 0.0f)
	{
		return;
	}
	Gear.Remove(Tool);
	Say(Tool == Torch
		? FString(ForestText(TEXT("Your torch burns out."), TEXT("Ta torche s'éteint."), TEXT("Tu antorcha se apaga."), TEXT("La tua torcia si spegne.")))
		: ForestLanguage::Format(ForestText(TEXT("Your %s breaks!"), TEXT("Ton outil se casse : %s !"), TEXT("¡Se rompe: %s!"), TEXT("Si rompe: %s!")), *ThingName(Tool)), 3.0f);
	Held = Hands;
	ShowHeld();
}

USceneComponent* UForestLife::HandAnchor(APawn* Player, FName& OutSocket) const
{
	OutSocket = NAME_None;
	ACharacter* Character = Cast<ACharacter>(Player);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh)
	{
		// Whatever the skeleton calls its right hand.
		for (int32 Bone = 0; Bone < Mesh->GetNumBones(); ++Bone)
		{
			const FString Name = Mesh->GetBoneName(Bone).ToString().ToLower();
			if (Name.Contains(TEXT("hand")) && (Name.Contains(TEXT("_r")) || Name.Contains(TEXT("right")) || Name.Contains(TEXT("r_"))))
			{
				OutSocket = Mesh->GetBoneName(Bone);
				return Mesh;
			}
		}
	}
	return Player->GetRootComponent();
}

void UForestLife::ShowHeld()
{
	if (HeldRoot)
	{
		TArray<USceneComponent*> Children;
		HeldRoot->GetChildrenComponents(true, Children);
		for (USceneComponent* Child : Children)
		{
			Child->DestroyComponent();
		}
		HeldRoot->DestroyComponent();
		HeldRoot = nullptr;
		TorchLight = nullptr;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player || Held == Hands)
	{
		return;
	}

	FName Socket;
	USceneComponent* Anchor = HandAnchor(Player, Socket);
	HeldRoot = NewObject<USceneComponent>(Player);
	HeldRoot->SetupAttachment(Anchor, Socket);
	HeldRoot->RegisterComponent();
	if (Socket.IsNone())
	{
		// No hand bone: carry it at the right side.
		HeldRoot->SetRelativeLocation(FVector(30.0f, 38.0f, -5.0f));
	}
	HeldRoot->SetUsingAbsoluteRotation(true);

	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	auto Part = [Player, Manager, Root = HeldRoot.Get()](const TCHAR* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Player);
		Piece->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		Piece->SetupAttachment(Root);
		Piece->SetRelativeLocation(Location);
		Piece->SetRelativeRotation(Rotation);
		Piece->SetRelativeScale3D(Scale);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->RegisterComponent();
		if (Material)
		{
			Piece->SetMaterial(0, Material);
		}
		else if (Manager)
		{
			Manager->DressLikeWorld(Piece);
		}
		return Piece;
	};
	const TCHAR* LogMesh = TEXT("/Game/WorldBuilder/Imported/Kenney/log.log");
	const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	// Kenney's log lies along Y (71 long): Roll 90 stands it up along Z.
	const FRotator Up(0.0f, 0.0f, 90.0f);
	UMaterialInstanceDynamic* StoneGrey = Paint(Player, TintPath, FLinearColor(0.42f, 0.42f, 0.45f));

	if (Held == Branch)
	{
		Part(LogMesh, FVector(0.0f, 0.0f, 30.0f), Up, FVector(0.35f, 1.3f, 0.35f), nullptr);
	}
	else if (Held == Spear)
	{
		Part(LogMesh, FVector(0.0f, 0.0f, 40.0f), Up, FVector(0.22f, 2.0f, 0.22f), nullptr);
		Part(Cone, FVector(0.0f, 0.0f, 118.0f), FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.22f), StoneGrey);
	}
	else if (Held == Axe)
	{
		Part(LogMesh, FVector(0.0f, 0.0f, 25.0f), Up, FVector(0.3f, 1.1f, 0.3f), nullptr);
		Part(Cube, FVector(9.0f, 0.0f, 56.0f), FRotator::ZeroRotator, FVector(0.24f, 0.05f, 0.15f), StoneGrey);
	}
	else if (Held == Torch)
	{
		Part(LogMesh, FVector(0.0f, 0.0f, 20.0f), Up, FVector(0.3f, 0.9f, 0.3f), nullptr);
		Part(Cone, FVector(0.0f, 0.0f, 58.0f), FRotator::ZeroRotator, FVector(0.13f, 0.13f, 0.26f), Paint(Player, GlowPath, FLinearColor(1.0f, 0.45f, 0.1f), 5.0f));
		Part(Sphere, FVector(0.0f, 0.0f, 52.0f), FRotator::ZeroRotator, FVector(0.12f), Paint(Player, GlowPath, FLinearColor(1.0f, 0.8f, 0.3f), 4.0f));
		TorchLight = NewObject<UPointLightComponent>(Player);
		TorchLight->SetupAttachment(HeldRoot);
		TorchLight->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
		TorchLight->SetIntensityUnits(ELightUnits::Candelas);
		TorchLight->SetIntensity(45.0f);
		TorchLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.28f));
		TorchLight->SetAttenuationRadius(1300.0f);
		TorchLight->SetCastShadows(false);
		TorchLight->RegisterComponent();
	}
	else if (IsSeed(Held))
	{
		// A little seed pouch.
		Part(Sphere, FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.2f), Paint(Player, TintPath, FLinearColor(0.45f, 0.3f, 0.15f)));
	}
	SwingTime = -1.0f;
	UpdateSwing(0.0f);
}

void UForestLife::UpdateSwing(float DeltaSeconds)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!HeldRoot || !Player)
	{
		return;
	}
	// Held upright and a little forward; a swing chops down in front and back up.
	float Chop = 0.0f;
	if (SwingTime >= 0.0f)
	{
		SwingTime += DeltaSeconds;
		const float Alpha = FMath::Clamp(SwingTime / 0.35f, 0.0f, 1.0f);
		Chop = -85.0f * FMath::Sin(PI * Alpha);
		if (Alpha >= 1.0f)
		{
			SwingTime = -1.0f;
		}
	}
	HeldRoot->SetWorldRotation(FRotator(-15.0f + Chop, Player->GetActorRotation().Yaw, 0.0f));
}

void UForestLife::UpdateTorch(float DeltaSeconds, APawn* Player)
{
	if (Held != Torch || !Gear.Contains(Torch))
	{
		return;
	}
	if (TorchLight)
	{
		TorchLight->SetIntensity(45.0f * (0.85f + 0.15f * FMath::Sin(GetWorld()->GetTimeSeconds() * 12.0f)));
	}
	Wear(Torch, DeltaSeconds);
	// The creatures of the night fear the flame.
	TorchCheck -= DeltaSeconds;
	if (TorchCheck > 0.0f)
	{
		return;
	}
	TorchCheck = 0.5f;
	for (TActorIterator<AForestNightBeast> It(GetWorld()); It; ++It)
	{
		if (!It->IsFleeing() && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 520.0f)
		{
			It->Flee();
			Say(ForestText(TEXT("The creature fears your torch and flees!"), TEXT("La créature a peur de ta torche et s'enfuit !"),
				TEXT("¡La criatura teme tu antorcha y huye!"), TEXT("La creatura teme la tua torcia e fugge!")), 2.5f);
		}
	}
}

void UForestLife::Use(APawn* Player)
{
	if (UseCooldown > 0.0f || Held == Hands)
	{
		return;
	}
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	const FVector Here = Player->GetActorLocation();
	const FVector Forward = Player->GetActorForwardVector();

	// Seeds: dig a little hole in front and plant.
	if (IsSeed(Held))
	{
		UseCooldown = 0.4f;
		const FVector Spot = Here + Forward * 150.0f;
		float Z = 0.0f;
		if (!Manager || SeedCount(Held) <= 0)
		{
			return;
		}
		if (!GroundAt(Spot.X, Spot.Y, Z) || NearestCrop(Spot, 120.0f))
		{
			Say(ForestText(TEXT("Not here: find free, open ground."), TEXT("Pas ici : trouve un sol libre et dégagé."), TEXT("Aquí no: busca suelo libre."), TEXT("Non qui: trova un terreno libero.")), 2.0f);
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AForestCrop* Crop = GetWorld()->SpawnActor<AForestCrop>(AForestCrop::StaticClass(), FTransform(FVector(Spot.X, Spot.Y, Z)), Params))
		{
			const FName Planted = Held;
			Crop->Plant(Planted, 0.0f);
			Crops.Add(Crop);
			Manager->AddItem(Planted, -1);
			if (UForestSpirit* Spirit = UForestSpirit::Get(GetWorld()))
			{
				Spirit->ChangeHealth(3.0f);
			}
			Say(ForestText(TEXT("Planted! Rain makes it grow faster."), TEXT("Planté ! La pluie le fait pousser plus vite."), TEXT("¡Plantado! La lluvia lo hace crecer más rápido."), TEXT("Piantato! La pioggia lo fa crescere più in fretta.")), 2.5f);
			Log(Planted == BerrySeeds
				? ForestText(TEXT("You planted berry seeds."), TEXT("Tu as planté des graines de baies."), TEXT("Plantaste semillas de bayas."), TEXT("Hai piantato semi di bacche."))
				: ForestText(TEXT("You planted mushroom spores."), TEXT("Tu as planté des spores de champignons."), TEXT("Plantaste esporas de setas."), TEXT("Hai piantato spore di funghi.")));
			if (SeedCount(Planted) <= 0)
			{
				Select(Hands);
			}
		}
		return;
	}

	// A tool or a weapon: a swing in front.
	UseCooldown = 0.5f;
	SwingTime = 0.0f;
	const float Reach = Held == Spear ? 340.0f : 240.0f;
	auto InFront = [&](const AActor* Target)
	{
		FVector To = Target->GetActorLocation() - Here;
		To.Z = 0.0f;
		return To.Size() < Reach && FVector::DotProduct(To.GetSafeNormal(), Forward) > 0.35f;
	};

	// Night creatures first: hits wound them, enough hits and they flee.
	for (TActorIterator<AForestNightBeast> It(GetWorld()); It; ++It)
	{
		AForestNightBeast* Beast = *It;
		if (Beast->IsFleeing() || !InFront(Beast))
		{
			continue;
		}
		const int32 Damage = Held == Branch ? 1 : 2;
		int32& Health = Wounds.FindOrAdd(Beast, 3);
		Health -= Damage;
		FVector Push = Beast->GetActorLocation() - Here;
		Push.Z = 0.0f;
		Beast->SetActorLocation(Beast->GetActorLocation() + Push.GetSafeNormal() * 160.0f, true);
		Wear(Held, 1.0f);
		if (Health <= 0 || Held == Torch)
		{
			Beast->Flee();
			Wounds.Remove(Beast);
			Say(ForestLanguage::Format(ForestText(TEXT("You drove off %s!"), TEXT("Tu as fait fuir %s !"), TEXT("¡Has ahuyentado a %s!"), TEXT("Hai messo in fuga %s!")), *Beast->GetBeastName()), 2.5f);
			Log(ForestText(TEXT("You fought off a creature of the night."), TEXT("Tu as repoussé une créature de la nuit."), TEXT("Rechazaste a una criatura de la noche."), TEXT("Hai respinto una creatura della notte.")));
		}
		else
		{
			Say(ForestText(TEXT("Hit! It staggers..."), TEXT("Touché ! Elle recule..."), TEXT("¡Golpe! Retrocede..."), TEXT("Colpito! Indietreggia...")), 1.5f);
		}
		return;
	}

	// An animal that is not tame: a knock makes it back off (a charging boar too).
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		AForestAnimal* Animal = *It;
		if (Animal->IsTame() || !InFront(Animal))
		{
			continue;
		}
		FVector Push = Animal->GetActorLocation() - Here;
		Push.Z = 0.0f;
		Animal->SetActorLocation(Animal->GetActorLocation() + Push.GetSafeNormal() * 220.0f, true);
		Animal->Refuse();
		Wear(Held, 1.0f);
		Say(ForestLanguage::Format(ForestText(TEXT("%s backs off."), TEXT("%s recule."), TEXT("%s retrocede."), TEXT("%s indietreggia.")),
			*Capitalized(ForestLanguage::Animal(Animal->DisplayName.ToString()))), 2.0f);
		return;
	}

	// The axe chops wood from a tree.
	if (Held == Axe && Manager)
	{
		for (const FVector& Tree : TreeSpots)
		{
			FVector To = Tree - Here;
			To.Z = 0.0f;
			if (To.Size() < 280.0f && FVector::DotProduct(To.GetSafeNormal(), Forward) > 0.3f)
			{
				Manager->AddItem(ForestCrafting::Wood, 2);
				Wear(Axe, 1.0f);
				// Cutting living trees hurts the forest.
				if (UForestSpirit* Spirit = UForestSpirit::Get(GetWorld()))
				{
					Spirit->ChangeHealth(-2.0f);
				}
				// Sometimes the cut bark weeps resin: torches and glow blocks.
				const bool bResin = FMath::FRand() < 0.4f;
				if (bResin)
				{
					Manager->AddItem(ForestCrafting::Resin, 1);
				}
				Say(ForestLanguage::Format(ForestText(TEXT("Chop! +2 wood (%d)"), TEXT("Tchac ! +2 bois (%d)"), TEXT("¡Zas! +2 madera (%d)"), TEXT("Zac! +2 legna (%d)")), Manager->Count(ForestCrafting::Wood))
					+ (bResin ? FString(ForestLanguage::Pick(TEXT("  +1 resin"), TEXT("  +1 résine"))) : FString()), 1.5f);
				return;
			}
		}
	}
}

// --- Chests and crops ---------------------------------------------------------

void UForestLife::Interact(APawn* Player)
{
	const FVector Here = Player->GetActorLocation();
	if (AForestChest* Chest = NearestChest(Here, ChestReach))
	{
		OpenChest(Chest);
		return;
	}
	if (AForestCrop* Crop = NearestCrop(Here, CropReach))
	{
		if (!Crop->IsRipe())
		{
			return;
		}
		int32 Amount = 0;
		const FName Fruit = Crop->Harvest(Amount);
		if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
		{
			Manager->AddItem(Fruit, Amount);
		}
		Say(ForestLanguage::Format(ForestText(TEXT("Harvest: +%s. It will grow again."), TEXT("Récolte : +%s. Ça va repousser."), TEXT("Cosecha: +%s. Volverá a crecer."), TEXT("Raccolto: +%s. Ricrescerà.")),
			*ForestCrafting::ItemName(Fruit, Amount)), 2.5f);
		Log(ForestLanguage::Format(ForestText(TEXT("You harvested %s from your garden."), TEXT("Tu as récolté %s dans ton jardin."), TEXT("Cosechaste %s de tu huerto."), TEXT("Hai raccolto %s dal tuo orto.")),
			*ForestCrafting::ItemName(Fruit, Amount)));
	}
}

void UForestLife::OpenChest(AForestChest* Chest)
{
	const bool bRare = Chest->IsStarChest();
	Chest->Open();
	TArray<FString> Found;
	GiveLoot(bRare, Found);
	GiveLoot(bRare, Found);
	if (bRare)
	{
		GiveLoot(true, Found);
	}
	const FString List = FString::Join(Found, TEXT(", "));
	Say(ForestLanguage::Format(ForestText(TEXT("In the chest: %s"), TEXT("Dans le coffre : %s"), TEXT("En el cofre: %s"), TEXT("Nel forziere: %s")), *List), 4.0f);
	Log(ForestLanguage::Format(bRare
		? ForestText(TEXT("You opened the star's chest: %s."), TEXT("Tu as ouvert le coffre de l'étoile : %s."), TEXT("Abriste el cofre de la estrella: %s."), TEXT("Hai aperto il forziere della stella: %s."))
		: ForestText(TEXT("You found a hidden chest: %s."), TEXT("Tu as trouvé un coffre caché : %s."), TEXT("Encontraste un cofre escondido: %s."), TEXT("Hai trovato un forziere nascosto: %s.")), *List));
	SaveLife();
}

void UForestLife::GiveLoot(bool bRare, TArray<FString>& OutFound)
{
	struct FLoot
	{
		FName Thing;
		int32 Amount;
		float Common;
		float Rare;
	};
	static const FLoot Table[] = {
		{Branch, 1, 20.0f, 2.0f},
		{Spear, 1, 12.0f, 18.0f},
		{Axe, 1, 12.0f, 14.0f},
		{Torch, 1, 9.0f, 22.0f},
		{BerrySeeds, 3, 18.0f, 10.0f},
		{Spores, 3, 14.0f, 10.0f},
		{ForestCrafting::Berry, 4, 10.0f, 4.0f},
		{ForestCrafting::Mushroom, 3, 8.0f, 4.0f},
		{ForestCrafting::Stone, 3, 8.0f, 2.0f},
		{TEXT("HeartFruit"), 1, 5.0f, 20.0f},
	};
	float Total = 0.0f;
	for (const FLoot& Loot : Table)
	{
		Total += bRare ? Loot.Rare : Loot.Common;
	}
	float Roll = FMath::FRandRange(0.0f, Total);
	const FLoot* Pick = &Table[0];
	for (const FLoot& Loot : Table)
	{
		Roll -= bRare ? Loot.Rare : Loot.Common;
		if (Roll <= 0.0f)
		{
			Pick = &Loot;
			break;
		}
	}

	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (Pick->Thing == TEXT("HeartFruit"))
	{
		// A glowing fruit that heals at once.
		if (UForestSurvival* Survival = UForestSurvival::Get(GetWorld()))
		{
			Survival->Heal(4);
		}
		Food = FMath::Min(MaxFood, Food + 30.0f);
		OutFound.Add(ForestText(TEXT("a heart fruit (+2 hearts)"), TEXT("un fruit-cœur (+2 cœurs)"), TEXT("una fruta corazón (+2 corazones)"), TEXT("un frutto cuore (+2 cuori)")));
		return;
	}
	if (NewWear(Pick->Thing) > 0.0f)
	{
		// A tool: new, or a fresh one for the worn one.
		const bool bHad = Gear.Contains(Pick->Thing);
		Gear.Add(Pick->Thing, NewWear(Pick->Thing));
		OutFound.Add(bHad
			? ForestLanguage::Format(ForestText(TEXT("a new %s"), TEXT("%s neuf"), TEXT("%s nuevo"), TEXT("%s nuovo")), *ThingName(Pick->Thing).ToLower())
			: ThingName(Pick->Thing).ToLower());
		if (Held == Hands)
		{
			Select(Pick->Thing);
		}
		return;
	}
	if (Manager)
	{
		Manager->AddItem(Pick->Thing, Pick->Amount);
	}
	OutFound.Add(IsSeed(Pick->Thing)
		? FString::Printf(TEXT("%d %s"), Pick->Amount, *ThingName(Pick->Thing).ToLower())
		: ForestCrafting::ItemName(Pick->Thing, Pick->Amount));
}

AForestChest* UForestLife::NearestChest(const FVector& From, float Range) const
{
	AForestChest* Best = nullptr;
	float BestDistance = Range;
	for (AForestChest* Chest : Chests)
	{
		if (Chest && !Chest->IsOpened())
		{
			const float Distance = FVector::Dist(From, Chest->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Chest;
			}
		}
	}
	return Best;
}

AForestCrop* UForestLife::NearestCrop(const FVector& From, float Range) const
{
	AForestCrop* Best = nullptr;
	float BestDistance = Range;
	for (AForestCrop* Crop : Crops)
	{
		if (Crop)
		{
			const float Distance = FVector::Dist2D(From, Crop->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Crop;
			}
		}
	}
	return Best;
}

void UForestLife::UpdatePrompt(APawn* Player)
{
	Prompt.Reset();
	const AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (Manager && (!Manager->Prompt.IsEmpty() || Manager->IsBuildMenuOpen() || Manager->IsPlacing()))
	{
		return;
	}
	const FVector Here = Player->GetActorLocation();
	if (NearestChest(Here, ChestReach))
	{
		Prompt = ForestText(TEXT("[E]  Open the chest"), TEXT("[E]  Ouvrir le coffre"), TEXT("[E]  Abrir el cofre"), TEXT("[E]  Apri il forziere"));
		return;
	}
	if (const AForestCrop* Crop = NearestCrop(Here, CropReach))
	{
		Prompt = Crop->IsRipe()
			? FString(ForestText(TEXT("[E]  Harvest"), TEXT("[E]  Récolter"), TEXT("[E]  Cosechar"), TEXT("[E]  Raccogli")))
			: ForestLanguage::Format(ForestText(TEXT("Growing... %d%%"), TEXT("Ça pousse... %d%%"), TEXT("Creciendo... %d%%"), TEXT("Sta crescendo... %d%%")), FMath::RoundToInt(Crop->GetGrowth() * 100.0f));
		return;
	}
	if (IsSeed(Held))
	{
		Prompt = ForestLanguage::Format(ForestText(TEXT("[Left click]  Plant (%d left)"), TEXT("[Clic gauche]  Planter (reste %d)"), TEXT("[Clic izq.]  Plantar (quedan %d)"), TEXT("[Clic sinistro]  Pianta (ne restano %d)")), SeedCount(Held));
	}
}

// --- Where to hide the chests -------------------------------------------------

void UForestLife::GatherSpots()
{
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		// The World Builder gives every animal the playable circle.
		WorldCenter = It->WorldCenter;
		WorldRadius = It->WorldRadius;
		break;
	}
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
				if (Component->GetInstanceTransform(Index, Transform, true) && FVector::Dist2D(Transform.GetLocation(), WorldCenter) < WorldRadius)
				{
					(bTree ? TreeSpots : RockSpots).Add(Transform.GetLocation());
				}
			}
		}
	}
}

bool UForestLife::GroundAt(float X, float Y, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestLifeGround), false);
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		Params.AddIgnoredActor(Player);
	}
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, Y, WorldCenter.Z + 20000.0f), FVector(X, Y, WorldCenter.Z - 20000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	// On a tree, a rock, a building or in the river: not open ground.
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Component || Component->IsA<UInstancedStaticMeshComponent>() || Cast<AForestBuilding>(Hit.GetActor()) || Hit.ImpactNormal.Z < 0.75f)
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
	OutZ = Hit.ImpactPoint.Z;
	return true;
}

AForestChest* UForestLife::SpawnChestAt(const FVector& Location, bool bStar, float Yaw, bool bOpened)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Half sunk in the grass so it is not too easy to spot.
	AForestChest* Chest = GetWorld()->SpawnActor<AForestChest>(AForestChest::StaticClass(), FTransform(FRotator(0.0f, Yaw, 0.0f), Location - FVector(0.0f, 0.0f, bStar ? 0.0f : 6.0f)), Params);
	if (Chest)
	{
		Chest->Build(bStar);
		if (bOpened)
		{
			Chest->Open();
		}
		Chests.Add(Chest);
	}
	return Chest;
}

bool UForestLife::SpawnChestNear(const FVector& Around, float MinDistance, float MaxDistance, bool bStar)
{
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Spot = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(MinDistance, MaxDistance);
		float Z = 0.0f;
		if (FVector::Dist2D(Spot, WorldCenter) < WorldRadius * 0.95f && GroundAt(Spot.X, Spot.Y, Z))
		{
			return SpawnChestAt(FVector(Spot.X, Spot.Y, Z), bStar, FMath::FRandRange(0.0f, 360.0f)) != nullptr;
		}
	}
	return false;
}

void UForestLife::HideChests(int32 Count)
{
	const TArray<FVector>& Near = RockSpots.Num() > 10 ? RockSpots : TreeSpots;
	if (Near.IsEmpty())
	{
		return;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	int32 Hidden = 0;
	for (int32 Try = 0; Try < Count * 30 && Hidden < Count; ++Try)
	{
		// Tucked against a rock (or a trunk), on its far side.
		const FVector& Cover = (Try % 3 == 2 && !TreeSpots.IsEmpty()) ? TreeSpots[FMath::RandRange(0, TreeSpots.Num() - 1)] : Near[FMath::RandRange(0, Near.Num() - 1)];
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Spot = Cover + Dir * FMath::FRandRange(90.0f, 170.0f);
		float Z = 0.0f;
		if (FVector::Dist2D(Spot, WorldCenter) > WorldRadius * 0.92f || !GroundAt(Spot.X, Spot.Y, Z))
		{
			continue;
		}
		bool bCrowded = Player && FVector::Dist2D(Player->GetActorLocation(), Spot) < 900.0f;
		for (const AForestChest* Other : Chests)
		{
			bCrowded |= Other && FVector::Dist2D(Other->GetActorLocation(), Spot) < 1600.0f;
		}
		if (bCrowded)
		{
			continue;
		}
		SpawnChestAt(FVector(Spot.X, Spot.Y, Z), false, Dir.Rotation().Yaw + 90.0f);
		++Hidden;
	}
}

// --- The calendar and the journal -------------------------------------------

int32 UForestLife::GetSurvivalDay() const
{
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	return DaysBefore + (Sky ? Sky->GetDay() : 1);
}

FString UForestLife::DateText() const
{
	const FDateTime Date = StartDate + FTimespan::FromDays(GetSurvivalDay() - 1);
	const int32 Language = LanguageIndex();
	const TCHAR* Weekday = Weekdays[Language][(int32)Date.GetDayOfWeek()];
	const TCHAR* Month = Months[Language][Date.GetMonth() - 1];
	switch (Language)
	{
	case 0: return FString::Printf(TEXT("%s %d %s %d"), Weekday, Date.GetDay(), Month, Date.GetYear());
	case 2: return Capitalized(FString::Printf(TEXT("%s %d de %s de %d"), Weekday, Date.GetDay(), Month, Date.GetYear()));
	case 4: return Capitalized(FString::Printf(TEXT("%s, %d de %s de %d"), Weekday, Date.GetDay(), Month, Date.GetYear()));
	case 5: return FString::Printf(TEXT("%d年%d月%d日 %s"), Date.GetYear(), Date.GetMonth(), Date.GetDay(), Weekday);
	case 6: return FString::Printf(TEXT("%d年%d月%d日（%s）"), Date.GetYear(), Date.GetMonth(), Date.GetDay(), Weekday);
	default: return Capitalized(FString::Printf(TEXT("%s %d %s %d"), Weekday, Date.GetDay(), Month, Date.GetYear()));
	}
}

FString UForestLife::DayText() const
{
	return ForestLanguage::Format(ForestText(TEXT("Day %d of survival"), TEXT("Jour %d de survie"), TEXT("Día %d de supervivencia"), TEXT("Giorno %d di sopravvivenza")), GetSurvivalDay());
}

void UForestLife::Log(const FString& Line)
{
	const FDateTime Date = StartDate + FTimespan::FromDays(GetSurvivalDay() - 1);
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	const int32 Hour = Sky ? FMath::FloorToInt(Sky->GetHour()) : 0;
	const int32 Minute = Sky ? FMath::FloorToInt(FMath::Frac(Sky->GetHour()) * 60.0f) : 0;
	Journal.Add(FString::Printf(TEXT("%02d/%02d  %02d:%02d  —  %s"), Date.GetDay(), Date.GetMonth(), Hour, Minute, *Line));
	if (Journal.Num() > 60)
	{
		Journal.RemoveAt(0);
	}
}

void UForestLife::UpdateDay()
{
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	if (!Sky)
	{
		return;
	}
	if (LastSkyDay == 0)
	{
		LastSkyDay = Sky->GetDay();
		return;
	}
	if (Sky->GetDay() != LastSkyDay)
	{
		LastSkyDay = Sky->GetDay();
		const FString Line = ForestLanguage::Format(ForestText(TEXT("A new day: %s"), TEXT("Un nouveau jour : %s"), TEXT("Un nuevo día: %s"), TEXT("Un nuovo giorno: %s")), *DateText());
		Say(Line + TEXT("  —  ") + DayText(), 5.0f);
		Log(ForestText(TEXT("You survived another night."), TEXT("Tu as survécu à une nuit de plus."), TEXT("Sobreviviste a otra noche."), TEXT("Sei sopravvissuta a un'altra notte.")));
		SaveLife();
	}
}

// --- Saving -------------------------------------------------------------------

FString UForestLife::SaveSlot() const
{
	return TEXT("ForestLife_") + UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
}

void UForestLife::SaveLife()
{
	SaveTimer = 0.0f;
	UForestLifeSave* Save = Cast<UForestLifeSave>(UGameplayStatics::CreateSaveGameObject(UForestLifeSave::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->StartDate = StartDate;
	// Each Play is a new morning: the days lived so far carry over.
	Save->DaysBefore = GetSurvivalDay();
	Save->Food = Food;
	Save->Gear = Gear;
	Save->Held = Held;
	Save->Journal = Journal;
	for (const AForestChest* Chest : Chests)
	{
		if (Chest)
		{
			FForestSavedChest& Saved = Save->Chests.AddDefaulted_GetRef();
			Saved.Location = Chest->GetActorLocation();
			Saved.Yaw = Chest->GetActorRotation().Yaw;
			Saved.bOpened = Chest->IsOpened();
			Saved.bStar = Chest->IsStarChest();
		}
	}
	for (const AForestCrop* Crop : Crops)
	{
		if (Crop)
		{
			FForestSavedCrop& Saved = Save->Crops.AddDefaulted_GetRef();
			Saved.Seed = Crop->GetSeed();
			Saved.Location = Crop->GetActorLocation();
			Saved.Growth = Crop->GetGrowth();
		}
	}
	UGameplayStatics::SaveGameToSlot(Save, SaveSlot(), 0);
}

void UForestLife::LoadLife()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0))
	{
		return;
	}
	const UForestLifeSave* Save = Cast<UForestLifeSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0));
	if (!Save)
	{
		return;
	}
	StartDate = Save->StartDate;
	DaysBefore = Save->DaysBefore;
	Food = Save->Food;
	Gear = Save->Gear;
	Held = Save->Held.IsNone() ? Hands : Save->Held;
	Journal = Save->Journal;
	for (const FForestSavedChest& Saved : Save->Chests)
	{
		// The saved spot was already sunk into the grass.
		SpawnChestAt(Saved.Location + FVector(0.0f, 0.0f, Saved.bStar ? 0.0f : 6.0f), Saved.bStar, Saved.Yaw, Saved.bOpened);
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FForestSavedCrop& Saved : Save->Crops)
	{
		if (AForestCrop* Crop = GetWorld()->SpawnActor<AForestCrop>(AForestCrop::StaticClass(), FTransform(Saved.Location), Params))
		{
			Crop->Plant(Saved.Seed, Saved.Growth);
			Crops.Add(Crop);
		}
	}
}

void UForestLife::ResetLife(APawn* Player)
{
	for (AForestChest* Chest : Chests)
	{
		if (Chest)
		{
			Chest->Destroy();
		}
	}
	for (AForestCrop* Crop : Crops)
	{
		if (Crop)
		{
			Crop->Destroy();
		}
	}
	Chests.Reset();
	Crops.Reset();
	Gear.Reset();
	Journal.Reset();
	Food = MaxFood;
	DaysBefore = 0;
	StartDate = FDateTime::Now().GetDate();
	Held = Hands;
	ShowHeld();
	UGameplayStatics::DeleteGameInSlot(SaveSlot(), 0);
	HideChests(HiddenChests);
	Log(ForestText(TEXT("A new life begins in the forest."), TEXT("Une nouvelle vie commence dans la forêt."), TEXT("Una nueva vida empieza en el bosque."), TEXT("Una nuova vita inizia nella foresta.")));
}

void UForestLife::Give(FName Thing, int32 Amount)
{
	if (NewWear(Thing) > 0.0f)
	{
		Gear.Add(Thing, NewWear(Thing));
		Select(Thing);
	}
	else if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->AddItem(Thing, Amount);
	}
}
