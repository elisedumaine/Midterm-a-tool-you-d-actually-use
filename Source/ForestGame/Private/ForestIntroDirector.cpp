#include "ForestIntroDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/CapsuleComponent.h"
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
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AForestIntroDirector::AForestIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AForestIntroDirector::StartIntro(APlayerController* InPlayerController)
{
	if (bStarted || !InPlayerController)
	{
		Destroy();
		return;
	}

	bStarted = true;
	PlayerController = InPlayerController;
	PlayerCharacter = Cast<ACharacter>(InPlayerController->GetPawn());
	if (!PlayerCharacter)
	{
		Destroy();
		return;
	}

	PlacePlayerInForest();
	SetMorningLight();

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

	if (USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh())
	{
		OriginalMeshLocation = Mesh->GetRelativeLocation();
		OriginalMeshRotation = Mesh->GetRelativeRotation();
		Mesh->SetRelativeLocation(OriginalMeshLocation + FVector(0.0f, 0.0f, -38.0f));
		Mesh->SetRelativeRotation(OriginalMeshRotation + FRotator(0.0f, 0.0f, 88.0f));
	}

	IntroWidget = CreateWidget<UForestIntroWidget>(PlayerController, UForestIntroWidget::StaticClass());
	if (IntroWidget)
	{
		IntroWidget->AddToViewport(90);
		IntroWidget->SetNarrative(FText::FromString(ForestLanguage::Pick(TEXT("The rain has stopped. Morning rises between the pines..."), TEXT("La pluie s'est arrêtée. Le matin se lève entre les pins..."))));
	}

	PlayerController->ClientSetCameraFade(true, FColor::Black, FVector2D(1.0f, 1.0f), 0.0f, false, true);
	SetActorTickEnabled(true);
}

void AForestIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStarted || !PlayerController || !PlayerCharacter)
	{
		Destroy();
		return;
	}

	IntroTime += DeltaSeconds;
	if (IntroTime >= 0.45f && IntroTime - DeltaSeconds < 0.45f)
	{
		PlayerController->ClientSetCameraFade(true, FColor::Black, FVector2D(1.0f, 0.18f), 0.85f, false, true);
	}
	if (IntroTime >= 1.55f && IntroTime - DeltaSeconds < 1.55f && IntroWidget)
	{
		IntroWidget->SetNarrative(FText::FromString(ForestLanguage::Pick(TEXT("You open your eyes, lying in the heart of the forest."), TEXT("Tu ouvres les yeux, allongée au cœur de la forêt."))));
	}
	if (IntroTime >= 2.55f)
	{
		bStanding = true;
	}
	if (bStanding)
	{
		if (USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh())
		{
			Mesh->SetRelativeLocation(FMath::VInterpTo(Mesh->GetRelativeLocation(), OriginalMeshLocation, DeltaSeconds, 4.0f));
			Mesh->SetRelativeRotation(FMath::RInterpTo(Mesh->GetRelativeRotation(), OriginalMeshRotation, DeltaSeconds, 4.8f));
		}
	}
	if (IntroTime >= 3.15f && IntroTime - DeltaSeconds < 3.15f && IntroWidget)
	{
		IntroWidget->SetNarrative(FText::FromString(ForestLanguage::Pick(TEXT("You get up. The animals of the forest need you."), TEXT("Tu te relèves. Les animaux de la forêt ont besoin de toi."))));
	}
	if (IntroTime >= 4.1f && !bMorningFadeStarted)
	{
		bMorningFadeStarted = true;
		PlayerController->ClientSetCameraFade(true, FColor::Black, FVector2D(0.18f, 0.0f), 0.55f, false, false);
	}
	if (IntroTime >= 5.1f)
	{
		FinishIntro();
	}
}

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
	// not a trunk, a rock or a canopy).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestIntroGround), false, PlayerCharacter);
	const float CapsuleHalfHeight = PlayerCharacter->GetCapsuleComponent()
		? PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 90.0f;
	for (const float Distance : {350.0f, 550.0f, 800.0f})
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

	// With the living sky: dawn, the rain has just stopped, mist in the valleys.
	if (AForestSky* Sky = AForestSky::Get(this))
	{
		Sky->SetMoment(6.2f, TEXT("Mist"), true);
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
	if (IntroWidget)
	{
		IntroWidget->RemoveFromParent();
	}
	if (PlayerCharacter)
	{
		if (USkeletalMeshComponent* Mesh = PlayerCharacter->GetMesh())
		{
			Mesh->SetRelativeLocation(OriginalMeshLocation);
			Mesh->SetRelativeRotation(OriginalMeshRotation);
		}
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
	}
	if (AForestGameManager* Manager = AForestGameManager::Get(this))
	{
		Manager->ShowMessage(FText::FromString(ForestLanguage::Pick(TEXT("Morning is here. Find mushrooms to help the animals."), TEXT("Le matin est là. Trouve des champignons pour aider les animaux."))), 6.0f);
	}
	Destroy();
}
