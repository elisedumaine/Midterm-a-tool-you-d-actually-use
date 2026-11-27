#pragma once

#include "CoreMinimal.h"

/** The languages of the title screen, in the order of its buttons. */
enum class EForestLanguage : uint8
{
	English,
	French,
	Spanish,
	Italian,
	Portuguese,
	Chinese,
	Japanese,
	Count
};

/**
 * The game speaks English, French, Spanish, Italian, Portuguese, Chinese
 * (simplified) or Japanese.  The player picks on the
 * title screen; the choice is remembered between sessions (GameUserSettings.ini).
 *
 * Code writes every text in English and French (Pick, FOREST_PRINTF); the
 * other languages come from the tables in ForestTranslations.cpp, looked up
 * by the English text.  A text missing from the table shows in English.
 */
namespace ForestLanguage
{
	FORESTGAME_API EForestLanguage Get();
	FORESTGAME_API void Set(EForestLanguage Language);

	/** The language's own name for its button: "Español"... */
	FORESTGAME_API const TCHAR* NativeName(EForestLanguage Language);

	FORESTGAME_API bool IsFrench();
	FORESTGAME_API bool IsEnglish();

	/** The text in the chosen language (Spanish and Italian through the table). */
	FORESTGAME_API const TCHAR* Pick(const TCHAR* English, const TCHAR* French);

	/** printf with a format chosen at run time (the formats come from Pick). */
	FORESTGAME_API FString Format(const TCHAR* Format, ...);

	/** An animal's name ("the deer") in the chosen language; bTo gives "au cerf" / "al ciervo". */
	FORESTGAME_API FString Animal(const FString& EnglishName, bool bTo = false);

	/** A weather name (Clear, SunShower...) for the HUD. */
	FORESTGAME_API FString Weather(FName Weather);

	/** ForestTranslations.cpp: an English text in another language, or null. */
	const TCHAR* Translate(const TCHAR* English, EForestLanguage Language);
}

/** A formatted text with an English and a French format (Spanish and Italian follow the English one). */
#define FOREST_PRINTF(English, French, ...) 	ForestLanguage::Format(ForestLanguage::Pick(English, French), ##__VA_ARGS__)
