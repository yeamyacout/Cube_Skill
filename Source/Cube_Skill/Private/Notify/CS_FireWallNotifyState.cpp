#include "Notify/CS_FireWallNotifyState.h"

#include "Actor/CS_FireWallActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UCS_FireWallNotifyState::UCS_FireWallNotifyState()
{
}

void UCS_FireWallNotifyState::DestroyWalls(FCS_FireWallRuntime& State) const
{
	if (State.LeftWall.IsValid())
	{
		State.LeftWall->Destroy();
	}
	if (State.RightWall.IsValid())
	{
		State.RightWall->Destroy();
	}
	State.LeftWall.Reset();
	State.RightWall.Reset();
}

void UCS_FireWallNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!World || (!World->IsGameWorld() && !World->IsPreviewWorld()))
	{
		return;
	}

	const AActor* Anchor = nullptr;
	if (World->IsGameWorld())
	{
		Anchor = UGameplayStatics::GetPlayerPawn(World, 0);
	}
	else
	{
		Anchor = MeshComp->GetOwner();
	}
	if (!Anchor)
	{
		return;
	}

	FVector Right = Anchor->GetActorRightVector();
	Right.Z = 0.f;
	if (!Right.Normalize())
	{
		return;
	}

	FVector Forward = Anchor->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	if (FCS_FireWallRuntime* Existing = ActiveWalls.Find(MeshComp))
	{
		DestroyWalls(*Existing);
	}

	UClass* WallClass = FireWallActorClass
		? FireWallActorClass.Get()
		: ACS_FireWallActor::StaticClass();
	const ACS_FireWallActor* WallDefaults = WallClass
		? WallClass->GetDefaultObject<ACS_FireWallActor>()
		: nullptr;
	if (!WallDefaults)
	{
		return;
	}

	const FVector BodyCenter = Anchor->GetActorLocation();
	float HalfHeight = 0.f;
	if (const ACharacter* AnchorCharacter = Cast<ACharacter>(Anchor))
	{
		if (const UCapsuleComponent* Capsule = AnchorCharacter->GetCapsuleComponent())
		{
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	FCS_FireWallRuntime& State = ActiveWalls.FindOrAdd(MeshComp);
	State.GroundCenter = BodyCenter - FVector(0.f, 0.f, HalfHeight);
	// 间距/前移/长度都从火墙类默认值读，改蓝图即可，不用改通知。
	State.SideOffset = Right * WallDefaults->GetSideDistance();
	State.ForwardOffset = Forward * WallDefaults->GetForwardDistance();
	State.MergeLocation = BodyCenter;
	State.Duration = FMath::Max(TotalDuration, KINDA_SMALL_NUMBER);
	State.Elapsed = 0.f;

	const FRotator WallRotation(0.f, Anchor->GetActorRotation().Yaw, 0.f);
	AActor* DamageCauser = MeshComp->GetOwner();
	APawn* InstigatorPawn = Cast<APawn>(DamageCauser);

	auto SpawnWall = [&](const FVector& Location) -> ACS_FireWallActor*
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = DamageCauser;
		Params.Instigator = InstigatorPawn;
		return World->SpawnActor<ACS_FireWallActor>(WallClass, Location, WallRotation, Params);
	};

	State.LeftWall = SpawnWall(State.GroundCenter + State.ForwardOffset - State.SideOffset);
	State.RightWall = SpawnWall(State.GroundCenter + State.ForwardOffset + State.SideOffset);
}

void UCS_FireWallNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FCS_FireWallRuntime* State = ActiveWalls.Find(MeshComp);
	if (!State)
	{
		return;
	}

	State->Elapsed += FrameDeltaTime;
	const float Alpha = FMath::Clamp(State->Elapsed / State->Duration, 0.f, 1.f);
	const FVector Offset = FMath::Lerp(State->SideOffset, FVector::ZeroVector, Alpha);

	if (State->LeftWall.IsValid())
	{
		State->LeftWall->SetActorLocation(State->GroundCenter + State->ForwardOffset - Offset);
	}
	if (State->RightWall.IsValid())
	{
		State->RightWall->SetActorLocation(State->GroundCenter + State->ForwardOffset + Offset);
	}
}

void UCS_FireWallNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (FCS_FireWallRuntime* State = ActiveWalls.Find(MeshComp))
	{
		const bool bWallsMerged = State->Elapsed >= State->Duration * 0.98f;
		UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
		AActor* DamageCauser = MeshComp ? MeshComp->GetOwner() : nullptr;

		if (bWallsMerged && World && World->IsGameWorld())
		{
			ACS_FireWallActor* SourceWall = State->LeftWall.Get();
			if (!SourceWall)
			{
				SourceWall = State->RightWall.Get();
			}
			if (SourceWall)
			{
				SourceWall->PlayMergeExplosion(State->MergeLocation, DamageCauser);
			}
		}

		DestroyWalls(*State);
		ActiveWalls.Remove(MeshComp);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UCS_FireWallNotifyState::GetNotifyName_Implementation() const
{
	return TEXT("火墙合拢");
}
