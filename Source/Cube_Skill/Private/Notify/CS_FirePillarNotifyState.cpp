#include "Notify/CS_FirePillarNotifyState.h"

#include "Actor/CS_FirePillarActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

UCS_FirePillarNotifyState::UCS_FirePillarNotifyState()
{
}

void UCS_FirePillarNotifyState::DestroyPillar(FCS_FirePillarRuntime& State) const
{
	if (State.Pillar.IsValid())
	{
		State.Pillar->Destroy();
	}
	State.Pillar.Reset();
}

void UCS_FirePillarNotifyState::UpdatePillarTransform(USkeletalMeshComponent* MeshComp, ACS_FirePillarActor* Pillar, float DeltaSeconds) const
{
	if (!MeshComp || !Pillar)
	{
		return;
	}

	const AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector SocketLocation = SocketName.IsNone()
		? MeshComp->GetComponentLocation()
		: MeshComp->GetSocketLocation(SocketName);

	// 朝向用敌人水平前方，不跟手骨扭动，保证柱子往前喷。
	FVector Forward = Owner->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	Pillar->UpdatePlacement(SocketLocation, Forward, DeltaSeconds);
}

void UCS_FirePillarNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!World || (!World->IsGameWorld() && !World->IsPreviewWorld()))
	{
		return;
	}

	if (FCS_FirePillarRuntime* Existing = ActivePillars.Find(MeshComp))
	{
		DestroyPillar(*Existing);
	}

	UClass* PillarClass = FirePillarActorClass
		? FirePillarActorClass.Get()
		: ACS_FirePillarActor::StaticClass();
	if (!PillarClass)
	{
		return;
	}

	AActor* DamageCauser = MeshComp->GetOwner();
	APawn* InstigatorPawn = Cast<APawn>(DamageCauser);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = DamageCauser;
	Params.Instigator = InstigatorPawn;

	ACS_FirePillarActor* Pillar = World->SpawnActor<ACS_FirePillarActor>(
		PillarClass,
		MeshComp->GetComponentLocation(),
		FRotator::ZeroRotator,
		Params);
	if (!Pillar)
	{
		return;
	}

	UpdatePillarTransform(MeshComp, Pillar, 0.f);

	FCS_FirePillarRuntime& State = ActivePillars.FindOrAdd(MeshComp);
	State.Pillar = Pillar;
}

void UCS_FirePillarNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	FCS_FirePillarRuntime* State = ActivePillars.Find(MeshComp);
	if (!State || !State->Pillar.IsValid())
	{
		return;
	}

	UpdatePillarTransform(MeshComp, State->Pillar.Get(), FrameDeltaTime);
}

void UCS_FirePillarNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (FCS_FirePillarRuntime* State = ActivePillars.Find(MeshComp))
	{
		DestroyPillar(*State);
		ActivePillars.Remove(MeshComp);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UCS_FirePillarNotifyState::GetNotifyName_Implementation() const
{
	return TEXT("火柱喷射");
}
