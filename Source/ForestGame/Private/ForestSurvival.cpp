#include "ForestSurvival.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestAnimal.h"
#include "ForestBlocks.h"
#include "ForestBuilding.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestSky.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** A hungry wild animal growls when the player comes this close... */
	constexpr float GrowlDistance = 650.0f;
	/** ...and bites this close, once the growl's warning has run out. */
	constexpr float BiteDistance = 270.0f;
	constexpr float WarningSeconds = 3.0f;
	constexpr float SecondsBetweenBites = 2.2f;
	/** Half a heart comes back this often, or much faster by a campfire. */
	constexpr float RegrowSeconds = 25.0f;
	constexpr float RegrowSecondsByFire = 4.0f;
	constexpr float CampfireWarmth = 450.0f;
	constexpr float FaintSeconds = 4.5f;

	/** How hard each species bites, in half hearts. */
	int32 BiteStrength(const AForestAnimal* Animal)
	{
		const FString Name = Animal->DisplayName.ToString();
		if (Name.Contains(TEXT("boar")) || Name.Contains(TEXT("fox")) || Name.Contains(TEXT("deer")))
		{
			return 2;
		}
		return 1;
	}

	FString Capitalised(FString Text)
	{
		if (!Text.IsEmpty())
		{
			Text[0] = FChar::ToUpper(Text[0]);
		}
		return Text;
	}

	/** The hearts in the bottom-left corner, the red flash of a bite, and the faint. */
	class SForestSurvivalOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestSurvivalOverlay) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestSurvival>, Survival)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			Survival = Args._Survival;
			TWeakObjectPtr<UForestSurvival> Weak = Survival;

			FSlateFontInfo HeartFont = FCoreStyle::GetDefaultFontStyle("Bold", 40);
			HeartFont.OutlineSettings.OutlineSize = 2;
			HeartFont.OutlineSettings.OutlineColor = FLinearColor(0.05f, 0.02f, 0.02f, 0.9f);

			TSharedRef<SHorizontalBox> Hearts = SNew(SHorizontalBox);
			for (int32 Index = 0; Index < UForestSurvival::MaxHealth / 2; ++Index)
			{
				Hearts->AddSlot()
					.AutoWidth()
					.Padding(FMargin(3.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("♥")))
						.Font(HeartFont)
						.ColorAndOpacity_Lambda([Weak, Index]()
						{
							const int32 Health = Weak.IsValid() ? Weak->GetHealth() : 0;
							if (Health >= (Index + 1) * 2)
							{
								return FSlateColor(FLinearColor(0.92f, 0.12f, 0.16f, 1.0f));
							}
							if (Health == Index * 2 + 1)
							{
								return FSlateColor(FLinearColor(0.95f, 0.55f, 0.55f, 0.85f));
							}
							return FSlateColor(FLinearColor(0.15f, 0.12f, 0.12f, 0.6f));
						})
					];
			}

			FSlateFontInfo FaintFont = FCoreStyle::GetDefaultFontStyle("Bold", 34);

			ChildSlot
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				// A bite turns the edges of the world red for a moment.
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([Weak]()
					{
						const float Flash = Weak.IsValid() ? Weak->GetFlash() : 0.0f;
						return FSlateColor(FLinearColor(0.75f, 0.02f, 0.02f, 0.4f * Flash));
					})
				]
				// Fainting: the screen goes dark and says what happened.
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([Weak]()
					{
						const float Dark = Weak.IsValid() ? Weak->GetFaintDarkness() : 0.0f;
						return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, Dark));
					})
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FaintFont)
						.Justification(ETextJustify::Center)
						.Text_Lambda([]()
						{
							return FText::FromString(ForestLanguage::Pick(TEXT("You fainted...\nThe forest is dangerous when its animals are hungry."),
								TEXT("Tu t'es évanoui...\nLa forêt est dangereuse quand ses animaux ont faim.")));
						})
						.ColorAndOpacity_Lambda([Weak]()
						{
							const float Dark = Weak.IsValid() ? Weak->GetFaintDarkness() : 0.0f;
							return FSlateColor(FLinearColor(0.96f, 0.9f, 0.8f, Dark));
						})
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(36.0f, 0.0f, 0.0f, 30.0f))
				[
					SNew(SBox)
					.Visibility_Lambda([Weak]()
					{
						return Weak.IsValid() && Weak->ShowsHearts() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					[
						Hearts
					]
				]
			];
		}

	private:
		TWeakObjectPtr<UForestSurvival> Survival;
	};
}

