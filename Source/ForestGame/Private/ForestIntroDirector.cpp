#include "ForestIntroDirector.h"

#include "ForestCreator.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ForestGameManager.h"
#include "ForestIntroWidget.h"
#include "ForestLanguage.h"
#include "ForestSky.h"
#include "ForestWakeUp.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	/** 0 before Start, 1 after End, a soft S-curve between. */
	float Ease(float Clock, float Start, float End)
	{
		const float Alpha = FMath::Clamp((Clock - Start) / FMath::Max(0.01f, End - Start), 0.0f, 1.0f);
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}

	/** Lying on the grass: the mesh is turned onto its side and lowered. */
	constexpr float LyingRoll = 88.0f;
	constexpr float LyingDrop = -38.0f;

	// Falling asleep (seconds after PLAY).
	constexpr float EyesCloseStart = 0.6f;
	constexpr float EyesCloseEnd = 4.4f;
	constexpr float NightfallAt = 5.2f;
	constexpr float WakeAt = 9.5f;

	// Waking up (seconds after WakeAt).
	constexpr float FlutterStart = 6.2f;
	constexpr float FlutterEnd = 7.8f;
	constexpr float HalfOpenStart = 10.0f;
	constexpr float HalfOpenEnd = 13.0f;
	constexpr float EyesOpenStart = 14.2f;
	constexpr float EyesOpenEnd = 18.5f;
	constexpr float LowShotAt = 18.5f;
	constexpr float SitStart = 21.0f;
	constexpr float SitEnd = 24.0f;
	constexpr float StandStart = 25.0f;
	constexpr float StandEnd = 28.0f;
	constexpr float BackToPlayerAt = 25.5f;
	constexpr float WakeEnd = 31.0f;
}

AForestIntroDirector::AForestIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

ACameraActor* AForestIntroDirector::SpawnCamera(const FVector& Location, const FRotator& Rotation, float Fov)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, Params);
	if (Camera)
	{
		UCameraComponent* Lens = Camera->GetCameraComponent();
		Lens->SetFieldOfView(Fov);
		Lens->SetConstraintAspectRatio(false);
		// A soft dark edge around the picture, like an old film.
		Lens->PostProcessSettings.bOverride_VignetteIntensity = true;
		Lens->PostProcessSettings.VignetteIntensity = 0.75f;
		Lens->PostProcessBlendWeight = 1.0f;
	}
	return Camera;
}

bool AForestIntroDirector::GroundAt(const FVector& Spot, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestIntroGround), false, PlayerCharacter);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 20000.0f), Spot - FVector(0.0f, 0.0f, 20000.0f), ECC_Visibility, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

namespace
{
	/** The ground just under a camera spot (a short ray, so a canopy high above does not count). */
	bool IntroGroundBelow(const UWorld* World, const AActor* Ignored, const FVector& Spot, float& OutZ)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestIntroGroundBelow), false, Ignored);
		if (World->LineTraceSingleByChannel(Hit, Spot + FVector(0.0f, 0.0f, 250.0f), Spot - FVector(0.0f, 0.0f, 3000.0f), ECC_Visibility, Params))
		{
			OutZ = Hit.ImpactPoint.Z;
			return true;
		}
		return false;
	}
}

bool AForestIntroDirector::SeesPlayer(const FVector& From, const FVector& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestIntroShot), false, PlayerCharacter);
	for (const float Up : {70.0f, 0.0f, -70.0f})
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, Target + FVector(0.0f, 0.0f, Up), ECC_Visibility, Params))
		{
			return false;
		}
	}
	// Not inside a trunk, a rock or the hill itself.
	return !GetWorld()->OverlapAnyTestByChannel(From, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.0f), Params);
}

