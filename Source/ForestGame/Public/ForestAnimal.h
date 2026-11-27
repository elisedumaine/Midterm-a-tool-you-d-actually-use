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
	Wary,		// the player came too close: it runs off
	Curious,	// the player carries food: it stops and watches
	Eating,
	Happy,
	Refusing,
	Following,	// tame animals stay near the player
	Dancing,	// every animal is tame
	Foraging,	// hungry: on its way to a bush, a mushroom or the feeder
	Sheltering,	// rain: under a tree or the camp's shelter
	Sleeping,	// at night (in the day for the fox), at home or by the campfire
	Staying,	// tame, told to stay: lives around the camp
	Charging,	// hungry and bold (boar, fox): comes at a player who has no food for it
	Growling	// hungry and shy: stands its ground instead of running off
};

/**
 * A forest animal (deer, bunny, fox...) with a little life of its own.
 *
 * It has needs: it gets hungry and goes to eat berries, mushrooms or the
 * camp's feeder; it sleeps at night (the fox sleeps in the day); it hides
 * under a tree or a shelter when it rains, and storms make it jumpy.  It is
 * shy of the player at first: each meal the player gives raises its trust,
 * and its favourite food counts double.  Once it trusts the player fully it
 * is tame: it follows them, can be told to stay at the camp, and sleeps by
 * the campfire.
 *
 * Each species is a Blueprint (BP_Deer, BP_Bunny, BP_Fox...) that sets its
 * mesh, animations, size, speed and how shy it is; its tastes and habits come
 * from the species table in ForestAnimal.cpp.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestAnimal : public AActor
{
	GENERATED_BODY()

public:
	AForestAnimal();

	virtual void Tick(float DeltaSeconds) override;

	/** Gives the animal some food.  Returns false if it cannot eat right now. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	bool Feed(AActor* Feeder, FName Food);

	/** The player tried to feed it with empty hands: a little head shake. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Refuse();

	/** A tame animal gets a pat: a happy little jump. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Pet();

	/** A tame animal stays at the camp (around where it is now), or follows again. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void ToggleStay();

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Dance();

	/** Fed at least once (kept for the HUD counters). */
	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsFed() const { return bTame; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsTame() const { return bTame; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsStaying() const { return State == EAnimalState::Staying || bStaying; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	EAnimalState GetAnimalState() const { return State; }

	/** 0 = wild, 1 = tame. */
	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetTrust() const { return Trust; }

	/** 0 = full, 1 = starving. */
	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetHunger() const { return Hunger; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsHungry() const { return Hunger > HungryAt; }

	/** Is it busy eating, sleeping...: what the HUD writes above it. */
	FString DescribeMood() const;

	/** The food it likes best (Berry, Mushroom or Wood for the beaver). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	FName FavoriteFood = TEXT("Berry");

	/** Meals of ordinary food before it is tame (the favourite counts double). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	int32 MealsToTame = 3;

	/** Awake at night, asleep in the day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	bool bNocturnal = false;

	/** Hungry and the player has nothing for it: it comes at them instead of growling in place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	bool bBold = false;

	/** Seconds from full to hungry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	float HungerSeconds = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Needs")
	float HungryAt = 0.65f;

	/** Can it eat this?  Berries and mushrooms for everyone; the beaver also gnaws wood. */
	bool Eats(FName Food) const;

	/** Restores what the save file remembers. */
	void Restore(float SavedTrust, bool bSavedTame, bool bSavedStaying, const FVector& SavedHome);
	FVector GetHome() const { return Home; }

	/** Moves into one of the player's shelters: it now lives around it (see UForestBonds). */
	void MoveIn(const FVector& NewHome);
	bool HasMovedIn() const { return bMovedIn; }

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

	/** Closer than this, a wild animal runs from the player (less as it learns to trust). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest|Behaviour")
	float ShyDistance = 650.0f;

	/** Closer than this, a player holding food makes it stop and watch. */
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
	void ApplySpecies();
	void SetState(EAnimalState NewState, float Duration = 0.0f);
	void PlayAnim(UAnimationAsset* Anim, bool bLoop, float Rate = 1.0f);
	/** Turns toward and walks to a point; returns false if something blocks the way. */
	bool MoveToward(const FVector& Destination, float Speed, float DeltaSeconds);
	/** Walks to a point, stepping around what blocks the way; true once it is there. */
	bool TravelTo(const FVector& Destination, float Speed, float Arrived, float DeltaSeconds);
	void FacePoint(const FVector& Point, float DeltaSeconds);
	void SnapToGround();
	void PickWanderTarget();
	FVector ClampToWorld(const FVector& Point) const;
	bool PlayerHasFood() const;
	bool IsSleepTime() const;
	bool IsRaining() const;
	bool IsStormy() const;

	/** One need after another; each returns true when it took over this frame. */
	bool UpdateFear(APawn* Player, float PlayerDistance, float DeltaSeconds);
	/** Hungry: the survival game's danger (ForestSurvival makes it bite). */
	bool UpdateThreat(APawn* Player, float PlayerDistance, float DeltaSeconds);
	bool UpdateShelter(float DeltaSeconds);
	bool UpdateSleep(APawn* Player, float PlayerDistance, float DeltaSeconds);
	bool UpdateHunger(float DeltaSeconds);
	void UpdateWander(float DeltaSeconds);
	void UpdateTame(APawn* Player, float DeltaSeconds);

	EAnimalState State = EAnimalState::Wander;
	bool bTame = false;
	bool bStaying = false;
	bool bMovedIn = false;
	float Trust = 0.0f;
	float Hunger = 0.3f;
	float StateTime = 0.0f;
	float PauseTime = 0.0f;
	float Detour = 0.0f;
	/** How long it has been walking to its meal, and a pause before trying again after giving up. */
	float ForageTime = 0.0f;
	float ForageCooldown = 0.0f;
	FVector DetourDirection = FVector::ZeroVector;
	FVector Home = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	/** Where it hides from the rain or sleeps; recomputed when the need starts. */
	FVector Refuge = FVector::ZeroVector;
	bool bHasRefuge = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> CurrentAnim;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Friend;

	/** The bush, mushroom or feeder it is walking to. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> Meal;
};
