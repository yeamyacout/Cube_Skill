// Fill out your copyright notice in the Description page of Project Settings.

#include "Task/CS_BTTask_BossDash.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

UCS_BTTask_BossDash::UCS_BTTask_BossDash()
{
	NodeName = TEXT("Boss Dash");

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_BossDash, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");
}

void UCS_BTTask_BossDash::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type UCS_BTTask_BossDash::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
		{
			if (AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)))
			{
				Controller->MoveToActor(Target, DashAcceptRadius, false, true, true, nullptr, true);
			}
		}
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
