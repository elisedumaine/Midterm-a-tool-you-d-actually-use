#pragma once

#include "CoreMinimal.h"

class ACharacter;

/** What the character creator lets the player choose, in the order of its rows. */
enum class EForestLookPart : uint8
{
	Body,
	Head,
	Skin,
	Hair,
	HairColor,
	Face,
	Top,
	TopColor,
	Bottom,
	BottomColor,
	Shoes,
	ShoeColor,
	Count
};

/**
 * The player's look: a body, a head, a skin colour, a haircut, a beard and
 * clothes, each with its colour.  Remembered between sessions
 * (GameUserSettings.ini, [ForestGame] Look=...).
 *
 * The pieces are Skeletal Meshes baked from the Houdini character generator
 * by Content/Python/lowpoly_character_parts.py (/Game/LowPolyStudio/CharacterParts):
 * the body is the player's own mesh, the other pieces follow its animation
 * (leader pose).  Their colours are painted at run time by
 * M_LPS_CharacterTint (Skin and Tint parameters).
 *
 * Hats, glasses, backpacks and scarves are not chosen here: they will be
 * found or bought in the forest later.
 */
struct FORESTGAME_API FForestLook
{
	int32 Values[(int32)EForestLookPart::Count] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 7};

	static FForestLook Load();
	void Save() const;
	/** A surprise: every row at random (the "Surprise me" button). */
	static FForestLook Random();

	int32 Get(EForestLookPart Part) const { return Values[(int32)Part]; }
	void Step(EForestLookPart Part, int32 Step);

	static int32 NumChoices(EForestLookPart Part);
	/** "Ponytail", "Queue de cheval"... */
	static FString ChoiceName(EForestLookPart Part, int32 Index);
	FString ValueName(EForestLookPart Part) const { return ChoiceName(Part, Get(Part)); }

	/** Dresses the player.  Does nothing if the pieces were never baked. */
	void Apply(ACharacter* Player) const;
};
