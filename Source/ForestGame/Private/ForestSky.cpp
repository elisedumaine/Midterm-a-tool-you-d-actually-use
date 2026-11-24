#include "ForestSky.h"

#include "ForestLanguage.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sound/SoundBase.h"

namespace
{
	struct FWeatherLook
	{
		const TCHAR* Name;
		float Clouds, Rain, Snow, Mist, Wind, Sunny;
		float MinHours, MaxHours;   // how long it lasts, in game hours
	};

	// Every weather as a mix of a few ingredients; the sky blends between them.
	const FWeatherLook GLooks[] = {
		{TEXT("Clear"),     0.05f, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f, 2.0f, 5.0f},
		{TEXT("Cloudy"),    0.75f, 0.0f, 0.0f, 0.10f, 0.40f, 0.0f, 1.5f, 3.5f},
		{TEXT("Drizzle"),   0.65f, 0.35f, 0.0f, 0.25f, 0.30f, 0.0f, 1.0f, 2.5f},
		{TEXT("Rain"),      0.85f, 0.80f, 0.0f, 0.25f, 0.55f, 0.0f, 1.0f, 3.0f},
		{TEXT("Storm"),     1.00f, 1.00f, 0.0f, 0.30f, 1.00f, 0.0f, 0.7f, 1.5f},
		{TEXT("SunShower"), 0.30f, 0.55f, 0.0f, 0.0f, 0.30f, 1.0f, 0.6f, 1.2f},
		{TEXT("Mist"),      0.25f, 0.0f, 0.0f, 1.00f, 0.05f, 0.0f, 1.0f, 3.0f},
		{TEXT("Snow"),      0.80f, 0.0f, 0.90f, 0.30f, 0.35f, 0.0f, 1.5f, 3.5f},
		{TEXT("LightSnow"), 0.55f, 0.0f, 0.40f, 0.15f, 0.20f, 0.0f, 1.5f, 3.0f},
	};

	const FWeatherLook& LookFor(FName Weather)
	{
		for (const FWeatherLook& Look : GLooks)
		{
			if (Weather == FName(Look.Name))
			{
				return Look;
			}
		}
		return GLooks[0];
	}

	FName Choose(std::initializer_list<TPair<const TCHAR*, float>> Options)
	{
		float Total = 0.0f;
		for (const auto& Option : Options) Total += Option.Value;
		float Roll = FMath::FRand() * Total;
		for (const auto& Option : Options)
		{
			Roll -= Option.Value;
			if (Roll <= 0.0f) return FName(Option.Key);
		}
		return FName(Options.begin()->Key);
	}

	float Approach(float From, float To, float DeltaSeconds, float Speed)
	{
		return FMath::FInterpTo(From, To, DeltaSeconds, Speed);
	}
}

AForestSky::AForestSky()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(RootComponent);
	MoonLight->SetIntensity(0.0f);
	MoonLight->SetLightColor(FLinearColor(0.55f, 0.65f, 1.0f));
	MoonLight->SetAtmosphereSunLight(true);
	MoonLight->SetAtmosphereSunLightIndex(1);
	MoonLight->LightSourceAngle = 2.2f;
	MoonLight->SetCastShadows(false);

	Stars = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(RootComponent);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	// The dome is moved to the camera by its material: never cull it.
	Stars->SetBoundsScale(1000.0f);

	Leaves = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Leaves"));
	Leaves->SetupAttachment(RootComponent);
	Leaves->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Leaves->SetCastShadow(false);
	Leaves->SetCanEverAffectNavigation(false);

	auto Decoration = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(RootComponent);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetHiddenInGame(true);
		return Component;
	};
	Rainbow = Decoration(TEXT("Rainbow"));
	Bolt = Decoration(TEXT("Bolt"));
	Bolt->SetBoundsScale(4.0f);

	Flash = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(RootComponent);
	Flash->SetIntensity(0.0f);
	Flash->SetLightColor(FLinearColor(0.75f, 0.82f, 1.0f));
	Flash->SetCastShadows(false);

	RainSound = CreateDefaultSubobject<UAudioComponent>(TEXT("RainSound"));
	RainSound->SetupAttachment(RootComponent);
	RainSound->bAutoActivate = false;
	WindSound = CreateDefaultSubobject<UAudioComponent>(TEXT("WindSound"));
	WindSound->SetupAttachment(RootComponent);
	WindSound->bAutoActivate = false;

	// Trees sway; grass, shrubs and flowers are lighter and sway more.
	SwayingLayers.Add(TEXT("IWB_Trees"), 1.0f);
	SwayingLayers.Add(TEXT("IWB_Grass"), 2.0f);
	SwayingLayers.Add(TEXT("IWB_Flowers"), 1.5f);
}

