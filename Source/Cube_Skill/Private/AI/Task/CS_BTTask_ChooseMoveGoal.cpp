#include "AI/Task/CS_BTTask_ChooseMoveGoal.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "NavigationSystem.h"

namespace
{
	bool ProjectToNavMesh(const APawn* Pawn, FVector& InOutPoint)
	{
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
		if (!NavSys)
		{
			return false;
		}

		FNavLocation Projected;
		const FVector Extent(200.f, 200.f, 300.f);
		if (!NavSys->ProjectPointToNavigation(InOutPoint, Projected, Extent, &Pawn->GetNavAgentPropertiesRef()))
		{
			return false;
		}

		InOutPoint = Projected.Location;
		return true;
	}
}

UCS_BTTask_ChooseMoveGoal::UCS_BTTask_ChooseMoveGoal()
{
	NodeName = TEXT("选择移动目标");
	INIT_TASK_NODE_NOTIFY_FLAGS();

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_ChooseMoveGoal, TargetKey), AActor::StaticClass());
	MoveGoalKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_ChooseMoveGoal, MoveGoalKey));
	TargetKey.SelectedKeyName = TEXT("Target");
	MoveGoalKey.SelectedKeyName = TEXT("MoveGoal");
}

void UCS_BTTask_ChooseMoveGoal::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
		MoveGoalKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		TargetKey.InvalidateResolvedKey();
		MoveGoalKey.InvalidateResolvedKey();
	}
}

EBTNodeResult::Type UCS_BTTask_ChooseMoveGoal::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BlackboardComp || !MoveGoalKey.IsSet())
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = TargetKey.IsSet() ? Cast<AActor>(BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName)) : nullptr;
	if (!IsValid(Target))
	{
		return EBTNodeResult::Failed;
	}

	const FVector SelfLocation = Pawn->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector Away = (SelfLocation - TargetLocation).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -Pawn->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector Destination = SelfLocation;

	switch (Goal)
	{
	case ECS_EliteMoveGoal::Retreat:
		Destination = SelfLocation + Away * RetreatDistance;
		if (!ProjectToNavMesh(Pawn, Destination))
		{
			return EBTNodeResult::Failed;
		}
		break;

	case ECS_EliteMoveGoal::Relocate:
	{
		const FVector Side = FVector::CrossProduct(FVector::UpVector, -Away) * (FMath::RandBool() ? 1.f : -1.f);
		Destination = TargetLocation + Side.GetSafeNormal2D() * RelocateDistance;
		if (!ProjectToNavMesh(Pawn, Destination))
		{
			return EBTNodeResult::Failed;
		}
		break;
	}

	case ECS_EliteMoveGoal::Approach:
		Destination = TargetLocation + Away * ApproachDistance;
		if (!ProjectToNavMesh(Pawn, Destination))
		{
			return EBTNodeResult::Failed;
		}
		break;

	case ECS_EliteMoveGoal::Peek:
	{
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
		if (!NavSys)
		{
			return EBTNodeResult::Failed;
		}

		bool bFound = false;
		for (int32 Attempt = 0; Attempt < PeekAttempts; ++Attempt)
		{
			FNavLocation Candidate;
			if (!NavSys->GetRandomReachablePointInRadius(SelfLocation, PeekRadius, Candidate))
			{
				continue;
			}

			const float Dist2D = FVector::Dist2D(Candidate.Location, TargetLocation);
			if (Dist2D < ComfortMin || Dist2D > ComfortMax)
			{
				continue;
			}

			// 视线要从候选站位点发出，不能用当前所在位置，否则选出来的点可能其实被墙挡住。
			const FVector ViewPoint = Candidate.Location + FVector(0.f, 0.f, Pawn->BaseEyeHeight);
			if (!Controller->LineOfSightTo(Target, ViewPoint))
			{
				continue;
			}

			Destination = Candidate.Location;
			bFound = true;
			break;
		}

		if (!bFound)
		{
			return EBTNodeResult::Failed;
		}
		break;
	}

	default:
		return EBTNodeResult::Failed;
	}

	BlackboardComp->SetValueAsVector(MoveGoalKey.SelectedKeyName, Destination);
	return EBTNodeResult::Succeeded;
}