bool UForestSurvival::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UForestSurvival::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Health = MaxHealth;
}

void UForestSurvival::Deinitialize()
{
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

TStatId UForestSurvival::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestSurvival, STATGROUP_Tickables);
}

UForestSurvival* UForestSurvival::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestSurvival>() : nullptr;
}

void UForestSurvival::EnsureOverlay()
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
	Overlay = SNew(SForestSurvivalOverlay).Survival(this);
	// Above the game, below the title menu.
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 5);
	Viewport = Client;
}

float UForestSurvival::GetFaintDarkness() const
{
	if (!bFainted)
	{
		return 0.0f;
	}
	// Darkens in the first second, stays black, clears in the last second.
	const float Elapsed = FaintSeconds - FaintTime;
	return FMath::Clamp(FMath::Min(Elapsed, FaintTime), 0.0f, 1.0f);
}

void UForestSurvival::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

AForestAnimal* UForestSurvival::FindThreat(const FVector& From, float& OutDistance) const
{
	AForestAnimal* Best = nullptr;
	OutDistance = GrowlDistance;
	// The days are gentle: hungry animals only get dangerous after dark.
	const AForestSky* Sky = AForestSky::Get(GetWorld());
	if (Sky && !Sky->IsNight())
	{
		return nullptr;
	}
	// Nothing reaches the player inside their own home.
	if (const UForestBlocks* Blocks = UForestBlocks::Get(GetWorld()))
	{
		if (Blocks->IsHome(From))
		{
			return nullptr;
		}
	}
	for (TActorIterator<AForestAnimal> It(GetWorld()); It; ++It)
	{
		AForestAnimal* Animal = *It;
		if (Animal->IsTame() || !Animal->IsHungry())
		{
			continue;
		}
		// Busy animals leave the player alone.
		switch (Animal->GetAnimalState())
		{
		case EAnimalState::Sleeping:
		case EAnimalState::Eating:
		case EAnimalState::Happy:
		case EAnimalState::Dancing:
			continue;
		default:
			break;
		}
		const float Distance = FVector::Dist2D(From, Animal->GetActorLocation());
		if (Distance < OutDistance)
		{
			OutDistance = Distance;
			Best = Animal;
		}
	}
	return Best;
}

bool UForestSurvival::NearCampfire(const FVector& From) const
{
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		if (It->GetBuildingType() == TEXT("Campfire") && FVector::Dist2D(From, It->GetActorLocation()) < CampfireWarmth)
		{
			return true;
		}
	}
	return false;
}