AForestSky* AForestSky::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AForestSky> It(World);
	return It ? *It : nullptr;
}

FForestWeatherMix AForestSky::MixFor(FName Weather)
{
	const FWeatherLook& Look = LookFor(Weather);
	FForestWeatherMix Result;
	Result.Clouds = Look.Clouds;
	Result.Rain = Look.Rain;
	Result.Snow = Look.Snow;
	Result.Mist = Look.Mist;
	Result.Wind = Look.Wind;
	Result.Sunny = Look.Sunny;
	return Result;
}

FName AForestSky::PickNext() const
{
	const bool bMorning = Hour > 4.0f && Hour < 9.5f;
	FName Next = Current;
	if (Current == TEXT("Clear"))
	{
		Next = bMorning ? Choose({{TEXT("Mist"), 3.0f}, {TEXT("Cloudy"), 2.0f}, {TEXT("Clear"), 1.0f}})
		                : Choose({{TEXT("Cloudy"), 4.0f}, {TEXT("SunShower"), 1.5f}, {TEXT("Clear"), 1.0f}});
	}
	else if (Current == TEXT("Cloudy"))
	{
		Next = Choose({{TEXT("Drizzle"), 3.0f}, {TEXT("Rain"), 2.0f}, {TEXT("Clear"), 2.0f}, {TEXT("SunShower"), 1.0f}});
	}
	else if (Current == TEXT("Drizzle"))
	{
		Next = Choose({{TEXT("Rain"), 3.0f}, {TEXT("Cloudy"), 2.0f}, {TEXT("SunShower"), 1.5f}});
	}
	else if (Current == TEXT("Rain"))
	{
		Next = Choose({{TEXT("Storm"), 2.0f}, {TEXT("Drizzle"), 2.0f}, {TEXT("SunShower"), 1.5f}, {TEXT("Cloudy"), 1.0f}});
	}
	else if (Current == TEXT("Storm"))
	{
		Next = Choose({{TEXT("Rain"), 3.0f}, {TEXT("SunShower"), 1.0f}});
	}
	else if (Current == TEXT("SunShower"))
	{
		Next = Choose({{TEXT("Clear"), 3.0f}, {TEXT("Cloudy"), 1.0f}});
	}
	else if (Current == TEXT("Mist"))
	{
		Next = Choose({{TEXT("Clear"), 3.0f}, {TEXT("Cloudy"), 2.0f}});
	}
	else if (Current == TEXT("Snow"))
	{
		Next = Choose({{TEXT("LightSnow"), 3.0f}, {TEXT("Cloudy"), 2.0f}});
	}
	else if (Current == TEXT("LightSnow"))
	{
		Next = Choose({{TEXT("Snow"), 2.0f}, {TEXT("Cloudy"), 2.0f}, {TEXT("Clear"), 2.0f}});
	}
	if (bWinter)
	{
		// In winter the clouds bring snow instead of rain.
		if (Next == TEXT("Drizzle") || Next == TEXT("SunShower")) Next = TEXT("LightSnow");
		if (Next == TEXT("Rain") || Next == TEXT("Storm")) Next = TEXT("Snow");
	}
	return Next;
}

float AForestSky::HoursToSeconds(float Hours) const
{
	// With time stopped, the weather still changes every minute or two.
	return DayMinutes > 0.0f ? Hours / 24.0f * DayMinutes * 60.0f : Hours * 30.0f;
}

