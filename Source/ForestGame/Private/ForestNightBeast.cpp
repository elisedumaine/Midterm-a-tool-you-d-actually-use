#include "ForestNightBeast.h"

#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ForestBlocks.h"
#include "ForestBuilding.h"
#include "ForestLanguage.h"
#include "ForestSurvival.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	constexpr float BiteReach = 190.0f;
	constexpr float SecondsBetweenBites = 2.5f;
	/** How far the light of a campfire, or of a lantern, keeps the beasts away. */
	constexpr float CampfireLight = 1000.0f;
	constexpr float LanternLight = 450.0f;
	/** How close a beast prowls to the player's home. */
	constexpr float HomeSafety = 500.0f;

	struct FBody
	{
		const TCHAR* Model;
		const TCHAR* English;
		const TCHAR* French;
		float Height;
		float Radius;
		int32 Bite;	// half hearts
	};

	const FBody GBodies[] = {
		{TEXT("animal-fox"), TEXT("the shadow wolf"), TEXT("le loup des ombres"), 120.0f, 40.0f, 2},
		{TEXT("animal-hog"), TEXT("the black boar"), TEXT("le sanglier noir"), 125.0f, 55.0f, 3},
	};

	template <typename T>
	T* LoadAnimalAsset(const FString& Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/WorldBuilder/Imported/Animals/%s.%s"), *Name, *Name));
	}
}

AForestNightBeast::AForestNightBeast()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(45.0f, 60.0f);
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = Capsule;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LeftEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftEye"));
	RightEye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightEye"));
	for (UStaticMeshComponent* Eye : {LeftEye.Get(), RightEye.Get()})
	{
		Eye->SetupAttachment(Capsule);
		Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Eye->SetCastShadow(false);
	}

	EyeGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeGlow"));
	EyeGlow->SetupAttachment(Capsule);
	EyeGlow->SetLightColor(FLinearColor(1.0f, 0.05f, 0.02f));
	EyeGlow->SetIntensityUnits(ELightUnits::Candelas);
	EyeGlow->SetIntensity(6.0f);
	EyeGlow->SetAttenuationRadius(260.0f);
	EyeGlow->SetCastShadows(false);
}

void AForestNightBeast::Setup(int32 NewKind)
{
	Kind = FMath::Clamp(NewKind, 0, UE_ARRAY_COUNT(GBodies) - 1);
	const FBody& Body = GBodies[Kind];
	const FString Model = Body.Model;
	const float Half = Body.Height * 0.5f;
	Capsule->SetCapsuleSize(Body.Radius, Half);

	float Scale = 1.0f;
	float Length = 60.0f;
	if (USkeletalMesh* Skeletal = LoadAnimalAsset<USkeletalMesh>(Model))
	{
		Mesh->SetSkeletalMeshAsset(Skeletal);
		const FBoxSphereBounds Bounds = Skeletal->GetBounds();
		Scale = Body.Height / FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f);
		// Kenney's animals face +Y; the actor walks along +X.
		Length = Bounds.BoxExtent.Y * Scale;
	}
	Mesh->SetRelativeScale3D(FVector(Scale));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -Half));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	// As dark as the night: whatever tint parameter the animal material has.
	Mesh->SetVectorParameterValueOnMaterials(TEXT("Tint"), FVector(0.05f, 0.04f, 0.06f));
	Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(0.05f, 0.04f, 0.06f));

	IdleAnim = LoadAnimalAsset<UAnimationAsset>(Model + TEXT("idle"));
	WalkAnim = LoadAnimalAsset<UAnimationAsset>(Model + TEXT("walk"));
	RunAnim = LoadAnimalAsset<UAnimationAsset>(Model + TEXT("run"));
	AttackAnim = LoadAnimalAsset<UAnimationAsset>(Model + TEXT("gesture-negative"));
	PlayAnim(IdleAnim);

	// Two burning red eyes at the front of the head.
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow"));
	UMaterialInstanceDynamic* Red = Glow ? UMaterialInstanceDynamic::Create(Glow, this) : nullptr;
	if (Red)
	{
		Red->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.0f, 0.03f, 0.01f));
		Red->SetScalarParameterValue(TEXT("Glow"), 25.0f);
	}
	const FVector EyeSpot(Length * 0.85f, 0.0f, Half * 0.55f);
	const float Apart = Body.Radius * 0.28f;
	LeftEye->SetStaticMesh(Sphere);
	RightEye->SetStaticMesh(Sphere);
	LeftEye->SetRelativeLocation(EyeSpot + FVector(0.0f, -Apart, 0.0f));
	RightEye->SetRelativeLocation(EyeSpot + FVector(0.0f, Apart, 0.0f));
	LeftEye->SetRelativeScale3D(FVector(0.07f));
	RightEye->SetRelativeScale3D(FVector(0.07f));
	if (Red)
	{
		LeftEye->SetMaterial(0, Red);
		RightEye->SetMaterial(0, Red);
	}
	EyeGlow->SetRelativeLocation(EyeSpot + FVector(15.0f, 0.0f, 0.0f));

	WalkSpeed = Kind == 0 ? 260.0f : 220.0f;
	RunSpeed = Kind == 0 ? 470.0f : 430.0f;
	CircleSide = FMath::RandBool() ? 1.0f : -1.0f;
	SnapToGround();
}

