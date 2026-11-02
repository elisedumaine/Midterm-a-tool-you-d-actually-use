#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ForestHUD.generated.h"

/** Basket and animal counters, the interaction prompt, and short messages. */
UCLASS()
class FORESTGAME_API AForestHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void DrawLine(const FString& Text, float X, float Y, float Scale, const FLinearColor& Color, bool bCentered = false);
};
