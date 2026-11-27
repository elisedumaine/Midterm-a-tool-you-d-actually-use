#include "ForestMusic.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestGame.h"
#include "ForestNight.h"
#include "ForestNightBeast.h"
#include "ForestPauseMenu.h"
#include "ForestSky.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* MusicFolder = TEXT("/Game/Music");
	const TCHAR* MoodNames[] = {TEXT("Title"), TEXT("Day"), TEXT("Dusk"), TEXT("Night"), TEXT("Danger")};
	/** Seconds for a full fade from one track to the next (danger comes in faster). */
	constexpr float FadeSeconds = 6.0f;
	constexpr float DangerFadeSeconds = 1.5f;
	/** A beast this close, out of the light: danger. */
	constexpr float DangerDistance = 1800.0f;
	/** The danger music lingers this long after the beast is gone. */
	constexpr float DangerLinger = 6.0f;
}

bool UForestMusic::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestMusic::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestMusic, STATGROUP_Tickables);
}

UForestMusic* UForestMusic::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestMusic>() : nullptr;
}

void UForestMusic::Deinitialize()
{
	for (UAudioComponent* Player : Players)
	{
		if (Player)
		{
			Player->Stop();
		}
	}
	Players.Reset();
	Super::Deinitialize();
}

void UForestMusic::FindTracks()
{
	bFound = true;
	Tracks.Init(nullptr, Count);
	Players.Init(nullptr, Count);

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(MusicFolder), Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	for (const FAssetData& Asset : Found)
	{
		if (!Asset.IsInstanceOf(USoundBase::StaticClass()))
		{
			continue;
		}
		const FString Name = Asset.AssetName.ToString();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Tracks[Index] && Name.Contains(MoodNames[Index], ESearchCase::IgnoreCase))
			{
				Tracks[Index] = Cast<USoundBase>(Asset.GetAsset());
				UE_LOG(LogForestGame, Log, TEXT("Music: %s plays for %s."), *Name, MoodNames[Index]);
				break;
			}
		}
	}
}

UForestMusic::EMood UForestMusic::Playable(EMood Wanted) const
{
	auto Has = [this](EMood Each) { return Tracks.IsValidIndex(Each) && Tracks[Each] != nullptr; };
	if (Has(Wanted))
	{
		return Wanted;
	}
	switch (Wanted)
	{
	case Danger: return Has(Night) ? Night : Playable(Day);
	case Dusk: return Has(Day) ? Day : Playable(Night);
	case Title: return Has(Dusk) ? Dusk : Day;
	default: return Wanted;
	}
}

UForestMusic::EMood UForestMusic::WantedMood() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	const AForestSky* Sky = AForestSky::Get(World);
	// The title, the creative pages and the wake-up: the player does not walk yet.
	if (!Player || !Sky || (Controller->IsMoveInputIgnored() && !World->IsPaused()))
	{
		return Title;
	}
	if (CalmFor < DangerLinger)
	{
		return Danger;
	}
	if (Sky->IsNight())
	{
		return Night;
	}
	return Sky->GetHour() >= 17.5f ? Dusk : Day;
}

void UForestMusic::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !World->GetFirstPlayerController())
	{
		return;
	}
	if (!bFound)
	{
		FindTracks();
	}

	// Danger: red eyes nearby while the player stands in the dark.
	if (!World->IsPaused())
	{
		CalmFor += DeltaSeconds;
		const APawn* Player = World->GetFirstPlayerController()->GetPawn();
		const UForestNight* Night_ = UForestNight::Get(World);
		if (Player && !(Night_ && Night_->IsInLight(Player->GetActorLocation())))
		{
			for (TActorIterator<AForestNightBeast> It(World); It; ++It)
			{
				if (!It->IsFleeing() && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < DangerDistance)
				{
					CalmFor = 0.0f;
					break;
				}
			}
		}
	}

	const EMood Now = Playable(WantedMood());
	if (Now != Mood)
	{
		Mood = Now;
		UE_LOG(LogForestGame, Log, TEXT("Music: %s."), MoodNames[Mood]);
	}

	// Softer in the pause menu, and always under the Music setting.
	const float Setting = ForestSettings::MusicMultiplier() * (World->IsPaused() ? 0.5f : 1.0f);
	const float Fade = DeltaSeconds / (Mood == Danger ? DangerFadeSeconds : FadeSeconds);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USoundBase* Track = Tracks[Index];
		if (!Track)
		{
			continue;
		}
		Volume[Index] = FMath::FInterpConstantTo(Volume[Index], Index == Mood ? 1.0f : 0.0f, 1.0f, Fade);
		TObjectPtr<UAudioComponent>& Player = Players[Index];
		if (Volume[Index] <= 0.001f)
		{
			if (Player && Player->IsPlaying())
			{
				Player->Stop();
			}
			continue;
		}
		if (!Player)
		{
			Player = UGameplayStatics::CreateSound2D(World, Track, 1.0f, 1.0f, 0.0f, nullptr, false, false);
			if (!Player)
			{
				continue;
			}
			// Keeps playing in the pause menu.
			Player->bIsUISound = true;
		}
		// Each track loops, even if its sound file does not.
		if (!Player->IsPlaying())
		{
			Player->Play();
		}
		Player->SetVolumeMultiplier(FMath::Max(0.001f, Volume[Index] * Volume[Index] * Setting));
	}
}
