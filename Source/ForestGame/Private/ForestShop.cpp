#include "ForestShop.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "ForestBlocks.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestLife.h"
#include "ForestLook.h"
#include "ForestPauseMenu.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* ShopSection = TEXT("ForestGame");
	const FLinearColor ShopGold(0.98f, 0.84f, 0.55f, 1.0f);
	const FLinearColor ShopCream(0.97f, 0.95f, 0.88f, 1.0f);

	const FForestAccessory ShopItems[] = {
		{TEXT("Cap"), true, 1, 15, TEXT("Cap"), TEXT("Casquette"), true},
		{TEXT("Beanie"), true, 2, 15, TEXT("Beanie"), TEXT("Bonnet"), true},
		{TEXT("BucketHat"), true, 5, 20, TEXT("Bucket hat"), TEXT("Bob"), true},
		{TEXT("FlowerCrown"), true, 4, 30, TEXT("Flower crown"), TEXT("Couronne de fleurs"), false},
		{TEXT("WizardHat"), true, 3, 45, TEXT("Wizard hat"), TEXT("Chapeau de sorcier"), true},
		{TEXT("Scarf"), false, 2, 20, TEXT("Scarf"), TEXT("Écharpe"), true},
		{TEXT("Glasses"), false, 3, 20, TEXT("Glasses"), TEXT("Lunettes"), false},
		{TEXT("Backpack"), false, 1, 30, TEXT("Backpack"), TEXT("Sac à dos"), true},
	};

	FAutoConsoleCommandWithWorldAndArgs GFirefliesCommand(
		TEXT("Forest.Fireflies"),
		TEXT("Gives fireflies (the shop's money): Forest.Fireflies 50"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForestWallet::Add(World, Args.IsEmpty() ? 50 : FCString::Atoi(*Args[0]));
		}));
}

// --- Fireflies --------------------------------------------------------------------

int32 ForestWallet::Fireflies()
{
	int32 Value = 0;
	if (GConfig)
	{
		GConfig->GetInt(ShopSection, TEXT("Fireflies"), Value, GGameUserSettingsIni);
	}
	return FMath::Max(0, Value);
}

