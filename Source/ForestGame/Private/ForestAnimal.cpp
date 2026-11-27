#include "ForestAnimal.h"

#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "ForestBuilding.h"
#include "ForestCrafting.h"
#include "ForestGameManager.h"
#include "ForestLanguage.h"
#include "ForestPickup.h"
#include "ForestSky.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Tastes and habits of each species, found by its English name. */
	struct FSpecies
	{
		const TCHAR* Name;
		const TCHAR* Favorite;
		int32 MealsToTame;
		bool bNocturnal;
		bool bBold;
		float HungerSeconds;
	};

	const FSpecies GSpecies[] = {
		{TEXT("the deer"), TEXT("Berry"), 5, false, false, 220.0f},
		{TEXT("the bunny"), TEXT("Berry"), 4, false, false, 160.0f},
		{TEXT("the fox"), TEXT("Mushroom"), 4, true, true, 200.0f},
		{TEXT("the boar"), TEXT("Mushroom"), 4, false, true, 150.0f},
		{TEXT("the beaver"), TEXT("Wood"), 3, false, false, 240.0f},
	};
}

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

void AForestAnimal::ApplySpecies()
{
	const FString Name = DisplayName.ToString();
	for (const FSpecies& Species : GSpecies)
	{
		if (Name.Equals(Species.Name, ESearchCase::IgnoreCase))
		{
			FavoriteFood = Species.Favorite;
			MealsToTame = Species.MealsToTame;
			bNocturnal = Species.bNocturnal;
			bBold = Species.bBold;
			HungerSeconds = Species.HungerSeconds;
			return;
		}
	}
}

void AForestAnimal::BeginPlay()
{
	Super::BeginPlay();
	ApplySpecies();
	Home = GetActorLocation();
	SnapToGround();
	PickWanderTarget();
	// Not every animal starts walking, or gets hungry, at the same moment.
	PauseTime = FMath::FRandRange(0.5f, 4.0f);
	Hunger = FMath::FRandRange(0.1f, 0.55f);
	PlayAnim(IdleAnim, true);
}

void AForestAnimal::Restore(float SavedTrust, bool bSavedTame, bool bSavedStaying, const FVector& SavedHome)
{
	Trust = FMath::Clamp(SavedTrust, 0.0f, 1.0f);
	bTame = bSavedTame;
	bStaying = bSavedTame && bSavedStaying;
	if (bTame)
	{
		Home = SavedHome;
		SetActorLocation(SavedHome + FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleHalfHeight()));
		SnapToGround();
		Friend = UGameplayStatics::GetPlayerPawn(this, 0);
		SetState(bStaying ? EAnimalState::Staying : EAnimalState::Following);
	}
}

void AForestAnimal::MoveIn(const FVector& NewHome)
{
	bMovedIn = true;
	Home = NewHome;
	// It lives close to its new home, and trusts the one who built it a little more.
	WanderRadius = FMath::Min(WanderRadius, 600.0f);
	Trust = FMath::Max(Trust, FMath::Min(Trust + 0.2f, 0.95f));
	WanderTarget = Home;
}

bool AForestAnimal::Eats(FName Food) const
{
	return ForestCrafting::IsFood(Food) || Food == FavoriteFood;
}

bool AForestAnimal::PlayerHasFood() const
{
	const AForestGameManager* Manager = AForestGameManager::Get(this);
	return Manager && Manager->BestFoodFor(this) != NAME_None;
}

bool AForestAnimal::IsSleepTime() const
{
	const AForestSky* Sky = AForestSky::Get(this);
	if (!Sky)
	{
		return false;
	}
	if (bNocturnal)
	{
		// The fox naps through the middle of the day.
		return Sky->GetHour() > 10.0f && Sky->GetHour() < 16.5f;
	}
	return Sky->IsNight();
}

bool AForestAnimal::IsRaining() const
{
	const AForestSky* Sky = AForestSky::Get(this);
	return Sky && Sky->IsRaining();
}

bool AForestAnimal::IsStormy() const
{
	const AForestSky* Sky = AForestSky::Get(this);
	return Sky && Sky->IsStorm();
}

