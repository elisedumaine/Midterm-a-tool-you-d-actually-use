#include "ForestHUD.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "ForestAnimal.h"
#include "ForestCrafting.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestSky.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void AForestHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, Width, Height);
}

void AForestHUD::DrawLine(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, bool bCentered)
{
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Text, Width, Height, Font, Scale);
	DrawText(Text, Color, bCentered ? X - Width * 0.5f : X, Y, Font, Scale);
}

void AForestHUD::DrawHUD()
{
	Super::DrawHUD();
	const AForestGameManager* Manager = AForestGameManager::Get(this);
	if (!Manager || !Canvas)
	{
		return;
	}
	// Nothing over the title screen and the waking: the picture stays clean.
	if (PlayerOwner && PlayerOwner->IsMoveInputIgnored())
	{
		return;
	}

	const float UiScale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.7f, 2.0f);
	const FLinearColor Cream(0.98f, 0.94f, 0.84f, 1.0f);
	const FLinearColor Leaf(0.62f, 0.90f, 0.56f, 1.0f);
	const FLinearColor Dim(0.65f, 0.65f, 0.6f, 1.0f);
	const FLinearColor Shade(0.02f, 0.03f, 0.02f, 0.55f);
	const float Pad = 24.0f * UiScale;
	const float Row = 32.0f * UiScale;

	// Above the animals near the player: their mood and how much they trust you.
	DrawAnimals(Manager, UiScale);

	// Top left: the basket and the tame counter.
	const TArray<FName>& Items = ForestCrafting::Items();
	const float BasketHeight = (Items.Num() + (Manager->AnimalsTotal > 0 ? 1 : 0)) * Row + 26.0f * UiScale;
	DrawPanel(Pad, Pad, 330.0f * UiScale, BasketHeight, Shade);
	float Y = Pad * 1.45f;
	for (const FName Item : Items)
	{
		const int32 Count = Manager->Count(Item);
		// "2 mushrooms" without its number: the plural name.
		FString Name = ForestCrafting::ItemName(Item, 2).RightChop(2);
		Name[0] = FChar::ToUpper(Name[0]);
		DrawLine(FString::Printf(TEXT("%s: %d"), *Name, Count), Pad * 1.6f, Y, 1.0f * UiScale, Count > 0 ? Cream : Dim);
		Y += Row;
	}
	if (Manager->AnimalsTotal > 0)
	{
		DrawLine(FOREST_PRINTF(TEXT("Animals tamed: %d / %d"), TEXT("Animaux apprivoisés : %d / %d"), Manager->AnimalsTamed, Manager->AnimalsTotal),
			Pad * 1.6f, Y, 1.0f * UiScale, Manager->bWon ? Leaf : Cream);
	}

	// Left: the build menu.
	if (Manager->IsBuildMenuOpen())
	{
		const TArray<ForestCrafting::FRecipe>& Recipes = ForestCrafting::Recipes();
		const float MenuY = Pad + BasketHeight + 20.0f * UiScale;
		const float LineHeight = 58.0f * UiScale;
		DrawPanel(Pad, MenuY, 640.0f * UiScale, (Recipes.Num() + 1) * LineHeight + 20.0f * UiScale, Shade);
		DrawLine(ForestLanguage::Pick(TEXT("BUILD  —  press a number   [B] Close"), TEXT("CONSTRUIRE  —  appuie sur un chiffre   [B] Fermer")),
			Pad * 1.6f, MenuY + 14.0f * UiScale, 1.05f * UiScale, Leaf);
		for (int32 Index = 0; Index < Recipes.Num(); ++Index)
		{
			const ForestCrafting::FRecipe& Recipe = Recipes[Index];
			const bool bAfford = Manager->CanAfford(Recipe.Id);
			const float LineY = MenuY + (Index + 1) * LineHeight + 4.0f * UiScale;
			DrawLine(FString::Printf(TEXT("[%d]  %s   —   %s"), Index + 1, ForestLanguage::Pick(Recipe.English, Recipe.French), *ForestCrafting::CostText(Recipe)),
				Pad * 1.6f, LineY, 1.0f * UiScale, bAfford ? Cream : Dim);
			DrawLine(ForestLanguage::Pick(Recipe.EnglishHint, Recipe.FrenchHint), Pad * 1.6f + 40.0f * UiScale, LineY + 26.0f * UiScale, 0.75f * UiScale, Dim);
		}
	}

	// Bottom centre: what E would do.
	if (!Manager->Prompt.IsEmpty())
	{
		const float PromptY = Canvas->ClipY - 150.0f * UiScale;
		const FString Text = Manager->Prompt.ToString();
		float Width = 600.0f * UiScale;
		if (UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr)
		{
			float Height = 0.0f;
			GetTextSize(Text, Width, Height, Font, 1.15f * UiScale);
			Width = FMath::Max(600.0f * UiScale, Width + 60.0f * UiScale);
		}
		DrawPanel(Canvas->ClipX * 0.5f - Width * 0.5f, PromptY - 10.0f * UiScale, Width, 52.0f * UiScale, Shade);
		DrawLine(Text, Canvas->ClipX * 0.5f, PromptY, 1.15f * UiScale, Cream, true);
	}

	// Bottom right: the keys (H hides them).
	if (Manager->bShowHelp && !Manager->IsPlacing())
	{
		const TCHAR* Lines[] = {
			ForestLanguage::Pick(TEXT("[E] Pick up / feed / pet"), TEXT("[E] Ramasser / nourrir / caresser")),
			ForestLanguage::Pick(TEXT("[F] Tame animal: stay / follow"), TEXT("[F] Animal apprivoisé : reste / suis")),
			ForestLanguage::Pick(TEXT("[B] Build camp pieces"), TEXT("[B] Construire le camp")),
			ForestLanguage::Pick(TEXT("[G] Blocks   [T] Craft   [Z] Sleep at home"), TEXT("[G] Blocs   [T] Fabriquer   [Z] Dormir chez toi")),
			ForestLanguage::Pick(TEXT("[H] Hide this help   [F9] New camp"), TEXT("[H] Cacher l'aide   [F9] Nouveau camp")),
			ForestLanguage::Pick(TEXT("[P] Pause and settings"), TEXT("[P] Pause et options")),
		};
		const float HelpWidth = 420.0f * UiScale;
		const float HelpHeight = (UE_ARRAY_COUNT(Lines) * 26.0f + 18.0f) * UiScale;
		const float HelpX = Canvas->ClipX - Pad - HelpWidth;
		const float HelpY = Canvas->ClipY - Pad - HelpHeight;
		DrawPanel(HelpX, HelpY, HelpWidth, HelpHeight, Shade);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
		{
			DrawLine(Lines[Index], HelpX + 16.0f * UiScale, HelpY + (10.0f + Index * 26.0f) * UiScale, 0.8f * UiScale, Cream);
		}
	}

	// Top right: the day, the hour and the weather of the living sky.
	if (const AForestSky* Sky = AForestSky::Get(this))
	{
		const FString Clock = Sky->Describe();
		DrawPanel(Canvas->ClipX - Pad - 360.0f * UiScale, Pad, 360.0f * UiScale, 52.0f * UiScale, Shade);
		DrawLine(Clock, Canvas->ClipX - Pad - 180.0f * UiScale, Pad * 1.45f, 1.0f * UiScale, Cream, true);
	}

	// Upper centre: short-lived messages.
	if (Manager->MessageTimeLeft > 0.0f && !Manager->Message.IsEmpty())
	{
		const float Alpha = FMath::Clamp(Manager->MessageTimeLeft / 0.5f, 0.0f, 1.0f);
		DrawLine(Manager->Message.ToString(), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.18f, 1.3f * UiScale, FLinearColor(Leaf.R, Leaf.G, Leaf.B, Alpha), true);
	}
}

