#include "ForestLanguage.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* Section = TEXT("ForestGame");
	const TCHAR* Key = TEXT("French");

	struct FWord
	{
		const TCHAR* English;
		const TCHAR* French;     // with its article
		const TCHAR* FrenchTo;   // after "à"
	};

	const FWord GAnimals[] = {
		{TEXT("the deer"), TEXT("le cerf"), TEXT("au cerf")},
		{TEXT("the bunny"), TEXT("le lapin"), TEXT("au lapin")},
		{TEXT("the fox"), TEXT("le renard"), TEXT("au renard")},
		{TEXT("the boar"), TEXT("le sanglier"), TEXT("au sanglier")},
		{TEXT("the beaver"), TEXT("le castor"), TEXT("au castor")},
	};

	const FWord GWeathers[] = {
		{TEXT("Clear"), TEXT("Dégagé"), nullptr},
		{TEXT("Cloudy"), TEXT("Nuageux"), nullptr},
		{TEXT("Drizzle"), TEXT("Bruine"), nullptr},
		{TEXT("Rain"), TEXT("Pluie"), nullptr},
		{TEXT("Storm"), TEXT("Orage"), nullptr},
		{TEXT("SunShower"), TEXT("Averse au soleil"), nullptr},
		{TEXT("Mist"), TEXT("Brume"), nullptr},
		{TEXT("Snow"), TEXT("Neige"), nullptr},
		{TEXT("LightSnow"), TEXT("Neige légère"), nullptr},
	};
}

bool ForestLanguage::IsFrench()
{
	bool bFrench = false;
	if (GConfig)
	{
		GConfig->GetBool(Section, Key, bFrench, GGameUserSettingsIni);
	}
	return bFrench;
}

void ForestLanguage::SetFrench(bool bFrench)
{
	if (GConfig)
	{
		GConfig->SetBool(Section, Key, bFrench, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

FString ForestLanguage::Animal(const FString& EnglishName, bool bTo)
{
	if (IsFrench())
	{
		for (const FWord& Word : GAnimals)
		{
			if (EnglishName.Equals(Word.English, ESearchCase::IgnoreCase))
			{
				return bTo ? Word.FrenchTo : Word.French;
			}
		}
		return bTo ? TEXT("à ") + EnglishName : EnglishName;
	}
	return bTo ? TEXT("to ") + EnglishName : EnglishName;
}

FString ForestLanguage::Weather(FName Weather)
{
	for (const FWord& Word : GWeathers)
	{
		if (Weather == FName(Word.English))
		{
			if (IsFrench())
			{
				return Word.French;
			}
			return Weather == TEXT("SunShower") ? TEXT("Sun shower") : Weather == TEXT("LightSnow") ? TEXT("Light snow") : Word.English;
		}
	}
	return Weather.ToString();
}
