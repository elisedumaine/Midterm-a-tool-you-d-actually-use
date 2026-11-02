#include "ForestAmbience.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
	UInstancedStaticMeshComponent* MakeLayer(AForestAmbience* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Layer = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Layer->SetupAttachment(Root);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		return Layer;
	}

	void ApplyTransforms(UInstancedStaticMeshComponent* Layer, const TArray<FTransform>& Transforms)
	{
		if (Layer && Transforms.Num() && Layer->GetInstanceCount() == Transforms.Num())
		{
			Layer->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
		}
	}

	void ResetLayer(UInstancedStaticMeshComponent* Layer, int32 Count)
	{
		Layer->ClearInstances();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Layer->AddInstance(FTransform::Identity, true);
		}
	}
}

AForestAmbience::AForestAmbience()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Motes = MakeLayer(this, RootComponent, TEXT("Motes"));
	ButterflyWingsA = MakeLayer(this, RootComponent, TEXT("ButterflyWingsA"));
	ButterflyWingsB = MakeLayer(this, RootComponent, TEXT("ButterflyWingsB"));
	DragonflyWings = MakeLayer(this, RootComponent, TEXT("DragonflyWings"));
	Bodies = MakeLayer(this, RootComponent, TEXT("Bodies"));
}

void AForestAmbience::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Populate();
}

void AForestAmbience::BeginPlay()
{
	Super::BeginPlay();
	Populate();
}

FVector AForestAmbience::Viewpoint() const
{
	const UWorld* World = GetWorld();
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		return World->ViewLocationsRenderedLastFrame[0];
	}
	return WorldCenter + FVector(0.0f, 0.0f, 200.0f);
}

float AForestAmbience::GroundHeight(float X, float Y) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AmbienceGround), false, this);
	const FVector Start(X, Y, 4000.0f);
	const FVector End(X, Y, -1000.0f);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return 0.0f;
}

FVector AForestAmbience::RandomPointNear(const FVector& Center, float Radius) const
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = Radius * FMath::Sqrt(FMath::FRand());
	FVector Point(Center.X + FMath::Cos(Angle) * Distance, Center.Y + FMath::Sin(Angle) * Distance, 0.0f);
	FVector Offset = Point - WorldCenter;
	Offset.Z = 0.0f;
	if (Offset.Size() > WorldRadius)
	{
		Offset = Offset.GetSafeNormal() * WorldRadius;
		Point = FVector(WorldCenter.X + Offset.X, WorldCenter.Y + Offset.Y, 0.0f);
	}
	return Point;
}

FVector AForestAmbience::MoteSpawn(const FVector& Around, bool bAnywhere) const
{
	// Respawned motes appear in the outer part of the sphere so they drift in
	// from the distance instead of popping up next to the camera.
	const float Distance = MoteRadius * (bAnywhere ? FMath::Sqrt(FMath::FRand()) : FMath::FRandRange(0.6f, 1.0f));
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float X = Around.X + FMath::Cos(Angle) * Distance;
	const float Y = Around.Y + FMath::Sin(Angle) * Distance;
	return FVector(X, Y, GroundHeight(X, Y) + FMath::FRandRange(40.0f, 520.0f));
}