void AForestHUD::DrawAnimals(const AForestGameManager* Manager, float UiScale)
{
	const APawn* Player = GetOwningPawn();
	if (!Player)
	{
		return;
	}
	const FVector Eye = Player->GetActorLocation();
	for (const AForestAnimal* Animal : Manager->GetAnimals())
	{
		if (!Animal)
		{
			continue;
		}
		const float Distance = FVector::Dist(Eye, Animal->GetActorLocation());
		if (Distance > 2200.0f)
		{
			continue;
		}
		const float Top = Animal->Capsule ? Animal->Capsule->GetScaledCapsuleHalfHeight() + 35.0f : 100.0f;
		const FVector Screen = Project(Animal->GetActorLocation() + FVector(0.0f, 0.0f, Top), true);
		if (Screen.Z <= 0.0f)
		{
			continue;
		}
		// Fade out with distance so the forest is not covered in labels.
		const float Alpha = FMath::Clamp((2200.0f - Distance) / 600.0f, 0.0f, 1.0f);
		const float Scale = 0.8f * UiScale;

		// Trust: a little bar that fills up; green with a heart once tame.
		const float BarWidth = 70.0f * UiScale;
		const float BarHeight = 7.0f * UiScale;
		const float BarX = Screen.X - BarWidth * 0.5f;
		const float BarY = Screen.Y;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f * Alpha), BarX - 2.0f, BarY - 2.0f, BarWidth + 4.0f, BarHeight + 4.0f);
		const FLinearColor Fill = Animal->IsTame() ? FLinearColor(0.45f, 0.95f, 0.45f, Alpha) : FLinearColor(1.0f, 0.55f, 0.65f, Alpha);
		DrawRect(Fill, BarX, BarY, BarWidth * Animal->GetTrust(), BarHeight);

		const FString Mood = Animal->DescribeMood();
		if (!Mood.IsEmpty())
		{
			const FLinearColor Color = Animal->GetAnimalState() == EAnimalState::Wary ? FLinearColor(1.0f, 0.6f, 0.4f, Alpha)
				: Animal->IsHungry() && !Animal->IsTame() ? FLinearColor(1.0f, 0.85f, 0.45f, Alpha)
				: FLinearColor(0.95f, 0.95f, 0.9f, Alpha);
			DrawLine(Mood, Screen.X, BarY - 30.0f * UiScale, Scale, Color, true);
		}
	}
}
