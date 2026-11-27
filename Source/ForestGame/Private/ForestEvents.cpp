#include "ForestEvents.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "ForestBlocks.h"
#include "ForestGame.h"
#include "ForestLanguage.h"
#include "ForestLife.h"
#include "ForestSky.h"
#include "ForestSurvival.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

namespace
{
	const TCHAR* GlowPath = TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow");
	const TCHAR* ThunderPath = TEXT("/Game/WorldBuilder/Audio/SW_IWB_Thunder.SW_IWB_Thunder");

	constexpr float WarningSeconds = 2.4f;
	constexpr float StrikeRadius = 330.0f;
	constexpr float QuakeSeconds = 6.0f;
	constexpr float StarSeconds = 3.2f;

	UMaterialInstanceDynamic* Glow(UObject* Outer, const FLinearColor& Color, float Strength)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, GlowPath);
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Outer);
		Material->SetVectorParameterValue(TEXT("Tint"), Color);
		Material->SetScalarParameterValue(TEXT("Glow"), Strength);
		return Material;
	}

	UStaticMeshComponent* AddMesh(AActor* Owner, const TCHAR* Path, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Path));
		Mesh->SetupAttachment(Owner->GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetAbsolute(true, true, true);
		Mesh->RegisterComponent();
		if (Material)
		{
			Mesh->SetMaterial(0, Material);
		}
		return Mesh;
	}

	APawn* ActivePlayer(const UWorld* World)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Player = Controller ? Controller->GetPawn() : nullptr;
		// Nothing happens during the title screen and the wake-up.
		return Player && !Controller->IsMoveInputIgnored() ? Player : nullptr;
	}
}

// --- A fire left by lightning ----------------------------------------------

AForestFire::AForestFire()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AForestFire::Ignite(float Seconds)
{
	Lifetime = Seconds;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UMaterialInstanceDynamic* Flame = Glow(this, Index % 2 ? FLinearColor(1.0f, 0.7f, 0.15f) : FLinearColor(1.0f, 0.32f, 0.05f), 4.0f);
		UStaticMeshComponent* Cone = NewObject<UStaticMeshComponent>(this);
		Cone->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
		Cone->SetupAttachment(RootComponent);
		const float Angle = Index * 2.0f * PI / 6.0f;
		const float Ring = Index == 0 ? 0.0f : FMath::FRandRange(40.0f, 110.0f);
		Cone->SetRelativeLocation(FVector(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 35.0f));
		Cone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cone->SetCastShadow(false);
		Cone->RegisterComponent();
		if (Flame)
		{
			Cone->SetMaterial(0, Flame);
		}
		Flames.Add(Cone);
	}
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(120.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.5f, 0.18f));
	Light->SetAttenuationRadius(1500.0f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
}

void AForestFire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Life += DeltaSeconds;
	// It flares up, burns, then dies down over its last seconds.
	const float Strength = FMath::Clamp(Life / 1.0f, 0.0f, 1.0f) * FMath::Clamp((Lifetime - Life) / 5.0f, 0.0f, 1.0f);
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		const float Phase = Life * (6.0f + Index * 1.7f) + Index * 2.1f;
		const float Stretch = 1.0f + 0.25f * FMath::Sin(Phase);
		const float Size = (Index == 0 ? 0.9f : 0.6f) * Strength;
		Flames[Index]->SetRelativeScale3D(FVector(Size * 0.5f, Size * 0.5f, Size * 1.1f * Stretch));
	}
	if (Light)
	{
		Light->SetIntensity(120.0f * Strength * (0.8f + 0.2f * FMath::Sin(Life * 13.0f)));
	}

	// Standing in the flames burns.
	BurnCooldown -= DeltaSeconds;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && Strength > 0.3f && BurnCooldown <= 0.0f && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < 170.0f)
	{
		BurnCooldown = 1.2f;
		if (UForestSurvival* Survival = UForestSurvival::Get(this))
		{
			Survival->Hurt(1, this);
		}
	}
	if (Life >= Lifetime)
	{
		Destroy();
	}
}

// --- The events ---------------------------------------------------------------

bool UForestEvents::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UForestEvents::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForestEvents, STATGROUP_Tickables);
}

UForestEvents* UForestEvents::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UForestEvents>() : nullptr;
}

