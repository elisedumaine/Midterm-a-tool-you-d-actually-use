#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestChest.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A small wooden chest hidden in the forest: behind a rock, at the foot of a
 * tree, half sunk in the grass.  It glints faintly when the player comes near,
 * so a careful eye can spot it.  Opening it (E) lifts the lid and gives what
 * is inside (see UForestLife::OpenChest).  A shooting star can also leave one
 * where it lands, glowing brightly.
 */
UCLASS()
class FORESTGAME_API AForestChest : public AActor
{
	GENERATED_BODY()

public:
	AForestChest();

	virtual void Tick(float DeltaSeconds) override;

	/** Builds the chest; a star chest glows until it is opened. */
	void Build(bool bStarChest);

	/** Lifts the lid (and stops the glint). */
	void Open();

	bool IsOpened() const { return bOpened; }
	bool IsStarChest() const { return bStar; }

private:
	UStaticMeshComponent* AddBox(const FVector& Location, const FVector& Size, const FLinearColor& Color, USceneComponent* Parent = nullptr);

	UPROPERTY(Transient) TObjectPtr<USceneComponent> LidHinge;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Glint;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlintMaterial;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> StarLight;

	bool bOpened = false;
	bool bStar = false;
	float LidAngle = 0.0f;
	float Time = 0.0f;
};
