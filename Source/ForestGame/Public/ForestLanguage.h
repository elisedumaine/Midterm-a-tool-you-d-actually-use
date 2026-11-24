#pragma once

#include "CoreMinimal.h"

/**
 * The game speaks English or French.  The player picks on the title screen;
 * the choice is remembered between sessions (GameUserSettings.ini).
 */
namespace ForestLanguage
{
	FORESTGAME_API bool IsFrench();
	FORESTGAME_API void SetFrench(bool bFrench);

	/** The English or the French text, whichever the player chose. */
	inline const TCHAR* Pick(const TCHAR* English, const TCHAR* French)
	{
		return IsFrench() ? French : English;
	}

	/** An animal's name ("the deer") in the chosen language; bTo gives "au cerf" in French. */
	FORESTGAME_API FString Animal(const FString& EnglishName, bool bTo = false);

	/** A weather name (Clear, SunShower...) for the HUD. */
	FORESTGAME_API FString Weather(FName Weather);
}

/** FString::Printf with an English and a French format (Printf needs literal formats). */
#define FOREST_PRINTF(English, French, ...) 	(ForestLanguage::IsFrench() ? FString::Printf(French, ##__VA_ARGS__) : FString::Printf(English, ##__VA_ARGS__))