void AForestSky::BeginPlay()
{
	Super::BeginPlay();
	Hour = FMath::Fmod(StartHour, 24.0f);
	Current = StartWeather;
	Mix = Target = MixFor(Current);
	const FWeatherLook& Look = LookFor(Current);
	TimeToChange = HoursToSeconds(FMath::FRandRange(Look.MinHours, Look.MaxHours));
	WetSurfaces = Mix.Rain;
	SnowCover = Mix.Snow;

	if (Sun)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunIntensity = Light->Intensity;
			SunColor = Light->GetLightColor();
			Light->SetAtmosphereSunLightIndex(0);
		}
	}
	if (Fog)
	{
		if (UExponentialHeightFogComponent* Component = Fog->GetComponent())
		{
			FogDensity = Component->FogDensity;
			FogColor = Component->FogInscatteringLuminance;
			ValleyFogDensity = Component->SecondFogData.FogDensity;
		}
	}
	auto TakeMaterial = [](AActor* Actor, float& BaseOpacity) -> UMaterialInstanceDynamic*
	{
		UStaticMeshComponent* Component = Actor ? Actor->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (!Component)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0));
		if (!Material)
		{
			Material = Component->CreateAndSetMaterialInstanceDynamic(0);
		}
		if (Material)
		{
			float Opacity = BaseOpacity;
			if (Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Opacity")), Opacity))
			{
				BaseOpacity = Opacity;
			}
		}
		return Material;
	};
	RainMaterial = TakeMaterial(RainActor, RainOpacity);
	SnowMaterial = TakeMaterial(SnowActor, SnowOpacity);

	FlashIntensity = FMath::Max(SunIntensity, 3.0f);
	if (Rainbow && Rainbow->GetStaticMesh())
	{
		RainbowMaterial = Rainbow->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (Bolt && Bolt->GetStaticMesh())
	{
		BoltMaterial = Bolt->CreateAndSetMaterialInstanceDynamic(0);
	}
	for (UAudioComponent* Sound : {RainSound.Get(), WindSound.Get()})
	{
		if (Sound && Sound->Sound)
		{
			Sound->SetVolumeMultiplier(0.001f);
			Sound->Play();
		}
	}
	StartSway();

	// Leaves: one instance each, coloured by a random value the material reads.
	const FVector View = Viewpoint();
	LeafState.SetNum(Leaves && Leaves->GetStaticMesh() ? FMath::Max(0, LeafCount) : 0);
	if (Leaves)
	{
		Leaves->ClearInstances();
		Leaves->SetNumCustomDataFloats(1);
	}
	for (int32 Index = 0; Index < LeafState.Num(); ++Index)
	{
		FLeaf& Leaf = LeafState[Index];
		Leaf = LeafSpawn(View, true);
		Leaves->AddInstance(FTransform(FQuat::Identity, Leaf.Position, FVector(0.001f)), true);
		Leaves->SetCustomDataValue(Index, 0, FMath::FRand(), true);
	}
}

void AForestSky::SetMoment(float AtHour, FName Weather, bool bAfterRain)
{
	Hour = FMath::Fmod(AtHour, 24.0f);
	Current = Weather;
	Mix = Target = MixFor(Current);
	const FWeatherLook& Look = LookFor(Current);
	TimeToChange = HoursToSeconds(FMath::FRandRange(Look.MinHours, Look.MaxHours));
	WetSurfaces = bAfterRain ? 1.0f : Mix.Rain;
}

void AForestSky::StartSway()
{
	// Every plant's material reads Wind from the weather parameters; Sway
	// says how far this one may bend (0 = rocks and ground stay still).
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		float Amount = 0.0f;
		for (const TPair<FName, float>& Layer : SwayingLayers)
		{
			if (It->ActorHasTag(Layer.Key))
			{
				Amount = FMath::Max(Amount, Layer.Value);
			}
		}
		if (Amount <= 0.0f)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot));
				if (!Material)
				{
					Material = Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
				}
				if (Material)
				{
					Material->SetScalarParameterValue(TEXT("Sway"), Amount);
				}
			}
		}
	}
}

FVector AForestSky::Viewpoint() const
{
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		return Camera->GetCameraLocation();
	}
	const UWorld* World = GetWorld();
	if (World && World->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		return World->ViewLocationsRenderedLastFrame[0];
	}
	return GetActorLocation();
}