FVector AForestIntroDirector::FindClearShot(const FVector& Target, float PreferredYaw, float Distance, float Height, float MinAboveGround) const
{
	auto SpotAt = [&](float Yaw, float Reach)
	{
		FVector Spot = Target + FRotator(0.0f, Yaw, 0.0f).Vector() * Reach + FVector(0.0f, 0.0f, Height);
		float Ground = Spot.Z - 10000.0f;
		if (IntroGroundBelow(GetWorld(), PlayerCharacter, Spot, Ground))
		{
			Spot.Z = FMath::Max(Spot.Z, Ground + MinAboveGround);
		}
		return Spot;
	};
	for (const float Reach : {Distance, Distance * 0.8f, Distance * 0.6f})
	{
		for (const float Turn : {0.0f, 25.0f, -25.0f, 50.0f, -50.0f, 80.0f, -80.0f, 115.0f, -115.0f, 150.0f, -150.0f, 180.0f})
		{
			const FVector Spot = SpotAt(PreferredYaw + Turn, Reach);
			if (SeesPlayer(Spot, Target))
			{
				return Spot;
			}
		}
	}
	return SpotAt(PreferredYaw, Distance * 0.6f);
}

// --- Title ------------------------------------------------------------------

void AForestIntroDirector::StartTitle(APlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
	PlayerCharacter = InPlayerController ? Cast<ACharacter>(InPlayerController->GetPawn()) : nullptr;
	if (!PlayerController)
	{
		return;
	}
	// The middle of the generated world, seen from above the treetops.
	TitleCenter = PlayerCharacter ? PlayerCharacter->GetActorLocation() : FVector::ZeroVector;
	if (const AForestGameManager* Manager = AForestGameManager::Get(this))
	{
		TitleCenter = Manager->GetActorLocation();
	}
	TitleGround = TitleCenter.Z;
	GroundAt(TitleCenter, TitleGround);
	TitleAngle = FMath::FRandRange(0.0f, 360.0f);

	// Golden hour: the sun is low and warm behind the hills.
	if (AForestSky* Sky = AForestSky::Get(this))
	{
		Sky->SetMoment(17.6f, TEXT("Clear"), false);
	}
	TitleCamera = SpawnCamera(TitleCenter, FRotator::ZeroRotator, 62.0f);
	TickTitle(0.0f);
	if (TitleCamera)
	{
		PlayerController->SetViewTargetWithBlend(TitleCamera, 0.0f);
	}
	Phase = EPhase::Title;
	SetActorTickEnabled(true);
}

void AForestIntroDirector::TickTitle(float DeltaSeconds)
{
	if (!TitleCamera)
	{
		return;
	}
	// A slow circle, a little lower and closer, then back: the forest breathes.
	TitleAngle += DeltaSeconds * 1.6f;
	const float Radians = FMath::DegreesToRadians(TitleAngle);
	const float Wave = FMath::Sin(Radians * 0.7f);
	const float Radius = 4300.0f + 500.0f * Wave;
	const FVector Location = FVector(TitleCenter.X, TitleCenter.Y, TitleGround)
		+ FVector(FMath::Cos(Radians) * Radius, FMath::Sin(Radians) * Radius, 1900.0f + 250.0f * Wave);
	const FVector LookAt(TitleCenter.X, TitleCenter.Y, TitleGround + 450.0f);
	TitleCamera->SetActorLocationAndRotation(Location, (LookAt - Location).Rotation());
}

// --- The creative pages ----------------------------------------------------

void AForestIntroDirector::ShowPortrait()
{
	if (!PlayerController || !PlayerCharacter || (Phase != EPhase::Title && Phase != EPhase::Portrait))
	{
		return;
	}
	if (!PortraitCamera)
	{
		// In the glade where they will wake up, turned away from the tree: open ground in front for the camera.
		PlacePlayerInForest();
		// A clear view of the whole body, then the player turns to face the camera.
		const FVector Body = PlayerCharacter->GetActorLocation();
		PortraitSpot = FindClearShot(Body, PlayerCharacter->GetActorRotation().Yaw + 180.0f, 600.0f, 15.0f, 60.0f);
		PlayerCharacter->SetActorRotation(FRotator(0.0f, (PortraitSpot - Body).Rotation().Yaw, 0.0f));
		PortraitCamera = SpawnCamera(PortraitSpot, FRotator::ZeroRotator, 40.0f);
	}
	PortraitTime = 0.0f;
	TickPortrait(0.0f);
	if (PortraitCamera && Phase != EPhase::Portrait)
	{
		PlayerController->SetViewTargetWithBlend(PortraitCamera, 1.4f, VTBlend_EaseInOut, 2.0f);
	}
	Phase = EPhase::Portrait;
}

