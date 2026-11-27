#include "ForestBlocks.h"

#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestBuilding.h"
#include "ForestCrafting.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestLife.h"
#include "ForestSky.h"
#include "ForestSurvival.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
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

namespace
{
	using ForestCrafting::FCost;

	enum EBlock : uint8 { Log, Plank, Stone, Leaf, Glow, Door };

	struct FBlockKind
	{
		const TCHAR* English;
		const TCHAR* French;
		FLinearColor Color;
		TArray<FCost> Cost;
	};

	const FBlockKind& Kind(int32 Type)
	{
		static const FBlockKind Kinds[UForestBlocks::NumTypes] = {
			{TEXT("Log"), TEXT("Rondin"), FLinearColor(0.20f, 0.10f, 0.04f), {{ForestCrafting::Wood, 1}}},
			{TEXT("Plank"), TEXT("Planche"), FLinearColor(0.55f, 0.36f, 0.16f), {{ForestCrafting::Plank, 1}}},
			{TEXT("Stone"), TEXT("Pierre"), FLinearColor(0.30f, 0.30f, 0.28f), {{ForestCrafting::Stone, 1}}},
			{TEXT("Leaves"), TEXT("Feuillage"), FLinearColor(0.10f, 0.28f, 0.05f), {{ForestCrafting::Fiber, 1}}},
			{TEXT("Glow"), TEXT("Lumière"), FLinearColor(1.0f, 0.68f, 0.28f), {{ForestCrafting::Resin, 1}, {ForestCrafting::Stone, 1}}},
			{TEXT("Door"), TEXT("Porte"), FLinearColor(0.30f, 0.16f, 0.06f), {{ForestCrafting::Plank, 2}}},
		};
		return Kinds[FMath::Clamp(Type, 0, UForestBlocks::NumTypes - 1)];
	}

	/** What the crafting panel (T) makes. */
	struct FCraftRecipe
	{
		const TCHAR* English;
		const TCHAR* French;
		TArray<FCost> Cost;
		bool bWorkbench;
		FName Out;
		int32 Amount;
	};

	const TArray<FCraftRecipe>& CraftRecipes()
	{
		static const TArray<FCraftRecipe> List = {
			{TEXT("4 planks"), TEXT("4 planches"), {{ForestCrafting::Wood, 2}}, true, ForestCrafting::Plank, 4},
			{TEXT("Stone axe (chops trees, finds resin)"), TEXT("Hache en pierre (coupe les arbres, trouve de la résine)"),
				{{ForestCrafting::Wood, 2}, {ForestCrafting::Stone, 2}, {ForestCrafting::Fiber, 2}}, true, TEXT("Axe"), 1},
			{TEXT("Spear"), TEXT("Lance"), {{ForestCrafting::Wood, 2}, {ForestCrafting::Stone, 1}, {ForestCrafting::Fiber, 2}}, false, TEXT("Spear"), 1},
			{TEXT("Torch"), TEXT("Torche"), {{ForestCrafting::Wood, 1}, {ForestCrafting::Resin, 1}}, false, TEXT("Torch"), 1},
			{TEXT("2 berry seeds"), TEXT("2 graines de baies"), {{ForestCrafting::Berry, 3}}, false, TEXT("BerrySeeds"), 2},
			{TEXT("Healing herbs (+1 heart)"), TEXT("Herbes qui soignent (+1 cœur)"), {{ForestCrafting::Fiber, 2}, {ForestCrafting::Mushroom, 1}}, false, TEXT("Heal"), 2},
		};
		return List;
	}

	FString CostText(const TArray<FCost>& Cost)
	{
		TArray<FString> Parts;
		for (const FCost& Part : Cost)
		{
			Parts.Add(ForestCrafting::ItemName(Part.Item, Part.Amount));
		}
		return FString::Join(Parts, TEXT(" + "));
	}

	bool Affords(const TArray<FCost>& Cost, const AForestGameManager* Manager)
	{
		if (!Manager)
		{
			return false;
		}
		for (const FCost& Part : Cost)
		{
			if (Manager->Count(Part.Item) < Part.Amount)
			{
				return false;
			}
		}
		return true;
	}