float AForestSky::GroundHeight(float X, float Y) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SkyGround), false, this);
	const FVector Start(X, Y, 30000.0f);
	const FVector End(X, Y, -5000.0f);
	if (GetWorld() && GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return ValleyHeight;
}

AForestSky::FLeaf AForestSky::LeafSpawn(const FVector& Around, bool bAnywhere) const
{
	// New leaves come in from upwind, so they blow towards the player.
	const FVector Along = WindDirection.GetSafeNormal2D();
	const FVector Across(-Along.Y, Along.X, 0.0f);
	FVector Point;
	if (bAnywhere)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = LeafRadius * FMath::Sqrt(FMath::FRand());
		Point = Around + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
	}
	else
	{
		Point = Around - Along * LeafRadius * FMath::FRandRange(0.6f, 0.95f) + Across * LeafRadius * FMath::FRandRange(-0.7f, 0.7f);
	}
	const float Ground = GroundHeight(Point.X, Point.Y);
	FLeaf Leaf;
	Leaf.Position = FVector(Point.X, Point.Y, Ground + FMath::FRandRange(60.0f, 900.0f));
	Leaf.Spin = FVector(FMath::FRandRange(-300.0f, 300.0f), FMath::FRandRange(-200.0f, 200.0f), FMath::FRandRange(-250.0f, 250.0f));
	Leaf.Phase = FMath::FRandRange(0.0f, 2.0f * PI);
	Leaf.Floor = Ground - 40.0f;
	Leaf.Size = FMath::FRandRange(0.7f, 1.3f);
	return Leaf;
}

void AForestSky::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	// Time.
	if (DayMinutes > 0.0f)
	{
		Hour += DeltaSeconds * 24.0f / (DayMinutes * 60.0f);
		if (Hour >= 24.0f)
		{
			Hour -= 24.0f;
			++Day;
		}
	}

	// Weather: a new target now and then, reached slowly.
	if (bLivingWeather)
	{
		TimeToChange -= DeltaSeconds;
		if (TimeToChange <= 0.0f)
		{
			Current = PickNext();
			Target = MixFor(Current);
			const FWeatherLook& Look = LookFor(Current);
			TimeToChange = HoursToSeconds(FMath::FRandRange(Look.MinHours, Look.MaxHours));
		}
	}
	const float Blend = 0.12f;   // about 20 seconds to settle
	Mix.Clouds = Approach(Mix.Clouds, Target.Clouds, DeltaSeconds, Blend);
	Mix.Rain = Approach(Mix.Rain, Target.Rain, DeltaSeconds, Blend);
	Mix.Snow = Approach(Mix.Snow, Target.Snow, DeltaSeconds, Blend);
	Mix.Mist = Approach(Mix.Mist, Target.Mist, DeltaSeconds, Blend * 0.6f);
	Mix.Wind = Approach(Mix.Wind, Target.Wind, DeltaSeconds, Blend);
	Mix.Sunny = Approach(Mix.Sunny, Target.Sunny, DeltaSeconds, Blend);

	// Sun height: up from 6:00, highest at noon, down at 18:00, below at night.
	const float Elevation = FMath::Sin((Hour - 6.0f) / 12.0f * PI);
	const float Night = FMath::Clamp(-Elevation * 5.0f + 0.25f, 0.0f, 1.0f);
	const FVector View = Viewpoint();
	const float Step = FMath::Min(DeltaSeconds, 0.1f);
	Time += DeltaSeconds;
	ApplySun(Elevation);
	UpdateStorm(Step, View);
	ApplyWeather(DeltaSeconds, Night);
	UpdateLeaves(Step, View);
	UpdateRainbow(View, Elevation);
	UpdateMist();
	UpdateSounds();
}

