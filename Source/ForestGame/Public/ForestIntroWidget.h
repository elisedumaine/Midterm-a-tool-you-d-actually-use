#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForestIntroWidget.generated.h"

class STextBlock;

/** Minimal cinematic overlay used while the player wakes up. */
UCLASS()
class FORESTGAME_API UForestIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetNarrative(const FText& NewNarrative);

private:
	TSharedPtr<STextBlock> NarrativeText;
};
