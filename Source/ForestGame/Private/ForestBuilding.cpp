#include "ForestBuilding.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ForestGameManager.h"
#include "ForestSky.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* Log = TEXT("/Game/WorldBuilder/Imported/Kenney/log.log");
	const TCHAR* LogLarge = TEXT("/Game/WorldBuilder/Imported/Kenney/log_large.log_large");
	const TCHAR* Stone = TEXT("/Game/WorldBuilder/Imported/Kenney/stone_smallA.stone_smallA");
	const TCHAR* Stump = TEXT("/Game/WorldBuilder/Imported/Kenney/stump_round.stump_round");
	const TCHAR* Lantern = TEXT("/Game/WorldBuilder/Imported/FantasyTown/lantern.lantern");
	const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	// Kenney's log lies along Y, 71 units long and about 17 thick.
	constexpr float LogLength = 71.0f;
	constexpr float LogThickness = 17.0f;
}

AForestBuilding::AForestBuilding()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AForestBuilding::AddPart(const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bCollides)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Part->SetupAttachment(RootComponent);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Scale);
	// Solid pieces block like the trees and rocks (the animals look for WorldStatic).
	Part->SetCollisionProfileName(bCollides && !bGhost ? TEXT("BlockAll") : TEXT("NoCollision"));
	Part->RegisterComponent();
	if (const AForestGameManager* Manager = AForestGameManager::Get(this))
	{
		Manager->DressLikeWorld(Part);
	}
	Parts.Add(Part);
	return Part;
}

void AForestBuilding::AddPost(const FVector& Foot, float Height, float Thickness)
{
	const float Across = Thickness / LogThickness;
	AddPart(Log, Foot + FVector(0.0f, 0.0f, Height * 0.5f), FRotator(0.0f, 0.0f, 90.0f), FVector(Across, Height / LogLength, Across));
}

void AForestBuilding::AddLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Range)
{
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(Range);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	LightIntensity = Intensity;
}