void AForestSky::UpdateLeaves(float DeltaSeconds, const FVector& View)
{
	if (!Leaves || LeafState.Num() == 0 || Leaves->GetInstanceCount() != LeafState.Num())
	{
		return;
	}
	// A breeze lifts a few leaves; a storm fills the air with them.  Fewer in winter.
	const float Strength = FMath::Clamp((Mix.Wind - 0.12f) / 0.6f, 0.0f, 1.0f) * (bWinter ? 0.3f : 1.0f) * (1.0f - 0.6f * Mix.Snow);
	const int32 Shown = FMath::RoundToInt(Strength * LeafState.Num());
	const FVector Along = WindDirection.GetSafeNormal2D();
	const float Gust = 0.7f + 0.3f * FMath::Sin(Time * 0.6f) + 0.2f * FMath::Sin(Time * 1.7f + 1.0f);
	const float Speed = (120.0f + 850.0f * Mix.Wind) * Gust;
	TArray<FTransform> Transforms;
	Transforms.Reserve(LeafState.Num());
	for (int32 Index = 0; Index < LeafState.Num(); ++Index)
	{
		FLeaf& Leaf = LeafState[Index];
		const FVector Flutter(
			FMath::Sin(Time * 2.1f + Leaf.Phase) * 70.0f,
			FMath::Cos(Time * 1.7f + Leaf.Phase * 1.3f) * 70.0f,
			-80.0f + FMath::Sin(Time * 1.3f + Leaf.Phase) * 110.0f * Mix.Wind);
		Leaf.Position += (Along * Speed + Flutter) * DeltaSeconds;
		if (FVector::Dist2D(Leaf.Position, View) > LeafRadius || Leaf.Position.Z < Leaf.Floor || Leaf.Position.Z > View.Z + 2500.0f)
		{
			Leaf = LeafSpawn(View, false);
		}
		const FRotator Tumble(Time * Leaf.Spin.X + Leaf.Phase * 57.0f, Time * Leaf.Spin.Y, Time * Leaf.Spin.Z);
		const float Scale = Index < Shown ? Leaf.Size : 0.001f;
		Transforms.Add(FTransform(Tumble, Leaf.Position, FVector(Scale)));
	}
	Leaves->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}

void AForestSky::UpdateRainbow(const FVector& View, float Elevation)
{
	if (!Rainbow || !RainbowMaterial || !Sun)
	{
		return;
	}
	// Rain and sun together, the sun behind you and not too high: a rainbow
	// circles the point opposite the sun (only its upper part rises above the horizon).
	const float Amount = FMath::Clamp(Mix.Sunny * Mix.Rain * 2.5f, 0.0f, 1.0f)
		* FMath::Clamp(Elevation * 6.0f, 0.0f, 1.0f) * (1.0f - 0.5f * Mix.Clouds);
	const bool bShow = Amount > 0.01f;
	Rainbow->SetHiddenInGame(!bShow);
	if (bShow)
	{
		const FVector AntiSolar = Sun->GetActorForwardVector();
		Rainbow->SetWorldLocationAndRotation(View, AntiSolar.Rotation());
		Rainbow->SetWorldScale3D(FVector(RainbowDistance));
		RainbowMaterial->SetScalarParameterValue(TEXT("Strength"), Amount);
	}
}

void AForestSky::UpdateStorm(float DeltaSeconds, const FVector& View)
{
	// A storm is strong wind and heavy rain together.
	const float Storm = FMath::Clamp((Mix.Wind - 0.7f) / 0.3f, 0.0f, 1.0f) * FMath::Clamp(Mix.Rain * 1.2f, 0.0f, 1.0f);
	NextStrike -= DeltaSeconds * Storm;
	if (NextStrike <= 0.0f)
	{
		NextStrike = FMath::FRandRange(5.0f, 14.0f);
		if (Storm > 0.3f)
		{
			Strike(View);
		}
	}

	// A bright stroke, a short dark gap, a second weaker stroke.
	FlashAge += DeltaSeconds;
	float Glow = FMath::Exp(-FlashAge * 10.0f);
	if (FlashAge > 0.2f)
	{
		Glow += 0.75f * FMath::Exp(-(FlashAge - 0.2f) * 12.0f);
	}
	if (FlashAge > 0.12f && FlashAge < 0.2f)
	{
		Glow *= 0.15f;
	}
	FlashGlow = FlashAge < 2.0f ? Glow : 0.0f;
	if (Flash)
	{
		Flash->SetIntensity(FlashIntensity * FlashGlow);
	}
	if (Bolt)
	{
		Bolt->SetHiddenInGame(FlashGlow < 0.05f);
		if (BoltMaterial)
		{
			BoltMaterial->SetScalarParameterValue(TEXT("Glow"), FlashGlow);
		}
	}

	// Thunder arrives later the farther the strike: sound covers about 343 m a second.
	if (ThunderDelay >= 0.0f)
	{
		ThunderDelay -= DeltaSeconds;
		if (ThunderDelay < 0.0f && ThunderSound)
		{
			UGameplayStatics::PlaySound2D(this, ThunderSound, ThunderVolume, FMath::FRandRange(0.85f, 1.1f));
		}
	}
}

