// Fill out your copyright notice in the Description page of Project Settings.

#include "Task/CS_BTTask_BossStrafe.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

namespace
{
	constexpr float StrafeLocationAcceptance = 40.f;
}

UCS_BTTask_BossStrafe::UCS_BTTask_BossStrafe()
{
	NodeName = TEXT("Boss Strafe");
	INIT_TASK_NODE_NOTIFY_FLAGS();

	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_BossStrafe, TargetActorKey), AActor::StaticClass());
	TargetActorKey.SelectedKeyName = TEXT("TargetActor");

	StrafeSignKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_BossStrafe, StrafeSignKey));
	StrafeSignKey.SelectedKeyName = TEXT("StrafeSign");
}

void UCS_BTTask_BossStrafe::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BlackboardAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BlackboardAsset);
		StrafeSignKey.ResolveSelectedKey(*BlackboardAsset);
	}
}

EBTNodeResult::Type UCS_BTTask_BossStrafe::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	AActor* Target = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (!Controller || !Blackboard || !Pawn || !Target)
	{
		return EBTNodeResult::Failed;
	}

	FCS_BossStrafeMemory* Memory = CastInstanceNodeMemory<FCS_BossStrafeMemory>(NodeMemory);
	Memory->Elapsed = 0.f;

	const float Distance = Pawn->GetDistanceTo(Target);
	if (Distance > TooFar)
	{
		Controller->MoveToActor(Target, PreferredDistance, false, true, true, nullptr, true);
	}
	else if (Distance < TooClose)
	{
		const FVector BossLocation = Pawn->GetActorLocation();
		const FVector Away = (BossLocation - Target->GetActorLocation()).GetSafeNormal();
		const FVector Destination = BossLocation + Away * (PreferredDistance - Distance);
		Controller->MoveToLocation(Destination, StrafeLocationAcceptance, true, true, true, true, nullptr, true);
	}
	else
	{
		const float Sign = Blackboard->GetValueAsFloat(StrafeSignKey.SelectedKeyName);
		const float Step = Sign < 0.f ? -StrafeStep : StrafeStep;
		const FVector Destination = Pawn->GetActorLocation() + Pawn->GetActorRightVector() * Step;
		Controller->MoveToLocation(Destination, StrafeLocationAcceptance, true, true, true, true, nullptr, true);
		Blackboard->SetValueAsFloat(StrafeSignKey.SelectedKeyName, Sign < 0.f ? 1.f : -1.f);
	}

	return EBTNodeResult::InProgress;
}

void UCS_BTTask_BossStrafe::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FCS_BossStrafeMemory* Memory = CastInstanceNodeMemory<FCS_BossStrafeMemory>(NodeMemory);
	Memory->Elapsed += DeltaSeconds;
	if (Memory->Elapsed >= StepTime)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UCS_BTTask_BossStrafe::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* Controller = OwnerComp.GetAIOwner())
	{
		Controller->StopMovement();
	}
	return EBTNodeResult::Aborted;
}

uint16 UCS_BTTask_BossStrafe::GetInstanceMemorySize() const
{
	return sizeof(FCS_BossStrafeMemory);
}
