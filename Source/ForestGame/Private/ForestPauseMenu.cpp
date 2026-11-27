#include "ForestPauseMenu.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "ForestBlocks.h"
#include "ForestGameInstance.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* PauseSection = TEXT("ForestGame");
	const FLinearColor PauseGold(0.98f, 0.84f, 0.55f, 1.0f);
	const FLinearColor PauseCream(0.97f, 0.95f, 0.88f, 1.0f);

	int32 ReadSetting(const TCHAR* Key, int32 Default, int32 Max)
	{
		int32 Value = Default;
		if (GConfig)
		{
			GConfig->GetInt(PauseSection, Key, Value, GGameUserSettingsIni);
		}
		return FMath::Clamp(Value, 0, Max);
	}

	void WriteSetting(const TCHAR* Key, int32 Value)
	{
		if (GConfig)
		{
			GConfig->SetInt(PauseSection, Key, Value, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	FString PauseQualityName(int32 Level)
	{
		switch (Level)
		{
		case 0: return ForestLanguage::Pick(TEXT("Low"), TEXT("Basse"));
		case 1: return ForestLanguage::Pick(TEXT("Medium"), TEXT("Moyenne"));
		case 2: return ForestLanguage::Pick(TEXT("High"), TEXT("Haute"));
		default: return ForestLanguage::Pick(TEXT("Epic"), TEXT("Épique"));
		}
	}

	FString PauseVolumeText(int32 Value)
	{
		return Value == 0 ? FString(ForestLanguage::Pick(TEXT("Off"), TEXT("Coupé"))) : FString::Printf(TEXT("%d %%"), Value * 10);
	}
}

// --- The settings ---------------------------------------------------------------------

int32 ForestSettings::MasterVolume() { return ReadSetting(TEXT("MasterVolume"), 10, 10); }
int32 ForestSettings::MusicVolume() { return ReadSetting(TEXT("MusicVolume"), 7, 10); }
int32 ForestSettings::NatureVolume() { return ReadSetting(TEXT("NatureVolume"), 10, 10); }
int32 ForestSettings::Quality() { return ReadSetting(TEXT("Quality"), 3, 3); }

void ForestSettings::SetMasterVolume(int32 Value) { WriteSetting(TEXT("MasterVolume"), FMath::Clamp(Value, 0, 10)); }
void ForestSettings::SetMusicVolume(int32 Value) { WriteSetting(TEXT("MusicVolume"), FMath::Clamp(Value, 0, 10)); }
void ForestSettings::SetNatureVolume(int32 Value) { WriteSetting(TEXT("NatureVolume"), FMath::Clamp(Value, 0, 10)); }
void ForestSettings::SetQuality(int32 Value) { WriteSetting(TEXT("Quality"), FMath::Clamp(Value, 0, 3)); }

float ForestSettings::NatureMultiplier() { return NatureVolume() / 10.0f; }
float ForestSettings::MusicMultiplier() { return MusicVolume() / 10.0f; }

void ForestSettings::Apply(UWorld* World)
{
	if (World)
	{
		if (FAudioDeviceHandle Device = World->GetAudioDevice())
		{
			Device->SetTransientPrimaryVolume(MasterVolume() / 10.0f);
		}
	}
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (Settings->GetOverallScalabilityLevel() != Quality())
		{
			Settings->SetOverallScalabilityLevel(Quality());
			Settings->ApplySettings(false);
		}
	}
}

// --- The menu ------------------------------------------------------------------------------

TSharedRef<SWidget> UForestPauseWidget::Button(TFunction<FString()> Label, TFunction<void()> Click, bool bMain)
{
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", bMain ? 22 : 16);
	Font.LetterSpacing = bMain ? 260 : 140;
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ContentPadding(bMain ? FMargin(56.0f, 12.0f) : FMargin(26.0f, 9.0f))
		.HAlign(HAlign_Center)
		.ButtonColorAndOpacity(bMain ? FLinearColor(0.95f, 0.78f, 0.45f, 0.34f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.3f))
		.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text_Lambda([Label]() { return FText::FromString(Label()); })
			.Font(Font)
			.ColorAndOpacity(PauseCream)
			.ShadowOffset(FVector2D(0.0f, 2.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
		];
}

TSharedRef<SWidget> UForestPauseWidget::Row(TFunction<FString()> Label, TFunction<FString()> Value, TFunction<void(int32)> Step)
{
	auto Arrow = [Step](const TCHAR* Glyph, int32 Direction) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ContentPadding(FMargin(14.0f, 4.0f))
			.ButtonColorAndOpacity(FLinearColor(0.95f, 0.78f, 0.45f, 0.22f))
			.OnClicked_Lambda([Step, Direction]() { Step(Direction); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text(FText::FromString(Glyph))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity(PauseGold)
			];
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(230.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Label]() { return FText::FromString(Label()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 17))
				.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.8f))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT("<"), -1)]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(220.0f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([Value]() { return FText::FromString(Value()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity(PauseCream)
				.Justification(ETextJustify::Center)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Arrow(TEXT(">"), 1)];
}

TSharedRef<SWidget> UForestPauseWidget::RebuildWidget()
{
	TWeakObjectPtr<UForestPauseWidget> Self = this;
	auto Pick = [](const TCHAR* English, const TCHAR* French) { return [English, French]() { return FString(ForestLanguage::Pick(English, French)); }; };
	auto Menu = [Self]() { return Self.IsValid() ? UForestPauseMenu::Get(Self.Get()) : nullptr; };

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Light", 46);
	TitleFont.LetterSpacing = 300;

	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	auto AddRow = [&Rows](TSharedRef<SWidget> Widget) { Rows->AddSlot().AutoHeight().Padding(FMargin(0.0f, 6.0f))[Widget]; };

	AddRow(Row(Pick(TEXT("Volume"), TEXT("Volume général")),
		[]() { return PauseVolumeText(ForestSettings::MasterVolume()); },
		[Self](int32 Step)
		{
			ForestSettings::SetMasterVolume(ForestSettings::MasterVolume() + Step);
			ForestSettings::Apply(Self.IsValid() ? Self->GetWorld() : nullptr);
		}));
	AddRow(Row(Pick(TEXT("Music"), TEXT("Musique")),
		[]() { return PauseVolumeText(ForestSettings::MusicVolume()); },
		[](int32 Step) { ForestSettings::SetMusicVolume(ForestSettings::MusicVolume() + Step); }));
	AddRow(Row(Pick(TEXT("Nature sounds"), TEXT("Sons de la nature")),
		[]() { return PauseVolumeText(ForestSettings::NatureVolume()); },
		[](int32 Step) { ForestSettings::SetNatureVolume(ForestSettings::NatureVolume() + Step); }));
	AddRow(Row(Pick(TEXT("Graphics"), TEXT("Graphismes")),
		[]() { return PauseQualityName(ForestSettings::Quality()); },
		[Self](int32 Step)
		{
			ForestSettings::SetQuality(ForestSettings::Quality() + Step);
			ForestSettings::Apply(Self.IsValid() ? Self->GetWorld() : nullptr);
		}));
	AddRow(Row(Pick(TEXT("Language"), TEXT("Langue")),
		[]() { return FString(ForestLanguage::NativeName(ForestLanguage::Get())); },
		[](int32 Step)
		{
			const int32 Count = (int32)EForestLanguage::Count;
			ForestLanguage::Set((EForestLanguage)((((int32)ForestLanguage::Get() + Step) % Count + Count) % Count));
		}));
	AddRow(Row(Pick(TEXT("Help on screen"), TEXT("Aide à l'écran")),
		[Self]()
		{
			const AForestGameManager* Manager = Self.IsValid() ? AForestGameManager::Get(Self.Get()) : nullptr;
			return FString(Manager && Manager->bShowHelp ? ForestLanguage::Pick(TEXT("Shown"), TEXT("Affichée")) : ForestLanguage::Pick(TEXT("Hidden"), TEXT("Cachée")));
		},
		[Self](int32)
		{
			if (AForestGameManager* Manager = Self.IsValid() ? AForestGameManager::Get(Self.Get()) : nullptr)
			{
				Manager->bShowHelp = !Manager->bShowHelp;
			}
		}));

	return SNew(SOverlay)
		// The forest stays visible, darkened, behind the menu.
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.02f, 0.62f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("PAUSE"), TEXT("PAUSE"))); })
				.Font(TitleFont)
				.ColorAndOpacity(PauseGold)
				.ShadowOffset(FVector2D(0.0f, 3.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 30.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("The forest waits for you."), TEXT("La forêt t'attend."))); })
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 18))
				.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.85f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Rows]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 34.0f, 0.0f, 0.0f))
			[
				Button(Pick(TEXT("RESUME"), TEXT("REPRENDRE")), [Menu]() { if (UForestPauseMenu* M = Menu()) { M->Close(); } }, true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					Button(Pick(TEXT("TITLE SCREEN"), TEXT("ÉCRAN TITRE")), [Menu]() { if (UForestPauseMenu* M = Menu()) { M->BackToTitle(); } }, false)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(16.0f, 0.0f, 0.0f, 0.0f))
				[
					Button(Pick(TEXT("QUIT"), TEXT("QUITTER")), [Self]()
					{
						if (Self.IsValid())
						{
							UKismetSystemLibrary::QuitGame(Self.Get(), Self->GetOwningPlayer(), EQuitPreference::Quit, false);
						}
					}, false)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 22.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("P: back to the forest"), TEXT("P : retour à la forêt"))); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.55f))
			]
		];
}

