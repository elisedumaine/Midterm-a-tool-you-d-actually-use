#include "ForestBirds.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	UInstancedStaticMeshComponent* MakeBirdLayer(AForestBirds* Owner, USceneComponent* Root, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Layer = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Layer->SetupAttachment(Root);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		return Layer;
	}

	void FillLayer(UInstancedStaticMeshComponent* Layer, int32 Count)
	{
		Layer->ClearInstances();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Layer->AddInstance(FTransform::Identity, true);
		}
	}

	void PushLayer(UInstancedStaticMeshComponent* Layer, const TArray<FTransform>& Transforms)
	{
		if (Layer && Transforms.Num() && Layer->GetInstanceCount() == Transforms.Num())
		{
			Layer->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
		}
	}

	/** A flat wing hinged along the body; Flap lifts it, Side picks left or right. */
	FTransform Wing(const FVector& Root, float Yaw, float Flap, float Side, float Span, float Chord, float Offset)
	{
		const float Lift = FMath::DegreesToRadians(Flap);
		const FVector Out(0.0f, Side * FMath::Cos(Lift), FMath::Sin(Lift));
		const FQuat Heading = FRotator(0.0f, Yaw, 0.0f).Quaternion();
		const FQuat Local = FRotationMatrix::MakeFromXY(FVector::ForwardVector, Out).ToQuat();
		const FVector Centre = FVector(Offset, 0.0f, 0.0f) + Out * (Span * 0.5f);
		return FTransform(Heading * Local, Root + Heading.RotateVector(Centre), FVector(Chord / 100.0f, Span / 100.0f, 1.0f));
	}

	float Ease(float Alpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}
}

AForestBirds::AForestBirds()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SmallBodies = MakeBirdLayer(this, RootComponent, TEXT("SmallBodies"));
	SmallHeads = MakeBirdLayer(this, RootComponent, TEXT("SmallHeads"));
	SmallWings = MakeBirdLayer(this, RootComponent, TEXT("SmallWings"));
	LargeBodies = MakeBirdLayer(this, RootComponent, TEXT("LargeBodies"));
	LargeHeads = MakeBirdLayer(this, RootComponent, TEXT("LargeHeads"));
	LargeWings = MakeBirdLayer(this, RootComponent, TEXT("LargeWings"));
}

void AForestBirds::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Populate();
}

void AForestBirds::BeginPlay()
{
	Super::BeginPlay();
	Populate();
}

float AForestBirds::GroundHeight(float X, float Y) const
{
	// The first solid surface from above: open ground, a hilltop, or the top
	// of a tree, where a bird can perch just as well.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BirdGround), false, this);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, Y, 5000.0f), FVector(X, Y, -1000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return 0.0f;
}

FVector AForestBirds::RandomGroundPoint(const FVector& Near, float MinDistance, float MaxDistance) const
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
	FVector Offset(Near.X + FMath::Cos(Angle) * Distance - WorldCenter.X, Near.Y + FMath::Sin(Angle) * Distance - WorldCenter.Y, 0.0f);
	if (Offset.Size() > WorldRadius)
	{
		Offset = Offset.GetSafeNormal() * WorldRadius * FMath::FRandRange(0.5f, 0.95f);
	}
	const float X = WorldCenter.X + Offset.X;
	const float Y = WorldCenter.Y + Offset.Y;
	return FVector(X, Y, GroundHeight(X, Y));
}

void AForestBirds::FlyTo(FBird& Bird, const FVector& Destination, float Speed)
{
	Bird.From = Bird.Position;
	Bird.Target = Destination;
	Bird.Progress = 0.0f;
	Bird.FlightTime = FMath::Max(0.8f, FVector::Dist(Bird.From, Destination) / FMath::Max(1.0f, Speed));
	Bird.State = EBirdState::Flying;
}

void AForestBirds::PlayAt(USoundBase* Sound, const FVector& Location, float Volume, float Pitch) const
{
	// Only in play: the editor viewport stays silent while building worlds.
	if (Sound && GetWorld() && GetWorld()->IsGameWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.0f, Attenuation);
	}
}