FString AForestAnimal::DescribeMood() const
{
	switch (State)
	{
	case EAnimalState::Sleeping: return TEXT("Zzz");
	case EAnimalState::Eating: return ForestLanguage::Pick(TEXT("eating"), TEXT("mange"));
	case EAnimalState::Sheltering: return ForestLanguage::Pick(TEXT("hiding from the rain"), TEXT("s'abrite de la pluie"));
	case EAnimalState::Wary: return ForestLanguage::Pick(TEXT("scared!"), TEXT("a peur !"));
	case EAnimalState::Foraging: return ForestLanguage::Pick(TEXT("looking for food"), TEXT("cherche à manger"));
	case EAnimalState::Curious: return ForestLanguage::Pick(TEXT("curious"), TEXT("curieux"));
	case EAnimalState::Staying: return ForestLanguage::Pick(TEXT("stays at camp"), TEXT("reste au camp"));
	case EAnimalState::Charging: return ForestLanguage::Pick(TEXT("hungry: coming for you!"), TEXT("affamé : il fonce sur toi !"));
	case EAnimalState::Growling: return ForestLanguage::Pick(TEXT("hungry: growls"), TEXT("affamé : grogne"));
	default: break;
	}
	return IsHungry() ? ForestLanguage::Pick(TEXT("hungry"), TEXT("a faim")) : FString();
}

void AForestAnimal::SetState(EAnimalState NewState, float Duration)
{
	if (State == EAnimalState::Sleeping && NewState != EAnimalState::Sleeping && Mesh)
	{
		Mesh->SetPlayRate(1.0f);
	}
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
	case EAnimalState::Sleeping:
		// No sleep animation in the kit: a very slow idle reads as breathing in its sleep.
		PlayAnim(IdleAnim, true, 0.15f);
		break;
	default:
		break;
	}
}

