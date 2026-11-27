#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestPickup.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * Something the player can pick up in the forest: a mushroom (BP_MushroomPickup,
 * placed by the World Builder), or wood, stones and berry bushes that the Forest
 * Game Manager scatters around the trees and rocks when Play starts.
 *
 * A berry bush stays in place: only its berries go, and they grow back.  Wood
 * and stones vanish and grow back at the same spot later, so the
 * forest never runs out.  The Forest Game Manager decides when the player is
 * close enough to collect it; hungry animals eat berries and mushrooms too.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestPickup : public AActor
{
	GENERATED_BODY()

public:
	AForestPickup();

	virtual void Tick(float DeltaSeconds) override;

	/** Makes this a runtime pickup of the given item (Wood, Stone, Berry) with its mesh. */
	void Configure(FName Item);

	/** Picks the item up: it shrinks, rises, and disappears (or a bush loses its berries). */
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Collect();

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsCollected() const { return bCollected; }

	/** Blueprint hook, called when the player picks this up (sound, particles...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Forest")
	void OnCollected();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The berries of a bush (empty for other pickups). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	TObjectPtr<UInstancedStaticMeshComponent> Berries;

	/** Name shown in the interaction prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FText DisplayName;

	/** What goes into the basket: Mushroom, Berry, Wood or Stone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FName Item = TEXT("Mushroom");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	int32 Amount = 1;

	/** Seconds before it grows back (a few game hours); 0 = gone for good. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float RegrowSeconds = 240.0f;

	/** A gentle pulse so pickups read as interactive without looking like arcade coins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float PulseAmount = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float SpinSpeed = 18.0f;

protected:
	virtual void BeginPlay() override;

private:
	bool IsBush() const { return Berries != nullptr; }

	bool bCollected = false;
	bool bVanished = false;
	float Age = 0.0f;
	float CollectAge = 0.0f;
	float RegrowLeft = 0.0f;
	FVector BaseLocation = FVector::ZeroVector;
	FVector BaseScale = FVector::OneVector;
};
