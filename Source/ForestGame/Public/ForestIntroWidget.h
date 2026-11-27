#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForestIntroWidget.generated.h"

/**
 * The cinematic overlay of falling asleep and waking up: two eyelids that
 * close and open over the screen, a dreamy blur that clears as the eyes open,
 * and one line of text at a time that fades in and out.  The intro director
 * drives it every frame.
 */
UCLASS()
class FORESTGAME_API UForestIntroWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 0 = eyes shut (black screen), 1 = wide open. */
	void SetEyes(float Openness) { Eyes = FMath::Clamp(Openness, 0.0f, 1.0f); }
	/** How blurry the world looks (0 = sharp). */
	void SetBlur(float Strength) { Blur = FMath::Max(0.0f, Strength); }
	/** Fades the current line out and the new one in. */
	void ShowLine(const FString& Line);
	void HideLine();
	void ShowSkipHint(bool bShow) { bSkipHint = bShow; }

private:
	float LineAlpha() const;

	float Eyes = 0.0f;
	float Blur = 0.0f;
	FString Line;
	double LineChangedAt = -100.0;
	bool bLineVisible = false;
	bool bSkipHint = false;
};