float UForestEvents::GetBannerAlpha() const
{
	return FMath::Clamp(FMath::Min(BannerTime / 0.4f, (BannerLength - BannerTime) / 0.8f), 0.0f, 1.0f);
}

void UForestEvents::ShowBanner(const FString& Text, float Seconds)
{
	Banner = Text;
	BannerTime = 0.0f;
	BannerLength = Seconds;
}

bool UForestEvents::GroundAt(float X, float Y, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestEventsGround), false);
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		Params.AddIgnoredActor(Player);
	}
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, Y, 30000.0f), FVector(X, Y, -30000.0f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void UForestEvents::EnsureVisuals()
{
	if (Visuals.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	Visuals = Holder;

	UMaterialInstanceDynamic* Warning = Glow(Holder, FLinearColor(0.75f, 0.85f, 1.0f), 0.0f);
	WarningMaterial = Warning;
	WarningDisc = AddMesh(Holder, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), Warning);
	WarningDisc->SetVisibility(false);

	UMaterialInterface* BoltBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/WorldBuilder/Materials/M_IWB_Bolt.M_IWB_Bolt"));
	UMaterialInstanceDynamic* BoltPaint = BoltBase ? UMaterialInstanceDynamic::Create(BoltBase, Holder) : nullptr;
	BoltMaterial = BoltPaint;
	Bolt = AddMesh(Holder, TEXT("/Game/WorldBuilder/Meshes/SM_IWB_Bolt.SM_IWB_Bolt"), BoltPaint);
	Bolt->SetBoundsScale(4.0f);
	Bolt->SetVisibility(false);

	Star = AddMesh(Holder, TEXT("/Engine/BasicShapes/Sphere.Sphere"), Glow(Holder, FLinearColor(0.7f, 0.85f, 1.0f), 8.0f));
	Star->SetVisibility(false);

	UPointLightComponent* Light = NewObject<UPointLightComponent>(Holder);
	Light->SetupAttachment(Root);
	Light->SetAbsolute(true, true, true);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(0.0f);
	Light->SetLightColor(FLinearColor(0.8f, 0.88f, 1.0f));
	Light->SetAttenuationRadius(7000.0f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	BoltLight = Light;
}

void UForestEvents::Trigger(FName Event)
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	if (Event == TEXT("Lightning"))
	{
		WarnLightning(Player->GetActorLocation(), 250.0f, 700.0f);
	}
	else if (Event == TEXT("Earthquake"))
	{
		StartQuake();
	}
	else if (Event == TEXT("Star"))
	{
		StartShootingStar();
	}
	else if (Event == TEXT("Storm"))
	{
		if (AForestSky* Sky = AForestSky::Get(GetWorld()))
		{
			Sky->SetMoment(Sky->GetHour(), TEXT("Storm"), false);
			ShowBanner(ForestText(TEXT("The sky darkens... A storm is coming. Watch out for lightning!"), TEXT("Le ciel s'assombrit... Un orage arrive. Attention aux éclairs !"), TEXT("El cielo se oscurece... Llega una tormenta. ¡Cuidado con los rayos!"), TEXT("Il cielo si oscura... Arriva un temporale. Attenta ai fulmini!")), 5.0f);
			if (UForestLife* Life = UForestLife::Get(GetWorld()))
			{
				Life->Log(ForestText(TEXT("A storm broke over the forest."), TEXT("Un orage a éclaté sur la forêt."), TEXT("Estalló una tormenta sobre el bosque."), TEXT("Un temporale è scoppiato sulla foresta.")));
			}
		}
	}
}

void UForestEvents::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	BannerTime += DeltaSeconds;
	APawn* Player = ActivePlayer(World);
	if (!Player)
	{
		return;
	}
	EnsureVisuals();
	const AForestSky* Sky = AForestSky::Get(World);
	const FVector Here = Player->GetActorLocation();

	// In a storm, bolts keep falling around the player.
	if (Sky && Sky->IsStorm() && StrikeCountdown < 0.0f)
	{
		NextStormBolt -= DeltaSeconds;
		if (NextStormBolt <= 0.0f)
		{
			NextStormBolt = FMath::FRandRange(14.0f, 32.0f);
			const bool bClose = FMath::FRand() < 0.35f;
			WarnLightning(Here, bClose ? 200.0f : 700.0f, bClose ? 650.0f : 1800.0f);
		}
	}

	// Every few minutes, something happens.
	NextEvent -= DeltaSeconds;
	if (NextEvent <= 0.0f && StrikeCountdown < 0.0f && QuakeLeft <= 0.0f && StarTime < 0.0f)
	{
		NextEvent = FMath::FRandRange(150.0f, 300.0f);
		const bool bClearNight = Sky && Sky->IsNight() && !Sky->IsRaining();
		const float Roll = FMath::FRand();
		if (bClearNight && Roll < 0.45f)
		{
			StartShootingStar();
		}
		else if (Roll < 0.7f)
		{
			StartQuake();
		}
		else if (Sky && !Sky->IsStorm())
		{
			Trigger(TEXT("Storm"));
		}
	}

	// The warned bolt: the circle pulses faster and brighter, then it falls.
	if (StrikeCountdown >= 0.0f)
	{
		StrikeCountdown -= DeltaSeconds;
		const float Urgency = 1.0f - FMath::Clamp(StrikeCountdown / WarningSeconds, 0.0f, 1.0f);
		if (WarningMaterial.IsValid())
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (6.0f + 20.0f * Urgency));
			WarningMaterial->SetScalarParameterValue(TEXT("Glow"), (0.4f + 2.0f * Urgency) * (0.4f + 0.6f * Pulse));
		}
		if (StrikeCountdown < 0.0f)
		{
			Strike();
		}
	}

	// The flash fades quickly.
	if (BoltGlow > 0.0f)
	{
		BoltGlow = FMath::Max(0.0f, BoltGlow - DeltaSeconds / 0.4f);
		const float Flicker = BoltGlow * (0.6f + 0.4f * FMath::Sin(GetWorld()->GetTimeSeconds() * 60.0f));
		if (BoltMaterial.IsValid())
		{
			BoltMaterial->SetScalarParameterValue(TEXT("Glow"), 20.0f * Flicker);
		}
		if (BoltLight.IsValid())
		{
			BoltLight->SetIntensity(30000.0f * Flicker);
		}
		if (Bolt.IsValid())
		{
			Bolt->SetVisibility(BoltGlow > 0.05f);
		}
	}

	TickQuake(DeltaSeconds);
	TickStar(DeltaSeconds);
}