FReply UForestPauseWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::P || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
	{
		if (UForestPauseMenu* Menu = UForestPauseMenu::Get(this))
		{
			Menu->Close();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// --- Opening and closing ---------------------------------------------------------------

bool UForestPauseMenu::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestPauseMenu::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestPauseMenu, STATGROUP_Tickables);
}

UForestPauseMenu* UForestPauseMenu::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestPauseMenu>() : nullptr;
}

void UForestPauseMenu::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		return;
	}
	if (!bApplied)
	{
		bApplied = true;
		ForestSettings::Apply(World);
	}
	if (IsOpen())
	{
		return;
	}
	// Escape stops Play in the editor: there only P (and the gamepad's Start) opens the menu.
	const bool bPressed = Controller->WasInputKeyJustPressed(EKeys::P) || Controller->WasInputKeyJustPressed(EKeys::Gamepad_Special_Right)
		|| (!GIsEditor && Controller->WasInputKeyJustPressed(EKeys::Escape));
	if (!bPressed)
	{
		return;
	}
	// Not during the title, the creative pages, the wake-up, or while building and crafting.
	const UForestBlocks* Blocks = UForestBlocks::Get(World);
	const AForestGameManager* Manager = AForestGameManager::Get(World);
	if (Controller->IsMoveInputIgnored() || (Blocks && Blocks->WantsInput()) || (Manager && Manager->IsBuildMenuOpen()))
	{
		return;
	}
	Open();
}

void UForestPauseMenu::Open()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || IsOpen())
	{
		return;
	}
	Widget = CreateWidget<UForestPauseWidget>(Controller, UForestPauseWidget::StaticClass());
	if (!Widget)
	{
		return;
	}
	Widget->SetIsFocusable(true);
	Widget->AddToViewport(200);
	bMouseWasShown = Controller->bShowMouseCursor;
	Controller->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);
	UGameplayStatics::SetGamePaused(World, true);
}

void UForestPauseMenu::Close()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	if (World)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	if (Controller)
	{
		Controller->bShowMouseCursor = bMouseWasShown;
		Controller->SetInputMode(FInputModeGameOnly());
	}
}

void UForestPauseMenu::BackToTitle()
{
	UWorld* World = GetWorld();
	Close();
	if (!World)
	{
		return;
	}
	if (UForestGameInstance* GameInstance = Cast<UForestGameInstance>(World->GetGameInstance()))
	{
		GameInstance->ReturnToTitle();
	}
	// Loading the level again saves the camp on the way out and starts on the title screen.
	UGameplayStatics::OpenLevel(World, FName(*UGameplayStatics::GetCurrentLevelName(World, true)));
}