	constexpr float Reach = 650.0f;
	constexpr float WorkbenchReach = 450.0f;
	constexpr int32 MaxLights = 24;
	/** How far a home's walls and roof may be from the player, in blocks. */
	constexpr int32 HomeWalls = 8;
	constexpr int32 HomeRoof = 6;

	const FLinearColor Cream(0.97f, 0.95f, 0.88f, 1.0f);
	const FLinearColor Dim(0.62f, 0.62f, 0.58f, 1.0f);
	const FLinearColor Gold(0.98f, 0.80f, 0.42f, 1.0f);
	const FLinearColor Shade(0.02f, 0.03f, 0.02f, 0.6f);

	/** The block bar, the crafting panel, the home badge and the dark of sleep. */
	class SForestBlocksOverlay : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SForestBlocksOverlay) {}
			SLATE_ARGUMENT(TWeakObjectPtr<UForestBlocks>, Blocks)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args)
		{
			TWeakObjectPtr<UForestBlocks> Weak = Args._Blocks;

			TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
			for (int32 Type = 0; Type < UForestBlocks::NumTypes; ++Type)
			{
				Bar->AddSlot()
					.AutoWidth()
					.Padding(4.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(118.0f)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor_Lambda([Weak, Type]()
							{
								const bool bChosen = Weak.IsValid() && Weak->GetSelected() == Type;
								return FSlateColor(bChosen ? FLinearColor(0.55f, 0.40f, 0.12f, 0.85f) : Shade);
							})
							.Padding(FMargin(8.0f, 6.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.Text_Lambda([Weak, Type]() { return FText::FromString(Weak.IsValid() ? FString::Printf(TEXT("%d  %s"), Type + 1, *Weak->BlockName(Type)) : FString()); })
									.ColorAndOpacity_Lambda([Weak, Type]() { return FSlateColor(Weak.IsValid() && Weak->CanAffordBlock(Type) ? Cream : Dim); })
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.AutoWrapText(true)
									.Text_Lambda([Weak, Type]() { return FText::FromString(Weak.IsValid() ? Weak->BlockCostText(Type) : FString()); })
									.ColorAndOpacity(Dim)
								]
							]
						]
					];
			}

			TSharedRef<SVerticalBox> Recipes = SNew(SVerticalBox);
			Recipes->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(Gold)
					.Text_Lambda([Weak]()
					{
						const bool bBench = Weak.IsValid() && Weak->IsNearWorkbench();
						return FText::FromString(bBench
							? ForestLanguage::Pick(TEXT("CRAFT at the workbench  —  press a number   [T] Close"), TEXT("FABRIQUER à l'établi  —  appuie sur un chiffre   [T] Fermer"))
							: ForestLanguage::Pick(TEXT("CRAFT  —  press a number   [T] Close   (no workbench near)"), TEXT("FABRIQUER  —  appuie sur un chiffre   [T] Fermer   (pas d'établi ici)")));
					})
				];
			const int32 Count = Weak.IsValid() ? Weak->NumRecipes() : 0;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Recipes->AddSlot()
					.AutoHeight()
					.Padding(0.0f, 3.0f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
						.Text_Lambda([Weak, Index]() { return FText::FromString(Weak.IsValid() ? Weak->RecipeText(Index) : FString()); })
						.ColorAndOpacity_Lambda([Weak, Index]() { return FSlateColor(Weak.IsValid() && Weak->CanCraft(Index) ? Cream : Dim); })
					];
			}

			ChildSlot
			[
				SNew(SOverlay)
				.Visibility(EVisibility::HitTestInvisible)
				// Sleep: the screen goes dark while the sky rushes to dawn.
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([Weak]() { return FSlateColor(FLinearColor(0.0f, 0.0f, 0.02f, Weak.IsValid() ? Weak->GetSleepDarkness() : 0.0f)); })
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->GetSleepDarkness() > 0.01f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				]
				// Top centre, under the date: at home.
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.0f, 96.0f, 0.0f, 0.0f))
				[
					SNew(SBorder)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsHud() && Weak->IsPlayerHome() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Shade)
					.Padding(FMargin(18.0f, 6.0f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.ColorAndOpacity(Gold)
						.Text_Lambda([]()
						{
							return FText::FromString(ForestLanguage::Pick(TEXT("At home  —  the night cannot reach you   [Z] Sleep"),
								TEXT("Chez toi  —  la nuit ne peut pas t'atteindre   [Z] Dormir")));
						})
					]
				]
				// Bottom centre: the blocks.
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 210.0f))
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsHud() && Weak->IsBlockMode() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						.ColorAndOpacity(Gold)
						.ShadowOffset(FVector2D(0.0f, 1.5f))
						.Text_Lambda([]()
						{
							return FText::FromString(ForestLanguage::Pick(TEXT("BLOCKS   Left click: place   Right click: take back   Wheel / 1-6: choose   [G] Done"),
								TEXT("BLOCS   Clic gauche : poser   Clic droit : reprendre   Molette / 1-6 : choisir   [G] Fini")));
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						Bar
					]
				]
				// Left: the crafting panel.
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(24.0f, 120.0f, 0.0f, 0.0f))
				[
					SNew(SBorder)
					.Visibility_Lambda([Weak]() { return Weak.IsValid() && Weak->ShowsHud() && Weak->IsCraftMenuOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Shade)
					.Padding(FMargin(20.0f, 14.0f))
					[
						Recipes
					]
				]
			];
		}
	};
}

