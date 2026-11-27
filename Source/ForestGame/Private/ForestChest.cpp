#include "ForestChest.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* TintMaterial = TEXT("/Game/WorldBuilder/Materials/M_IWB_Tint.M_IWB_Tint");
	const TCHAR* GlowMaterial = TEXT("/Game/WorldBuilder/Materials/M_IWB_Glow.M_IWB_Glow");

	const FLinearColor Wood(0.32f, 0.17f, 0.08f);
	const FLinearColor DarkWood(0.2f, 0.1f, 0.05f);
	const FLinearColor Brass(0.75f, 0.55f, 0.2f);
}

AForestChest::AForestChest()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AForestChest::AddBox(const FVector& Location, const FVector& Size, const FLinearColor& Color, USceneComponent* Parent)
{
	UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this);
	Box->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Cube));
	Box->SetupAttachment(Parent ? Parent : RootComponent.Get());
	Box->SetRelativeLocation(Location);
	// The engine cube is 100 units wide.
	Box->SetRelativeScale3D(Size / 100.0f);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->RegisterComponent();
	if (UMaterialInterface* Tint = LoadObject<UMaterialInterface>(nullptr, TintMaterial))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Tint, this);
		Paint->SetVectorParameterValue(TEXT("Tint"), Color);
		Paint->SetScalarParameterValue(TEXT("Sway"), 0.0f);
		Box->SetMaterial(0, Paint);
	}
	return Box;
}

void AForestChest::Build(bool bStarChest)
{
	bStar = bStarChest;

	// The body: planks with brass corners.
	AddBox(FVector(0.0f, 0.0f, 18.0f), FVector(70.0f, 46.0f, 36.0f), Wood);
	AddBox(FVector(0.0f, 0.0f, 3.0f), FVector(74.0f, 50.0f, 6.0f), DarkWood);
	for (const float Side : {-1.0f, 1.0f})
	{
		AddBox(FVector(Side * 34.0f, 0.0f, 18.0f), FVector(5.0f, 48.0f, 37.0f), Brass);
	}

	// The lid turns around a hinge on the back edge.
	LidHinge = NewObject<USceneComponent>(this);
	LidHinge->SetupAttachment(RootComponent);
	LidHinge->SetRelativeLocation(FVector(0.0f, -23.0f, 36.0f));
	LidHinge->RegisterComponent();
	AddBox(FVector(0.0f, 23.0f, 7.0f), FVector(72.0f, 48.0f, 14.0f), Wood, LidHinge);
	AddBox(FVector(0.0f, 47.0f, 2.0f), FVector(12.0f, 3.0f, 12.0f), Brass, LidHinge);

	// The glint: a little light that winks now and then when the player is near.
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, GlowMaterial))
	{
		Glint = NewObject<UStaticMeshComponent>(this);
		Glint->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Sphere));
		Glint->SetupAttachment(RootComponent);
		Glint->SetRelativeLocation(FVector(30.0f, 24.0f, 44.0f));
		Glint->SetRelativeScale3D(FVector(0.12f));
		Glint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glint->SetCastShadow(false);
		Glint->RegisterComponent();
		GlintMaterial = UMaterialInstanceDynamic::Create(Glow, this);
		GlintMaterial->SetVectorParameterValue(TEXT("Tint"), bStar ? FLinearColor(0.6f, 0.8f, 1.0f) : FLinearColor(1.0f, 0.9f, 0.55f));
		GlintMaterial->SetScalarParameterValue(TEXT("Glow"), 0.0f);
		Glint->SetMaterial(0, GlintMaterial);
	}
	if (bStar)
	{
		StarLight = NewObject<UPointLightComponent>(this);
		StarLight->SetupAttachment(RootComponent);
		StarLight->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
		StarLight->SetIntensityUnits(ELightUnits::Candelas);
		StarLight->SetIntensity(60.0f);
		StarLight->SetLightColor(FLinearColor(0.55f, 0.75f, 1.0f));
		StarLight->SetAttenuationRadius(900.0f);
		StarLight->SetCastShadows(false);
		StarLight->RegisterComponent();
	}
}

void AForestChest::Open()
{
	if (bOpened)
	{
		return;
	}
	bOpened = true;
	if (Glint)
	{
		Glint->SetVisibility(false);
	}
	if (StarLight)
	{
		StarLight->SetVisibility(false);
	}
}

void AForestChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	if (bOpened)
	{
		// The lid swings open, with a small bounce.
		LidAngle = FMath::FInterpTo(LidAngle, 105.0f, DeltaSeconds, 5.0f);
		if (LidHinge)
		{
			LidHinge->SetRelativeRotation(FRotator(0.0f, 0.0f, -LidAngle));
		}
		return;
	}

	if (!GlintMaterial)
	{
		return;
	}
	// A star chest always shines; a hidden one only winks when someone is close.
	float Strength = 0.0f;
	if (bStar)
	{
		Strength = 3.0f + 1.5f * FMath::Sin(Time * 3.0f);
	}
	else if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const float Distance = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
		const float Near = FMath::Clamp(1.0f - (Distance - 300.0f) / 900.0f, 0.0f, 1.0f);
		const float Wink = FMath::Pow(FMath::Max(0.0f, FMath::Sin(Time * 1.7f)), 12.0f);
		Strength = Near * Wink * 6.0f;
	}
	GlintMaterial->SetScalarParameterValue(TEXT("Glow"), Strength);
	if (Glint)
	{
		Glint->SetRelativeScale3D(FVector(0.08f + 0.1f * FMath::Min(1.0f, Strength / 4.0f)));
	}
}