void AForestIntroDirector::ShowForest()
{
	if (!PlayerController || Phase != EPhase::Portrait)
	{
		return;
	}
	Phase = EPhase::Title;
	if (TitleCamera)
	{
		PlayerController->SetViewTargetWithBlend(TitleCamera, 1.6f, VTBlend_EaseInOut, 2.0f);
	}
}

void AForestIntroDirector::TickPortrait(float DeltaSeconds)
{
	if (!PortraitCamera || !PlayerCharacter)
	{
		return;
	}
	// In front of the player, drifting gently from side to side (only while the view stays clear).
	PortraitTime += DeltaSeconds;
	const FVector Body = PlayerCharacter->GetActorLocation();
	const FVector Across = FVector::CrossProduct((Body - PortraitSpot).GetSafeNormal2D(), FVector::UpVector);
	FVector Location = PortraitSpot + Across * 70.0f * FMath::Sin(PortraitTime * 0.35f);
	if (!SeesPlayer(Location, Body))
	{
		Location = PortraitSpot;
	}
	FRotator Look = (Body - FVector(0.0f, 0.0f, 5.0f) - Location).Rotation();
	// The page is on the left: the character stands a little to the right.
	Look.Yaw -= 9.0f;
	PortraitCamera->SetActorLocationAndRotation(Location, Look);
}

// --- Falling asleep --------------------------------------------------------

void AForestIntroDirector::StartIntro(APlayerController* InPlayerController)
{
	if (!InPlayerController)
	{
		Destroy();
		return;
	}
	PlayerController = InPlayerController;
	PlayerCharacter = Cast<ACharacter>(InPlayerController->GetPawn());
	if (!PlayerCharacter)
	{
		Destroy();
		return;
	}

	PlayerController->bShowMouseCursor = false;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	PlayerCharacter->DisableInput(PlayerController);
	if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	IntroWidget = CreateWidget<UForestIntroWidget>(PlayerController, UForestIntroWidget::StaticClass());
	if (IntroWidget)
	{
		IntroWidget->SetEyes(1.0f);
		IntroWidget->AddToViewport(90);
	}
	if (!TitleCamera)
	{
		// Started without the title (from Python): begin straight in the dark.
		Nightfall();
		Clock = NightfallAt;
	}
	else
	{
		Clock = 0.0f;
	}
	Phase = EPhase::Asleep;
	SetActorTickEnabled(true);
}

void AForestIntroDirector::TickAsleep()
{
	if (IntroWidget)
	{
		IntroWidget->SetEyes(1.0f - Ease(Clock, EyesCloseStart, EyesCloseEnd));
		IntroWidget->SetBlur(7.0f * Ease(Clock, 1.2f, EyesCloseEnd));
	}
	if (IntroWidget && Clock >= 4.6f && Clock < NightfallAt)
	{
		IntroWidget->HideLine();
	}
}

void AForestIntroDirector::Nightfall()
{
	// In the dark, the night passes: the player lies down in a clearing and dawn comes.
	PlacePlayerInForest();
	SetMorningLight();
	if (USkeletalMeshComponent* Mesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr)
	{
		if (!bMeshSaved)
		{
			OriginalMeshLocation = Mesh->GetRelativeLocation();
			OriginalMeshRotation = Mesh->GetRelativeRotation();
			bMeshSaved = true;
		}
	}
	// On the back in the grass, breathing slowly (falls back to tilting the mesh).
	WakeUpAnim = UForestWakeUp::For(PlayerCharacter);
	if (WakeUpAnim.IsValid())
	{
		WakeUpAnim->LieAsleep();
		bAnimatedWake = WakeUpAnim->IsAsleep();
	}
	if (!bAnimatedWake)
	{
		SetMeshPose(1.0f, 0.0f);
	}

	// Lying on the back, looking up into the leaves and the morning sky.
	const FVector Head = PlayerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 25.0f);
	SkyYaw = PlayerCharacter->GetActorRotation().Yaw;
	SkyCamera = SpawnCamera(Head, FRotator(80.0f, SkyYaw, 0.0f), 75.0f);
	if (SkyCamera)
	{
		PlayerController->SetViewTargetWithBlend(SkyCamera, 0.0f);
	}
	if (TitleCamera)
	{
		TitleCamera->Destroy();
		TitleCamera = nullptr;
	}
	if (PortraitCamera)
	{
		PortraitCamera->Destroy();
		PortraitCamera = nullptr;
	}
	if (IntroWidget)
	{
		IntroWidget->ShowSkipHint(true);
	}
}