// --- Life of the subsystem ------------------------------------------------------

bool UForestBlocks::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UForestBlocks::Deinitialize()
{
	if (bDirty)
	{
		SaveBlocks();
	}
	if (Overlay.IsValid() && Viewport.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	}
	Overlay.Reset();
	Super::Deinitialize();
}

TStatId UForestBlocks::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestBlocks, STATGROUP_Tickables);
}

UForestBlocks* UForestBlocks::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestBlocks>() : nullptr;
}

FIntVector UForestBlocks::CellOf(const FVector& Where)
{
	return FIntVector(FMath::FloorToInt(Where.X / CellSize), FMath::FloorToInt(Where.Y / CellSize), FMath::FloorToInt(Where.Z / CellSize));
}

FVector UForestBlocks::CenterOf(const FIntVector& Cell)
{
	return (FVector(Cell) + FVector(0.5f)) * CellSize;
}

bool UForestBlocks::ShowsHud() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return bStarted && Controller && !Controller->IsMoveInputIgnored();
}

void UForestBlocks::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

void UForestBlocks::Start()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint"));
	UMaterialInterface* GlowMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow"));
	for (int32 Type = 0; Type < NumTypes; ++Type)
	{
		UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(Holder);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetupAttachment(Root);
		// Doors let the player through; everything else is solid to stand and climb on.
		Mesh->SetCollisionProfileName(Type == Door ? TEXT("NoCollision") : TEXT("BlockAll"));
		Mesh->RegisterComponent();
		UMaterialInterface* Base = Type == Glow ? GlowMaterial : Tint;
		if (Base)
		{
			UMaterialInstanceDynamic* Look = UMaterialInstanceDynamic::Create(Base, Holder);
			Look->SetVectorParameterValue(TEXT("Tint"), Kind(Type).Color);
			Look->SetScalarParameterValue(TEXT("Sway"), 0.0f);
			if (Type == Glow)
			{
				Look->SetScalarParameterValue(TEXT("Glow"), 3.0f);
			}
			Mesh->SetMaterial(0, Look);
		}
		Meshes.Add(Mesh);
	}

	Ghost = NewObject<UStaticMeshComponent>(Holder);
	Ghost->SetStaticMesh(Cube);
	Ghost->SetMobility(EComponentMobility::Movable);
	Ghost->SetupAttachment(Root);
	Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ghost->SetCastShadow(false);
	Ghost->SetVisibility(false);
	Ghost->SetRelativeScale3D(FVector(1.02f));
	Ghost->RegisterComponent();
	if (GlowMaterial)
	{
		GhostMaterial = UMaterialInstanceDynamic::Create(GlowMaterial, Holder);
		GhostMaterial->SetScalarParameterValue(TEXT("Glow"), 0.6f);
		Ghost->SetMaterial(0, GhostMaterial);
	}

	LoadBlocks();
	Rebuild();
	bStarted = true;
	UE_LOG(LogForestGame, Log, TEXT("Blocks: %d blocks loaded."), Blocks.Num());
}

void UForestBlocks::EnsureOverlay()
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
	Overlay = SNew(SForestBlocksOverlay).Blocks(this);
	Client->AddViewportWidgetContent(Overlay.ToSharedRef(), 6);
	Viewport = Client;
}