void AForestAmbience::Populate()
{
	if (!Motes || !GetWorld())
	{
		return;
	}
	const FVector View = Viewpoint();
	MoteState.SetNum(FMath::Max(0, MoteCount));
	for (FMote& Mote : MoteState)
	{
		Mote.Position = MoteSpawn(View, true);
		Mote.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
		Mote.Size = FMath::FRandRange(0.6f, 1.4f);
	}

	auto MakeFlyers = [this](TArray<FFlyer>& Flyers, int32 Count, float MinHeight, float MaxHeight, float MinSize, float MaxSize)
	{
		Flyers.SetNum(FMath::Max(0, Count));
		for (FFlyer& Flyer : Flyers)
		{
			Flyer.Home = RandomPointNear(WorldCenter, WorldRadius);
			Flyer.Position = Flyer.Home + FVector(0.0f, 0.0f, GroundHeight(Flyer.Home.X, Flyer.Home.Y) + FMath::FRandRange(MinHeight, MaxHeight));
			Flyer.Target = Flyer.Position;
			Flyer.Yaw = FMath::FRandRange(0.0f, 360.0f);
			Flyer.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
			Flyer.Pause = FMath::FRandRange(0.0f, 2.0f);
			Flyer.Size = FMath::FRandRange(MinSize, MaxSize);
		}
	};
	MakeFlyers(Butterflies, ButterflyCount, 60.0f, 220.0f, 0.9f, 1.4f);
	MakeFlyers(Dragonflies, DragonflyCount, 50.0f, 180.0f, 0.9f, 1.2f);

	ResetLayer(Motes, MoteState.Num());
	ResetLayer(ButterflyWingsA, ((Butterflies.Num() + 1) / 2) * 2);
	ResetLayer(ButterflyWingsB, (Butterflies.Num() / 2) * 2);
	ResetLayer(DragonflyWings, Dragonflies.Num() * 4);
	ResetLayer(Bodies, Butterflies.Num() + Dragonflies.Num());
	bPopulated = true;
	Animate(0.0f);
}

FTransform AForestAmbience::WingTransform(const FVector& Body, float Yaw, float Flap, float Side, float Width, float Length, float Offset)
{
	// A wing is a flat plane hinged along the body: Flap lifts it around the
	// body's long axis, Side picks the left or right wing.
	const float Lift = FMath::DegreesToRadians(Flap);
	const FVector Span(0.0f, Side * FMath::Cos(Lift), FMath::Sin(Lift));
	const FQuat Heading = FRotator(0.0f, Yaw, 0.0f).Quaternion();
	const FQuat Local = FRotationMatrix::MakeFromXY(FVector::ForwardVector, Span).ToQuat();
	const FVector Centre = FVector(Offset, 0.0f, 0.0f) + Span * (Width * 0.5f);
	// The engine plane is 100 x 100 units: X is the wing's chord, Y its span.
	return FTransform(Heading * Local, Body + Heading.RotateVector(Centre), FVector(Length / 100.0f, Width / 100.0f, 1.0f));
}

void AForestAmbience::UpdateMotes(float DeltaSeconds, const FVector& View)
{
	TArray<FTransform> Transforms;
	Transforms.Reserve(MoteState.Num());
	for (FMote& Mote : MoteState)
	{
		const FVector Drift(
			FMath::Sin(Time * 0.7f + Mote.Phase) * 22.0f,
			FMath::Cos(Time * 0.5f + Mote.Phase * 1.3f) * 22.0f,
			FMath::Sin(Time * 0.9f + Mote.Phase) * 10.0f);
		Mote.Position += (Wind + Drift) * DeltaSeconds;
		if (FVector::Dist2D(Mote.Position, View) > MoteRadius || Mote.Position.Z > View.Z + 1200.0f)
		{
			Mote.Position = MoteSpawn(View, false);
		}
		// A slow twinkle, like pollen catching the low sun.
		const float Twinkle = 0.65f + 0.35f * FMath::Sin(Time * 1.8f + Mote.Phase * 3.0f);
		const float Scale = 0.085f * Mote.Size * Twinkle;
		Transforms.Add(FTransform(FQuat::Identity, Mote.Position, FVector(Scale)));
	}
	ApplyTransforms(Motes, Transforms);
}

