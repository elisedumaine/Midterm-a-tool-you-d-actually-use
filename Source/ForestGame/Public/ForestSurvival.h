#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestSurvival.generated.h"

class AForestAnimal;
class SWidget;
class UGameViewportClient;

/**
 * The survival side of the forest game: the player has hearts.
 *
 * A wild animal that is hungry gets cross when the player comes close
 * without feeding it: it growls first (a few seconds to give it food or step
 * back), then bites, which costs hearts and knocks the player back.  Tame,
 * sleeping or eating animals never bite.  Hearts come back slowly on their
 * own, and quickly by a campfire.  With no hearts left the player faints and
 * wakes up by their campfire (or where the game started) with full hearts.
 *
 * It lives in every game world by itself (a world subsystem), so the World
 * Builder does not have to place anything; it draws its own hearts on screen.
 */
UCLASS()
class FORESTGAME_API UForestSurvival : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestSurvival* Get(const UObject* WorldContext);

	/** Health counted in half hearts: 10 = five full hearts. */
	UFUNCTION(BlueprintPure, Category = "Forest|Survival")
	int32 GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Forest|Survival")
	int32 GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "Forest|Survival")
	void Hurt(int32 HalfHearts, AActor* Source);

	UFUNCTION(BlueprintCallable, Category = "Forest|Survival")
	void Heal(int32 HalfHearts);

	/** Read by the on-screen hearts. */
	bool ShowsHearts() const { return bActive || bFainted; }
	bool IsFainted() const { return bFainted; }
	float GetFlash() const { return Flash; }
	float GetFaintDarkness() const;
	/** The animal growling at the player right now, if any. */
	const AForestAnimal* GetThreat() const { return Threat.Get(); }

	static constexpr int32 MaxHealth = 10;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void EnsureOverlay();
	AForestAnimal* FindThreat(const FVector& From, float& OutDistance) const;
	void Bite(AForestAnimal* Animal);
	void Faint();
	void WakeUp();
	bool NearCampfire(const FVector& From) const;
	void Say(const FString& Text, float Seconds) const;

	int32 Health = MaxHealth;
	bool bActive = false;
	bool bFainted = false;
	bool bWokenUp = false;
	float FaintTime = 0.0f;
	float Flash = 0.0f;
	float BiteCooldown = 0.0f;
	float Grace = 0.0f;
	float RegrowTimer = 0.0f;

	TWeakObjectPtr<AForestAnimal> Threat;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	TSharedPtr<SWidget> Overlay;
};