// --- Lightning ----------------------------------------------------------------

void UForestEvents::WarnLightning(const FVector& Around, float MinDistance, float MaxDistance)
{
	EnsureVisuals();
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = FMath::FRandRange(MinDistance, MaxDistance);
		const FVector Spot = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
		float Z = 0.0f;
		if (GroundAt(Spot.X, Spot.Y, Z))
		{
			StrikeSpot = FVector(Spot.X, Spot.Y, Z);
			StrikeCountdown = WarningSeconds;
			if (WarningDisc.IsValid())
			{
				// The engine cylinder is 100 wide and 100 tall.
				WarningDisc->SetWorldLocation(StrikeSpot + FVector(0.0f, 0.0f, 4.0f));
				WarningDisc->SetWorldScale3D(FVector(StrikeRadius * 2.0f / 100.0f, StrikeRadius * 2.0f / 100.0f, 0.04f));
				WarningDisc->SetVisibility(true);
			}
			if (Distance < 900.0f)
			{
				ShowBanner(ForestText(TEXT("The air crackles... Lightning is about to strike. Move!"), TEXT("L'air crépite... La foudre va tomber. Bouge !"), TEXT("El aire chisporrotea... Va a caer un rayo. ¡Muévete!"), TEXT("L'aria crepita... Sta per cadere un fulmine. Spostati!")), 2.6f);
			}
			return;
		}
	}
}

