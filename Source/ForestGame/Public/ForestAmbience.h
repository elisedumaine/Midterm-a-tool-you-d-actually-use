#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestAmbience.generated.h"

class UInstancedStaticMeshComponent;

/**
 * The quiet life of the forest air: glowing motes drifting around the
 * camera, butterflies fluttering between flowers, and dragonflies that hover
 * and dart.  Everything is drawn with instanced meshes animated here, so it
 * costs a handful of draw calls and also moves in the editor viewport.
 * Meshes and materials are set in the Blueprint (BP_ForestAmbience).
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestAmbience : public AActor
{
	GENERATED_BODY()

public:
	AForestAmbience();

	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UInstancedStaticMeshComponent> Motes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UInstancedStaticMeshComponent> ButterflyWingsA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UInstancedStaticMeshComponent> ButterflyWingsB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UInstancedStaticMeshComponent> DragonflyWings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambience")
	TObjectPtr<UInstancedStaticMeshComponent> Bodies;

	/** Motes live in a sphere around the camera, so there are always some close by. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	int32 MoteCount = 260;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	float MoteRadius = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	int32 ButterflyCount = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	int32 DragonflyCount = 8;

	/** Insects stay inside this circle (set by the World Builder to the forest inside the mountains). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	float WorldRadius = 4500.0f;

	/** A gentle breeze carries the motes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience")
	FVector Wind = FVector(18.0f, 9.0f, 4.0f);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	struct FMote
	{
		FVector Position;
		float Phase;
		float Size;
	};

	struct FFlyer
	{
		FVector Position;
		FVector Target;
		FVector Home;
		float Yaw;
		float Phase;
		float Pause;
		float Size;
	};

	void Populate();
	void Animate(float DeltaSeconds);
	FVector Viewpoint() const;
	float GroundHeight(float X, float Y) const;
	FVector RandomPointNear(const FVector& Center, float Radius) const;
	FVector MoteSpawn(const FVector& Around, bool bAnywhere) const;
	void UpdateMotes(float DeltaSeconds, const FVector& View);
	void UpdateButterflies(float DeltaSeconds);
	void UpdateDragonflies(float DeltaSeconds);
	static FTransform WingTransform(const FVector& Body, float Yaw, float Flap, float Side, float Width, float Length, float Offset);

	TArray<FMote> MoteState;
	TArray<FFlyer> Butterflies;
	TArray<FFlyer> Dragonflies;
	float Time = 0.0f;
	bool bPopulated = false;
};
