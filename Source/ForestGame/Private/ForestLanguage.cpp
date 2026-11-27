#include "ForestLanguage.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CString.h"

namespace
{
	const TCHAR* Section = TEXT("ForestGame");
	const TCHAR* LanguageKey = TEXT("Language");
	/** The older setting, from when the game only spoke English and French. */
	const TCHAR* FrenchKey = TEXT("French");

	struct FWord
	{
		const TCHAR* Words[(int32)EForestLanguage::Count];		// with its article
		const TCHAR* WordsTo[(int32)EForestLanguage::Count];	// "to the deer", "au cerf"...
	};

	const FWord GAnimals[] = {
		{{TEXT("the deer"), TEXT("le cerf"), TEXT("el ciervo"), TEXT("il cervo"), TEXT("o cervo"), TEXT("小鹿"), TEXT("シカ")},
			{TEXT("to the deer"), TEXT("au cerf"), TEXT("al ciervo"), TEXT("al cervo"), TEXT("ao cervo"), TEXT("小鹿"), TEXT("シカ")}},
		{{TEXT("the bunny"), TEXT("le lapin"), TEXT("el conejo"), TEXT("il coniglio"), TEXT("o coelho"), TEXT("兔子"), TEXT("ウサギ")},
			{TEXT("to the bunny"), TEXT("au lapin"), TEXT("al conejo"), TEXT("al coniglio"), TEXT("ao coelho"), TEXT("兔子"), TEXT("ウサギ")}},
		{{TEXT("the fox"), TEXT("le renard"), TEXT("el zorro"), TEXT("la volpe"), TEXT("a raposa"), TEXT("狐狸"), TEXT("キツネ")},
			{TEXT("to the fox"), TEXT("au renard"), TEXT("al zorro"), TEXT("alla volpe"), TEXT("à raposa"), TEXT("狐狸"), TEXT("キツネ")}},
		{{TEXT("the boar"), TEXT("le sanglier"), TEXT("el jabalí"), TEXT("il cinghiale"), TEXT("o javali"), TEXT("野猪"), TEXT("イノシシ")},
			{TEXT("to the boar"), TEXT("au sanglier"), TEXT("al jabalí"), TEXT("al cinghiale"), TEXT("ao javali"), TEXT("野猪"), TEXT("イノシシ")}},
		{{TEXT("the beaver"), TEXT("le castor"), TEXT("el castor"), TEXT("il castoro"), TEXT("o castor"), TEXT("海狸"), TEXT("ビーバー")},
			{TEXT("to the beaver"), TEXT("au castor"), TEXT("al castor"), TEXT("al castoro"), TEXT("ao castor"), TEXT("海狸"), TEXT("ビーバー")}},
	};

	const FWord GWeathers[] = {
		{{TEXT("Clear"), TEXT("Dégagé"), TEXT("Despejado"), TEXT("Sereno"), TEXT("Limpo"), TEXT("晴"), TEXT("晴れ")}, {}},
		{{TEXT("Cloudy"), TEXT("Nuageux"), TEXT("Nublado"), TEXT("Nuvoloso"), TEXT("Nublado"), TEXT("多云"), TEXT("くもり")}, {}},
		{{TEXT("Drizzle"), TEXT("Bruine"), TEXT("Llovizna"), TEXT("Pioggerella"), TEXT("Garoa"), TEXT("毛毛雨"), TEXT("霧雨")}, {}},
		{{TEXT("Rain"), TEXT("Pluie"), TEXT("Lluvia"), TEXT("Pioggia"), TEXT("Chuva"), TEXT("雨"), TEXT("雨")}, {}},
		{{TEXT("Storm"), TEXT("Orage"), TEXT("Tormenta"), TEXT("Temporale"), TEXT("Tempestade"), TEXT("暴风雨"), TEXT("嵐")}, {}},
		{{TEXT("SunShower"), TEXT("Averse au soleil"), TEXT("Lluvia con sol"), TEXT("Pioggia col sole"), TEXT("Chuva com sol"), TEXT("太阳雨"), TEXT("天気雨")}, {}},
		{{TEXT("Mist"), TEXT("Brume"), TEXT("Neblina"), TEXT("Foschia"), TEXT("Névoa"), TEXT("薄雾"), TEXT("霧")}, {}},
		{{TEXT("Snow"), TEXT("Neige"), TEXT("Nieve"), TEXT("Neve"), TEXT("Neve"), TEXT("雪"), TEXT("雪")}, {}},
		{{TEXT("LightSnow"), TEXT("Neige légère"), TEXT("Nieve ligera"), TEXT("Neve leggera"), TEXT("Neve fraca"), TEXT("小雪"), TEXT("小雪")}, {}},
	};