void AForestAnimal::PlayAnim(UAnimationAsset* Anim, bool bLoop, float Rate)
{
	if (!Anim || !Mesh)
	{
		return;
	}
	if (Anim != CurrentAnim || !bLoop)
	{
		Mesh->PlayAnimation(Anim, bLoop);
		CurrentAnim = Anim;
	}
	Mesh->SetPlayRate(Rate);
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
	// Animals staying at the camp keep close to it.
	const float Radius = State == EAnimalState::Staying ? 450.0f : WanderRadius;
	const float Distance = Radius * FMath::Sqrt(FMath::FRand());
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

	// Look one stride ahead at hip height: trees, rocks, cliffs, the camp's
	// fences, and the world's edge all block the way.
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

bool AForestAnimal::TravelTo(const FVector& Destination, float Speed, float Arrived, float DeltaSeconds)
{
	if (FVector::Dist2D(GetActorLocation(), Destination) < Arrived)
	{
		return true;
	}
	if (Detour > 0.0f)
	{
		// Stepping around a tree or a rock for a moment before heading on.
		Detour -= DeltaSeconds;
		if (!MoveToward(GetActorLocation() + DetourDirection * 300.0f, Speed, DeltaSeconds))
		{
			DetourDirection = -DetourDirection;
		}
		return false;
	}
	if (!MoveToward(Destination, Speed, DeltaSeconds))
	{
		FVector To = Destination - GetActorLocation();
		To.Z = 0.0f;
		DetourDirection = FVector::CrossProduct(To.GetSafeNormal(), FVector::UpVector) * (FMath::RandBool() ? 1.0f : -1.0f);
		Detour = FMath::FRandRange(0.6f, 1.2f);
	}
	return false;
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
	// Start just above the back, below the tree canopies (and the shelter's
	// roof), so the animal follows the ground and never lands on top of a tree.
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
	ForageCooldown -= DeltaSeconds;
	if (State != EAnimalState::Sleeping)
	{
		Hunger = FMath::Min(1.0f, Hunger + DeltaSeconds / FMath::Max(10.0f, HungerSeconds));
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const float PlayerDistance = Player ? FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) : BIG_NUMBER;

	// Short moments that play to the end first.
	switch (State)
	{
	case EAnimalState::Eating:
		if (Player && !Meal)
		{
			FacePoint(Player->GetActorLocation(), DeltaSeconds);
		}
		if (StateTime <= 0.0f)
		{
			// Fed by hand (no meal of its own): a tame animal thanks the player.
			const bool bFedByHand = !Meal;
			Meal = nullptr;
			if (bTame && bFedByHand)
			{
				SetState(EAnimalState::Happy, 1.8f);
			}
			else
			{
				SetState(bTame ? (bStaying ? EAnimalState::Staying : EAnimalState::Following) : EAnimalState::Wander);
			}
		}
		return;

	case EAnimalState::Happy:
		if (StateTime <= 0.0f)
		{
			SetState(bTame ? (bStaying ? EAnimalState::Staying : EAnimalState::Following) : EAnimalState::Wander);
		}
		return;

	case EAnimalState::Refusing:
		if (StateTime <= 0.0f)
		{
			SetState(bTame ? EAnimalState::Following : EAnimalState::Wander);
		}
		return;

	case EAnimalState::Dancing:
		if (StateTime <= 0.0f)
		{
			SetState(bTame ? (bStaying ? EAnimalState::Staying : EAnimalState::Following) : EAnimalState::Wander);
		}
		return;

	default:
		break;
	}

	if (bTame)
	{
		UpdateTame(Player, DeltaSeconds);
		return;
	}

	// A wild animal: its needs, from the most urgent to the least.
	if (UpdateThreat(Player, PlayerDistance, DeltaSeconds)
		|| UpdateFear(Player, PlayerDistance, DeltaSeconds)
		|| UpdateShelter(DeltaSeconds)
		|| UpdateSleep(Player, PlayerDistance, DeltaSeconds)
		|| UpdateHunger(DeltaSeconds))
	{
		return;
	}
	if (Player && PlayerDistance < CuriousDistance && PlayerHasFood())
	{
		State = EAnimalState::Curious;
		FacePoint(Player->GetActorLocation(), DeltaSeconds);
		PlayAnim(IdleAnim, true);
		return;
	}
	UpdateWander(DeltaSeconds);
}

bool AForestAnimal::UpdateFear(APawn* Player, float PlayerDistance, float DeltaSeconds)
{
	if (!Player)
	{
		return false;
	}
	// Trust shrinks the distance it keeps; storms make everyone jumpy; food in
	// the player's hands makes it curious instead; a trusting sleeper stays asleep.
	float Shy = ShyDistance * (1.0f - 0.75f * Trust) * (IsStormy() ? 1.4f : 1.0f);
	if (PlayerHasFood())
	{
		Shy *= 0.35f;
	}
	if (State == EAnimalState::Sleeping)
	{
		Shy *= Trust > 0.3f ? 0.0f : 0.5f;
	}
	if (PlayerDistance >= Shy)
	{
		if (State == EAnimalState::Wary)
		{
			SetState(EAnimalState::Wander);
		}
		return false;
	}
	SetState(EAnimalState::Wary);
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
	bHasRefuge = false;
	Meal = nullptr;
	return true;
}

bool AForestAnimal::UpdateThreat(APawn* Player, float PlayerDistance, float DeltaSeconds)
{
	// Only hungry, awake animals, and only if the player has nothing for them.
	const bool bThreat = Player && IsHungry() && State != EAnimalState::Sleeping && !PlayerHasFood()
		&& PlayerDistance < (bBold ? 1500.0f : ShyDistance);
	if (!bThreat)
	{
		if (State == EAnimalState::Charging || State == EAnimalState::Growling)
		{
			SetState(EAnimalState::Wander);
		}
		return false;
	}
	Meal = nullptr;
	bHasRefuge = false;
	const FVector Target = Player->GetActorLocation();
	if (bBold && PlayerDistance > 200.0f)
	{
		// Comes at the player, not quite at full speed so they can still get away.
		State = EAnimalState::Charging;
		TravelTo(Target, RunSpeed * 0.7f, 200.0f, DeltaSeconds);
		PlayAnim(RunAnim, true);
		return true;
	}
	State = bBold ? EAnimalState::Charging : EAnimalState::Growling;
	FacePoint(Target, DeltaSeconds);
	PlayAnim(IdleAnim, true);
	return true;
}

bool AForestAnimal::UpdateShelter(float DeltaSeconds)
{
	if (!IsRaining())
	{
		if (State == EAnimalState::Sheltering)
		{
			SetState(EAnimalState::Wander);
			bHasRefuge = false;
		}
		return false;
	}
	AForestGameManager* Manager = AForestGameManager::Get(this);
	if (State != EAnimalState::Sheltering || !bHasRefuge)
	{
		SetState(EAnimalState::Sheltering);
		bHasRefuge = Manager && Manager->FindShelter(GetActorLocation(), Refuge);
	}
	if (!bHasRefuge)
	{
		PlayAnim(IdleAnim, true);
		return true;
	}
	if (TravelTo(Refuge, RunSpeed * 0.6f, 70.0f, DeltaSeconds))
	{
		// Huddled under the leaves (or the roof), waiting for the rain to stop.
		PlayAnim(IdleAnim, true, 0.6f);
	}
	else
	{
		PlayAnim(RunAnim, true);
	}
	return true;
}

bool AForestAnimal::UpdateSleep(APawn* Player, float PlayerDistance, float DeltaSeconds)
{
	if (!IsSleepTime())
	{
		if (State == EAnimalState::Sleeping)
		{
			SetState(EAnimalState::Wander);
		}
		bHasRefuge = State == EAnimalState::Sheltering && bHasRefuge;
		return false;
	}
	if (State == EAnimalState::Sleeping)
	{
		return true;
	}
	// Going home to sleep: tame animals pick the campfire if there is one.
	if (!bHasRefuge)
	{
		Refuge = Home;
		const AForestGameManager* Manager = AForestGameManager::Get(this);
		FVector Fire;
		if (bTame && Manager && Manager->FindCampfire(GetActorLocation(), 8000.0f, Fire))
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			Refuge = Fire + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * FMath::FRandRange(170.0f, 260.0f);
		}
		bHasRefuge = true;
	}
	if (TravelTo(Refuge, WalkSpeed * 1.3f, 120.0f, DeltaSeconds))
	{
		SetState(EAnimalState::Sleeping);
		bHasRefuge = false;
		return true;
	}
	State = bTame ? State : EAnimalState::Wander;
	PlayAnim(WalkAnim, true);
	return true;
}

