#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestPickup.generated.h"

class UStaticMeshComponent;

/**
 * Something the player can pick up in the forest (a mushroom by default).
 * The mesh is chosen in the Blueprint (BP_MushroomPickup); the Forest Game
 * Manager decides when the player is close enough to collect it.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestPickup : public AActor
{
	GENERATED_BODY()

public:
	AForestPickup();

	virtual void Tick(float DeltaSeconds) override;

	/** Picks the item up: it shrinks, rises, and disappears. */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Collect();

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsCollected() const { return bCollected; }

	/** Blueprint hook, called when the player picks this up (sound, particles...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Forest")
	void OnCollected();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Name shown in the interaction prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FText DisplayName;

	/** A gentle pulse so pickups read as interactive without looking like arcade coins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float PulseAmount = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float SpinSpeed = 18.0f;

protected:
	virtual void BeginPlay() override;

private:
	bool bCollected = false;
	float Age = 0.0f;
	float CollectAge = 0.0f;
	FVector BaseLocation = FVector::ZeroVector;
	FVector BaseScale = FVector::OneVector;
};
