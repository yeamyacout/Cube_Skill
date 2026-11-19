// Fill out your copyright notice in the Description page of Project Settings.

#include "Task/CS_BTTask_BossAttack.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"

UCS_BTTask_BossAttack::UCS_BTTask_BossAttack()
{
	NodeName = TEXT("Boss Attack");
	INIT_TASK_NODE_NOTIFY_FLAGS();

	AttackLockKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_BossAttack, AttackLockKey));
	AttackLockKey.SelectedKeyName = TEXT("AttackLock");

	AttackReadyKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_BossAttack, AttackReadyKey));
	AttackReadyKey.SelectedKeyName = TEXT("bAttackReady");
}

void UCS_BTTask_BossAttack::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		AttackLockKey.ResolveSelectedKey(*BlackboardAsset);
		AttackReadyKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type UCS_BTTask_BossAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Blackboard || !Pawn)
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->SetValueAsFloat(AttackLockKey.SelectedKeyName, AttackLockDuration);
	Blackboard->SetValueAsBool(AttackReadyKey.SelectedKeyName, false);

	FCS_BossAttackMemory* Memory = CastInstanceNodeMemory<FCS_BossAttackMemory>(NodeMemory);
	Memory->Elapsed = 0.f;
	Memory->WaitDuration = HoldTime;

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (AttackMontage)
		{
			const float PlayedLength = Character->PlayAnimMontage(AttackMontage);
			if (PlayedLength > 0.f)
			{
				Memory->WaitDuration = PlayedLength;
			}
		}
	}

	return EBTNodeResult::InProgress;
}

void UCS_BTTask_BossAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FCS_BossAttackMemory* Memory = CastInstanceNodeMemory<FCS_BossAttackMemory>(NodeMemory);
	Memory->Elapsed += DeltaSeconds;
	if (Memory->Elapsed >= Memory->WaitDuration)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UCS_BTTask_BossAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	StopControlledMovement(OwnerComp);
	return EBTNodeResult::Aborted;
}

uint16 UCS_BTTask_BossAttack::GetInstanceMemorySize() const
{
	return sizeof(FCS_BossAttackMemory);
}

void UCS_BTTask_BossAttack::StopControlledMovement(UBehaviorTreeComponent& OwnerComp) const
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
	}
}
