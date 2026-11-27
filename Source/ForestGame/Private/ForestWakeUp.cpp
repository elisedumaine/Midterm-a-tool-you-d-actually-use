#include "ForestWakeUp.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

namespace
{
	const TCHAR* FallPath = TEXT("/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01.MM_Death_Back_01");

	/**
	 * The wake-up as moments: at this share of the wake-up (first), the body is
	 * this far into the fall (second; 1 = lying, 0 = standing).
	 */
	struct FWakeKey
	{
		float Alpha;
		float Fall;
	};
	const FWakeKey GWakeKeys[] = {
		{0.00f, 1.00f},		// asleep on its back
		{0.22f, 0.93f},		// a stir: it lifts a little, then settles
		{0.30f, 0.95f},
		{0.52f, 0.74f},		// the chest rises: half sitting
		{0.66f, 0.71f},		// a pause there, breathing in the morning
		{1.00f, 0.00f},		// all the way up
	};

	float Smooth(float Alpha)
	{
		return Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}
}

UForestWakeUp::UForestWakeUp()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UForestWakeUp* UForestWakeUp::For(ACharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}
	if (UForestWakeUp* Existing = Character->FindComponentByClass<UForestWakeUp>())
	{
		return Existing;
	}
	UForestWakeUp* WakeUp = NewObject<UForestWakeUp>(Character, TEXT("ForestWakeUp"));
	WakeUp->RegisterComponent();
	return WakeUp;
}

bool UForestWakeUp::Prepare()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	if (!Fall)
	{
		Fall = LoadObject<UAnimSequence>(nullptr, FallPath);
	}
	if (!Fall)
	{
		return false;
	}
	if (!bPrepared)
	{
		MeshLocation = Mesh->GetRelativeLocation();
		bPrepared = true;
	}
	// The fall drives the body directly, frame by frame (see SetPose).
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->SetAnimation(Fall);
	Mesh->Play(false);
	Mesh->SetPlayRate(0.0f);
	SetComponentTickEnabled(true);
	return true;
}

void UForestWakeUp::SetPose(float AnimTime)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr)
	{
		Mesh->SetPosition(FMath::Clamp(AnimTime, 0.0f, Fall->GetPlayLength()), false);
	}
}

void UForestWakeUp::FallAsleep(float Seconds)
{
	if (!Prepare())
	{
		return;
	}
	Phase = EPhase::FallingAsleep;
	Time = 0.0f;
	Duration = FMath::Max(0.1f, Seconds);
	SetPose(0.0f);
}

void UForestWakeUp::LieAsleep()
{
	if (!Prepare())
	{
		return;
	}
	Phase = EPhase::Asleep;
	SetPose(Fall->GetPlayLength());
}

void UForestWakeUp::WakeUp(float Seconds)
{
	if (Phase == EPhase::Awake && !Prepare())
	{
		return;
	}
	if (!Fall)
	{
		return;
	}
	Phase = EPhase::WakingUp;
	Time = 0.0f;
	Duration = FMath::Max(0.1f, Seconds);
}

float UForestWakeUp::GetWakeProgress() const
{
	switch (Phase)
	{
	case EPhase::Awake: return 1.0f;
	case EPhase::WakingUp: return FMath::Clamp(Time / Duration, 0.0f, 1.0f);
	default: return 0.0f;
	}
}

float UForestWakeUp::WakeCurve(float Alpha) const
{
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(GWakeKeys); ++Index)
	{
		const FWakeKey& From = GWakeKeys[Index - 1];
		const FWakeKey& To = GWakeKeys[Index];
		if (Alpha <= To.Alpha)
		{
			const float Local = (Alpha - From.Alpha) / FMath::Max(0.001f, To.Alpha - From.Alpha);
			return FMath::Lerp(From.Fall, To.Fall, Smooth(FMath::Clamp(Local, 0.0f, 1.0f)));
		}
	}
	return 0.0f;
}

void UForestWakeUp::Finish()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr)
	{
		if (bPrepared)
		{
			Mesh->SetRelativeLocation(MeshLocation);
		}
		Mesh->SetPlayRate(1.0f);
		// Back to the character's own animation blueprint (idle, walk, jump...).
		Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	}
	Phase = EPhase::Awake;
	bPrepared = false;
	SetComponentTickEnabled(false);
}

void UForestWakeUp::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (!Fall)
	{
		return;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	Time += DeltaSeconds;
	Breath += DeltaSeconds;
	const float Length = Fall->GetPlayLength();
	float Breathing = 0.0f;

	switch (Phase)
	{
	case EPhase::FallingAsleep:
	{
		// Lying down: slow at first, settling softly at the end.
		const float Alpha = FMath::Clamp(Time / Duration, 0.0f, 1.0f);
		SetPose(Length * Smooth(Alpha));
		if (Alpha >= 1.0f)
		{
			Phase = EPhase::Asleep;
		}
		break;
	}
	case EPhase::Asleep:
		SetPose(Length);
		Breathing = 1.0f;
		break;
	case EPhase::WakingUp:
	{
		const float Alpha = FMath::Clamp(Time / Duration, 0.0f, 1.0f);
		SetPose(Length * WakeCurve(Alpha));
		// Still breathing deeply while half asleep, less and less.
		Breathing = FMath::Clamp(1.0f - Alpha * 1.6f, 0.0f, 1.0f);
		if (Alpha >= 1.0f)
		{
			Finish();
			return;
		}
		break;
	}
	default:
		return;
	}
	// The slow rise and fall of a sleeper's chest, about one breath every five seconds.
	Mesh->SetRelativeLocation(MeshLocation + FVector(0.0f, 0.0f, 1.2f * Breathing * FMath::Sin(Breath * 1.25f)));
}