// --- Waking up ---------------------------------------------------------------

void AForestIntroDirector::LineAt(float Now, float DeltaSeconds, float At, const TCHAR* English, const TCHAR* French)
{
	if (IntroWidget && Now >= At && Now - DeltaSeconds < At)
	{
		IntroWidget->ShowLine(ForestLanguage::Pick(English, French));
	}
}

void AForestIntroDirector::TickWaking(float DeltaSeconds)
{
	const float W = Clock;

	// The eyes: a first flutter, half open, heavy again, then open for good.
	float Eyes = 0.0f;
	if (W >= FlutterStart && W < FlutterEnd)
	{
		Eyes = 0.12f * FMath::Sin(PI * (W - FlutterStart) / (FlutterEnd - FlutterStart));
	}
	else if (W >= HalfOpenStart)
	{
		Eyes = 0.42f * Ease(W, HalfOpenStart, HalfOpenEnd) - 0.14f * Ease(W, HalfOpenEnd, EyesOpenStart)
			+ 0.72f * Ease(W, EyesOpenStart, EyesOpenEnd);
	}
	if (IntroWidget)
	{
		IntroWidget->SetEyes(Eyes);
		IntroWidget->SetBlur(16.0f * (1.0f - Ease(W, HalfOpenStart, EyesOpenEnd + 0.8f)));
	}

	LineAt(W, DeltaSeconds, 0.9f, TEXT("The rain has stopped."), TEXT("La pluie s'est arrêtée."));
	LineAt(W, DeltaSeconds, 3.6f, TEXT("Somewhere above you, the leaves are still dripping..."), TEXT("Quelque part au-dessus de toi, les feuilles s'égouttent encore..."));
	LineAt(W, DeltaSeconds, 8.2f, TEXT("The forest breathes slowly. So do you."), TEXT("La forêt respire lentement. Toi aussi."));
	LineAt(W, DeltaSeconds, 11.8f, TEXT("Light slips between the branches..."), TEXT("La lumière se glisse entre les branches..."));
	LineAt(W, DeltaSeconds, 15.6f, TEXT("You open your eyes."), TEXT("Tu ouvres les yeux."));
	LineAt(W, DeltaSeconds, SitStart + 0.4f, TEXT("Slowly, you rise. The forest has been waiting for you."), TEXT("Lentement, tu te relèves. La forêt t'attendait."));
	if (IntroWidget && ((W >= 19.0f && W - DeltaSeconds < 19.0f) || (W >= 28.0f && W - DeltaSeconds < 28.0f)))
	{
		IntroWidget->HideLine();
	}

	// Above: the treetops turn slowly, as the eyes wander over the leaves.
	if (SkyCamera && !bLowShot)
	{
		SkyYaw += DeltaSeconds * 2.2f;
		SkyCamera->SetActorRotation(FRotator(80.0f - 8.0f * Ease(W, 0.0f, LowShotAt), SkyYaw, 0.0f));
	}

	// Down to the grass: a low shot beside the player, drifting closer.
	if (!bLowShot && W >= LowShotAt && PlayerCharacter)
	{
		bLowShot = true;
		// Beside the player, low in the grass, wherever no trunk or slope hides them.
		const FVector Body = PlayerCharacter->GetActorLocation();
		const FVector LookAt = Body - FVector(0.0f, 0.0f, 45.0f);
		const float SideYaw = PlayerCharacter->GetActorRotation().Yaw + 90.0f + 18.0f;
		const FVector Spot = FindClearShot(LookAt, SideYaw, 380.0f, 30.0f, 50.0f);
		LowCamera = SpawnCamera(Spot, (LookAt - Spot).Rotation(), 55.0f);
		if (LowCamera)
		{
			PlayerController->SetViewTargetWithBlend(LowCamera, 4.5f, VTBlend_EaseInOut, 2.0f);
		}
	}
	if (LowCamera && PlayerCharacter)
	{
		const FVector Body = PlayerCharacter->GetActorLocation() - FVector(0.0f, 0.0f, 45.0f - 60.0f * Ease(W, SitStart, StandEnd));
		// Drifts closer, but never into the ground, a trunk, or too close.
		const FVector Toward = (Body - LowCamera->GetActorLocation()).GetSafeNormal2D();
		FVector Next = LowCamera->GetActorLocation() + Toward * 12.0f * DeltaSeconds;
		float Ground = Next.Z - 10000.0f;
		if (IntroGroundBelow(GetWorld(), PlayerCharacter, Next, Ground))
		{
			Next.Z = FMath::Max(Next.Z, Ground + 45.0f);
		}
		if (FVector::Dist2D(Next, Body) > 230.0f && SeesPlayer(Next, Body))
		{
			LowCamera->SetActorLocation(Next);
		}
		LowCamera->SetActorRotation(FMath::RInterpTo(LowCamera->GetActorRotation(), (Body - LowCamera->GetActorLocation()).Rotation(), DeltaSeconds, 1.5f));
	}

	// The body: asleep and breathing, then a stir, half sitting, a pause, standing.
	if (bAnimatedWake)
	{
		if (W >= SitStart && W - DeltaSeconds < SitStart && WakeUpAnim.IsValid())
		{
			WakeUpAnim->WakeUp(StandEnd - SitStart + 2.0f);
		}
	}
	const float Lying = 1.0f - 0.55f * Ease(W, SitStart, SitEnd) - 0.45f * Ease(W, StandStart, StandEnd);
	const float Breath = (1.0f - Ease(W, SitStart, SitEnd)) * 1.6f * FMath::Sin(W * 1.25f);
	if (!bAnimatedWake)
	{
		SetMeshPose(Lying, Breath);
	}

	// Back behind the player, gently.
	if (!bBackToPlayer && W >= BackToPlayerAt)
	{
		bBackToPlayer = true;
		PlayerController->SetViewTargetWithBlend(PlayerCharacter, 4.5f, VTBlend_EaseInOut, 2.0f);
		if (IntroWidget)
		{
			IntroWidget->ShowSkipHint(false);
		}
	}
	if (W >= WakeEnd)
	{
		FinishIntro();
	}
}