void AForestAmbience::UpdateButterflies(float DeltaSeconds)
{
	TArray<FTransform> WingsA;
	TArray<FTransform> WingsB;
	TArray<FTransform> BodyTransforms;
	for (int32 Index = 0; Index < Butterflies.Num(); ++Index)
	{
		FFlyer& Fly = Butterflies[Index];
		FVector ToTarget = Fly.Target - Fly.Position;
		if (Fly.Pause > 0.0f)
		{
			Fly.Pause -= DeltaSeconds;
		}
		else if (ToTarget.Size() < 40.0f)
		{
			const FVector Next = RandomPointNear(Fly.Home, 700.0f);
			Fly.Target = FVector(Next.X, Next.Y, GroundHeight(Next.X, Next.Y) + FMath::FRandRange(50.0f, 220.0f));
			Fly.Pause = FMath::FRandRange(0.0f, 1.2f);
		}
		else
		{
			const FVector Step = ToTarget.GetSafeNormal() * 160.0f * DeltaSeconds;
			Fly.Position += Step + FVector(0.0f, 0.0f, FMath::Sin(Time * 3.0f + Fly.Phase) * 40.0f * DeltaSeconds);
			const float DesiredYaw = ToTarget.Rotation().Yaw;
			Fly.Yaw = FMath::FixedTurn(Fly.Yaw, DesiredYaw, 240.0f * DeltaSeconds);
		}
		const float Flap = 15.0f + 55.0f * FMath::Sin(Time * 14.0f + Fly.Phase);
		TArray<FTransform>& Wings = (Index % 2 == 0) ? WingsA : WingsB;
		Wings.Add(WingTransform(Fly.Position, Fly.Yaw, Flap, 1.0f, 9.0f * Fly.Size, 8.0f * Fly.Size, 0.0f));
		Wings.Add(WingTransform(Fly.Position, Fly.Yaw, Flap, -1.0f, 9.0f * Fly.Size, 8.0f * Fly.Size, 0.0f));
		BodyTransforms.Add(FTransform(FRotator(0.0f, Fly.Yaw, 0.0f), Fly.Position, FVector(0.09f, 0.015f, 0.015f) * Fly.Size));
	}
	ApplyTransforms(ButterflyWingsA, WingsA);
	ApplyTransforms(ButterflyWingsB, WingsB);

	// Dragonfly bodies follow the butterflies' in the shared Bodies layer.
	for (FFlyer& Fly : Dragonflies)
	{
		BodyTransforms.Add(FTransform(FRotator(0.0f, Fly.Yaw, 0.0f), Fly.Position, FVector(0.24f, 0.012f, 0.012f) * Fly.Size));
	}
	ApplyTransforms(Bodies, BodyTransforms);
}

void AForestAmbience::UpdateDragonflies(float DeltaSeconds)
{
	TArray<FTransform> Wings;
	for (FFlyer& Fly : Dragonflies)
	{
		const FVector ToTarget = Fly.Target - Fly.Position;
		if (Fly.Pause > 0.0f)
		{
			// Hovering: tiny jitters in place.
			Fly.Pause -= DeltaSeconds;
			Fly.Position += FVector(FMath::Sin(Time * 9.0f + Fly.Phase), FMath::Cos(Time * 7.0f + Fly.Phase), FMath::Sin(Time * 5.0f)) * 12.0f * DeltaSeconds;
		}
		else if (ToTarget.Size() < 30.0f)
		{
			const FVector Next = RandomPointNear(Fly.Home, 900.0f);
			Fly.Target = FVector(Next.X, Next.Y, GroundHeight(Next.X, Next.Y) + FMath::FRandRange(50.0f, 180.0f));
			Fly.Pause = FMath::FRandRange(0.8f, 2.6f);
			Fly.Yaw = (Fly.Target - Fly.Position).Rotation().Yaw;
		}
		else
		{
			// A quick dart to the next spot.
			Fly.Position += ToTarget.GetSafeNormal() * FMath::Min(ToTarget.Size(), 650.0f * DeltaSeconds);
		}
		const float Flap = 25.0f * FMath::Sin(Time * 45.0f + Fly.Phase);
		for (const float Offset : { 3.0f, -3.0f })
		{
			Wings.Add(WingTransform(Fly.Position, Fly.Yaw, Flap, 1.0f, 15.0f * Fly.Size, 3.5f * Fly.Size, Offset * Fly.Size));
			Wings.Add(WingTransform(Fly.Position, Fly.Yaw, Flap, -1.0f, 15.0f * Fly.Size, 3.5f * Fly.Size, Offset * Fly.Size));
		}
	}
	ApplyTransforms(DragonflyWings, Wings);
}

void AForestAmbience::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPopulated)
	{
		Animate(DeltaSeconds);
	}
}

void AForestAmbience::Animate(float DeltaSeconds)
{
	// Large editor hitches should not fling everything across the map.
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
	Time += DeltaSeconds;
	UpdateMotes(DeltaSeconds, Viewpoint());
	UpdateDragonflies(DeltaSeconds);
	UpdateButterflies(DeltaSeconds);
}
