#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForestWakeUp.generated.h"

class ACharacter;
class UAnimSequence;

/**
 * How the player's character lies down to sleep and gets up again, slowly.
 *
 * The kit has no "get up" animation, but it has a fall onto the back
 * (MM_Death_Back_01, 1.1 s).  Played forward very slowly it lies the body down
 * in the grass; played backward, with a time curve that lingers, it lifts the
 * body up again: first a stir, then the chest rising, a pause half-sitting,
 * and only then the rest of the way to standing.  While lying, the body
 * breathes.  When it is up, the character's normal animations take over.
 *
 * Used by the intro: For(Character)->LieAsleep(), then ->WakeUp(Seconds).
 */
UCLASS()
class FORESTGAME_API UForestWakeUp : public UActorComponent
{
	GENERATED_BODY()

public:
	UForestWakeUp();

	/** The character's wake-up component, added the first time it is asked for. */
	static UForestWakeUp* For(ACharacter* Character);

	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Lies down slowly, over Seconds, then sleeps there. */
	void FallAsleep(float Seconds);

	/** Already asleep: lying in the grass, breathing. */
	void LieAsleep();

	/** Gets up slowly, over Seconds, then hands back to the usual animations. */
	void WakeUp(float Seconds);

	/** Stops at once and gives the character its usual animations back (a skipped intro). */
	void Finish();

	bool IsAsleep() const { return Phase == EPhase::Asleep; }
	bool IsAwake() const { return Phase == EPhase::Awake; }

	/** 0 when it starts to get up, 1 when it stands. */
	float GetWakeProgress() const;

private:
	enum class EPhase : uint8 { Awake, FallingAsleep, Asleep, WakingUp };

	bool Prepare();
	void SetPose(float AnimTime);
	/** Where in the fall the body is, for a moment of the wake-up (0..1). */
	float WakeCurve(float Alpha) const;

	EPhase Phase = EPhase::Awake;
	float Time = 0.0f;
	float Duration = 1.0f;
	float Breath = 0.0f;
	FVector MeshLocation = FVector::ZeroVector;
	bool bPrepared = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> Fall;
};
