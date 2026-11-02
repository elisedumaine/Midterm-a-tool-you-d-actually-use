#include "ForestAnimal.h"

#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "ForestGameManager.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AForestAnimal::AForestAnimal()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(45.0f, 70.0f);
	// The player bumps into animals instead of walking through them.
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Capsule;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -70.0f));

	DisplayName = FText::FromString(TEXT("the deer"));
}

void AForestAnimal::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	SnapToGround();
	PickWanderTarget();
	// Not every animal starts walking at the same moment.
	PauseTime = FMath::FRandRange(0.5f, 4.0f);
	PlayAnim(IdleAnim, true);
}

int32 AForestAnimal::PlayerMushrooms() const
{
	const AForestGameManager* Manager = AForestGameManager::Get(this);
	return Manager ? Manager->Mushrooms : 0;
}

void AForestAnimal::SetState(EAnimalState NewState, float Duration)
{
	State = NewState;
	StateTime = Duration;
	switch (NewState)
	{
	case EAnimalState::Eating:
		PlayAnim(EatAnim, true);
		break;
	case EAnimalState::Happy:
		PlayAnim(HappyAnim, false);
		break;
	case EAnimalState::Refusing:
		PlayAnim(RefuseAnim, false);
		break;
	case EAnimalState::Dancing:
		PlayAnim(DanceAnim, true);
		break;
	default:
		break;
	}
}

void AForestAnimal::PlayAnim(UAnimationAsset* Anim, bool bLoop)
{
	if (!Anim || !Mesh || (Anim == CurrentAnim && bLoop))
	{
		return;
	}
	Mesh->PlayAnimation(Anim, bLoop);
	CurrentAnim = Anim;
}

FVector AForestAnimal::ClampToWorld(const FVector& Point) const
{
	FVector Offset = Point - WorldCenter;
	Offset.Z = 0.0f;
	if (Offset.Size() > WorldRadius)
	{
		Offset = Offset.GetSafeNormal() * WorldRadius;
	}
	return FVector(WorldCenter.X + Offset.X, WorldCenter.Y + Offset.Y, Point.Z);
}

void AForestAnimal::PickWanderTarget()
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = WanderRadius * FMath::Sqrt(FMath::FRand());
	WanderTarget = ClampToWorld(Home + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f));
}

void AForestAnimal::FacePoint(const FVector& Point, float DeltaSeconds)
{
	const FVector To = Point - GetActorLocation();
	if (To.SizeSquared2D() < 1.0f)
	{
		return;
	}
	const FRotator Desired(0.0f, To.Rotation().Yaw, 0.0f);
	const FRotator Smooth = FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, 5.0f);
	SetActorRotation(FRotator(0.0f, Smooth.Yaw, 0.0f));
}

bool AForestAnimal::MoveToward(const FVector& Destination, float Speed, float DeltaSeconds)
{
	const FVector Location = GetActorLocation();
	FVector Direction = Destination - Location;
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() < 1.0f)
	{
		return true;
	}
	Direction.Normalize();

	// Look one stride ahead at hip height: trees, rocks, cliffs, and the
	// world's edge all block the way.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnimalAhead), false, this);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0f, 0.0f, 10.0f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Direction * 110.0f, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}

	FacePoint(Destination, DeltaSeconds);
	SetActorLocation(Location + Direction * Speed * DeltaSeconds);
	SnapToGround();
	return true;
}

void AForestAnimal::SnapToGround()
{
	const FVector Location = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnimalGround), false, this);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	// Start just above the back, below the tree canopies, so the animal
	// follows the ground and never lands on top of a tree.
	const FVector Start = Location + FVector(0.0f, 0.0f, 60.0f);
	const FVector End = Location - FVector(0.0f, 0.0f, 800.0f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SetActorLocation(FVector(Location.X, Location.Y, Hit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight()));
	}
}