void AForestSky::Strike(const FVector& View)
{
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(2500.0f, 9000.0f);
	const FVector Ground2D = View + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
	const FVector Base(Ground2D.X, Ground2D.Y, GroundHeight(Ground2D.X, Ground2D.Y));
	const float Height = FMath::Max(View.Z, Base.Z) + FMath::FRandRange(5000.0f, 8000.0f) - Base.Z;
	if (Bolt)
	{
		// The bolt is a flat jagged ribbon in its XZ plane: turn it to face the player.
		const float Yaw = (View - Base).Rotation().Yaw - 90.0f;
		const float Mirror = FMath::RandBool() ? 1.0f : -1.0f;
		Bolt->SetWorldLocationAndRotation(Base, FRotator(0.0f, Yaw, 0.0f));
		Bolt->SetWorldScale3D(FVector(Height * Mirror, Height, Height));
	}
	if (Flash)
	{
		Flash->SetWorldRotation(FRotator(-45.0f, (View - Base).Rotation().Yaw, 0.0f));
	}
	FlashAge = 0.0f;
	ThunderDelay = Distance / 34300.0f + 0.15f;
	ThunderVolume = FMath::GetMappedRangeValueClamped(FVector2D(2500.0f, 9000.0f), FVector2D(1.0f, 0.45f), Distance);
}

void AForestSky::UpdateMist()
{
	UExponentialHeightFogComponent* Component = Fog ? Fog->GetComponent() : nullptr;
	if (!Component)
	{
		return;
	}
	// Mist pools in the low ground at dawn (and a little in the evening), more
	// after rain, and the wind blows it away.  It is a second, low fog layer.
	const float Dawn = FMath::Clamp(1.0f - FMath::Abs(Hour - 6.5f) / 2.5f, 0.0f, 1.0f);
	const float Evening = 0.4f * FMath::Clamp(1.0f - FMath::Abs(Hour - 21.0f) / 3.0f, 0.0f, 1.0f);
	const float Calm = 1.0f - 0.8f * FMath::Clamp(Mix.Wind * 1.5f, 0.0f, 1.0f);
	const float Valley = FMath::Clamp(Mix.Mist + (Dawn + Evening) * (0.55f + 0.45f * WetSurfaces) * Calm, 0.0f, 1.2f);
	Component->SetSecondFogDensity(ValleyFogDensity + FogDensity * 6.0f * Valley);
	Component->SetSecondFogHeightFalloff(1.2f);
	Component->SetSecondFogHeightOffset(ValleyHeight + 150.0f - Fog->GetActorLocation().Z);
}

void AForestSky::UpdateSounds()
{
	if (RainSound && RainSound->IsPlaying())
	{
		RainSound->SetVolumeMultiplier(FMath::Max(0.001f, FMath::Pow(Mix.Rain, 0.8f) * 0.9f));
	}
	if (WindSound && WindSound->IsPlaying())
	{
		const float Blow = FMath::Clamp((Mix.Wind - 0.1f) / 0.9f, 0.0f, 1.0f);
		WindSound->SetVolumeMultiplier(FMath::Max(0.001f, Blow * 0.8f));
		WindSound->SetPitchMultiplier(0.85f + 0.3f * Blow);
	}
}