FString AForestNightBeast::GetBeastName() const
{
	return ForestLanguage::Pick(GBodies[Kind].English, GBodies[Kind].French);
}

void AForestNightBeast::PlayAnim(UAnimationAsset* Anim)
{
	if (Anim && Anim != CurrentAnim)
	{
		Mesh->PlayAnimation(Anim, Anim != AttackAnim);
		CurrentAnim = Anim;
	}
}

void AForestNightBeast::FacePoint(const FVector& Point, float DeltaSeconds)
{
	const FVector To = Point - GetActorLocation();
	if (To.SizeSquared2D() < 1.0f)
	{
		return;
	}
	const FRotator Smooth = FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, To.Rotation().Yaw, 0.0f), DeltaSeconds, 6.0f);
	SetActorRotation(FRotator(0.0f, Smooth.Yaw, 0.0f));
}

bool AForestNightBeast::MoveToward(const FVector& Destination, float Speed, float DeltaSeconds)
{
	const FVector Location = GetActorLocation();
	FVector Direction = Destination - Location;
	Direction.Z = 0.0f;
	if (Direction.SizeSquared() < 1.0f)
	{
		return true;
	}
	Direction.Normalize();
	if (Detour > 0.0f)
	{
		// Slipping round a tree or a rock.
		Detour -= DeltaSeconds;
		Direction = DetourDirection;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeastAhead), false, this);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0f, 0.0f, 10.0f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, Start + Direction * 120.0f, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		DetourDirection = FVector::CrossProduct(Direction, FVector::UpVector) * (FMath::RandBool() ? 1.0f : -1.0f);
		Detour = FMath::FRandRange(0.5f, 1.0f);
		return false;
	}
	FacePoint(Location + Direction * 100.0f, DeltaSeconds);
	SetActorLocation(Location + Direction * Speed * DeltaSeconds);
	SnapToGround();
	return true;
}

void AForestNightBeast::SnapToGround()
{
	const FVector Location = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BeastGround), false, this);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0f, 0.0f, 80.0f);
	const FVector End = Location - FVector(0.0f, 0.0f, 1500.0f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SetActorLocation(FVector(Location.X, Location.Y, Hit.ImpactPoint.Z + Capsule->GetScaledCapsuleHalfHeight()));
	}
}

bool AForestNightBeast::NearestLight(const FVector& From, FVector& OutLight, float& OutRadius) const
{
	bool bFound = false;
	float Best = BIG_NUMBER;
	for (TActorIterator<AForestBuilding> It(GetWorld()); It; ++It)
	{
		const FName Type = It->GetBuildingType();
		const float Radius = Type == TEXT("Campfire") ? CampfireLight : Type == TEXT("Lantern") ? LanternLight : 0.0f;
		if (Radius <= 0.0f)
		{
			continue;
		}
		// The light whose edge is closest (or that it stands deepest inside).
		const float Edge = FVector::Dist2D(From, It->GetActorLocation()) - Radius;
		if (Edge < Best)
		{
			Best = Edge;
			OutLight = It->GetActorLocation();
			OutRadius = Radius;
			bFound = true;
		}
	}
	// The player's glow blocks shine like lanterns, and a home is as safe as the fire:
	// the beast prowls around it but cannot get in.
	if (const UForestBlocks* Blocks = UForestBlocks::Get(this))
	{
		for (const FVector& Glow : Blocks->GetGlowSpots())
		{
			const float Edge = FVector::Dist2D(From, Glow) - LanternLight;
			if (Edge < Best)
			{
				Best = Edge;
				OutLight = Glow;
				OutRadius = LanternLight;
				bFound = true;
			}
		}
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		if (Player && Blocks->IsHome(Player->GetActorLocation()))
		{
			const float Edge = FVector::Dist2D(From, Player->GetActorLocation()) - HomeSafety;
			if (Edge < Best)
			{
				Best = Edge;
				OutLight = Player->GetActorLocation();
				OutRadius = HomeSafety;
				bFound = true;
			}
		}
	}
	return bFound;
}

