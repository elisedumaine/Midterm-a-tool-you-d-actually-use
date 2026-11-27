#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/WorldSubsystem.h"
#include "ForestShop.generated.h"

class ACameraActor;

/**
 * Fireflies ("lucioles"), the forest's money: earned by surviving nights,
 * taming animals, welcoming residents and caring for the forest (UForestSpirit),
 * spent in the shop.  Remembered between sessions (GameUserSettings.ini).
 */
namespace ForestWallet
{
	FORESTGAME_API int32 Fireflies();
	/** Adds (or removes) fireflies; with bSay a "+5 fireflies" line on screen. */
	FORESTGAME_API void Add(const UObject* WorldContext, int32 Amount, bool bSay = true);
	/** Pays if there is enough; returns false otherwise. */
	FORESTGAME_API bool Spend(int32 Amount);
}

/** An accessory of the shop: a hat or an extra (backpack, scarf, glasses). */
struct FORESTGAME_API FForestAccessory
{
	FName Id;
	bool bHat = true;
	/** Its number in the Houdini generator (hat 1..5, extra 1..3). */
	int32 Index = 1;
	int32 Price = 10;
	const TCHAR* English;
	const TCHAR* French;
	/** Can its colour be chosen (the flower crown and the glasses keep theirs). */
	bool bColor = true;

	static TArrayView<const FForestAccessory> All();
	static const FForestAccessory* Find(FName Id);
	FString Name() const;
};

/**
 * What the player owns and wears, beyond the clothes chosen at the start.
 * Remembered between sessions (GameUserSettings.ini).
 */
struct FORESTGAME_API FForestWardrobe
{
	TArray<FName> Owned;
	FName Hat;
	FName Extra;
	int32 HatColor = 4;
	int32 ExtraColor = 2;

	static FForestWardrobe Load();
	void Save() const;
	bool Owns(FName Id) const { return Owned.Contains(Id); }
	bool Wears(FName Id) const { return Id != NAME_None && (Hat == Id || Extra == Id); }
};

/** The shop's page: every accessory, its price, buy / wear / take off, and its colour. */
UCLASS()
class FORESTGAME_API UForestShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
};

/**
 * The forest's shop: U opens it (not during the title, the intro, building or
 * at night when creatures hunt).  The camera turns to look at the player so
 * they see what they try on.  Lives in every game world by itself.
 *
 * Console: Forest.Fireflies 50 gives fireflies (tests, presentation).
 */
UCLASS()
class FORESTGAME_API UForestShop : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	static UForestShop* Get(const UObject* WorldContext);

	void Open();
	void Close();
	bool IsOpen() const { return Widget != nullptr; }

	/** Buys, wears or takes off an accessory (the shop's button). */
	void Choose(FName Id);
	void StepColor(bool bHat, int32 Step);
	/** Shown on the shop's page for a moment (not enough fireflies...). */
	FString Notice;
	double NoticeAt = 0.0;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void Dress() const;

	UPROPERTY(Transient)
	TObjectPtr<UForestShopWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
