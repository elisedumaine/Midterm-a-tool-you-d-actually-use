#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestNightBeast.generated.h"

class UAnimationAsset;
class UCapsuleComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * A creature of the night: a big black fox or boar with glowing red eyes.
 *
 * It only exists between dusk and dawn (UForestNight brings it).  It stalks
 * the player through the dark, and bites when it reaches them.  Fire is the
 * one thing it fears: it will not step into a campfire's light (or close to a
 * lantern), so it circles at the edge of the glow, staring.  At dawn it runs
 * off and fades away.
 */
UCLASS()
class FORESTGAME_API AForestNightBeast : public AActor
{
	GENERATED_BODY()

public:
	AForestNightBeast();

	virtual void Tick(float DeltaSeconds) override;

	/** Picks its body (0 = fox, 1 = boar) and its size. */
	void Setup(int32 Kind);

	/** Dawn: it runs away from the player and fades out. */
	void Flee();

	bool IsFleeing() const { return bFleeing; }

	/** "the shadow wolf", in the chosen language. */
	FString GetBeastName() const;

	UPROPERTY(VisibleAnywhere, Category = "Night")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Night")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Night")
	TObjectPtr<UStaticMeshComponent> LeftEye;

	UPROPERTY(VisibleAnywhere, Category = "Night")
	TObjectPtr<UStaticMeshComponent> RightEye;

	UPROPERTY(VisibleAnywhere, Category = "Night")
	TObjectPtr<UPointLightComponent> EyeGlow;

private:
	bool MoveToward(const FVector& Destination, float Speed, float DeltaSeconds);
	void FacePoint(const FVector& Point, float DeltaSeconds);
	void SnapToGround();
	void PlayAnim(UAnimationAsset* Anim);
	/** The nearest fire or lantern whose light keeps it away, and how far that light reaches. */
	bool NearestLight(const FVector& From, FVector& OutLight, float& OutRadius) const;

	int32 Kind = 0;
	bool bFleeing = false;
	float FadeTime = 0.0f;
	float BiteCooldown = 1.0f;
	float WalkSpeed = 240.0f;
	float RunSpeed = 470.0f;
	float Time = 0.0f;
	float Detour = 0.0f;
	FVector DetourDirection = FVector::ZeroVector;
	float CircleSide = 1.0f;

	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> IdleAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> WalkAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> RunAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> AttackAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimationAsset> CurrentAnim;
};