void AForestBirds::Populate()
{
	if (!SmallBodies || !GetWorld())
	{
		return;
	}
	Small.Reset();
	Large.Reset();
	Flocks.SetNum(FMath::Max(0, SmallFlockCount));
	for (int32 FlockIndex = 0; FlockIndex < Flocks.Num(); ++FlockIndex)
	{
		FFlock& Flock = Flocks[FlockIndex];
		Flock.Home = RandomGroundPoint(WorldCenter, 0.0f, WorldRadius * 0.9f);
		Flock.Timer = FMath::FRandRange(25.0f, 60.0f);
		Flock.ChirpTimer = FMath::FRandRange(1.0f, 6.0f);
		Flock.bFlying = false;
		const int32 Members = FMath::RandRange(3, 6);
		for (int32 Member = 0; Member < Members; ++Member)
		{
			FBird Bird;
			const FVector Spot = RandomGroundPoint(Flock.Home, 0.0f, 150.0f);
			Bird.Position = Spot;
			Bird.Target = Spot;
			Bird.Yaw = FMath::FRandRange(0.0f, 360.0f);
			Bird.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
			Bird.Size = FMath::FRandRange(0.85f, 1.2f);
			Bird.Hop = FMath::FRandRange(0.5f, 3.0f);
			Bird.FlightTime = 0.0f;
			Bird.Flock = FlockIndex;
			Small.Add(Bird);
		}
	}
	for (int32 Index = 0; Index < LargeBirdCount; ++Index)
	{
		FBird Bird;
		const FVector Ground = RandomGroundPoint(WorldCenter, 0.0f, WorldRadius * 0.7f);
		Bird.Circle = Ground + FVector(0.0f, 0.0f, FMath::FRandRange(900.0f, 1500.0f));
		Bird.Radius = FMath::FRandRange(600.0f, 1300.0f);
		Bird.Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		Bird.Position = Bird.Circle + FVector(FMath::Cos(Bird.Angle), FMath::Sin(Bird.Angle), 0.0f) * Bird.Radius;
		Bird.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
		Bird.Size = FMath::FRandRange(0.9f, 1.2f);
		Bird.Timer = FMath::FRandRange(30.0f, 80.0f);
		Bird.Hop = FMath::FRandRange(5.0f, 25.0f);
		Bird.State = EBirdState::Soaring;
		Large.Add(Bird);
	}
	FillLayer(SmallBodies, Small.Num());
	FillLayer(SmallHeads, Small.Num());
	FillLayer(SmallWings, Small.Num() * 2);
	FillLayer(LargeBodies, Large.Num());
	FillLayer(LargeHeads, Large.Num());
	FillLayer(LargeWings, Large.Num() * 2);
	bPopulated = true;
	Draw();
}

void AForestBirds::LaunchFlock(int32 FlockIndex, const FVector& AwayFrom, bool bScared)
{
	FFlock& Flock = Flocks[FlockIndex];
	FVector Away = Flock.Home - AwayFrom;
	Away.Z = 0.0f;
	FVector NewHome;
	if (bScared && !Away.IsNearlyZero())
	{
		// Fly away from the player, then settle somewhere quieter.
		const FVector Ahead = Flock.Home + Away.GetSafeNormal() * FMath::FRandRange(1500.0f, 3000.0f);
		NewHome = RandomGroundPoint(Ahead, 0.0f, 500.0f);
	}
	else
	{
		NewHome = RandomGroundPoint(Flock.Home, 1200.0f, 3000.0f);
	}
	Flock.Home = NewHome;
	Flock.Timer = FMath::FRandRange(25.0f, 60.0f);
	Flock.bFlying = true;
	bool bChirped = false;
	for (FBird& Bird : Small)
	{
		if (Bird.Flock != FlockIndex)
		{
			continue;
		}
		FlyTo(Bird, RandomGroundPoint(NewHome, 0.0f, 160.0f), (bScared ? 650.0f : 450.0f) * FMath::FRandRange(0.85f, 1.15f));
		if (bScared && !bChirped && ChirpSounds.Num())
		{
			PlayAt(ChirpSounds[FMath::RandRange(0, ChirpSounds.Num() - 1)], Bird.Position, ChirpVolume, FMath::FRandRange(1.3f, 1.6f));
			bChirped = true;
		}
	}
}