void ForestWallet::Add(const UObject* WorldContext, int32 Amount, bool bSay)
{
	if (!GConfig || Amount == 0)
	{
		return;
	}
	GConfig->SetInt(ShopSection, TEXT("Fireflies"), FMath::Max(0, Fireflies() + Amount), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	if (bSay && Amount > 0)
	{
		if (AForestGameManager* Manager = AForestGameManager::Get(WorldContext))
		{
			Manager->ShowMessage(FText::FromString(FOREST_PRINTF(TEXT("+%d fireflies  (%d)"), TEXT("+%d lucioles  (%d)"), Amount, Fireflies())), 2.5f);
		}
	}
}

bool ForestWallet::Spend(int32 Amount)
{
	if (Fireflies() < Amount)
	{
		return false;
	}
	Add(nullptr, -Amount, false);
	return true;
}

// --- Accessories and wardrobe -----------------------------------------------------------

TArrayView<const FForestAccessory> FForestAccessory::All()
{
	return ShopItems;
}

const FForestAccessory* FForestAccessory::Find(FName Id)
{
	for (const FForestAccessory& Item : ShopItems)
	{
		if (Item.Id == Id)
		{
			return &Item;
		}
	}
	return nullptr;
}

FString FForestAccessory::Name() const
{
	return ForestLanguage::Pick(English, French);
}

FForestWardrobe FForestWardrobe::Load()
{
	FForestWardrobe Wardrobe;
	if (!GConfig)
	{
		return Wardrobe;
	}
	FString Owned;
	GConfig->GetString(ShopSection, TEXT("Owned"), Owned, GGameUserSettingsIni);
	TArray<FString> Names;
	Owned.ParseIntoArray(Names, TEXT(","));
	for (const FString& Name : Names)
	{
		if (FForestAccessory::Find(FName(*Name)))
		{
			Wardrobe.Owned.Add(FName(*Name));
		}
	}
	FString Hat, Extra;
	GConfig->GetString(ShopSection, TEXT("Hat"), Hat, GGameUserSettingsIni);
	GConfig->GetString(ShopSection, TEXT("Extra"), Extra, GGameUserSettingsIni);
	Wardrobe.Hat = Wardrobe.Owns(FName(*Hat)) ? FName(*Hat) : NAME_None;
	Wardrobe.Extra = Wardrobe.Owns(FName(*Extra)) ? FName(*Extra) : NAME_None;
	GConfig->GetInt(ShopSection, TEXT("HatColor"), Wardrobe.HatColor, GGameUserSettingsIni);
	GConfig->GetInt(ShopSection, TEXT("ExtraColor"), Wardrobe.ExtraColor, GGameUserSettingsIni);
	return Wardrobe;
}

void FForestWardrobe::Save() const
{
	if (!GConfig)
	{
		return;
	}
	TArray<FString> Names;
	for (const FName& Id : Owned)
	{
		Names.Add(Id.ToString());
	}
	GConfig->SetString(ShopSection, TEXT("Owned"), *FString::Join(Names, TEXT(",")), GGameUserSettingsIni);
	GConfig->SetString(ShopSection, TEXT("Hat"), *(Hat == NAME_None ? FString() : Hat.ToString()), GGameUserSettingsIni);
	GConfig->SetString(ShopSection, TEXT("Extra"), *(Extra == NAME_None ? FString() : Extra.ToString()), GGameUserSettingsIni);
	GConfig->SetInt(ShopSection, TEXT("HatColor"), HatColor, GGameUserSettingsIni);
	GConfig->SetInt(ShopSection, TEXT("ExtraColor"), ExtraColor, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

// --- The shop's page ----------------------------------------------------------------------

TSharedRef<SWidget> UForestShopWidget::RebuildWidget()
{
	TWeakObjectPtr<UForestShopWidget> Self = this;
	auto Shop = [Self]() { return Self.IsValid() ? UForestShop::Get(Self.Get()) : nullptr; };
	auto SmallButton = [](TFunction<FString()> Label, TFunction<void()> Click, TFunction<FLinearColor()> Color) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ContentPadding(FMargin(16.0f, 5.0f))
			.HAlign(HAlign_Center)
			.ButtonColorAndOpacity_Lambda([Color]() { return Color(); })
			.OnClicked_Lambda([Click]() { Click(); return FReply::Handled(); })
			[
				SNew(STextBlock)
				.Text_Lambda([Label]() { return FText::FromString(Label()); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				.ColorAndOpacity(ShopCream)
			];
	};

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Light", 40);
	TitleFont.LetterSpacing = 280;

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FForestAccessory& Item : FForestAccessory::All())
	{
		const FName Id = Item.Id;
		const int32 Price = Item.Price;
		List->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(230.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Id]() { const FForestAccessory* Each = FForestAccessory::Find(Id); return FText::FromString(Each ? Each->Name() : FString()); })
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 17))
					.ColorAndOpacity(ShopCream)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(140.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([Id, Price]()
					{
						return FText::FromString(FForestWardrobe::Load().Owns(Id)
							? FString(ForestLanguage::Pick(TEXT("yours"), TEXT("à toi")))
							: FOREST_PRINTF(TEXT("%d fireflies"), TEXT("%d lucioles"), Price));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(ShopGold)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(150.0f)
				[
					SmallButton(
						[Id]()
						{
							const FForestWardrobe Wardrobe = FForestWardrobe::Load();
							return FString(!Wardrobe.Owns(Id) ? ForestLanguage::Pick(TEXT("BUY"), TEXT("ACHETER"))
								: Wardrobe.Wears(Id) ? ForestLanguage::Pick(TEXT("TAKE OFF"), TEXT("ENLEVER"))
								: ForestLanguage::Pick(TEXT("WEAR"), TEXT("PORTER")));
						},
						[Shop, Id]() { if (UForestShop* Each = Shop()) { Each->Choose(Id); } },
						[Id, Price]()
						{
							const FForestWardrobe Wardrobe = FForestWardrobe::Load();
							if (!Wardrobe.Owns(Id))
							{
								return ForestWallet::Fireflies() >= Price ? FLinearColor(0.95f, 0.78f, 0.45f, 0.4f) : FLinearColor(0.3f, 0.3f, 0.3f, 0.25f);
							}
							return Wardrobe.Wears(Id) ? FLinearColor(0.5f, 0.8f, 0.5f, 0.35f) : FLinearColor(0.2f, 0.2f, 0.2f, 0.3f);
						})
				]
			]
		];
	}

	// The colours of what is worn.
	auto ColorRow = [Shop, SmallButton](bool bHat) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			.Visibility_Lambda([bHat]()
			{
				const FForestWardrobe Wardrobe = FForestWardrobe::Load();
				const FForestAccessory* Worn = FForestAccessory::Find(bHat ? Wardrobe.Hat : Wardrobe.Extra);
				return Worn && Worn->bColor ? EVisibility::Visible : EVisibility::Collapsed;
			})
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(230.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([bHat]()
					{
						const FForestWardrobe Wardrobe = FForestWardrobe::Load();
						const FForestAccessory* Worn = FForestAccessory::Find(bHat ? Wardrobe.Hat : Wardrobe.Extra);
						return FText::FromString(FOREST_PRINTF(TEXT("Colour: %s"), TEXT("Couleur : %s"), Worn ? *Worn->Name() : TEXT("")));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 15))
					.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.8f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[SmallButton([]() { return FString(TEXT("<")); }, [Shop, bHat]() { if (UForestShop* Each = Shop()) { Each->StepColor(bHat, -1); } }, []() { return FLinearColor(0.95f, 0.78f, 0.45f, 0.22f); })]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(140.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([bHat]()
					{
						const FForestWardrobe Wardrobe = FForestWardrobe::Load();
						return FText::FromString(FForestLook::ChoiceName(EForestLookPart::TopColor, bHat ? Wardrobe.HatColor : Wardrobe.ExtraColor));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(ShopCream)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[SmallButton([]() { return FString(TEXT(">")); }, [Shop, bHat]() { if (UForestShop* Each = Shop()) { Each->StepColor(bHat, 1); } }, []() { return FLinearColor(0.95f, 0.78f, 0.45f, 0.22f); })];
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.02f, 0.7f))
			.Padding(FMargin(70.0f, 90.0f, 50.0f, 40.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("FOREST SHOP"), TEXT("BOUTIQUE"))); })
					.Font(TitleFont)
					.ColorAndOpacity(ShopGold)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 6.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(ForestLanguage::Pick(TEXT("The forest pays in fireflies."), TEXT("La forêt paie en lucioles."))); })
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 17))
					.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.85f))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 22.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return FText::FromString(FOREST_PRINTF(TEXT("You have %d fireflies"), TEXT("Tu as %d lucioles"), ForestWallet::Fireflies())); })
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 19))
					.ColorAndOpacity(FLinearColor(0.85f, 1.0f, 0.55f))
				]
				+ SVerticalBox::Slot().AutoHeight()[List]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 20.0f, 0.0f, 4.0f))[ColorRow(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 4.0f))[ColorRow(false)]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([Shop]()
					{
						const UForestShop* Each = Shop();
						return FText::FromString(Each && FPlatformTime::Seconds() - Each->NoticeAt < 3.0 ? Each->Notice : FString());
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.45f))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)[SNew(SBox)]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SmallButton([]() { return FString(ForestLanguage::Pick(TEXT("BACK TO THE FOREST  (U)"), TEXT("RETOUR À LA FORÊT  (U)"))); },
						[Shop]() { if (UForestShop* Each = Shop()) { Each->Close(); } },
						[]() { return FLinearColor(0.95f, 0.78f, 0.45f, 0.34f); })
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.WidthOverride(520.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([]()
						{
							return FText::FromString(ForestLanguage::Pick(
								TEXT("Earn fireflies by surviving nights, taming animals, welcoming residents and caring for the forest."),
								TEXT("Gagne des lucioles en survivant aux nuits, en apprivoisant les animaux, en accueillant des habitants et en prenant soin de la forêt.")));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 13))
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.97f, 0.95f, 0.88f, 0.6f))
					]
				]
			]
		];
}