void AForestBuilding::Build(FName NewType, bool bAsGhost)
{
	Type = NewType;
	bGhost = bAsGhost;

	if (Type == TEXT("Campfire"))
	{
		Radius = 260.0f;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = Index * 2.0f * PI / 8.0f;
			AddPart(Stone, FVector(FMath::Cos(Angle) * 55.0f, FMath::Sin(Angle) * 55.0f, 4.0f), FRotator(0.0f, Index * 47.0f, 0.0f), FVector(0.85f), false);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddPart(Log, FVector(0.0f, 0.0f, 6.0f + Index * 4.0f), FRotator(0.0f, Index * 60.0f, 0.0f), FVector(1.1f, 1.2f, 1.1f), false);
		}
		// Flames: glowing cones that flicker (see Tick).
		const FVector FlameSpots[] = {FVector(0.0f, 0.0f, 38.0f), FVector(12.0f, 8.0f, 30.0f), FVector(-10.0f, -9.0f, 28.0f)};
		const float FlameSizes[] = {1.0f, 0.7f, 0.6f};
		UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow"));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UStaticMeshComponent* Flame = AddPart(Cone, FlameSpots[Index], FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.6f) * FlameSizes[Index], false);
			Flame->SetCastShadow(false);
			if (Glow && !bGhost)
			{
				UMaterialInstanceDynamic* Fire = UMaterialInstanceDynamic::Create(Glow, this);
				Fire->SetVectorParameterValue(TEXT("Tint"), Index == 0 ? FLinearColor(1.0f, 0.42f, 0.08f) : FLinearColor(1.0f, 0.75f, 0.2f));
				Fire->SetScalarParameterValue(TEXT("Glow"), 4.0f);
				Flame->SetMaterial(0, Fire);
			}
			Flames.Add(Flame);
		}
		if (!bGhost)
		{
			AddLight(FVector(0.0f, 0.0f, 70.0f), FLinearColor(1.0f, 0.55f, 0.22f), 90.0f, 1600.0f);
		}
	}
	else if (Type == TEXT("Feeder"))
	{
		Radius = 160.0f;
		AddPart(Stump, FVector(0.0f, -55.0f, 4.0f), FRotator::ZeroRotator, FVector(1.5f));
		AddPart(Stump, FVector(0.0f, 55.0f, 4.0f), FRotator::ZeroRotator, FVector(1.5f));
		AddPart(LogLarge, FVector(0.0f, 0.0f, 22.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(1.5f, 1.1f, 0.9f));
		// The berries waiting in the feeder, one ball each.
		FoodPile = NewObject<UInstancedStaticMeshComponent>(this);
		FoodPile->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Sphere));
		FoodPile->SetupAttachment(RootComponent);
		FoodPile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FoodPile->SetCastShadow(false);
		FoodPile->RegisterComponent();
		if (UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint")))
		{
			UMaterialInstanceDynamic* Red = UMaterialInstanceDynamic::Create(Tint, this);
			Red->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.85f, 0.06f, 0.16f));
			Red->SetScalarParameterValue(TEXT("Sway"), 0.0f);
			FoodPile->SetMaterial(0, Red);
		}
		Parts.Add(FoodPile);
		Food = bGhost ? 0 : FeederCapacity;
		RefreshFood();
	}
	else if (Type == TEXT("Shelter"))
	{
		// A lean-to: tall posts in front, short ones at the back, a roof of logs between.
		Radius = 170.0f;
		const float Width = 280.0f;
		AddPost(FVector(95.0f, -Width * 0.42f, 0.0f), 205.0f, 22.0f);
		AddPost(FVector(95.0f, Width * 0.42f, 0.0f), 205.0f, 22.0f);
		AddPost(FVector(-95.0f, -Width * 0.42f, 0.0f), 95.0f, 22.0f);
		AddPost(FVector(-95.0f, Width * 0.42f, 0.0f), 95.0f, 22.0f);
		const int32 RoofLogs = 7;
		for (int32 Index = 0; Index < RoofLogs; ++Index)
		{
			const float Alpha = Index / float(RoofLogs - 1);
			const FVector Spot(FMath::Lerp(105.0f, -105.0f, Alpha), 0.0f, FMath::Lerp(215.0f, 105.0f, Alpha));
			AddPart(Log, Spot, FRotator::ZeroRotator, FVector(2.0f, Width / LogLength, 2.0f), false);
		}
	}
	else if (Type == TEXT("Fence"))
	{
		Radius = 130.0f;
		AddPost(FVector(0.0f, -100.0f, 0.0f), 115.0f, 20.0f);
		AddPost(FVector(0.0f, 100.0f, 0.0f), 115.0f, 20.0f);
		AddPart(Log, FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, FVector(1.1f, 220.0f / LogLength, 1.1f));
		AddPart(Log, FVector(0.0f, 0.0f, 85.0f), FRotator::ZeroRotator, FVector(1.1f, 220.0f / LogLength, 1.1f));
	}
	else if (Type == TEXT("Lantern"))
	{
		Radius = 100.0f;
		AddPart(Lantern, FVector::ZeroVector, FRotator::ZeroRotator, FVector(1.1f));
		if (!bGhost)
		{
			AddLight(FVector(0.0f, 0.0f, 150.0f), FLinearColor(1.0f, 0.78f, 0.42f), 40.0f, 1200.0f);
		}
	}
	else if (Type == TEXT("Workbench"))
	{
		// A thick plank on two stumps, a stone and a log waiting on top.
		Radius = 140.0f;
		AddPart(Stump, FVector(0.0f, -50.0f, 4.0f), FRotator::ZeroRotator, FVector(1.7f, 1.7f, 2.2f));
		AddPart(Stump, FVector(0.0f, 50.0f, 4.0f), FRotator::ZeroRotator, FVector(1.7f, 1.7f, 2.2f));
		AddPart(LogLarge, FVector(0.0f, 0.0f, 40.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(1.7f, 1.4f, 0.6f));
		AddPart(Stone, FVector(8.0f, -30.0f, 58.0f), FRotator(0.0f, 30.0f, 0.0f), FVector(0.7f), false);
		AddPart(Log, FVector(-6.0f, 28.0f, 56.0f), FRotator(0.0f, 70.0f, 0.0f), FVector(0.8f), false);
	}
	else
	{
		// Bench: a big log resting on two stumps.
		Radius = 120.0f;
		AddPart(Stump, FVector(0.0f, -55.0f, 4.0f), FRotator::ZeroRotator, FVector(1.4f));
		AddPart(Stump, FVector(0.0f, 55.0f, 4.0f), FRotator::ZeroRotator, FVector(1.4f));
		AddPart(LogLarge, FVector(0.0f, 0.0f, 24.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(1.5f, 0.8f, 0.7f));
	}

	if (bGhost)
	{
		UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow"));
		if (Glow)
		{
			GhostMaterial = UMaterialInstanceDynamic::Create(Glow, this);
			GhostMaterial->SetScalarParameterValue(TEXT("Glow"), 0.6f);
			for (UStaticMeshComponent* Part : Parts)
			{
				for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
				{
					Part->SetMaterial(Slot, GhostMaterial);
				}
				Part->SetCastShadow(false);
			}
		}
		SetGhostValid(true);
	}
}

void AForestBuilding::SetGhostValid(bool bValid)
{
	if (GhostMaterial)
	{
		GhostMaterial->SetVectorParameterValue(TEXT("Tint"), bValid ? FLinearColor(0.25f, 1.0f, 0.45f) : FLinearColor(1.0f, 0.18f, 0.12f));
	}
}

void AForestBuilding::RefreshFood()
{
	if (!FoodPile)
	{
		return;
	}
	FoodPile->ClearInstances();
	for (int32 Index = 0; Index < Food; ++Index)
	{
		// Two rows of berries along the trough.
		const float Along = -45.0f + (Index / 2) * 30.0f;
		const float Across = (Index % 2) ? 9.0f : -9.0f;
		FoodPile->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Across, Along, 52.0f), FVector(0.13f)));
	}
}