	// Read from the config once, then kept here: Pick runs many times a frame.
	EForestLanguage GLanguage = EForestLanguage::English;
	bool bLanguageRead = false;
}

EForestLanguage ForestLanguage::Get()
{
	if (!bLanguageRead && GConfig)
	{
		bLanguageRead = true;
		int32 Saved = -1;
		if (GConfig->GetInt(Section, LanguageKey, Saved, GGameUserSettingsIni) && Saved >= 0 && Saved < (int32)EForestLanguage::Count)
		{
			GLanguage = (EForestLanguage)Saved;
		}
		else
		{
			bool bFrench = false;
			GConfig->GetBool(Section, FrenchKey, bFrench, GGameUserSettingsIni);
			GLanguage = bFrench ? EForestLanguage::French : EForestLanguage::English;
		}
	}
	return GLanguage;
}

void ForestLanguage::Set(EForestLanguage Language)
{
	GLanguage = Language;
	bLanguageRead = true;
	if (GConfig)
	{
		GConfig->SetInt(Section, LanguageKey, (int32)Language, GGameUserSettingsIni);
		GConfig->SetBool(Section, FrenchKey, Language == EForestLanguage::French, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

const TCHAR* ForestLanguage::NativeName(EForestLanguage Language)
{
	switch (Language)
	{
	case EForestLanguage::French: return TEXT("Français");
	case EForestLanguage::Spanish: return TEXT("Español");
	case EForestLanguage::Italian: return TEXT("Italiano");
	case EForestLanguage::Portuguese: return TEXT("Português");
	case EForestLanguage::Chinese: return TEXT("中文");
	case EForestLanguage::Japanese: return TEXT("日本語");
	default: return TEXT("English");
	}
}

bool ForestLanguage::IsFrench()
{
	return Get() == EForestLanguage::French;
}

bool ForestLanguage::IsEnglish()
{
	return Get() == EForestLanguage::English;
}

const TCHAR* ForestLanguage::Pick(const TCHAR* English, const TCHAR* French)
{
	switch (Get())
	{
	case EForestLanguage::English:
		return English;
	case EForestLanguage::French:
		return French;
	default:
		break;
	}
	const TCHAR* Translated = Translate(English, Get());
	return Translated ? Translated : English;
}

FString ForestLanguage::Format(const TCHAR* Fmt, ...)
{
	TCHAR Buffer[2048];
	va_list Args;
	va_start(Args, Fmt);
	const TCHAR* Format = Fmt;
	const int32 Length = FCString::GetVarArgs(Buffer, UE_ARRAY_COUNT(Buffer), Format, Args);
	va_end(Args);
	return Length >= 0 ? FString(Buffer) : FString(Fmt);
}

FString ForestLanguage::Animal(const FString& EnglishName, bool bTo)
{
	const int32 Language = (int32)Get();
	for (const FWord& Word : GAnimals)
	{
		if (EnglishName.Equals(Word.Words[0], ESearchCase::IgnoreCase))
		{
			return bTo ? Word.WordsTo[Language] : Word.Words[Language];
		}
	}
	// Not in the list (a night creature...): its name through the table.
	const FString Name = Pick(*EnglishName, *EnglishName);
	return bTo ? Pick(TEXT("to "), TEXT("à ")) + Name : Name;
}

FString ForestLanguage::Weather(FName Weather)
{
	for (const FWord& Word : GWeathers)
	{
		if (Weather == FName(Word.Words[0]))
		{
			if (!IsEnglish())
			{
				return Word.Words[(int32)Get()];
			}
			return Weather == TEXT("SunShower") ? TEXT("Sun shower") : Weather == TEXT("LightSnow") ? TEXT("Light snow") : Word.Words[0];
		}
	}
	return Weather.ToString();
}