void UForestBlocks::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}
	if (!bStarted)
	{
		Start();
	}
	EnsureOverlay();
	UpdateSleep(DeltaSeconds, Controller);

	if (bDirty)
	{
		SaveTimer += DeltaSeconds;
		if (SaveTimer > 2.0f)
		{
			SaveBlocks();
		}
	}

	// A few times a second: is the player at home, is a workbench near?
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.0f)
	{
		CheckTimer = 0.25f;
		bPlayerHome = IsHome(Player->GetActorLocation());
		bNearWorkbench = NearWorkbench(Player->GetActorLocation());
	}

	// The title screen, the wake-up and sleep hold the player still: no keys.
	if (Controller->IsMoveInputIgnored())
	{
		Ghost->SetVisibility(false);
		return;
	}
	// The build menu (B) owns the keys while it is open.
	const AForestGameManager* Manager = AForestGameManager::Get(World);
	if (Manager && (Manager->IsBuildMenuOpen() || Manager->IsPlacing()))
	{
		bBlockMode = false;
		bCraftMenu = false;
	}
	ReadKeys(Controller, Player);
	UpdateTarget(Controller, Player);
}

// --- Keys -------------------------------------------------------------------------

void UForestBlocks::ReadKeys(APlayerController* Controller, APawn* Player)
{
	auto Pressed = [Controller](const FKey& Key) { return Controller->WasInputKeyJustPressed(Key); };
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	auto CloseBuildMenu = [Manager]()
	{
		if (Manager && (Manager->IsBuildMenuOpen() || Manager->IsPlacing()))
		{
			Manager->ToggleBuildMenu();
		}
	};

	if (Pressed(EKeys::F9))
	{
		ResetBlocks();
		return;
	}
	if (Pressed(EKeys::G))
	{
		bBlockMode = !bBlockMode;
		bCraftMenu = false;
		CloseBuildMenu();
		if (bBlockMode && Manager && Blocks.IsEmpty())
		{
			Say(ForestLanguage::Pick(TEXT("Build anything, block by block. Walls on four sides and a roof make a home: the night beasts cannot get in."),
				TEXT("Construis ce que tu veux, bloc par bloc. Quatre murs et un toit font une maison : les bêtes de la nuit ne peuvent pas entrer.")), 6.0f);
		}
		return;
	}
	if (Pressed(EKeys::T))
	{
		bCraftMenu = !bCraftMenu;
		bBlockMode = false;
		CloseBuildMenu();
		return;
	}
	if (Pressed(EKeys::Z))
	{
		Sleep(Controller, Player);
		return;
	}

	static const FKey Numbers[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	if (bCraftMenu)
	{
		for (int32 Index = 0; Index < NumRecipes() && Index < UE_ARRAY_COUNT(Numbers); ++Index)
		{
			if (Pressed(Numbers[Index]))
			{
				Craft(Index);
			}
		}
		if (Pressed(EKeys::RightMouseButton))
		{
			bCraftMenu = false;
		}
		return;
	}
	if (!bBlockMode)
	{
		return;
	}
	for (int32 Index = 0; Index < NumTypes; ++Index)
	{
		if (Pressed(Numbers[Index]))
		{
			Selected = Index;
		}
	}
	if (Pressed(EKeys::MouseScrollUp))
	{
		Selected = (Selected + NumTypes - 1) % NumTypes;
	}
	if (Pressed(EKeys::MouseScrollDown))
	{
		Selected = (Selected + 1) % NumTypes;
	}
	if (Pressed(EKeys::LeftMouseButton))
	{
		Place(Player);
	}
	if (Pressed(EKeys::RightMouseButton))
	{
		Break();
	}
}

// --- Aiming, placing, taking back ----------------------------------------------------

void UForestBlocks::UpdateTarget(APlayerController* Controller, APawn* Player)
{
	bHasTarget = false;
	bHasBreak = false;
	if (!bBlockMode)
	{
		Ghost->SetVisibility(false);
		return;
	}

	FVector Eye;
	FRotator View;
	Controller->GetPlayerViewPoint(Eye, View);
	// The camera is behind the player: aim from the camera, but only reach a few metres past the player.
	const float CameraToPlayer = FVector::Dist(Eye, Player->GetActorLocation());
	const FVector End = Eye + View.Vector() * (CameraToPlayer + Reach);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestBlocks), false, Player);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params))
	{
		Ghost->SetVisibility(false);
		return;
	}

	const bool bOnBlock = Hit.GetActor() == Holder;
	if (bOnBlock)
	{
		BreakCell = CellOf(Hit.ImpactPoint - Hit.ImpactNormal * (CellSize * 0.25f));
		bHasBreak = HasBlock(BreakCell);
	}
	TargetCell = CellOf(Hit.ImpactPoint + Hit.ImpactNormal * (CellSize * 0.5f));
	bHasTarget = FVector::Dist(CenterOf(TargetCell), Player->GetActorLocation()) < Reach;

	// Never inside the player.
	bool bInsidePlayer = false;
	if (const ACharacter* Character = Cast<ACharacter>(Player))
	{
		const FBox Body = Character->GetCapsuleComponent()->Bounds.GetBox().ExpandBy(-4.0f);
		const FBox Block = FBox::BuildAABB(CenterOf(TargetCell), FVector(CellSize * 0.5f));
		bInsidePlayer = Body.Intersect(Block);
	}
	bTargetValid = bHasTarget && !HasBlock(TargetCell) && !bInsidePlayer && CanAffordBlock(Selected);

	Ghost->SetVisibility(bHasTarget);
	Ghost->SetWorldLocation(CenterOf(TargetCell));
	if (GhostMaterial)
	{
		GhostMaterial->SetVectorParameterValue(TEXT("Tint"), bTargetValid ? FLinearColor(0.25f, 1.0f, 0.45f) : FLinearColor(1.0f, 0.18f, 0.12f));
	}
}

