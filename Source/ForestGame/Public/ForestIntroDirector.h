#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestIntroDirector.generated.h"

class ACameraActor;
class ACharacter;
class APlayerController;
class UForestIntroWidget;
class UForestWakeUp;

/**
 * The opening of the game, filmed like a short poem.
 *
 * Title: behind the see-through title screen, a camera circles slowly above
 * the forest at sunset.
 * Falling asleep (PLAY): the eyelids close over the sunset and the night
 * passes in the dark.
 * Waking up: the rain has stopped, it is dawn.  Through closed eyes, a few
 * words; the eyes flutter, half open on the treetops and the sky, close
 * again, then open for good while the blur clears.  The camera comes down to
 * the grass, the player slowly sits up, stands, and the camera settles
 * behind them.  Space skips it.
 */
UCLASS(NotBlueprintable)
class FORESTGAME_API AForestIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	AForestIntroDirector();

	/** The title screen's camera above the forest. */
	void StartTitle(APlayerController* InPlayerController);
	/** The character page: the camera turns slowly in front of the player. */
	void ShowPortrait();
	/** The world page: back to the camera above the forest. */
	void ShowForest();
	/** PLAY: fall asleep, then wake up. */
	void StartIntro(APlayerController* InPlayerController);
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class EPhase : uint8 { None, Title, Portrait, Asleep, Waking };

	void TickTitle(float DeltaSeconds);
	void TickPortrait(float DeltaSeconds);
	void TickAsleep();
	void TickWaking(float DeltaSeconds);
	/** Shows a line once, when the clock passes At. */
	void LineAt(float Clock, float DeltaSeconds, float At, const TCHAR* English, const TCHAR* French);

	void Nightfall();
	void PlacePlayerInForest();
	void SetMorningLight() const;
	void SetMeshPose(float Lying, float Breath);
	ACameraActor* SpawnCamera(const FVector& Location, const FRotator& Rotation, float Fov);
	bool GroundAt(const FVector& Spot, float& OutZ) const;
	/** Nothing (a trunk, a rock, a hill) between the camera spot and the player's head, chest and feet? */
	bool SeesPlayer(const FVector& From, const FVector& Target) const;
	/**
	 * A camera spot around the player with a clear view of them, above the
	 * ground: tries the preferred side first, then turns around them, then
	 * comes closer.  Returns the preferred spot if none is clear.
	 */
	FVector FindClearShot(const FVector& Target, float PreferredYaw, float Distance, float Height, float MinAboveGround) const;
	void Skip();
	void FinishIntro();

	EPhase Phase = EPhase::None;
	float Clock = 0.0f;
	float TitleAngle = 0.0f;
	FVector TitleCenter = FVector::ZeroVector;
	float TitleGround = 0.0f;
	float SkyYaw = 0.0f;
	float PortraitTime = 0.0f;
	FVector PortraitSpot = FVector::ZeroVector;
	bool bLowShot = false;
	bool bBackToPlayer = false;

	TObjectPtr<APlayerController> PlayerController;
	TObjectPtr<ACharacter> PlayerCharacter;
	FVector OriginalMeshLocation = FVector::ZeroVector;
	FRotator OriginalMeshRotation = FRotator::ZeroRotator;
	bool bMeshSaved = false;
	/** The real lying-down / getting-up animation (UForestWakeUp); the old tilt of the mesh is only a fallback. */
	TWeakObjectPtr<UForestWakeUp> WakeUpAnim;
	bool bAnimatedWake = false;

	UPROPERTY(Transient) TObjectPtr<UForestIntroWidget> IntroWidget;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> TitleCamera;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> PortraitCamera;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> SkyCamera;
	UPROPERTY(Transient) TObjectPtr<ACameraActor> LowCamera;
};
