#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "ForestGameManager.generated.h"

class AForestAnimal;
class AForestPickup;

/**
 * Runs the forest game: the player presses E (or the gamepad's left face
 * button) to pick up mushrooms and to feed the animals.  It keeps the score and the
 * on-screen messages, and installs the Forest HUD.  The World Builder places
 * one in every generated world.
 */
UCLASS(Blueprintable)
class FORESTGAME_API AForestGameManager : public AActor
{
	GENERATED_BODY()

public:
	AForestGameManager();

	virtual void Tick(float DeltaSeconds) override;

	/** The manager of the current world, if any. */
	static AForestGameManager* Get(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void Interact();

	UFUNCTION(BlueprintCallable, Category = "Forest")
	void ShowMessage(const FText& Message, float Seconds = 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	FKey InteractKey = EKeys::E;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float PickupRange = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forest")
	float FeedRange = 420.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	int32 Mushrooms = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	int32 AnimalsFed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	int32 AnimalsTotal = 0;

	/** What pressing E would do right now; empty when nothing is in reach. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	FText Prompt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	FText Message;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	float MessageTimeLeft = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forest")
	bool bWon = false;

protected:
	virtual void BeginPlay() override;

private:
	AForestPickup* NearestPickup(const FVector& From) const;
	AForestAnimal* NearestAnimal(const FVector& From) const;

	bool bHudInstalled = false;
};