FReply UForestShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::U || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (UForestShop* Shop = UForestShop::Get(this))
		{
			Shop->Close();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// --- The shop ---------------------------------------------------------------------------------

bool UForestShop::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestShop::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestShop, STATGROUP_Tickables);
}

UForestShop* UForestShop::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestShop>() : nullptr;
}

void UForestShop::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || IsOpen() || !Controller->WasInputKeyJustPressed(EKeys::U))
	{
		return;
	}
	const UForestBlocks* Blocks = UForestBlocks::Get(World);
	const AForestGameManager* Manager = AForestGameManager::Get(World);
	const UForestPauseMenu* Pause = UForestPauseMenu::Get(World);
	if (Controller->IsMoveInputIgnored() || World->IsPaused() || (Blocks && Blocks->WantsInput()) || (Manager && Manager->IsBuildMenuOpen())
		|| (Pause && Pause->IsOpen()))
	{
		return;
	}
	Open();
}

void UForestShop::Open()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	ACharacter* Player = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	if (!Player || IsOpen())
	{
		return;
	}
	Widget = CreateWidget<UForestShopWidget>(Controller, UForestShopWidget::StaticClass());
	if (!Widget)
	{
		return;
	}
	Widget->SetIsFocusable(true);
	Widget->AddToViewport(150);
	Controller->bShowMouseCursor = true;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Widget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);

	// A mirror: the camera in front of the player, a little to the side of the page.
	const FVector Body = Player->GetActorLocation();
	const FVector Forward = Player->GetActorForwardVector();
	FVector Spot = Body + Forward * 300.0f + FVector(0.0f, 0.0f, 25.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestShopCamera), false, Player);
	if (World->LineTraceSingleByChannel(Hit, Body + FVector(0.0f, 0.0f, 25.0f), Spot, ECC_Camera, Params))
	{
		Spot = Hit.Location + (Body - Hit.Location).GetSafeNormal() * 20.0f;
	}
	FRotator Look = (Body - Spot).Rotation();
	Look.Yaw -= 12.0f;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Spot, Look, SpawnParams);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(50.0f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Controller->SetViewTargetWithBlend(Camera, 0.6f, VTBlend_EaseInOut, 2.0f);
	}
}

