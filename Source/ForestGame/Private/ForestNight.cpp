#include "ForestNight.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ForestBlocks.h"
#include "ForestBuilding.h"
#include "ForestGame.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestNightBeast.h"
#include "ForestSky.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** Same reach as in ForestNightBeast.cpp: the beasts stop at the edge of this light. */
	constexpr float CampfireLight = 1000.0f;
	constexpr float LanternLight = 450.0f;
	/** The first beasts come at nightfall, then one more this often. */
	constexpr int32 FirstBeasts = 2;
	constexpr float SecondsPerBeast = 35.0f;
}

bool UForestNight::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestNight::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestNight, STATGROUP_Tickables);
}

UForestNight* UForestNight::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestNight>() : nullptr;
}

void UForestNight::Say(const FString& Text, float Seconds) const
{
	if (AForestGameManager* Manager = AForestGameManager::Get(GetWorld()))
	{
		Manager->ShowMessage(FText::FromString(Text), Seconds);
	}
}

bool UForestNight::IsInLight(const FVector& Where) const
{
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		const FName Type = It->GetBuildingType();
		const float Radius = Type == TEXT("Campfire") ? CampfireLight : Type == TEXT("Lantern") ? LanternLight : 0.0f;
		if (Radius > 0.0f && FVector::Dist2D(Where, It->GetActorLocation()) < Radius)
		{
			return true;
		}
	}
	if (const UForestBlocks* Blocks = UForestBlocks::Get(GetWorld()))
	{
		for (const FVector& Glow : Blocks->GetGlowSpots())
		{
			if (FVector::Dist2D(Where, Glow) < LanternLight)
			{
				return true;
			}
		}
	}
	return false;
}

void UForestNight::SpawnBeast(const FVector& Around)
{
	UWorld* World = GetWorld();
	for (int32 Try = 0; Try < 12; ++Try)
	{
		// Out in the dark, beyond what the player can see clearly.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Spot = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(2600.0f, 3600.0f);
		if (IsInLight(Spot))
		{
			continue;
		}
		FHitResult Hit;
		const FVector Start(Spot.X, Spot.Y, Around.Z + 6000.0f);
		const FVector End(Spot.X, Spot.Y, Around.Z - 6000.0f);
		if (!World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic))
			|| Hit.ImpactNormal.Z < 0.75f || Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			// Not open ground (a tree top, a rock, a cliff): try elsewhere.
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FRotator Facing(0.0f, (Around - Spot).Rotation().Yaw, 0.0f);
		AForestNightBeast* Beast = World->SpawnActor<AForestNightBeast>(AForestNightBeast::StaticClass(), Hit.ImpactPoint + FVector(0.0f, 0.0f, 80.0f), Facing, Params);
		if (Beast)
		{
			// Mostly wolves, now and then a black boar.
			Beast->Setup(Spawned % 3 == 2 ? 1 : 0);
			Beasts.Add(Beast);
			++Spawned;
		}
		return;
	}
}

void UForestNight::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const AForestSky* Sky = World ? AForestSky::Get(World) : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	// Nothing happens during the title screen and the wake-up intro.
	if (!Sky || !Player || Controller->IsMoveInputIgnored())
	{
		return;
	}
	Beasts.RemoveAll([](const TObjectPtr<AForestNightBeast>& Beast) { return !IsValid(Beast); });
	WarnCooldown -= DeltaSeconds;

	const bool bNowNight = Sky->IsNight();
	if (bNowNight && !bNight)
	{
		bNight = true;
		NightTime = 0.0f;
		SpawnTimer = 0.0f;
		Spawned = 0;
		Say(ForestLanguage::Pick(TEXT("Night falls... Something is moving in the dark. Stay close to a campfire!"),
			TEXT("La nuit tombe... Quelque chose bouge dans le noir. Reste près d'un feu de camp !")), 6.0f);
		for (int32 Index = 0; Index < FirstBeasts; ++Index)
		{
			SpawnBeast(Player->GetActorLocation());
		}
		UE_LOG(LogForestGame, Log, TEXT("Night: %d beasts come out."), Beasts.Num());
	}
	else if (!bNowNight && bNight)
	{
		bNight = false;
		for (AForestNightBeast* Beast : Beasts)
		{
			Beast->Flee();
		}
		if (Beasts.Num() > 0)
		{
			Say(ForestLanguage::Pick(TEXT("Dawn! The creatures of the night flee into the trees."),
				TEXT("L'aube ! Les créatures de la nuit s'enfuient dans les arbres.")), 5.0f);
		}
	}
	if (!bNight)
	{
		return;
	}

	// More of them as the night goes on.
	NightTime += DeltaSeconds;
	SpawnTimer += DeltaSeconds;
	if (SpawnTimer > SecondsPerBeast && Beasts.Num() < MaxBeasts)
	{
		SpawnTimer = 0.0f;
		SpawnBeast(Player->GetActorLocation());
	}

	// Lost far behind: it comes back out of the dark nearer the player.
	const FVector Here = Player->GetActorLocation();
	AForestNightBeast* Nearest = nullptr;
	float NearestDistance = BIG_NUMBER;
	int32 Lost = 0;
	for (AForestNightBeast* Beast : Beasts)
	{
		const float Distance = FVector::Dist2D(Here, Beast->GetActorLocation());
		if (Distance > 7000.0f && !Beast->IsFleeing())
		{
			Beast->Destroy();
			++Lost;
			continue;
		}
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Beast;
		}
	}

	Beasts.RemoveAll([](const TObjectPtr<AForestNightBeast>& Beast) { return !IsValid(Beast); });
	for (int32 Index = 0; Index < Lost; ++Index)
	{
		SpawnBeast(Here);
	}

	// A warning the first time red eyes come close in the dark.
	if (Nearest && NearestDistance < 1600.0f && WarnCooldown <= 0.0f && !IsInLight(Here))
	{
		WarnCooldown = 25.0f;
		FString Name = Nearest->GetBeastName();
		Name[0] = FChar::ToUpper(Name[0]);
		Say(FOREST_PRINTF(TEXT("Red eyes in the dark... %s is hunting you. Run to a fire!"), TEXT("Des yeux rouges dans le noir... %s te chasse. Cours vers un feu !"), *Name), 4.0f);
	}
}