void AForestSky::ApplySun(float Elevation)
{
	const float Angle = (Hour - 6.0f) / 12.0f;                 // 0 at sunrise, 1 at sunset
	const float Yaw = SunriseYaw + Angle * 180.0f;
	const float Pitch = -Elevation * NoonElevation;              // negative pitch points down: sun above
	const float Day01 = FMath::Clamp(Elevation * 4.0f, 0.0f, 1.0f);
	// Clouds dim the sun, except in a sun shower.
	const float Shade = 1.0f - (0.8f * Mix.Clouds + 0.15f * Mix.Rain) * (1.0f - Mix.Sunny);
	if (Sun)
	{
		Sun->SetActorRotation(FRotator(FMath::Min(Pitch, 4.0f), Yaw, 0.0f));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetIntensity(SunIntensity * Day01 * Shade);
			// Low sun is warm, high sun keeps the palette's colour.
			const FLinearColor Low(1.0f, 0.45f, 0.22f);
			Light->SetLightColor(FMath::Lerp(Low, SunColor, FMath::Clamp(Elevation * 2.5f, 0.0f, 1.0f)));
		}
	}
	if (MoonLight)
	{
		// The moon rides opposite the sun.
		MoonLight->SetWorldRotation(FRotator(FMath::Min(Elevation * NoonElevation * 0.8f, 4.0f), Yaw + 180.0f, 0.0f));
		MoonLight->SetIntensity(0.35f * FMath::Clamp(-Elevation * 4.0f, 0.0f, 1.0f) * (1.0f - 0.7f * Mix.Clouds));
	}
}

void AForestSky::ApplyWeather(float DeltaSeconds, float Night)
{
	// Surfaces get wet fast in the rain and dry slowly; snow settles and melts slowly.
	WetSurfaces = Approach(WetSurfaces, Mix.Rain > 0.05f ? 1.0f : 0.0f, DeltaSeconds, Mix.Rain > 0.05f ? 0.25f : 0.03f);
	SnowCover = Approach(SnowCover, Mix.Snow > 0.05f ? 1.0f : 0.0f, DeltaSeconds, Mix.Snow > 0.05f ? 0.05f : 0.02f);
	SetParameter(TEXT("Wet"), WetSurfaces);
	SetParameter(TEXT("Snow"), SnowCover);
	SetParameter(TEXT("Night"), Night);
	SetParameter(TEXT("Clouds"), Mix.Clouds);
	SetParameter(TEXT("Wind"), Mix.Wind);

	auto Fall = [](AActor* Actor, UMaterialInstanceDynamic* Material, float BaseOpacity, float Amount)
	{
		if (!Actor)
		{
			return;
		}
		Actor->SetActorHiddenInGame(Amount < 0.02f);
		if (Material)
		{
			Material->SetScalarParameterValue(TEXT("Opacity"), BaseOpacity * FMath::Clamp(Amount * 1.3f, 0.0f, 1.3f));
		}
	};
	Fall(RainActor, RainMaterial, RainOpacity, Mix.Rain);
	Fall(SnowActor, SnowMaterial, SnowOpacity, Mix.Snow);

	if (Fog)
	{
		if (UExponentialHeightFogComponent* Component = Fog->GetComponent())
		{
			Component->SetFogDensity(FogDensity * (1.0f + 3.0f * Mix.Mist + 1.0f * Mix.Rain + 0.5f * Mix.Clouds));
			// Grey under clouds, nearly black at night.
			const FLinearColor Grey(0.32f, 0.34f, 0.38f);
			FLinearColor Colour = FMath::Lerp(FogColor, Grey, Mix.Clouds * 0.7f * (1.0f - Mix.Sunny));
			Colour = FMath::Lerp(Colour, FLinearColor(0.012f, 0.016f, 0.03f), Night);
			// Lightning lights up the air for a blink.
			Colour = FMath::Lerp(Colour, FLinearColor(0.55f, 0.6f, 0.8f), FMath::Clamp(FlashGlow * 0.6f, 0.0f, 1.0f));
			Component->SetFogInscatteringColor(Colour);
		}
	}
}

void AForestSky::SetParameter(FName Name, float Value)
{
	if (WeatherParameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherParameters, Name, Value);
	}
}

FString AForestSky::Describe() const
{
	const int32 Hours = FMath::FloorToInt(Hour);
	const int32 Minutes = FMath::FloorToInt((Hour - Hours) * 60.0f);
	return FOREST_PRINTF(TEXT("Day %d  -  %02d:%02d  -  %s"), TEXT("Jour %d  -  %02d:%02d  -  %s"),
		Day, Hours, Minutes, *ForestLanguage::Weather(Current));
}
