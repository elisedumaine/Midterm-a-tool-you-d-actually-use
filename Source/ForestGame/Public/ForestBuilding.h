#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestBuilding.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A piece of the player's camp, built from the build menu (B): a campfire, a
 * feeder, a shelter, a fence, a lantern or a log bench.  Each one is put
 * together in code from the Kenney nature pieces (logs, stones, bushes), so no
 * Blueprint is needed.
 *
 * The animals use the camp: hungry ones come to eat at the feeder, everyone
 * hides under a shelter when it rains, and tame animals sleep by the fire.
 * While the player chooses where to put it, the same actor is shown as a
 * glowing ghost (green where it fits, red where it does not).
 */
UCLASS()
class FORESTGAME_API AForestBuilding : public AActor
{
	GENERATED_BODY()

public:
	AForestBuilding();

	virtual void Tick(float DeltaSeconds) override;

	/** Builds the pieces of this kind of building (a recipe id: Campfire, Feeder...). */
	void Build(FName NewType, bool bAsGhost);

	/** Ghost only: green when it can be placed here, red when not. */
	void SetGhostValid(bool bValid);

	UFUNCTION(BlueprintPure, Category = "Forest")
	FName GetBuildingType() const { return Type; }

	/** Feeder: how many berries are left, and an animal eating one. */
	UFUNCTION(BlueprintPure, Category = "Forest")
	int32 GetFood() const { return Food; }

	bool TakeFood();
	/** Adds berries up to the feeder's capacity; returns how many went in. */
	int32 AddFood(int32 Count);
	void SetFood(int32 Count);

	static constexpr int32 FeederCapacity = 6;

	/** How far around it an animal counts as "at" this building. */
	float GetRadius() const { return Radius; }

private:
	UStaticMeshComponent* AddPart(const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bCollides = true);
	/** A Kenney log standing upright, Height tall. */
	void AddPost(const FVector& Foot, float Height, float Thickness);
	void AddLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Range);
	void RefreshFood();

	UPROPERTY(VisibleAnywhere, Category = "Forest")
	FName Type;

	bool bGhost = false;
	int32 Food = 0;
	float Radius = 150.0f;
	float Time = 0.0f;
	float LightIntensity = 0.0f;

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> FoodPile;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GhostMaterial;
};