void AForestIntroDirector::SetMeshPose(float Lying, float Breath)
{
	USkeletalMeshComponent* Mesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	if (!Mesh || !bMeshSaved)
	{
		return;
	}
	Mesh->SetRelativeLocation(OriginalMeshLocation + FVector(0.0f, 0.0f, LyingDrop * Lying + Breath));
	Mesh->SetRelativeRotation(OriginalMeshRotation + FRotator(0.0f, 0.0f, LyingRoll * Lying));
}

// --- Every frame -------------------------------------------------------------

void AForestIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PlayerController)
	{
		Destroy();
		return;
	}
	Clock += DeltaSeconds;

	switch (Phase)
	{
	case EPhase::Title:
		TickTitle(DeltaSeconds);
		return;

	case EPhase::Portrait:
		TickTitle(DeltaSeconds);
		TickPortrait(DeltaSeconds);
		return;

	case EPhase::Asleep:
		TickTitle(DeltaSeconds * 0.5f);
		LineAt(Clock, DeltaSeconds, 0.5f, TEXT("The sun goes down. You lie down in the grass..."), TEXT("Le soleil se couche. Tu t'allonges dans l'herbe..."));
		LineAt(Clock, DeltaSeconds, NightfallAt + 0.6f, TEXT("The night passes over the forest."), TEXT("La nuit passe sur la forêt."));
		TickAsleep();
		if (Clock >= NightfallAt && Clock - DeltaSeconds < NightfallAt)
		{
			Nightfall();
		}
		if (Clock >= WakeAt - 1.4f && Clock - DeltaSeconds < WakeAt - 1.4f && IntroWidget)
		{
			IntroWidget->HideLine();
		}
		if (Clock >= WakeAt)
		{
			Phase = EPhase::Waking;
			Clock = 0.0f;
		}
		break;

	case EPhase::Waking:
		TickWaking(DeltaSeconds);
		break;

	default:
		return;
	}

	// Space, Enter or the gamepad skips the whole thing.
	if ((Phase == EPhase::Asleep || Phase == EPhase::Waking) && PlayerController
		&& (PlayerController->WasInputKeyJustPressed(EKeys::SpaceBar) || PlayerController->WasInputKeyJustPressed(EKeys::Enter)
			|| PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom)))
	{
		Skip();
	}
}

