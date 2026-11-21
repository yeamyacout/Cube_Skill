// Fill out your copyright notice in the Description page of Project Settings.


#include "Notify/CS_AttackNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Player/CS_PlayerController.h"


void UCS_AttackNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// 找到播放动画的角色，再找到它的 PlayerController
	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	APawn* OwningPawn = Cast<APawn>(OwnerActor);
	if (!OwningPawn)
	{
		return;
	}

	ACS_PlayerController* PC = Cast<ACS_PlayerController>(OwningPawn->GetController());
	if (!PC)
	{
		return;
	}

	// 通知控制器：动画已经播到投掷时机，可以生成方块了
	PC->OnAttackThrowNotify();
}


FString UCS_AttackNotify::GetNotifyName_Implementation() const
{
	// 在编辑器的时间轴上显示为这个名称，方便识别
	return TEXT("投掷方块");
}
