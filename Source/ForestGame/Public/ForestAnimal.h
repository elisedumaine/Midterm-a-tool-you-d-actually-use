#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestAnimal.generated.h"

class UAnimationAsset;
class UCapsuleComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EAnimalState : uint8
{
	Wander,		// grazing and strolling around its home glade
	Wary,		// the player came close empty-handed: it runs off
	Curious,	// the player carries a mushroom: it stops and watches
	Eating,
	Happy,
	Refusing,
	Following,	// fed animals stay near the player
	Dancing		// every animal has been fed
};

/**
 * A shy forest animal (deer, bunny, fox...).  It wanders around its glade,
 * runs from a player with empty hands, waits for a player carrying a
 * mushroom, and follows them once fed.  Each species is a Blueprint
 * (BP_Deer, BP_Bunny, BP_Fox...) that sets its mesh, animations, size,
 * speed, and how shy it is.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestAnimal : public AActor
{
	GENERATED_BODY()

public:
	AForestAnimal();

	virtual void Tick(float DeltaSeconds) override;

	/** Gives the animal a mushroom.  Returns false if it was already fed. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	bool Feed(AActor* Feeder);

	/** The player tried to feed it with empty hands: a little head shake. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Refuse();

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Dance();

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsFed() const { return bFed; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	EAnimalState GetAnimalState() const { return State; }

	/** Blueprint hook for effects (hearts, sounds) when the animal is fed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Forest")
	void OnFed();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TObjectPtr<UCapsuleComponent> Capsule;

	/** How the game names it, with its article: "the deer", "the bunny"... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> RunAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> EatAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> HappyAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> RefuseAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Animation")
	TObjectPtr<UAnimationAsset> DanceAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float WalkSpeed = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float RunSpeed = 480.0f;

	/** How far it strolls from the spot where it was placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float WanderRadius = 1400.0f;

	/** Closer than this, an empty-handed player scares it away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float ShyDistance = 650.0f;

	/** Closer than this, a player holding a mushroom makes it stop and watch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float CuriousDistance = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float FollowDistance = 380.0f;

	/** The animal never leaves this circle (set by the World Builder to stay inside the mountains). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float WorldRadius = 5000.0f;

protected:
	virtual void BeginPlay() override;

private:
	void SetState(EAnimalState NewState, float Duration = 0.0f);
	void PlayAnim(UAnimationAsset* Anim, bool bLoop);
	/** Turns toward and walks to a point; returns false if something blocks the way. */
	bool MoveToward(const FVector& Destination, float Speed, float DeltaSeconds);
	void FacePoint(const FVector& Point, float DeltaSeconds);
	void SnapToGround();
	void PickWanderTarget();
	FVector ClampToWorld(const FVector& Point) const;
	int32 PlayerMushrooms() const;

	EAnimalState State = EAnimalState::Wander;
	bool bFed = false;
	float StateTime = 0.0f;
	float PauseTime = 0.0f;
	FVector Home = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> CurrentAnim;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Friend;
};
