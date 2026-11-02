#include "ForestPickup.h"

#include "Components/StaticMeshComponent.h"

AForestPickup::AForestPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Mesh;

	DisplayName = FText::FromString(TEXT("the mushroom"));
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

	// Picked up: rise and shrink over a third of a second, then vanish.
	CollectAge += DeltaSeconds;
	const float Alpha = FMath::Clamp(CollectAge / 0.35f, 0.0f, 1.0f);
	SetActorLocation(BaseLocation + FVector(0.0f, 0.0f, 60.0f * Alpha));
	SetActorScale3D(BaseScale * FMath::Max(0.01f, 1.0f - Alpha));
	if (Alpha >= 1.0f)
	{
		Destroy();
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
	BaseLocation = GetActorLocation();
	OnCollected();
}