void UForestEvents::Strike()
{
	StrikeCountdown = -1.0f;
	if (WarningDisc.IsValid())
	{
		WarningDisc->SetVisibility(false);
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector View = Player ? Player->GetActorLocation() : StrikeSpot;
	if (Bolt.IsValid())
	{
		// The bolt is a flat jagged ribbon in its XZ plane: turn it to face the player.
		const float Height = 4500.0f;
		const float Mirror = FMath::RandBool() ? 1.0f : -1.0f;
		Bolt->SetWorldLocationAndRotation(StrikeSpot, FRotator(0.0f, (View - StrikeSpot).Rotation().Yaw - 90.0f, 0.0f));
		Bolt->SetWorldScale3D(FVector(Height * Mirror * 0.4f, Height, Height));
		Bolt->SetVisibility(true);
	}
	if (BoltLight.IsValid())
	{
		BoltLight->SetWorldLocation(StrikeSpot + FVector(0.0f, 0.0f, 600.0f));
	}
	BoltGlow = 1.0f;
	if (USoundBase* Thunder = LoadObject<USoundBase>(nullptr, ThunderPath))
	{
		// So close there is no gap between the flash and the crack.
		UGameplayStatics::PlaySound2D(GetWorld(), Thunder, 1.6f, 1.1f);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AForestFire* Fire = GetWorld()->SpawnActor<AForestFire>(AForestFire::StaticClass(), FTransform(StrikeSpot), Params))
	{
		Fire->Ignite(FMath::FRandRange(18.0f, 30.0f));
	}

	UForestLife* Life = UForestLife::Get(GetWorld());
	// A roof over the head (a home of blocks) keeps the player safe from the bolt.
	const UForestBlocks* Blocks = UForestBlocks::Get(GetWorld());
	const bool bSheltered = Player && Blocks && Blocks->IsHome(Player->GetActorLocation());
	if (Player && !bSheltered && FVector::Dist2D(Player->GetActorLocation(), StrikeSpot) < StrikeRadius)
	{
		if (UForestSurvival* Survival = UForestSurvival::Get(GetWorld()))
		{
			Survival->Hurt(3, Visuals.Get());
		}
		ShowBanner(ForestText(TEXT("Struck by lightning!"), TEXT("Frappée par la foudre !"), TEXT("¡Te ha alcanzado un rayo!"), TEXT("Colpita da un fulmine!")), 3.0f);
		if (Life)
		{
			Life->Log(ForestText(TEXT("Lightning struck right next to you."), TEXT("La foudre est tombée juste à côté de toi."), TEXT("Un rayo cayó justo a tu lado."), TEXT("Un fulmine è caduto proprio accanto a te.")));
		}
	}
	else if (Life && Player && FVector::Dist2D(Player->GetActorLocation(), StrikeSpot) < 1500.0f)
	{
		Life->Log(ForestText(TEXT("Lightning set the grass on fire nearby."), TEXT("La foudre a mis le feu à l'herbe tout près."), TEXT("Un rayo prendió fuego a la hierba cerca de ti."), TEXT("Un fulmine ha incendiato l'erba qui vicino.")));
	}
}

// --- Earthquake -------------------------------------------------------------

void UForestEvents::StartQuake()
{
	QuakeLeft = QuakeSeconds;
	QuakeTime = 0.0f;
	QuakeOffset = FRotator::ZeroRotator;
	ShowBanner(ForestText(TEXT("The ground is shaking! Earthquake!"), TEXT("Le sol tremble ! Tremblement de terre !"), TEXT("¡El suelo tiembla! ¡Terremoto!"), TEXT("La terra trema! Terremoto!")), 4.0f);
	if (USoundBase* Thunder = LoadObject<USoundBase>(nullptr, ThunderPath))
	{
		// Thunder played very low sounds like the earth rumbling.
		UGameplayStatics::PlaySound2D(GetWorld(), Thunder, 1.4f, 0.35f);
	}
}

void UForestEvents::TickQuake(float DeltaSeconds)
{
	if (QuakeLeft <= 0.0f)
	{
		return;
	}
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	QuakeLeft -= DeltaSeconds;
	QuakeTime += DeltaSeconds;
	// The camera shakes, strongest in the middle of the quake.
	const float Strength = 1.6f * FMath::Sin(PI * FMath::Clamp(QuakeTime / QuakeSeconds, 0.0f, 1.0f));
	const FRotator Shake(FMath::Sin(QuakeTime * 37.0f) * Strength, FMath::Sin(QuakeTime * 29.0f + 1.3f) * Strength * 0.7f, 0.0f);
	if (Controller)
	{
		Controller->SetControlRotation(Controller->GetControlRotation() - QuakeOffset + Shake);
	}
	QuakeOffset = Shake;

	if (QuakeLeft <= 0.0f)
	{
		if (Controller)
		{
			Controller->SetControlRotation(Controller->GetControlRotation() - QuakeOffset);
		}
		QuakeOffset = FRotator::ZeroRotator;
		UForestLife* Life = UForestLife::Get(GetWorld());
		const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
		// Sometimes the shaken earth opens on something buried.
		if (Life && Player && FMath::FRand() < 0.6f && Life->SpawnChestNear(Player->GetActorLocation(), 500.0f, 1100.0f, false))
		{
			ShowBanner(ForestText(TEXT("The earthquake uncovered a buried chest nearby..."), TEXT("Le séisme a fait apparaître un coffre enterré tout près..."), TEXT("El terremoto ha desenterrado un cofre cerca..."), TEXT("Il terremoto ha portato alla luce un forziere qui vicino...")), 5.0f);
			Life->Log(ForestText(TEXT("An earthquake shook the forest and uncovered a chest."), TEXT("Un tremblement de terre a secoué la forêt et fait apparaître un coffre."), TEXT("Un terremoto sacudió el bosque y desenterró un cofre."), TEXT("Un terremoto ha scosso la foresta e scoperto un forziere.")));
		}
		else if (Life)
		{
			Life->Log(ForestText(TEXT("An earthquake shook the forest."), TEXT("Un tremblement de terre a secoué la forêt."), TEXT("Un terremoto sacudió el bosque."), TEXT("Un terremoto ha scosso la foresta.")));
		}
	}
}

// --- Shooting star ------------------------------------------------------------

void UForestEvents::StartShootingStar()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player)
	{
		return;
	}
	EnsureVisuals();
	const FVector Here = Player->GetActorLocation();
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Land2D = Here + Dir * FMath::FRandRange(1400.0f, 2600.0f);
		float Z = 0.0f;
		if (!GroundAt(Land2D.X, Land2D.Y, Z))
		{
			continue;
		}
		StarTo = FVector(Land2D.X, Land2D.Y, Z + 30.0f);
		// It comes from far beyond, high in the sky, on the same side.
		StarFrom = StarTo + Dir.RotateAngleAxis(FMath::FRandRange(-40.0f, 40.0f), FVector::UpVector) * 9000.0f + FVector(0.0f, 0.0f, 7000.0f);
		StarTime = 0.0f;
		if (Star.IsValid())
		{
			Star->SetVisibility(true);
		}
		ShowBanner(ForestText(TEXT("A shooting star! It is falling into the forest..."), TEXT("Une étoile filante ! Elle tombe dans la forêt..."), TEXT("¡Una estrella fugaz! Está cayendo en el bosque..."), TEXT("Una stella cadente! Sta cadendo nella foresta...")), 4.0f);
		return;
	}
}