void UForestBlocks::Place(APawn* Player)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager || !bHasTarget)
	{
		return;
	}
	if (!CanAffordBlock(Selected))
	{
		const bool bNeedsPlanks = Selected == Plank || Selected == Door;
		Say(ForestLanguage::Format(ForestLanguage::Pick(TEXT("You need %s.%s"), TEXT("Il te faut %s.%s")), *BlockCostText(Selected),
			bNeedsPlanks ? ForestLanguage::Pick(TEXT(" Cut planks at a workbench (T)."), TEXT(" Fais des planches à l'établi (T).")) : TEXT("")), 3.0f);
		return;
	}
	if (!bTargetValid)
	{
		return;
	}
	for (const FCost& Part : Kind(Selected).Cost)
	{
		Manager->AddItem(Part.Item, -Part.Amount);
	}
	Blocks.Add(TargetCell, uint8(Selected));
	Rebuild();
	bDirty = true;
	SaveTimer = 0.0f;
}

void UForestBlocks::Break()
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager || !bHasBreak)
	{
		return;
	}
	const uint8 Type = Blocks.FindAndRemoveChecked(BreakCell);
	for (const FCost& Part : Kind(Type).Cost)
	{
		Manager->AddItem(Part.Item, Part.Amount);
	}
	Rebuild();
	bDirty = true;
	SaveTimer = 0.0f;
}

void UForestBlocks::Rebuild()
{
	TArray<TArray<FTransform>> PerType;
	PerType.SetNum(NumTypes);
	GlowSpots.Reset();
	RoofSpots.Reset();
	for (const TPair<FIntVector, uint8>& Block : Blocks)
	{
		const FIntVector& Cell = Block.Key;
		const int32 Type = FMath::Clamp(int32(Block.Value), 0, NumTypes - 1);
		// A hair smaller than the cell, so the seams between blocks show.
		FVector Scale(0.985f);
		if (Type == Door)
		{
			// Thin, across the wall it stands in.
			const bool bWallAlongX = HasBlock(Cell + FIntVector(1, 0, 0)) || HasBlock(Cell - FIntVector(1, 0, 0));
			Scale = bWallAlongX ? FVector(0.985f, 0.22f, 0.985f) : FVector(0.22f, 0.985f, 0.985f);
		}
		PerType[Type].Add(FTransform(FRotator::ZeroRotator, CenterOf(Cell), Scale));
		if (Type == Glow)
		{
			GlowSpots.Add(CenterOf(Cell));
		}
		// Two free cells under a block: room for an animal to stand dry.
		const FIntVector Below = Cell - FIntVector(0, 0, 1);
		if (Type != Door && !HasBlock(Below) && !HasBlock(Below - FIntVector(0, 0, 1)))
		{
			RoofSpots.Add(CenterOf(Below));
		}
	}
	for (int32 Type = 0; Type < NumTypes; ++Type)
	{
		Meshes[Type]->ClearInstances();
		if (PerType[Type].Num() > 0)
		{
			Meshes[Type]->AddInstances(PerType[Type], false);
		}
	}

	// A warm light in the first glow blocks (lights are costly; the beasts fear all of them anyway).
	for (UPointLightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->DestroyComponent();
		}
	}
	Lights.Reset();
	for (int32 Index = 0; Index < GlowSpots.Num() && Index < MaxLights; ++Index)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetupAttachment(Holder->GetRootComponent());
		Light->SetRelativeLocation(GlowSpots[Index]);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(25.0f);
		Light->SetLightColor(FLinearColor(1.0f, 0.7f, 0.35f));
		Light->SetAttenuationRadius(900.0f);
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		Lights.Add(Light);
	}
}