void AForestNightBeast::Flee()
{
	bFleeing = true;
	FadeTime = 0.0f;
}

void AForestNightBeast::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	BiteCooldown -= DeltaSeconds;

	// The eyes flicker a little, like embers.
	EyeGlow->SetIntensity(6.0f + 2.0f * FMath::Sin(Time * 9.0f) * FMath::Sin(Time * 3.1f));

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (bFleeing)
	{
		// Dawn: runs off into the trees and melts away.
		FadeTime += DeltaSeconds;
		const FVector Away = Player ? GetActorLocation() + (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D() * 500.0f : GetActorLocation() + GetActorForwardVector() * 500.0f;
		MoveToward(Away, RunSpeed, DeltaSeconds);
		PlayAnim(RunAnim);
		const float Fade = FMath::Clamp(1.0f - FadeTime / 3.0f, 0.0f, 1.0f);
		SetActorScale3D(FVector(FMath::Max(0.05f, Fade)));
		if (FadeTime > 3.0f)
		{
			Destroy();
		}
		return;
	}
	if (!Player)
	{
		PlayAnim(IdleAnim);
		return;
	}

	const FVector Here = GetActorLocation();
	const FVector Target = Player->GetActorLocation();
	const float Distance = FVector::Dist2D(Here, Target);

	// Fire: never steps into the light.  If the player is inside it, the beast
	// circles at the edge of the glow, always facing them.
	FVector Light;
	float LightRadius = 0.0f;
	if (NearestLight(Here, Light, LightRadius))
	{
		const float FromLight = FVector::Dist2D(Here, Light);
		const bool bPlayerInLight = FVector::Dist2D(Target, Light) < LightRadius;
		if (FromLight < LightRadius)
		{
			// Caught in the light: back out of it, fast.
			FVector Out = Here - Light;
			Out.Z = 0.0f;
			MoveToward(Here + Out.GetSafeNormal() * 400.0f, RunSpeed, DeltaSeconds);
			PlayAnim(RunAnim);
			return;
		}
		if (bPlayerInLight && FromLight < LightRadius + 500.0f)
		{
			FVector Radial = Here - Light;
			Radial.Z = 0.0f;
			Radial.Normalize();
			const FVector Around = FVector::CrossProduct(FVector::UpVector, Radial) * CircleSide;
			const FVector Edge = Light + Radial * (LightRadius + 120.0f) + Around * 150.0f;
			if (!MoveToward(Edge, WalkSpeed * 0.5f, DeltaSeconds))
			{
				CircleSide = -CircleSide;
			}
			FacePoint(Target, DeltaSeconds * 3.0f);
			PlayAnim(WalkAnim);
			return;
		}
	}

	// Hunting in the dark.
	if (Distance < BiteReach)
	{
		FacePoint(Target, DeltaSeconds);
		if (BiteCooldown <= 0.0f)
		{
			BiteCooldown = SecondsBetweenBites;
			PlayAnim(AttackAnim);
			if (UForestSurvival* Survival = UForestSurvival::Get(this))
			{
				Survival->Hurt(GBodies[Kind].Bite, this);
			}
		}
		return;
	}
	// Prowls slowly from afar, then rushes in.
	const bool bRush = Distance < 1400.0f;
	MoveToward(Target, bRush ? RunSpeed : WalkSpeed, DeltaSeconds);
	PlayAnim(bRush ? RunAnim : WalkAnim);
}