void AForestIntroDirector::Skip()
{
	if (Phase == EPhase::Asleep && Clock < NightfallAt)
	{
		Nightfall();
	}
	if (PlayerCharacter)
	{
		PlayerController->SetViewTargetWithBlend(PlayerCharacter, 0.8f, VTBlend_EaseInOut, 2.0f);
		bBackToPlayer = true;
	}
	FinishIntro();
}

// --- Where and when to wake up ---------------------------------------------

void AForestIntroDirector::PlacePlayerInForest()
{
	UWorld* World = GetWorld();
	if (!World || !PlayerCharacter)
	{
		return;
	}

	TArray<FVector> TreePoints;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		It->GetComponents(MeshComponents);
		for (UStaticMeshComponent* Component : MeshComponents)
		{
			const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
			if (!Mesh)
			{
				continue;
			}
			const FString MeshName = Mesh->GetName();
			// World Builder trees carry the IWB_Trees tag (Studio trees have other mesh names).
			if (!It->ActorHasTag(TEXT("IWB_Trees"))
				&& !MeshName.Contains(TEXT("tree"), ESearchCase::IgnoreCase)
				&& !MeshName.Contains(TEXT("pine"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Component))
			{
				const int32 InstanceCount = Instanced->GetInstanceCount();
				const int32 Step = FMath::Max(1, InstanceCount / 36);
				for (int32 Index = 0; Index < InstanceCount && TreePoints.Num() < 240; Index += Step)
				{
					FTransform InstanceTransform;
					if (Instanced->GetInstanceTransform(Index, InstanceTransform, true))
					{
						TreePoints.Add(InstanceTransform.GetLocation());
					}
				}
			}
			else if (TreePoints.Num() < 240)
			{
				TreePoints.Add(Component->GetComponentLocation());
			}
		}
	}

	if (TreePoints.Num() < 3)
	{
		return;
	}

	FVector BestPoint = TreePoints[0];
	int32 BestNeighbours = -1;
	for (const FVector& Candidate : TreePoints)
	{
		int32 Neighbours = 0;
		for (const FVector& Other : TreePoints)
		{
			if (FVector::DistSquared2D(Candidate, Other) < FMath::Square(1850.0f))
			{
				++Neighbours;
			}
		}
		if (Neighbours > BestNeighbours)
		{
			BestNeighbours = Neighbours;
			BestPoint = Candidate;
		}
	}

	// BestPoint is a tree's own position: wake up a few metres beside it, on
	// open ground (the first spot around it whose ray reaches the terrain,
	// not a trunk, a rock or a canopy).  Close enough that its leaves are
	// above the sleeper's eyes.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestIntroGround), false, PlayerCharacter);
	const float CapsuleHalfHeight = PlayerCharacter->GetCapsuleComponent()
		? PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.0f;
	for (const float Distance : {300.0f, 450.0f, 650.0f, 850.0f})
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const float Angle = Step * PI / 4.0f;
			const FVector Spot = BestPoint + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
			FHitResult GroundHit;
			const FVector TraceStart(Spot.X, Spot.Y, Spot.Z + 16000.0f);
			const FVector TraceEnd(Spot.X, Spot.Y, Spot.Z - 16000.0f);
			if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, Params)
				&& GroundHit.GetActor() && GroundHit.GetActor()->ActorHasTag(TEXT("IWB_Terrain")))
			{
				PlayerCharacter->SetActorLocation(GroundHit.ImpactPoint + FVector(0.0f, 0.0f, CapsuleHalfHeight + 3.0f), false, nullptr, ETeleportType::TeleportPhysics);
				// Face the tree: the sky shot looks up into its leaves.
				FVector ToTree = BestPoint - GroundHit.ImpactPoint;
				PlayerCharacter->SetActorRotation(FRotator(0.0f, ToTree.Rotation().Yaw, 0.0f));
				PlayerController->SetControlRotation(FRotator(-10.0f, ToTree.Rotation().Yaw, 0.0f));
				return;
			}
		}
	}
	// No open ground found: keep the level's player start.
}

