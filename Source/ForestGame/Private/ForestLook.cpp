#include "ForestLook.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ForestLanguage.h"
#include "ForestShop.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* LookFolder = TEXT("/Game/LowPolyStudio/CharacterParts");

	struct FLookChoice
	{
		const TCHAR* English;
		const TCHAR* French;
		FLinearColor Color = FLinearColor::White;
	};

	// The Houdini generator's own colours first (lowpoly_character_parts.palettes()), then a few more.
	const FLookChoice LookSkins[] = {
		{TEXT("Peach"), TEXT("Pêche"), FLinearColor(0.72f, 0.45f, 0.32f)},
		{TEXT("Light"), TEXT("Clair"), FLinearColor(0.82f, 0.58f, 0.45f)},
		{TEXT("Pale"), TEXT("Pâle"), FLinearColor(0.9f, 0.7f, 0.6f)},
		{TEXT("Tan"), TEXT("Hâlé"), FLinearColor(0.55f, 0.32f, 0.2f)},
		{TEXT("Olive"), TEXT("Olive"), FLinearColor(0.5f, 0.36f, 0.2f)},
		{TEXT("Brown"), TEXT("Brun"), FLinearColor(0.34f, 0.17f, 0.09f)},
		{TEXT("Deep"), TEXT("Foncé"), FLinearColor(0.17f, 0.08f, 0.045f)},
		{TEXT("Fairy blue"), TEXT("Bleu fée"), FLinearColor(0.3f, 0.46f, 0.78f)},
		{TEXT("Forest green"), TEXT("Vert forêt"), FLinearColor(0.3f, 0.52f, 0.28f)},
		{TEXT("Lilac sprite"), TEXT("Lutin lilas"), FLinearColor(0.55f, 0.42f, 0.78f)},
	};

	const FLookChoice LookHairColors[] = {
		{TEXT("Brown"), TEXT("Châtain"), FLinearColor(0.12f, 0.06f, 0.03f)},
		{TEXT("Black"), TEXT("Noir"), FLinearColor(0.02f, 0.016f, 0.016f)},
		{TEXT("Auburn"), TEXT("Auburn"), FLinearColor(0.3f, 0.08f, 0.04f)},
		{TEXT("Blonde"), TEXT("Blond"), FLinearColor(0.75f, 0.55f, 0.25f)},
		{TEXT("Ginger"), TEXT("Roux"), FLinearColor(0.7f, 0.25f, 0.05f)},
		{TEXT("Silver"), TEXT("Argent"), FLinearColor(0.6f, 0.6f, 0.62f)},
		{TEXT("White"), TEXT("Blanc"), FLinearColor(0.9f, 0.9f, 0.88f)},
		{TEXT("Pink"), TEXT("Rose"), FLinearColor(0.9f, 0.35f, 0.55f)},
		{TEXT("Blue"), TEXT("Bleu"), FLinearColor(0.15f, 0.3f, 0.8f)},
		{TEXT("Moss green"), TEXT("Vert mousse"), FLinearColor(0.2f, 0.5f, 0.22f)},
		{TEXT("Purple"), TEXT("Violet"), FLinearColor(0.42f, 0.18f, 0.6f)},
	};

	const FLookChoice LookCloth[] = {
		{TEXT("Apricot"), TEXT("Abricot"), FLinearColor(0.86f, 0.42f, 0.18f)},
		{TEXT("Cream"), TEXT("Crème"), FLinearColor(0.85f, 0.78f, 0.62f)},
		{TEXT("Blush"), TEXT("Rose poudré"), FLinearColor(0.7f, 0.27f, 0.23f)},
		{TEXT("Sage"), TEXT("Sauge"), FLinearColor(0.36f, 0.52f, 0.3f)},
		{TEXT("Forest"), TEXT("Forêt"), FLinearColor(0.08f, 0.2f, 0.08f)},
		{TEXT("Navy"), TEXT("Marine"), FLinearColor(0.04f, 0.07f, 0.2f)},
		{TEXT("Denim"), TEXT("Denim"), FLinearColor(0.15f, 0.25f, 0.45f)},
		{TEXT("Charcoal"), TEXT("Anthracite"), FLinearColor(0.05f, 0.05f, 0.06f)},
		{TEXT("Lilac"), TEXT("Lilas"), FLinearColor(0.45f, 0.35f, 0.7f)},
		{TEXT("Mustard"), TEXT("Moutarde"), FLinearColor(0.75f, 0.55f, 0.08f)},
		{TEXT("Red"), TEXT("Rouge"), FLinearColor(0.7f, 0.05f, 0.04f)},
		{TEXT("White"), TEXT("Blanc"), FLinearColor(0.9f, 0.9f, 0.88f)},
		{TEXT("Pink"), TEXT("Rose"), FLinearColor(0.95f, 0.5f, 0.62f)},
		{TEXT("Teal"), TEXT("Sarcelle"), FLinearColor(0.06f, 0.42f, 0.42f)},
		{TEXT("Sky"), TEXT("Ciel"), FLinearColor(0.45f, 0.68f, 0.9f)},
		{TEXT("Brown"), TEXT("Marron"), FLinearColor(0.25f, 0.13f, 0.06f)},
	};

	const FLookChoice LookBodies[] = {{TEXT("Average"), TEXT("Moyen")}, {TEXT("Slim"), TEXT("Mince")}, {TEXT("Sturdy"), TEXT("Costaud")}};
	const FLookChoice LookHeads[] = {{TEXT("Normal"), TEXT("Normale")}, {TEXT("Big"), TEXT("Grosse")}, {TEXT("Chibi"), TEXT("Chibi")}};
	const FLookChoice LookHairs[] = {
		{TEXT("Short"), TEXT("Courts")}, {TEXT("Bob"), TEXT("Carré")}, {TEXT("Long"), TEXT("Longs")}, {TEXT("Ponytail"), TEXT("Queue de cheval")},
		{TEXT("Bun"), TEXT("Chignon")}, {TEXT("Spiky"), TEXT("En pics")}, {TEXT("Mohawk"), TEXT("Crête")}, {TEXT("Curly"), TEXT("Bouclés")},
		{TEXT("Bald"), TEXT("Chauve")},
	};
	const FLookChoice LookFaces[] = {
		{TEXT("Nothing"), TEXT("Rien")}, {TEXT("Moustache"), TEXT("Moustache")}, {TEXT("Beard"), TEXT("Barbe")},
		{TEXT("Goatee"), TEXT("Bouc")}, {TEXT("Full beard"), TEXT("Grande barbe")},
	};
	const FLookChoice LookTops[] = {
		{TEXT("T-shirt"), TEXT("T-shirt")}, {TEXT("Long sleeves"), TEXT("Manches longues")}, {TEXT("Tank top"), TEXT("Débardeur")},
		{TEXT("Hoodie"), TEXT("Sweat à capuche")}, {TEXT("Striped"), TEXT("Rayé")},
	};
	const FLookChoice LookBottoms[] = {{TEXT("Trousers"), TEXT("Pantalon")}, {TEXT("Shorts"), TEXT("Short")}, {TEXT("Skirt"), TEXT("Jupe")}};
	const FLookChoice LookShoes[] = {{TEXT("Sneakers"), TEXT("Baskets")}, {TEXT("Boots"), TEXT("Bottes")}, {TEXT("Sandals"), TEXT("Sandales")}};

	TArrayView<const FLookChoice> LookChoices(EForestLookPart Part)
	{
		switch (Part)
		{
		case EForestLookPart::Body: return LookBodies;
		case EForestLookPart::Head: return LookHeads;
		case EForestLookPart::Skin: return LookSkins;
		case EForestLookPart::Hair: return LookHairs;
		case EForestLookPart::HairColor: return LookHairColors;
		case EForestLookPart::Face: return LookFaces;
		case EForestLookPart::Top: return LookTops;
		case EForestLookPart::Bottom: return LookBottoms;
		case EForestLookPart::Shoes: return LookShoes;
		default: return LookCloth;
		}
	}

	int32 LookWrap(int32 Value, int32 Count)
	{
		return Count > 0 ? ((Value % Count) + Count) % Count : 0;
	}

	USkeletalMesh* LookPiece(const FString& Name)
	{
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), LookFolder, *Name, *Name);
		return LoadObject<USkeletalMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The piece's own component on the player (made the first time), following the body's animation. */
	USkeletalMeshComponent* LookSlot(ACharacter* Player, USkeletalMeshComponent* Body, const TCHAR* Name)
	{
		TArray<USkeletalMeshComponent*> Components;
		Player->GetComponents(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component->GetFName() == Name)
			{
				return Component;
			}
		}
		USkeletalMeshComponent* Slot = NewObject<USkeletalMeshComponent>(Player, Name);
		Slot->SetupAttachment(Body);
		Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slot->RegisterComponent();
		Slot->SetLeaderPoseComponent(Body);
		return Slot;
	}

	void LookPaint(USkeletalMeshComponent* Component, const FLinearColor& Skin, const FLinearColor& Tint)
	{
		if (!Component || !Component->GetSkeletalMeshAsset())
		{
			return;
		}
		UMaterialInstanceDynamic* Paint = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
		if (!Paint)
		{
			Paint = Component->CreateDynamicMaterialInstance(0);
		}
		if (Paint)
		{
			Paint->SetVectorParameterValue(TEXT("Skin"), Skin);
			Paint->SetVectorParameterValue(TEXT("Tint"), Tint);
		}
	}
}