void UForestShop::Close()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	if (Controller)
	{
		Controller->bShowMouseCursor = false;
		Controller->ResetIgnoreMoveInput();
		Controller->ResetIgnoreLookInput();
		Controller->SetInputMode(FInputModeGameOnly());
		if (APawn* Player = Controller->GetPawn())
		{
			Controller->SetViewTargetWithBlend(Player, 0.6f, VTBlend_EaseInOut, 2.0f);
		}
	}
	if (Camera)
	{
		ACameraActor* Old = Camera;
		Camera = nullptr;
		if (World)
		{
			FTimerHandle Later;
			World->GetTimerManager().SetTimer(Later, [Old]() { if (IsValid(Old)) { Old->Destroy(); } }, 0.8f, false);
		}
	}
}

void UForestShop::Dress() const
{
	FForestLook::Load().Apply(Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
}

void UForestShop::Choose(FName Id)
{
	const FForestAccessory* Item = FForestAccessory::Find(Id);
	if (!Item)
	{
		return;
	}
	FForestWardrobe Wardrobe = FForestWardrobe::Load();
	if (!Wardrobe.Owns(Id))
	{
		if (!ForestWallet::Spend(Item->Price))
		{
			Notice = FOREST_PRINTF(TEXT("Not enough fireflies: %d more needed."), TEXT("Pas assez de lucioles : il en manque %d."), Item->Price - ForestWallet::Fireflies());
			NoticeAt = FPlatformTime::Seconds();
			return;
		}
		Wardrobe.Owned.Add(Id);
		Notice = FOREST_PRINTF(TEXT("%s is yours!"), TEXT("%s est à toi !"), *Item->Name());
		NoticeAt = FPlatformTime::Seconds();
		if (UForestLife* Life = UForestLife::Get(this))
		{
			Life->Log(FOREST_PRINTF(TEXT("You bought: %s."), TEXT("Tu as acheté : %s."), *Item->Name()));
		}
	}
	// Bought or already owned: wear it, or take it off.
	FName& Slot = Item->bHat ? Wardrobe.Hat : Wardrobe.Extra;
	Slot = Slot == Id ? NAME_None : Id;
	Wardrobe.Save();
	Dress();
}

void UForestShop::StepColor(bool bHat, int32 Step)
{
	FForestWardrobe Wardrobe = FForestWardrobe::Load();
	const int32 Count = FForestLook::NumChoices(EForestLookPart::TopColor);
	int32& Color = bHat ? Wardrobe.HatColor : Wardrobe.ExtraColor;
	Color = ((Color + Step) % Count + Count) % Count;
	Wardrobe.Save();
	Dress();
}
