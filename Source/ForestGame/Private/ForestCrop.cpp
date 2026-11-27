#include "ForestCrop.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ForestCrafting.h"
#include "ForestGameManager.h"
#include "ForestSky.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UStaticMesh* Kenney(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/WorldBuilder/Imported/Kenney/%s.%s"), Name, Name));
	}

	UMaterialInstanceDynamic* Paint(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint"));
		if (!Tint)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Tint, Outer);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
		Material->SetScalarParameterValue(TEXT("Sway"), 0.0f);
		return Material;
	}
}

AForestCrop::AForestCrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AForestCrop::Plant(FName NewSeed, float StartGrowth)
{
	Seed = NewSeed;
	Growth = FMath::Clamp(StartGrowth, 0.0f, 1.0f);
	const bool bBerries = Seed == TEXT("BerrySeeds");
	GrowSeconds = bBerries ? 150.0f : 110.0f;

	// A patch of dug earth.
	Soil = NewObject<UStaticMeshComponent>(this);
	Soil->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	Soil->SetupAttachment(RootComponent);
	Soil->SetRelativeLocation(FVector(0.0f, 0.0f, 1.0f));
	Soil->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.04f));
	Soil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Soil->RegisterComponent();
	if (UMaterialInstanceDynamic* Earth = Paint(this, FLinearColor(0.16f, 0.09f, 0.05f)))
	{
		Soil->SetMaterial(0, Earth);
	}

	PlantMesh = NewObject<UStaticMeshComponent>(this);
	PlantMesh->SetStaticMesh(Kenney(bBerries ? TEXT("plant_bushSmall") : TEXT("mushroom_tanGroup")));
	PlantMesh->SetupAttachment(RootComponent);
	PlantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlantMesh->RegisterComponent();
	if (const AForestGameManager* Manager = AForestGameManager::Get(this))
	{
		Manager->DressLikeWorld(PlantMesh);
	}

	// The fruit: red berries on the bush (the mushrooms are their own fruit).
	if (bBerries)
	{
		Fruit = NewObject<UInstancedStaticMeshComponent>(this);
		Fruit->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Fruit->SetupAttachment(RootComponent);
		Fruit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fruit->SetCastShadow(false);
		Fruit->RegisterComponent();
		if (UMaterialInstanceDynamic* Red = Paint(this, FLinearColor(0.85f, 0.06f, 0.16f)))
		{
			Fruit->SetMaterial(0, Red);
		}
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const float Angle = Index * 2.0f * PI / 7.0f + FMath::FRandRange(-0.3f, 0.3f);
			const FVector Spot(FMath::Cos(Angle) * 26.0f, FMath::Sin(Angle) * 26.0f, FMath::FRandRange(32.0f, 46.0f));
			Fruit->AddInstance(FTransform(FRotator::ZeroRotator, Spot, FVector(0.09f)));
		}
	}
	ShowGrowth();
}

void AForestCrop::ShowGrowth()
{
	if (!PlantMesh)
	{
		return;
	}
	// From a sprout barely out of the soil to a full plant.
	const float Size = FMath::Lerp(0.25f, 2.0f, FMath::Pow(Growth, 0.8f));
	const float Pulse = IsRipe() ? 1.0f + 0.03f * FMath::Sin(Time * 3.0f) : 1.0f;
	PlantMesh->SetRelativeScale3D(FVector(Size * Pulse));
	if (Fruit)
	{
		Fruit->SetVisibility(IsRipe());
	}
}

void AForestCrop::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	if (!IsRipe())
	{
		const AForestSky* Sky = AForestSky::Get(this);
		const float Rain = Sky && Sky->IsRaining() ? 2.0f : 1.0f;
		Growth = FMath::Min(1.0f, Growth + DeltaSeconds * Rain / GrowSeconds);
	}
	ShowGrowth();
}

FName AForestCrop::Harvest(int32& OutAmount)
{
	OutAmount = 0;
	if (!IsRipe())
	{
		return NAME_None;
	}
	Growth = 0.0f;
	ShowGrowth();
	const bool bBerries = Seed == TEXT("BerrySeeds");
	OutAmount = bBerries ? 4 : 3;
	return bBerries ? ForestCrafting::Berry : ForestCrafting::Mushroom;
}