void AForestBirds::UpdateSmall(float DeltaSeconds, const APawn* Player)
{
	for (int32 FlockIndex = 0; FlockIndex < Flocks.Num(); ++FlockIndex)
	{
		FFlock& Flock = Flocks[FlockIndex];
		if (Flock.bFlying)
		{
			continue;
		}
		if (Player && FVector::Dist2D(Player->GetActorLocation(), Flock.Home) < ScareDistance)
		{
			LaunchFlock(FlockIndex, Player->GetActorLocation(), true);
			continue;
		}
		Flock.Timer -= DeltaSeconds;
		if (Flock.Timer <= 0.0f)
		{
			LaunchFlock(FlockIndex, Flock.Home, false);
			continue;
		}
		Flock.ChirpTimer -= DeltaSeconds;
		if (Flock.ChirpTimer <= 0.0f && ChirpSounds.Num())
		{
			Flock.ChirpTimer = FMath::FRandRange(2.5f, 9.0f);
			PlayAt(ChirpSounds[FMath::RandRange(0, ChirpSounds.Num() - 1)], Flock.Home + FVector(0.0f, 0.0f, 30.0f), ChirpVolume, FMath::FRandRange(0.9f, 1.5f));
		}
	}

	for (FFlock& Flock : Flocks)
	{
		Flock.bFlying = false;
	}
	for (FBird& Bird : Small)
	{
		if (Bird.State == EBirdState::Flying)
		{
			Bird.Progress += DeltaSeconds;
			const float Alpha = Ease(Bird.Progress / Bird.FlightTime);
			const float Arc = FMath::Sin(Alpha * PI) * (250.0f + FVector::Dist2D(Bird.From, Bird.Target) * 0.12f);
			Bird.Position = FMath::Lerp(Bird.From, Bird.Target, Alpha) + FVector(0.0f, 0.0f, Arc);
			const FVector Direction = Bird.Target - Bird.From;
			if (!Direction.IsNearlyZero())
			{
				Bird.Yaw = Direction.Rotation().Yaw;
			}
			if (Bird.Progress >= Bird.FlightTime)
			{
				Bird.Position = Bird.Target;
				Bird.State = EBirdState::Grounded;
				Bird.FlightTime = 0.0f;
				Bird.Hop = FMath::FRandRange(0.5f, 3.0f);
			}
			else if (Flocks.IsValidIndex(Bird.Flock))
			{
				Flocks[Bird.Flock].bFlying = true;
			}
			continue;
		}
		// On the ground: little hops between pecks.
		if (Bird.Progress < Bird.FlightTime)
		{
			Bird.Progress += DeltaSeconds;
			const float Alpha = FMath::Clamp(Bird.Progress / Bird.FlightTime, 0.0f, 1.0f);
			Bird.Position = FMath::Lerp(Bird.From, Bird.Target, Alpha) + FVector(0.0f, 0.0f, FMath::Sin(Alpha * PI) * 14.0f);
			continue;
		}
		Bird.Hop -= DeltaSeconds;
		if (Bird.Hop <= 0.0f && Flocks.IsValidIndex(Bird.Flock))
		{
			Bird.From = Bird.Position;
			Bird.Target = RandomGroundPoint(Flocks[Bird.Flock].Home, 0.0f, 160.0f);
			Bird.Progress = 0.0f;
			Bird.FlightTime = 0.3f;
			Bird.Hop = FMath::FRandRange(1.0f, 4.0f);
			const FVector Direction = Bird.Target - Bird.From;
			if (!Direction.IsNearlyZero())
			{
				Bird.Yaw = Direction.Rotation().Yaw;
			}
		}
	}
}

