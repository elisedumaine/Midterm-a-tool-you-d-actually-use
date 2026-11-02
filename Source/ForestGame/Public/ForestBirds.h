#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestBirds.generated.h"

class APawn;
class UInstancedStaticMeshComponent;
class USoundAttenuation;
class USoundBase;

/**
 * Birds of the forest, for atmosphere only.  Flocks of small birds hop and
 * peck in the glades and burst into the air when the player comes close;
 * a few large birds circle high above, now and then gliding down to rest.
 * They chirp and call from where they are.  Drawn with instanced shapes,
 * like the butterflies.  Meshes, colours, and sounds are set in the
 * Blueprint (BP_ForestBirds).
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestBirds : public AActor
{
	GENERATED_BODY()

public:
	AForestBirds();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> SmallBodies;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> SmallHeads;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> SmallWings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> LargeBodies;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> LargeHeads;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Birds")
	TObjectPtr<UInstancedStaticMeshComponent> LargeWings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds")
	int32 SmallFlockCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds")
	int32 LargeBirdCount = 4;

	/** A grounded bird takes off when the player comes closer than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds")
	float ScareDistance = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds")
	float WorldRadius = 4500.0f;

	/** Short songs for the small birds; each is replayed at a random pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds|Sound")
	TArray<TObjectPtr<USoundBase>> ChirpSounds;

	/** The occasional distant call of a large bird. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds|Sound")
	TObjectPtr<USoundBase> LargeCallSound;

	/** How songs fade with distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds|Sound")
	float ChirpVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Birds|Sound")
	float CallVolume = 0.35f;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	enum class EBirdState : uint8 { Grounded, Flying, Soaring };

	struct FBird
	{
		FVector Position = FVector::ZeroVector;
		FVector From = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		FVector Circle = FVector::ZeroVector;
		float Yaw = 0.0f;
		float Phase = 0.0f;
		float Size = 1.0f;
		float Progress = 0.0f;
		float FlightTime = 1.0f;
		float Timer = 0.0f;
		float Angle = 0.0f;
		float Hop = 0.0f;
		float Radius = 800.0f;
		EBirdState State = EBirdState::Grounded;
		int32 Flock = INDEX_NONE;
	};

	struct FFlock
	{
		FVector Home = FVector::ZeroVector;
		float Timer = 0.0f;
		float ChirpTimer = 0.0f;
		bool bFlying = false;
	};

	void Populate();
	void Animate(float DeltaSeconds);
	float GroundHeight(float X, float Y) const;
	FVector RandomGroundPoint(const FVector& Near, float MinDistance, float MaxDistance) const;
	void LaunchFlock(int32 FlockIndex, const FVector& AwayFrom, bool bScared);
	void FlyTo(FBird& Bird, const FVector& Destination, float Speed);
	void UpdateSmall(float DeltaSeconds, const APawn* Player);
	void UpdateLarge(float DeltaSeconds, const APawn* Player);
	void PlayAt(USoundBase* Sound, const FVector& Location, float Volume, float Pitch) const;
	void Draw();

	TArray<FBird> Small;
	TArray<FBird> Large;
	TArray<FFlock> Flocks;
	float Time = 0.0f;
	bool bPopulated = false;
};