int32 FForestLook::NumChoices(EForestLookPart Part)
{
	return LookChoices(Part).Num();
}

FString FForestLook::ChoiceName(EForestLookPart Part, int32 Index)
{
	const TArrayView<const FLookChoice> Choices = LookChoices(Part);
	const FLookChoice& Choice = Choices[LookWrap(Index, Choices.Num())];
	return ForestLanguage::Pick(Choice.English, Choice.French);
}

void FForestLook::Step(EForestLookPart Part, int32 Step)
{
	Values[(int32)Part] = LookWrap(Values[(int32)Part] + Step, NumChoices(Part));
}

FForestLook FForestLook::Load()
{
	FForestLook Look;
	FString Saved;
	if (GConfig && GConfig->GetString(TEXT("ForestGame"), TEXT("Look"), Saved, GGameUserSettingsIni))
	{
		TArray<FString> Numbers;
		Saved.ParseIntoArray(Numbers, TEXT(","));
		for (int32 Index = 0; Index < Numbers.Num() && Index < (int32)EForestLookPart::Count; ++Index)
		{
			Look.Values[Index] = LookWrap(FCString::Atoi(*Numbers[Index]), NumChoices((EForestLookPart)Index));
		}
	}
	return Look;
}

void FForestLook::Save() const
{
	if (!GConfig)
	{
		return;
	}
	TArray<FString> Numbers;
	for (const int32 Value : Values)
	{
		Numbers.Add(FString::FromInt(Value));
	}
	GConfig->SetString(TEXT("ForestGame"), TEXT("Look"), *FString::Join(Numbers, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FForestLook FForestLook::Random()
{
	FForestLook Look;
	for (int32 Index = 0; Index < (int32)EForestLookPart::Count; ++Index)
	{
		Look.Values[Index] = FMath::RandHelper(NumChoices((EForestLookPart)Index));
	}
	return Look;
}

void FForestLook::Apply(ACharacter* Player) const
{
	USkeletalMeshComponent* Body = Player ? Player->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	const int32 Build = Get(EForestLookPart::Body);
	const int32 Head = Get(EForestLookPart::Head);
	USkeletalMesh* BodyMesh = LookPiece(FString::Printf(TEXT("SKM_LPC_Body_b%d_h%d"), Build, Head));
	if (!BodyMesh)
	{
		// The pieces were never baked (lowpoly_character_parts.bake_all): keep the Blueprint's character.
		return;
	}
	if (Body->GetSkeletalMeshAsset() != BodyMesh)
	{
		Body->SetSkeletalMeshAsset(BodyMesh);
	}

	const FLinearColor Skin = LookSkins[LookWrap(Get(EForestLookPart::Skin), UE_ARRAY_COUNT(LookSkins))].Color;
	const FLinearColor HairColor = LookHairColors[LookWrap(Get(EForestLookPart::HairColor), UE_ARRAY_COUNT(LookHairColors))].Color;
	auto Cloth = [this](EForestLookPart Part) { return LookCloth[LookWrap(Get(Part), UE_ARRAY_COUNT(LookCloth))].Color; };
	LookPaint(Body, Skin, Skin);

	struct FSlotChoice
	{
		const TCHAR* Slot;
		FString Piece;
		FLinearColor Tint;
	};
	const int32 Hair = Get(EForestLookPart::Hair);
	const int32 Face = Get(EForestLookPart::Face);
	const FSlotChoice Slots[] = {
		// "Bald" and "Nothing" are no piece at all.
		{TEXT("LookHair"), Hair < 8 ? FString::Printf(TEXT("SKM_LPC_Hair_%d_b%d_h%d"), Hair, Build, Head) : FString(), HairColor},
		{TEXT("LookFace"), Face > 0 ? FString::Printf(TEXT("SKM_LPC_Face_%d_b%d_h%d"), Face, Build, Head) : FString(), HairColor},
		{TEXT("LookTop"), FString::Printf(TEXT("SKM_LPC_Top_%d_b%d"), Get(EForestLookPart::Top), Build), Cloth(EForestLookPart::TopColor)},
		{TEXT("LookBottom"), FString::Printf(TEXT("SKM_LPC_Bottom_%d_b%d"), Get(EForestLookPart::Bottom), Build), Cloth(EForestLookPart::BottomColor)},
		{TEXT("LookShoes"), FString::Printf(TEXT("SKM_LPC_Shoes_%d_b%d"), Get(EForestLookPart::Shoes), Build), Cloth(EForestLookPart::ShoeColor)},
	};
	for (const FSlotChoice& Choice : Slots)
	{
		USkeletalMeshComponent* Slot = LookSlot(Player, Body, Choice.Slot);
		USkeletalMesh* Piece = Choice.Piece.IsEmpty() ? nullptr : LookPiece(Choice.Piece);
		if (Slot->GetSkeletalMeshAsset() != Piece)
		{
			Slot->SetSkeletalMeshAsset(Piece);
		}
		Slot->SetVisibility(Piece != nullptr);
		LookPaint(Slot, Skin, Choice.Tint);
	}

	// What was bought in the shop: a hat and an extra (backpack, scarf, glasses).
	const FForestWardrobe Wardrobe = FForestWardrobe::Load();
	for (const bool bHat : {true, false})
	{
		const FForestAccessory* Worn = FForestAccessory::Find(bHat ? Wardrobe.Hat : Wardrobe.Extra);
		USkeletalMeshComponent* Slot = LookSlot(Player, Body, bHat ? TEXT("LookHat") : TEXT("LookExtra"));
		USkeletalMesh* Piece = Worn ? LookPiece(FString::Printf(TEXT("SKM_LPC_%s_%d_b%d_h%d"), bHat ? TEXT("Hat") : TEXT("Extra"), Worn->Index, Build, Head)) : nullptr;
		if (Slot->GetSkeletalMeshAsset() != Piece)
		{
			Slot->SetSkeletalMeshAsset(Piece);
		}
		Slot->SetVisibility(Piece != nullptr);
		LookPaint(Slot, Skin, LookCloth[LookWrap(bHat ? Wardrobe.HatColor : Wardrobe.ExtraColor, UE_ARRAY_COUNT(LookCloth))].Color);
	}
}
