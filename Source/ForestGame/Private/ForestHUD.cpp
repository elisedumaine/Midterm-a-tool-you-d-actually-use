#include "ForestHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "ForestGameManager.h"

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

	const float UiScale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.7f, 2.0f);
	const FLinearColor Cream(0.98f, 0.94f, 0.84f, 1.0f);
	const FLinearColor Leaf(0.62f, 0.90f, 0.56f, 1.0f);
	const FLinearColor Shade(0.02f, 0.03f, 0.02f, 0.55f);

	// Top left: the basket and the animal counter.
	const float Pad = 24.0f * UiScale;
	DrawPanel(Pad, Pad, 330.0f * UiScale, 92.0f * UiScale, Shade);
	DrawLine(FString::Printf(TEXT("Mushrooms: %d"), Manager->Mushrooms), Pad * 1.6f, Pad * 1.45f, 1.1f * UiScale, Cream);
	if (Manager->AnimalsTotal > 0)
	{
		DrawLine(FString::Printf(TEXT("Animals fed: %d / %d"), Manager->AnimalsFed, Manager->AnimalsTotal), Pad * 1.6f, Pad * 1.45f + 36.0f * UiScale, 1.1f * UiScale, Manager->bWon ? Leaf : Cream);
	}

	// Bottom centre: what E would do.
	if (!Manager->Prompt.IsEmpty())
	{
		const float PromptY = Canvas->ClipY - 150.0f * UiScale;
		DrawPanel(Canvas->ClipX * 0.5f - 300.0f * UiScale, PromptY - 10.0f * UiScale, 600.0f * UiScale, 52.0f * UiScale, Shade);
		DrawLine(Manager->Prompt.ToString(), Canvas->ClipX * 0.5f, PromptY, 1.15f * UiScale, Cream, true);
	}

	// Upper centre: short-lived messages.
	if (Manager->MessageTimeLeft > 0.0f && !Manager->Message.IsEmpty())
	{
		const float Alpha = FMath::Clamp(Manager->MessageTimeLeft / 0.5f, 0.0f, 1.0f);
		DrawLine(Manager->Message.ToString(), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.18f, 1.3f * UiScale, FLinearColor(Leaf.R, Leaf.G, Leaf.B, Alpha), true);
	}
}