void UForestSurvival::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	EnsureOverlay();
	Flash = FMath::Max(0.0f, Flash - DeltaSeconds * 1.8f);

	if (bFainted)
	{
		FaintTime -= DeltaSeconds;
		if (!bWokenUp && FaintTime <= 1.0f)
		{
			WakeUp();
		}
		if (FaintTime <= 0.0f)
		{
			bFainted = false;
		}
		return;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	ACharacter* Player = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	// Nothing hurts during the title screen and the wake-up intro.
	bActive = Player && !Controller->IsMoveInputIgnored();
	if (!bActive)
	{
		Threat.Reset();
		return;
	}

	BiteCooldown -= DeltaSeconds;
	const FVector From = Player->GetActorLocation();
	float Distance = 0.0f;
	AForestAnimal* Animal = FindThreat(From, Distance);
	if (Animal && Animal != Threat.Get())
	{
		// A new animal noticed the player: it growls first.
		Threat = Animal;
		Grace = WarningSeconds;
		Animal->Refuse();
		Say(FOREST_PRINTF(TEXT("%s is hungry and growls... Feed it or step back!"), TEXT("%s a faim et grogne... Nourris-le ou recule !"),
			*Capitalised(ForestLanguage::Animal(Animal->DisplayName.ToString()))), 3.0f);
	}
	else if (!Animal)
	{
		Threat.Reset();
	}

	if (Animal)
	{
		Grace -= DeltaSeconds;
		if (Distance < BiteDistance && Grace <= 0.0f && BiteCooldown <= 0.0f)
		{
			Bite(Animal);
		}
		RegrowTimer = 0.0f;
		return;
	}

	// Safe: hearts grow back, quickly by the fire.
	if (Health < MaxHealth)
	{
		RegrowTimer += DeltaSeconds;
		if (RegrowTimer >= (NearCampfire(From) ? RegrowSecondsByFire : RegrowSeconds))
		{
			RegrowTimer = 0.0f;
			Heal(1);
		}
	}
}

void UForestSurvival::Bite(AForestAnimal* Animal)
{
	BiteCooldown = SecondsBetweenBites;
	Animal->Refuse();
	const FString Name = Capitalised(ForestLanguage::Animal(Animal->DisplayName.ToString()));
	Hurt(BiteStrength(Animal), Animal);
	if (!bFainted)
	{
		Say(FOREST_PRINTF(TEXT("Ouch! %s bit you. Give it some food!"), TEXT("Aïe ! %s t'a mordu. Donne-lui à manger !"), *Name), 2.5f);
	}
}

void UForestSurvival::Hurt(int32 HalfHearts, AActor* Source)
{
	if (bFainted || HalfHearts <= 0)
	{
		return;
	}
	Health = FMath::Max(0, Health - HalfHearts);
	Flash = 1.0f;
	RegrowTimer = 0.0f;

	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (ACharacter* Player = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr)
	{
		// Knocked back, away from what bit.
		FVector Away = Source ? Player->GetActorLocation() - Source->GetActorLocation() : -Player->GetActorForwardVector();
		Away.Z = 0.0f;
		Player->LaunchCharacter(Away.GetSafeNormal() * 650.0f + FVector(0.0f, 0.0f, 280.0f), true, true);
	}
	UE_LOG(LogForestGame, Log, TEXT("Player hurt: %d / %d half hearts."), Health, MaxHealth);
	if (Health <= 0)
	{
		Faint();
	}
}

void UForestSurvival::Heal(int32 HalfHearts)
{
	Health = FMath::Clamp(Health + HalfHearts, 0, MaxHealth);
}

void UForestSurvival::Faint()
{
	bFainted = true;
	bWokenUp = false;
	FaintTime = FaintSeconds;
	Threat.Reset();
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (APawn* Player = Controller ? Controller->GetPawn() : nullptr)
	{
		Player->DisableInput(Controller);
	}
}

void UForestSurvival::WakeUp()
{
	bWokenUp = true;
	Health = MaxHealth;
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}

	// Wake up by the campfire if there is one, otherwise where the game began.
	FVector Spot = Player->GetActorLocation();
	bool bByFire = false;
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		if (It->GetBuildingType() == TEXT("Campfire"))
		{
			Spot = It->GetActorLocation() + FVector(180.0f, 0.0f, 120.0f);
			bByFire = true;
			break;
		}
	}
	if (!bByFire)
	{
		if (AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
		{
			Spot = Start->GetActorLocation();
		}
	}
	Player->TeleportTo(Spot, Player->GetActorRotation());
	Player->EnableInput(Controller);
	Say(bByFire
		? ForestLanguage::Pick(TEXT("You wake up by your campfire, rested."), TEXT("Tu te réveilles près de ton feu de camp, reposé."))
		: ForestLanguage::Pick(TEXT("You wake up where your journey began. Build a campfire to wake up there."), TEXT("Tu te réveilles là où ton voyage a commencé. Construis un feu de camp pour t'y réveiller.")),
		5.0f);
}