// --- Home ---------------------------------------------------------------------------

bool UForestBlocks::IsHome(const FVector& Where) const
{
	if (Blocks.IsEmpty())
	{
		return false;
	}
	const FIntVector Cell = CellOf(Where);
	bool bRoof = false;
	for (int32 Up = 1; Up <= HomeRoof && !bRoof; ++Up)
	{
		bRoof = HasBlock(Cell + FIntVector(0, 0, Up));
	}
	if (!bRoof)
	{
		return false;
	}
	static const FIntVector Sides[] = {FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0)};
	for (const FIntVector& Side : Sides)
	{
		bool bWall = false;
		for (int32 Step = 1; Step <= HomeWalls && !bWall; ++Step)
		{
			// At the height of the body or the head (the ground is not flat).
			const FIntVector At = Cell + Side * Step;
			bWall = HasBlock(At) || HasBlock(At + FIntVector(0, 0, 1)) || HasBlock(At - FIntVector(0, 0, 1));
		}
		if (!bWall)
		{
			return false;
		}
	}
	return true;
}

bool UForestBlocks::FindRoofSpot(const FVector& From, float Range, FVector& OutSpot) const
{
	float Best = Range;
	bool bFound = false;
	for (const FVector& Spot : RoofSpots)
	{
		const float Distance = FVector::Dist2D(From, Spot);
		if (Distance < Best)
		{
			Best = Distance;
			OutSpot = Spot;
			bFound = true;
		}
	}
	return bFound;
}

// --- Sleep ----------------------------------------------------------------------------

void UForestBlocks::Sleep(APlayerController* Controller, APawn* Player)
{
	AForestSky* Sky = AForestSky::Get(GetWorld());
	if (!Sky || bSleeping)
	{
		return;
	}
	if (!Sky->IsNight() && Sky->GetHour() < 19.5f)
	{
		Say(ForestLanguage::Pick(TEXT("You are not tired yet. Sleep comes at night."), TEXT("Tu n'as pas encore sommeil. Le sommeil vient la nuit.")), 2.5f);
		return;
	}
	if (!bPlayerHome)
	{
		Say(ForestLanguage::Pick(TEXT("Too dangerous out here. Build a home first: walls on four sides and a roof (G)."),
			TEXT("Trop dangereux ici. Construis d'abord une maison : quatre murs et un toit (G).")), 4.0f);
		return;
	}
	bSleeping = true;
	bBlockMode = false;
	bCraftMenu = false;
	DayMinutesBeforeSleep = Sky->DayMinutes;
	// A whole day in half a minute: the night rushes past in about ten seconds.
	Sky->DayMinutes = 0.5f;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
	Say(ForestLanguage::Pick(TEXT("You fall asleep, safe in your home..."), TEXT("Tu t'endors, à l'abri chez toi...")), 3.0f);
}