void AForestBirds::UpdateLarge(float DeltaSeconds, const APawn* Player)
{
	for (FBird& Bird : Large)
	{
		Bird.Hop -= DeltaSeconds;
		if (Bird.Hop <= 0.0f)
		{
			Bird.Hop = FMath::FRandRange(15.0f, 40.0f);
			PlayAt(LargeCallSound, Bird.Position, CallVolume, FMath::FRandRange(0.85f, 1.05f));
		}
		switch (Bird.State)
		{
		case EBirdState::Soaring:
		{
			// Lazy circles on the evening air.
			Bird.Angle += 260.0f / Bird.Radius * DeltaSeconds;
			Bird.Position = Bird.Circle + FVector(FMath::Cos(Bird.Angle), FMath::Sin(Bird.Angle), 0.0f) * Bird.Radius;
			Bird.Yaw = FMath::RadiansToDegrees(Bird.Angle) + 90.0f;
			Bird.Timer -= DeltaSeconds;
			if (Bird.Timer <= 0.0f)
			{
				FlyTo(Bird, RandomGroundPoint(Bird.Circle, 0.0f, 1500.0f), 500.0f);
				Bird.Timer = FMath::FRandRange(10.0f, 25.0f);
			}
			break;
		}
		case EBirdState::Flying:
		{
			Bird.Progress += DeltaSeconds;
			const float Alpha = Ease(Bird.Progress / Bird.FlightTime);
			Bird.Position = FMath::Lerp(Bird.From, Bird.Target, Alpha);
			const FVector Direction = Bird.Target - Bird.From;
			if (!Direction.IsNearlyZero())
			{
				Bird.Yaw = Direction.Rotation().Yaw;
			}
			if (Bird.Progress >= Bird.FlightTime)
			{
				const bool bLanding = Bird.Target.Z < Bird.Circle.Z - 300.0f;
				Bird.Position = Bird.Target;
				if (bLanding)
				{
					Bird.State = EBirdState::Grounded;
				}
				else
				{
					Bird.State = EBirdState::Soaring;
					Bird.Angle = FMath::Atan2(Bird.Position.Y - Bird.Circle.Y, Bird.Position.X - Bird.Circle.X);
					Bird.Timer = FMath::FRandRange(30.0f, 80.0f);
				}
			}
			break;
		}
		case EBirdState::Grounded:
		{
			Bird.Timer -= DeltaSeconds;
			const bool bScared = Player && FVector::Dist2D(Player->GetActorLocation(), Bird.Position) < ScareDistance * 1.4f;
			if (bScared || Bird.Timer <= 0.0f)
			{
				// Back up to a new circle in the sky.
				const FVector Ground = RandomGroundPoint(WorldCenter, 0.0f, WorldRadius * 0.7f);
				Bird.Circle = Ground + FVector(0.0f, 0.0f, FMath::FRandRange(900.0f, 1500.0f));
				Bird.Radius = FMath::FRandRange(600.0f, 1300.0f);
				Bird.Angle = FMath::FRandRange(0.0f, 2.0f * PI);
				FlyTo(Bird, Bird.Circle + FVector(FMath::Cos(Bird.Angle), FMath::Sin(Bird.Angle), 0.0f) * Bird.Radius, bScared ? 600.0f : 400.0f);
				if (bScared)
				{
					PlayAt(LargeCallSound, Bird.Position, CallVolume * 1.4f, 1.1f);
				}
			}
			break;
		}
		}
	}
}

