#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestIntroDirector.generated.h"

class ACharacter;
class APlayerController;
class UForestIntroWidget;

/**
 * Places the player in a wooded clearing and plays the wake-up beat: grounded,
 * asleep, sitting up, then returning control.  It is spawned only after Play.
 */
UCLASS(NotBlueprintable)
class FORESTGAME_API AForestIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	AForestIntroDirector();
	void StartIntro(APlayerController* InPlayerController);
	virtual void Tick(float DeltaSeconds) override;

private:
	void PlacePlayerInForest();
	void SetMorningLight() const;
	void FinishIntro();

	TObjectPtr<APlayerController> PlayerController;
	TObjectPtr<ACharacter> PlayerCharacter;
	TObjectPtr<UForestIntroWidget> IntroWidget;
	FVector OriginalMeshLocation = FVector::ZeroVector;
	FRotator OriginalMeshRotation = FRotator::ZeroRotator;
	float IntroTime = 0.0f;
	bool bStarted = false;
	bool bStanding = false;
	bool bMorningFadeStarted = false;
};