void AForestIntroDirector::SetMorningLight() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// With the living sky: dawn, everything still wet from the rain, in the weather chosen on the world page.
	if (AForestSky* Sky = AForestSky::Get(this))
	{
		Sky->SetMoment(6.4f, FForestSetup::Load().WeatherName(), true);
		return;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* Sun = *It;
		Sun->SetActorRotation(FRotator(-18.0f, 52.0f, 0.0f));
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetIntensity(4.0f);
			Light->SetLightColor(FLinearColor(1.0f, 0.76f, 0.52f));
		}
		break;
	}
}

void AForestIntroDirector::FinishIntro()
{
	Phase = EPhase::None;
	if (IntroWidget)
	{
		// Hide it first and remove it a moment later, once the renderer is done
		// with its blur (removing it mid-frame is a risk we do not need).
		IntroWidget->SetBlur(0.0f);
		IntroWidget->SetEyes(1.0f);
		IntroWidget->SetVisibility(ESlateVisibility::Collapsed);
		TWeakObjectPtr<UForestIntroWidget> Widget = IntroWidget;
		FTimerHandle Later;
		GetWorldTimerManager().SetTimer(Later, [Widget]()
		{
			if (Widget.IsValid())
			{
				Widget->RemoveFromParent();
			}
		}, 0.5f, false);
		IntroWidget = nullptr;
	}
	if (bAnimatedWake && WakeUpAnim.IsValid() && !WakeUpAnim->IsAwake())
	{
		WakeUpAnim->Finish();
	}
	else if (!bAnimatedWake)
	{
		SetMeshPose(0.0f, 0.0f);
	}
	if (PlayerCharacter)
	{
		if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		PlayerCharacter->EnableInput(PlayerController);
	}
	if (PlayerController)
	{
		// The title menu and the intro both blocked move/look input, and each
		// block is counted: clear them all at once, or the player can only jump.
		PlayerController->ResetIgnoreInputFlags();
		PlayerController->ClientSetCameraFade(false, FColor::Black, FVector2D::ZeroVector, 0.0f, false, false);
		if (PlayerCharacter && PlayerController->GetViewTarget() != PlayerCharacter && !bBackToPlayer)
		{
			PlayerController->SetViewTargetWithBlend(PlayerCharacter, 0.8f, VTBlend_EaseInOut, 2.0f);
		}
	}
	if (AForestGameManager* Manager = AForestGameManager::Get(this))
	{
		Manager->ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("Morning is here. Gather food and wood, win the animals' trust, build your camp (B)."), TEXT("Le matin est là. Ramasse de quoi manger et du bois, gagne la confiance des animaux, construis ton camp (B)."))), 6.0f);
	}
	// The cameras go after the blend back to the player has finished.
	for (ACameraActor* Camera : {TitleCamera.Get(), SkyCamera.Get(), LowCamera.Get()})
	{
		if (Camera)
		{
			Camera->SetLifeSpan(6.0f);
		}
	}
	SetActorTickEnabled(false);
	SetLifeSpan(6.5f);
}