void UForestBlocks::UpdateSleep(float DeltaSeconds, APlayerController* Controller)
{
	const float Target = bSleeping ? 0.92f : 0.0f;
	SleepDarkness = FMath::FInterpConstantTo(SleepDarkness, Target, DeltaSeconds, 1.5f);
	if (!bSleeping)
	{
		return;
	}
	AForestSky* Sky = AForestSky::Get(GetWorld());
	const float Hour = Sky ? Sky->GetHour() : 7.0f;
	if (!Sky || (Hour > 6.5f && Hour < 12.0f))
	{
		bSleeping = false;
		if (Sky)
		{
			Sky->DayMinutes = DayMinutesBeforeSleep;
		}
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
		if (UForestLife* Life = UForestLife::Get(GetWorld()))
		{
			Life->Log(ForestLanguage::Pick(TEXT("Slept through the night in my own home."), TEXT("J'ai dormi toute la nuit dans ma maison.")));
		}
		Say(ForestLanguage::Pick(TEXT("Good morning. The beasts of the night are gone."), TEXT("Bonjour. Les bêtes de la nuit sont parties.")), 3.5f);
	}
}

// --- Crafting -------------------------------------------------------------------------

int32 UForestBlocks::NumRecipes() const
{
	return CraftRecipes().Num();
}

FString UForestBlocks::RecipeText(int32 Index) const
{
	if (!CraftRecipes().IsValidIndex(Index))
	{
		return FString();
	}
	const FCraftRecipe& Recipe = CraftRecipes()[Index];
	FString Text = FString::Printf(TEXT("[%d]  %s   —   %s"), Index + 1, ForestLanguage::Pick(Recipe.English, Recipe.French), *CostText(Recipe.Cost));
	if (Recipe.bWorkbench)
	{
		Text += ForestLanguage::Pick(TEXT("   (workbench)"), TEXT("   (établi)"));
	}
	return Text;
}

bool UForestBlocks::CanCraft(int32 Index) const
{
	if (!CraftRecipes().IsValidIndex(Index))
	{
		return false;
	}
	const FCraftRecipe& Recipe = CraftRecipes()[Index];
	return (!Recipe.bWorkbench || bNearWorkbench) && Affords(Recipe.Cost, AForestGameManager::Get(GetWorld()));
}

void UForestBlocks::Craft(int32 Index)
{
	AForestGameManager* Manager = AForestGameManager::Get(GetWorld());
	if (!Manager || !CraftRecipes().IsValidIndex(Index))
	{
		return;
	}
	const FCraftRecipe& Recipe = CraftRecipes()[Index];
	if (Recipe.bWorkbench && !bNearWorkbench)
	{
		Say(ForestLanguage::Pick(TEXT("This needs a workbench. Build one with B."), TEXT("Il faut un établi. Construis-en un avec B.")), 3.0f);
		return;
	}
	if (!Affords(Recipe.Cost, Manager))
	{
		Say(ForestLanguage::Format(ForestLanguage::Pick(TEXT("You need %s."), TEXT("Il te faut %s.")), *CostText(Recipe.Cost)), 3.0f);
		return;
	}
	for (const FCost& Part : Recipe.Cost)
	{
		Manager->AddItem(Part.Item, -Part.Amount);
	}
	if (Recipe.Out == TEXT("Heal"))
	{
		if (UForestSurvival* Survival = UForestSurvival::Get(GetWorld()))
		{
			Survival->Heal(Recipe.Amount);
		}
	}
	else if (UForestLife* Life = UForestLife::Get(GetWorld()))
	{
		// Tools go to the hand, seeds and planks to the basket.
		Life->Give(Recipe.Out, Recipe.Amount);
	}
	else
	{
		Manager->AddItem(Recipe.Out, Recipe.Amount);
	}
	Say(ForestLanguage::Format(ForestLanguage::Pick(TEXT("Made: %s"), TEXT("Fabriqué : %s")), ForestLanguage::Pick(Recipe.English, Recipe.French)), 2.5f);
}

bool UForestBlocks::NearWorkbench(const FVector& From) const
{
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		if (It->GetBuildingType() == TEXT("Workbench") && FVector::Dist(From, It->GetActorLocation()) < WorkbenchReach)
		{
			return true;
		}
	}
	return false;
}

// --- What the overlay shows ------------------------------------------------------------

FString UForestBlocks::BlockName(int32 Type) const
{
	return ForestLanguage::Pick(Kind(Type).English, Kind(Type).French);
}

FString UForestBlocks::BlockCostText(int32 Type) const
{
	return CostText(Kind(Type).Cost);
}