bool AForestAnimal::UpdateHunger(float DeltaSeconds)
{
	if ((!IsHungry() || ForageCooldown > 0.0f) && State != EAnimalState::Foraging)
	{
		return false;
	}
	AForestGameManager* Manager = AForestGameManager::Get(this);
	if (!Manager)
	{
		return false;
	}
	if (!Meal || State != EAnimalState::Foraging)
	{
		Meal = Manager->FindMeal(this, GetActorLocation(), 4500.0f);
		if (!Meal)
		{
			return false;
		}
		SetState(EAnimalState::Foraging);
		ForageTime = 0.0f;
	}
	// Stuck behind a rock or across the river: give up for a while and stroll elsewhere.
	ForageTime += DeltaSeconds;
	if (ForageTime > 20.0f)
	{
		Meal = nullptr;
		ForageCooldown = 15.0f;
		SetState(EAnimalState::Wander);
		PickWanderTarget();
		return false;
	}
	// Someone else ate it first.
	const AForestPickup* Pickup = Cast<AForestPickup>(Meal);
	const AForestBuilding* Feeder = Cast<AForestBuilding>(Meal);
	if ((Pickup && Pickup->IsCollected()) || (Feeder && Feeder->GetFood() <= 0))
	{
		Meal = nullptr;
		SetState(EAnimalState::Wander);
		return false;
	}
	const float Reach = Feeder ? 140.0f : 90.0f;
	if (!TravelTo(Meal->GetActorLocation(), WalkSpeed * 1.4f, Reach, DeltaSeconds))
	{
		PlayAnim(WalkAnim, true);
		return true;
	}
	// Arrived: eat it.
	FacePoint(Meal->GetActorLocation(), 1.0f);
	if (AForestPickup* Food = Cast<AForestPickup>(Meal))
	{
		Food->Collect();
	}
	else if (AForestBuilding* Trough = Cast<AForestBuilding>(Meal))
	{
		Trough->TakeFood();
		// The camp's food teaches it that the player's place is safe.
		Trust = FMath::Min(0.95f, Trust + 0.12f);
	}
	Hunger = 0.0f;
	SetState(EAnimalState::Eating, 3.0f);
	return true;
}

