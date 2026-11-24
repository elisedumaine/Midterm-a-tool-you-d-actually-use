#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "ForestStartMenu.generated.h"

/** Native, asset-free title screen for the forest adventure: Play, and English / Français. */
UCLASS()
class FORESTGAME_API UForestStartMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply HandlePlayClicked();
	TSharedRef<SWidget> LanguageButton(bool bFrench, const TCHAR* Label);
};