bool UForestBlocks::CanAffordBlock(int32 Type) const
{
	return Affords(Kind(Type).Cost, AForestGameManager::Get(GetWorld()));
}

// --- Saving -----------------------------------------------------------------------------

namespace
{
	FString SlotFor(const UObject* WorldContext)
	{
		return TEXT("ForestBlocks_") + UGameplayStatics::GetCurrentLevelName(WorldContext, true);
	}
}

void UForestBlocks::SaveBlocks()
{
	bDirty = false;
	SaveTimer = 0.0f;
	UForestBlocksSave* Save = Cast<UForestBlocksSave>(UGameplayStatics::CreateSaveGameObject(UForestBlocksSave::StaticClass()));
	for (const TPair<FIntVector, uint8>& Block : Blocks)
	{
		Save->Cells.Add(Block.Key);
		Save->Types.Add(Block.Value);
	}
	UGameplayStatics::SaveGameToSlot(Save, SlotFor(GetWorld()), 0);
}

void UForestBlocks::LoadBlocks()
{
	Blocks.Reset();
	const FString Slot = SlotFor(GetWorld());
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		return;
	}
	if (const UForestBlocksSave* Save = Cast<UForestBlocksSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
	{
		for (int32 Index = 0; Index < Save->Cells.Num() && Index < Save->Types.Num(); ++Index)
		{
			Blocks.Add(Save->Cells[Index], Save->Types[Index]);
		}
	}
}

void UForestBlocks::ResetBlocks()
{
	Blocks.Reset();
	bBlockMode = false;
	bCraftMenu = false;
	UGameplayStatics::DeleteGameInSlot(SlotFor(GetWorld()), 0);
	bDirty = false;
	Rebuild();
}

// --- For tests and the presentation ----------------------------------------------------

namespace
{
	/** Forest.House: builds a small cabin around the player. */
	FAutoConsoleCommandWithWorldAndArgs GHouseCommand(
		TEXT("Forest.House"),
		TEXT("Build a small log cabin around the player (a home: the night beasts cannot get in)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UForestBlocks* Blocks = UForestBlocks::Get(World))
			{
				Blocks->BuildDemoHouse();
			}
		}));
}

void UForestBlocks::BuildDemoHouse()
{
	UWorld* World = GetWorld();
	const APawn* Player = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Player || !bStarted)
	{
		return;
	}
	// The cell the player's feet stand in; the cabin is 5 x 5 with the player in the middle.
	const FIntVector Feet = CellOf(Player->GetActorLocation() - FVector(0.0f, 0.0f, 90.0f));
	for (int32 X = -2; X <= 2; ++X)
	{
		for (int32 Y = -2; Y <= 2; ++Y)
		{
			const bool bEdge = FMath::Abs(X) == 2 || FMath::Abs(Y) == 2;
			if (bEdge)
			{
				// Walls from just under the ground (it is not flat) up to three blocks high.
				for (int32 Z = -1; Z <= 2; ++Z)
				{
					const bool bDoor = Y == -2 && X == 0 && (Z == 0 || Z == 1);
					const bool bCorner = FMath::Abs(X) == 2 && FMath::Abs(Y) == 2;
					Blocks.Add(Feet + FIntVector(X, Y, Z), uint8(bDoor ? Door : bCorner ? Log : Z == -1 ? Stone : Log));
				}
			}
			Blocks.Add(Feet + FIntVector(X, Y, 3), uint8(Plank));
		}
	}
	// A glow block on the roof: the beasts see the light from afar and keep away.
	Blocks.Add(Feet + FIntVector(0, 0, 4), uint8(Glow));
	Rebuild();
	bDirty = true;
	SaveTimer = 0.0f;
	CheckTimer = 0.0f;
}

void UForestBlocks::SetBlockMode(bool bOn)
{
	bBlockMode = bOn;
	bCraftMenu = false;
}

void UForestBlocks::PlaceAimed()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (Controller && Controller->GetPawn())
	{
		UpdateTarget(Controller, Controller->GetPawn());
		Place(Controller->GetPawn());
	}
}

bool UForestBlocks::TrySleep()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || !Controller->GetPawn())
	{
		return false;
	}
	bPlayerHome = IsHome(Controller->GetPawn()->GetActorLocation());
	Sleep(Controller, Controller->GetPawn());
	return bSleeping;
}