void AForestAnimal::UpdateWander(float DeltaSeconds)
{
	if (State != EAnimalState::Staying)
	{
		State = EAnimalState::Wander;
	}
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

void AForestAnimal::UpdateTame(APawn* Player, float DeltaSeconds)
{
	// Tame animals still eat, sleep and get wet, but never run from the player.
	AForestGameManager* Manager = AForestGameManager::Get(this);
	if (IsRaining())
	{
		FVector Roof;
		if (Manager && Manager->FindBuilding(TEXT("Shelter"), GetActorLocation(), 4000.0f, Roof))
		{
			if (TravelTo(Roof, RunSpeed * 0.6f, 70.0f, DeltaSeconds))
			{
				PlayAnim(IdleAnim, true, 0.6f);
			}
			else
			{
				PlayAnim(RunAnim, true);
			}
			State = EAnimalState::Sheltering;
			return;
		}
	}
	FVector Fire;
	if (IsSleepTime() && Manager && Manager->FindCampfire(GetActorLocation(), 8000.0f, Fire))
	{
		const EAnimalState Was = State;
		UpdateSleep(Player, 0.0f, DeltaSeconds);
		if (State != EAnimalState::Sleeping && Was != EAnimalState::Sleeping)
		{
			State = bStaying ? EAnimalState::Staying : EAnimalState::Following;
		}
		return;
	}
	if (State == EAnimalState::Sleeping || State == EAnimalState::Sheltering)
	{
		SetState(bStaying ? EAnimalState::Staying : EAnimalState::Following);
		bHasRefuge = false;
	}
	if (IsHungry() && Manager && Manager->FindMeal(this, GetActorLocation(), 2000.0f))
	{
		const EAnimalState Was = State;
		if (Was != EAnimalState::Foraging)
		{
			State = EAnimalState::Foraging;
			Meal = nullptr;
		}
		if (UpdateHunger(DeltaSeconds))
		{
			return;
		}
		State = Was == EAnimalState::Foraging ? (bStaying ? EAnimalState::Staying : EAnimalState::Following) : Was;
	}

	if (bStaying)
	{
		State = EAnimalState::Staying;
		UpdateWander(DeltaSeconds);
		return;
	}

	State = EAnimalState::Following;
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
		TravelTo(TargetLocation, bFar ? RunSpeed : WalkSpeed * 1.6f, FollowDistance, DeltaSeconds);
		PlayAnim(bFar ? RunAnim : WalkAnim, true);
	}
	else
	{
		FacePoint(TargetLocation, DeltaSeconds);
		PlayAnim(IdleAnim, true);
	}
}

bool AForestAnimal::Feed(AActor* Feeder, FName Food)
{
	if (State == EAnimalState::Dancing || State == EAnimalState::Eating || !Eats(Food))
	{
		return false;
	}
	// Each meal raises its trust; its favourite food counts double.
	const float Step = (Food == FavoriteFood ? 2.0f : 1.0f) / FMath::Max(1, MealsToTame);
	Trust = FMath::Min(1.0f, Trust + Step);
	Hunger = 0.0f;
	Meal = nullptr;
	bHasRefuge = false;
	if (Trust >= 0.999f)
	{
		bTame = true;
		Friend = Feeder;
	}
	SetState(EAnimalState::Eating, 2.6f);
	OnFed();
	return true;
}

void AForestAnimal::Refuse()
{
	if (State == EAnimalState::Dancing || State == EAnimalState::Refusing || State == EAnimalState::Eating)
	{
		return;
	}
	SetState(EAnimalState::Refusing, 1.4f);
}

void AForestAnimal::Pet()
{
	if (!bTame || State == EAnimalState::Eating || State == EAnimalState::Happy || State == EAnimalState::Dancing)
	{
		return;
	}
	SetState(EAnimalState::Happy, 1.8f);
}

void AForestAnimal::ToggleStay()
{
	if (!bTame)
	{
		return;
	}
	bStaying = !bStaying;
	if (bStaying)
	{
		Home = GetActorLocation();
		PickWanderTarget();
	}
	SetState(bStaying ? EAnimalState::Staying : EAnimalState::Following);
}

void AForestAnimal::Dance()
{
	SetState(EAnimalState::Dancing, 12.0f);
}