void AForestBirds::Draw()
{
	TArray<FTransform> Bodies, Heads, Wings;
	for (const FBird& Bird : Small)
	{
		const float S = Bird.Size;
		const FQuat Heading = FRotator(0.0f, Bird.Yaw, 0.0f).Quaternion();
		const bool bAir = Bird.State == EBirdState::Flying;
		const bool bHopping = !bAir && Bird.Progress < Bird.FlightTime;
		// Pecking: the head dips now and then.
		const float Peck = (!bAir && !bHopping) ? FMath::Pow(FMath::Max(0.0f, FMath::Sin(Time * 3.0f + Bird.Phase)), 8.0f) : 0.0f;
		float Flap = -75.0f;
		if (bAir)
		{
			const bool bGlide = FMath::Sin(Time * 1.3f + Bird.Phase) > 0.6f;
			Flap = bGlide ? 8.0f : 10.0f + 60.0f * FMath::Sin(Time * 24.0f + Bird.Phase);
		}
		else if (bHopping)
		{
			Flap = 20.0f + 40.0f * FMath::Sin(Time * 30.0f);
		}
		const FVector Body = Bird.Position + FVector(0.0f, 0.0f, 7.0f * S);
		Bodies.Add(FTransform(Heading, Body, FVector(0.16f, 0.10f, 0.09f) * S));
		Heads.Add(FTransform(Heading, Body + Heading.RotateVector(FVector(8.0f, 0.0f, 4.0f - 7.0f * Peck) * S), FVector(0.075f) * S));
		Wings.Add(Wing(Body + FVector(0.0f, 0.0f, 2.0f * S), Bird.Yaw, Flap, 1.0f, 11.0f * S, 7.0f * S, -1.0f * S));
		Wings.Add(Wing(Body + FVector(0.0f, 0.0f, 2.0f * S), Bird.Yaw, Flap, -1.0f, 11.0f * S, 7.0f * S, -1.0f * S));
	}
	PushLayer(SmallBodies, Bodies);
	PushLayer(SmallHeads, Heads);
	PushLayer(SmallWings, Wings);

	Bodies.Reset();
	Heads.Reset();
	Wings.Reset();
	for (const FBird& Bird : Large)
	{
		const float S = Bird.Size;
		const FQuat Heading = FRotator(0.0f, Bird.Yaw, 0.0f).Quaternion();
		float Flap = -70.0f;
		if (Bird.State == EBirdState::Soaring)
		{
			// Mostly gliding, with a few slow beats.
			Flap = FMath::Sin(Time * 0.7f + Bird.Phase) > 0.7f ? 5.0f + 35.0f * FMath::Sin(Time * 6.0f + Bird.Phase) : 4.0f;
		}
		else if (Bird.State == EBirdState::Flying)
		{
			Flap = 5.0f + 35.0f * FMath::Sin(Time * 7.0f + Bird.Phase);
		}
		const FVector Body = Bird.Position + FVector(0.0f, 0.0f, 22.0f * S);
		Bodies.Add(FTransform(Heading, Body, FVector(0.6f, 0.3f, 0.28f) * S));
		Heads.Add(FTransform(Heading, Body + Heading.RotateVector(FVector(30.0f, 0.0f, 10.0f) * S), FVector(0.2f) * S));
		Wings.Add(Wing(Body + FVector(0.0f, 0.0f, 6.0f * S), Bird.Yaw, Flap, 1.0f, 60.0f * S, 26.0f * S, -4.0f * S));
		Wings.Add(Wing(Body + FVector(0.0f, 0.0f, 6.0f * S), Bird.Yaw, Flap, -1.0f, 60.0f * S, 26.0f * S, -4.0f * S));
	}
	PushLayer(LargeBodies, Bodies);
	PushLayer(LargeHeads, Heads);
	PushLayer(LargeWings, Wings);
}

void AForestBirds::Animate(float DeltaSeconds)
{
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
	Time += DeltaSeconds;
	// Only a real player scares birds; in the editor they just live their lives.
	const APawn* Player = (GetWorld() && GetWorld()->IsGameWorld()) ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
	UpdateSmall(DeltaSeconds, Player);
	UpdateLarge(DeltaSeconds, Player);
	Draw();
}

void AForestBirds::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPopulated)
	{
		Animate(DeltaSeconds);
	}
}
