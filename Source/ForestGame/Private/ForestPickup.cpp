#include "ForestPickup.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "ForestCrafting.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}
}

AForestPickup::AForestPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;

	DisplayName = FText::FromString(TEXT("the mushroom"));
}

void AForestPickup::Configure(FName NewItem)
{
	Item = NewItem;
	Amount = 1;
	if (Item == ForestCrafting::Wood)
	{
		Mesh->SetStaticMesh(LoadMesh(FMath::RandBool() ? TEXT("/Game/WorldBuilder/Imported/Kenney/log.log") : TEXT("/Game/WorldBuilder/Imported/Kenney/log_stack.log_stack")));
		SetActorScale3D(FVector(2.4f));
		Amount = 2;
		PulseAmount = 0.03f;
		SpinSpeed = 0.0f;
		RegrowSeconds = 300.0f;
	}
	else if (Item == ForestCrafting::Stone)
	{
		static const TCHAR* Stones[] = {
			TEXT("/Game/WorldBuilder/Imported/Kenney/stone_smallA.stone_smallA"),
			TEXT("/Game/WorldBuilder/Imported/Kenney/stone_smallC.stone_smallC"),
			TEXT("/Game/WorldBuilder/Imported/Kenney/stone_smallE.stone_smallE"),
		};
		Mesh->SetStaticMesh(LoadMesh(Stones[FMath::RandRange(0, 2)]));
		SetActorScale3D(FVector(1.8f));
		PulseAmount = 0.03f;
		SpinSpeed = 0.0f;
		RegrowSeconds = 300.0f;
	}
	else if (Item == ForestCrafting::Berry)
	{
		// A bush that keeps its leaves; a handful of red berries sits on top.
		Mesh->SetStaticMesh(LoadMesh(TEXT("/Game/WorldBuilder/Imported/Kenney/plant_bushLarge.plant_bushLarge")));
		SetActorScale3D(FVector(2.6f));
		Amount = 3;
		PulseAmount = 0.0f;
		SpinSpeed = 0.0f;
		RegrowSeconds = 180.0f;

		Berries = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Berries"));
		Berries->SetStaticMesh(LoadMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Berries->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Berries->SetCastShadow(false);
		Berries->SetupAttachment(Mesh);
		Berries->RegisterComponent();
		if (UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint")))
		{
			UMaterialInstanceDynamic* Red = UMaterialInstanceDynamic::Create(Tint, this);
			Red->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.85f, 0.06f, 0.16f));
			Red->SetScalarParameterValue(TEXT("Sway"), 0.0f);
			Berries->SetMaterial(0, Red);
		}
		const FBox Bounds = Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-20.0f), FVector(20.0f));
		const FVector Extent = Bounds.GetExtent();
		for (int32 Index = 0; Index < 9; ++Index)
		{
			// On the upper surface of the bush, in the bush's own units.
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Ring = FMath::FRandRange(0.45f, 0.9f);
			const FVector Spot(Bounds.GetCenter().X + FMath::Cos(Angle) * Extent.X * Ring,
				Bounds.GetCenter().Y + FMath::Sin(Angle) * Extent.Y * Ring,
				Bounds.Min.Z + Extent.Z * 2.0f * FMath::FRandRange(0.55f, 0.95f));
			Berries->AddInstance(FTransform(FRotator::ZeroRotator, Spot, FVector(0.035f)));
		}
	}
	DisplayName = FText::FromString(ForestCrafting::ItemName(Item));
	BaseLocation = GetActorLocation();
	BaseScale = GetActorScale3D();
}

void AForestPickup::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	BaseScale = GetActorScale3D();
	// Desynchronise the pulse so a patch of mushrooms does not breathe in unison.
	Age = FMath::FRandRange(0.0f, 6.0f);
}

void AForestPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (!bCollected)
	{
		const float Pulse = 1.0f + PulseAmount * FMath::Sin(Age * 3.0f);
		SetActorScale3D(BaseScale * Pulse);
		AddActorLocalRotation(FRotator(0.0f, SpinSpeed * DeltaSeconds, 0.0f));
		return;
	}

	if (bVanished)
	{
		// Waiting to grow back.
		RegrowLeft -= DeltaSeconds;
		if (RegrowLeft > 0.0f)
		{
			return;
		}
		bCollected = false;
		bVanished = false;
		SetActorLocation(BaseLocation);
		SetActorScale3D(BaseScale);
		SetActorHiddenInGame(false);
		if (Berries)
		{
			Berries->SetVisibility(true);
		}
		return;
	}

	if (IsBush())
	{
		// The bush stays; its berries are gone until they grow back.
		Berries->SetVisibility(false);
		bVanished = true;
		RegrowLeft = RegrowSeconds;
		return;
	}

	// Picked up: rise and shrink over a third of a second, then vanish.
	CollectAge += DeltaSeconds;
	const float Alpha = FMath::Clamp(CollectAge / 0.35f, 0.0f, 1.0f);
	SetActorLocation(BaseLocation + FVector(0.0f, 0.0f, 60.0f * Alpha));
	SetActorScale3D(BaseScale * FMath::Max(0.01f, 1.0f - Alpha));
	if (Alpha >= 1.0f)
	{
		if (RegrowSeconds <= 0.0f)
		{
			Destroy();
			return;
		}
		SetActorHiddenInGame(true);
		bVanished = true;
		RegrowLeft = RegrowSeconds;
	}
}

void AForestPickup::Collect()
{
	if (bCollected)
	{
		return;
	}
	bCollected = true;
	CollectAge = 0.0f;
	if (!IsBush())
	{
		BaseLocation = GetActorLocation();
	}
	OnCollected();
}
