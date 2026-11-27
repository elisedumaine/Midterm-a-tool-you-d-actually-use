#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "ForestLanguage.h"
#include "ForestStartMenu.generated.h"

/**
 * Native, asset-free title screen.  It is see-through: the forest stays visible
 * behind it, filmed from a slow camera above the trees at sunset (the intro
 * director moves that camera).  Only a soft shade at the bottom keeps the
 * text readable.  Play, and English / Français / Español / Italiano.
 */
UCLASS()
class FORESTGAME_API UForestStartMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FReply HandlePlayClicked();
	TSharedRef<SWidget> LanguageButton(EForestLanguage Language);
	/** 0 when the menu appears, 1 two seconds later: everything fades in. */
	float Appear(float Delay = 0.0f) const;

	double ShownAt = 0.0;
};