bool AForestBuilding::TakeFood()
{
	if (Food <= 0)
	{
		return false;
	}
	--Food;
	RefreshFood();
	return true;
}

int32 AForestBuilding::AddFood(int32 Count)
{
	const int32 Added = FMath::Clamp(FeederCapacity - Food, 0, Count);
	Food += Added;
	RefreshFood();
	return Added;
}

void AForestBuilding::SetFood(int32 Count)
{
	Food = FMath::Clamp(Count, 0, FeederCapacity);
	RefreshFood();
}

void AForestBuilding::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// The fire flickers: flames stretch and shrink, the light breathes with them.
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		const float Phase = Time * (7.0f + Index * 2.3f) + Index * 1.7f;
		const float Stretch = 1.0f + 0.18f * FMath::Sin(Phase) + 0.08f * FMath::Sin(Phase * 2.7f);
		const FVector Base = FVector(0.32f, 0.32f, 0.6f) * (Index == 0 ? 1.0f : Index == 1 ? 0.7f : 0.6f);
		Flames[Index]->SetRelativeScale3D(FVector(Base.X * (2.0f - Stretch), Base.Y * (2.0f - Stretch), Base.Z * Stretch));
	}
	if (!Light)
	{
		return;
	}
	if (Type == TEXT("Campfire"))
	{
		Light->SetIntensity(LightIntensity * (0.85f + 0.15f * FMath::Sin(Time * 11.0f) * FMath::Sin(Time * 4.3f)));
	}
	else if (Type == TEXT("Lantern"))
	{
		// Lit from dusk to dawn.
		const AForestSky* Sky = AForestSky::Get(this);
		const bool bOn = !Sky || Sky->IsNight() || Sky->GetHour() > 19.0f;
		Light->SetVisibility(bOn);
	}
}
