// Fill out your copyright notice in the Description page of Project Settings.


#include "Notify/CS_MeleeNotify.h"

#include "Characters/CS_PlayerCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UCS_MeleeNotify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
                                 const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	
	if (!IsValid(MeshComp)) return;

	if (!MeshComp->GetOwner()) return;

	TArray<FHitResult> HitResults;
	
	HitResults = PerformSphereTrace(MeshComp);

	SendEventToActor(HitResults , MeshComp);
}

TArray<FHitResult> UCS_MeleeNotify::PerformSphereTrace(USkeletalMeshComponent* MeshCom)
{
	TArray<FHitResult> HitResults;

	
	FTransform SocketTransform = MeshCom->GetSocketTransform(SocketName);
	FVector Start = SocketTransform.GetLocation();
	FVector ForWard =UKismetMathLibrary::GetForwardVector(SocketTransform.GetRotation().Rotator());
	FVector End = Start - ForWard * SocketExtensionOffset;

	if(bDebug)
	{
		UKismetSystemLibrary::SphereTraceMulti(MeshCom,Start,End ,SphereTraceRadius , ETraceTypeQuery::TraceTypeQuery1,false ,TArray<AActor*>{MeshCom->GetOwner()},EDrawDebugTrace::ForDuration,HitResults,true, FLinearColor::Red,FLinearColor::Green,1.f);

	}
	else
	{
		UKismetSystemLibrary::SphereTraceMulti(MeshCom,Start,End ,SphereTraceRadius , ETraceTypeQuery::TraceTypeQuery1,false ,TArray<AActor*>{MeshCom->GetOwner()},EDrawDebugTrace::None,HitResults,true);
	}
	
	return HitResults;
}

void UCS_MeleeNotify::SendEventToActor(TArray<FHitResult> HitResults, USkeletalMeshComponent* MeshCom)
{
	for (const FHitResult HitResult : HitResults)
	{
		ACS_PlayerCharacter* PlayerCharacter = Cast<ACS_PlayerCharacter>(HitResult.GetActor());
		if (!IsValid(PlayerCharacter)) continue;
		//应用伤害
		UE_LOG(LogTemp,Warning,TEXT("Attack Player Character"));
	}
}