void UForestEvents::TickStar(float DeltaSeconds)
{
	if (StarTime < 0.0f)
	{
		return;
	}
	StarTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(StarTime / StarSeconds, 0.0f, 1.0f);
	// Slow at first, then faster and faster as it falls.
	const FVector Where = FMath::Lerp(StarFrom, StarTo, Alpha * Alpha);
	if (Star.IsValid())
	{
		Star->SetWorldLocation(Where);
		Star->SetWorldScale3D(FVector(1.2f + 1.5f * Alpha));
	}
	if (BoltLight.IsValid())
	{
		BoltLight->SetWorldLocation(Where);
		BoltLight->SetIntensity(800.0f + 4000.0f * Alpha * Alpha);
	}
	if (Alpha < 1.0f)
	{
		return;
	}
	StarTime = -1.0f;
	if (Star.IsValid())
	{
		Star->SetVisibility(false);
	}
	BoltGlow = 0.6f;
	if (UForestLife* Life = UForestLife::Get(GetWorld()))
	{
		Life->SpawnChestAt(StarTo - FVector(0.0f, 0.0f, 30.0f), true);
		ShowBanner(ForestText(TEXT("The star landed nearby. Something is shining over there!"), TEXT("L'étoile est tombée tout près. Quelque chose brille là-bas !"), TEXT("La estrella cayó cerca. ¡Algo brilla por allí!"), TEXT("La stella è caduta qui vicino. Qualcosa brilla laggiù!")), 5.0f);
		Life->Log(ForestText(TEXT("A shooting star fell into the forest."), TEXT("Une étoile filante est tombée dans la forêt."), TEXT("Una estrella fugaz cayó en el bosque."), TEXT("Una stella cadente è caduta nella foresta.")));
	}
}