void AForestAnimal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime -= DeltaSeconds;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const float PlayerDistance = Player ? FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) : BIG_NUMBER;

	switch (State)
	{
	case EAnimalState::Eating:
		if (Player)
		{
			FacePoint(Player->GetActorLocation(), DeltaSeconds);
		}
		if (StateTime <= 0.0f)
		{
			SetState(EAnimalState::Happy, 1.8f);
		}
		return;

	case EAnimalState::Happy:
		if (StateTime <= 0.0f)
		{
			SetState(EAnimalState::Following);
		}
		return;

	case EAnimalState::Refusing:
		if (StateTime <= 0.0f)
		{
			SetState(EAnimalState::Wander);
		}
		return;

	case EAnimalState::Dancing:
		return;

	case EAnimalState::Following:
	{
		AActor* Target = Friend ? Friend.Get() : Player;
		if (!Target)
		{
			PlayAnim(IdleAnim, true);
			return;
		}
		const FVector TargetLocation = Target->GetActorLocation();
		const float Distance = FVector::Dist2D(GetActorLocation(), TargetLocation);
		if (Distance > FollowDistance)
		{
			const bool bFar = Distance > 1200.0f;
			MoveToward(TargetLocation, bFar ? RunSpeed : WalkSpeed * 1.6f, DeltaSeconds);
			PlayAnim(bFar ? RunAnim : WalkAnim, true);
		}
		else
		{
			FacePoint(TargetLocation, DeltaSeconds);
			PlayAnim(IdleAnim, true);
		}
		return;
	}

	default:
		break;
	}

	// Not fed yet: shy with empty hands, curious when a mushroom is offered.
	const int32 Mushrooms = PlayerMushrooms();
	if (Player && PlayerDistance < ShyDistance && Mushrooms == 0)
	{
		State = EAnimalState::Wary;
		FVector Away = GetActorLocation() - Player->GetActorLocation();
		Away.Z = 0.0f;
		const FVector Flee = ClampToWorld(GetActorLocation() + Away.GetSafeNormal() * 700.0f);
		if (!MoveToward(Flee, RunSpeed, DeltaSeconds))
		{
			// Cornered against a tree or a rock: slip sideways instead.
			const FVector Side = FVector::CrossProduct(Away.GetSafeNormal(), FVector::UpVector);
			MoveToward(GetActorLocation() + Side * 400.0f, RunSpeed, DeltaSeconds);
		}
		PlayAnim(RunAnim, true);
		PauseTime = 0.0f;
		return;
	}
	if (Player && PlayerDistance < CuriousDistance && Mushrooms > 0)
	{
		State = EAnimalState::Curious;
		FacePoint(Player->GetActorLocation(), DeltaSeconds);
		PlayAnim(IdleAnim, true);
		return;
	}

	State = EAnimalState::Wander;
	if (PauseTime > 0.0f)
	{
		PauseTime -= DeltaSeconds;
		return;
	}
	if (FVector::Dist2D(GetActorLocation(), WanderTarget) < 80.0f)
	{
		// Arrived: graze or look around for a few seconds before moving on.
		PauseTime = FMath::FRandRange(2.0f, 6.0f);
		PlayAnim(FMath::RandBool() ? EatAnim : IdleAnim, true);
		PickWanderTarget();
		return;
	}
	if (!MoveToward(WanderTarget, WalkSpeed, DeltaSeconds))
	{
		PickWanderTarget();
		return;
	}
	PlayAnim(WalkAnim, true);
}

bool AForestAnimal::Feed(AActor* Feeder)
{
	if (bFed || State == EAnimalState::Dancing)
	{
		return false;
	}
	bFed = true;
	Friend = Feeder;
	SetState(EAnimalState::Eating, 2.6f);
	OnFed();
	return true;
}

void AForestAnimal::Refuse()
{
	if (bFed || State == EAnimalState::Dancing || State == EAnimalState::Refusing)
	{
		return;
	}
	SetState(EAnimalState::Refusing, 1.4f);
}

void AForestAnimal::Dance()
{
	SetState(EAnimalState::Dancing);
}
