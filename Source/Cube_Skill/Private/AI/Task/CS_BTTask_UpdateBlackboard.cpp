#include "AI/Task/CS_BTTask_UpdateBlackboard.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

UCS_BTTask_UpdateBlackboard::UCS_BTTask_UpdateBlackboard()
{
	NodeName = TEXT("改黑板");
	INIT_TASK_NODE_NOTIFY_FLAGS();

	LightBurstCountKey.AddIntFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_UpdateBlackboard, LightBurstCountKey));
	NextLightIsThrowKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_UpdateBlackboard, NextLightIsThrowKey));
	MustRelocateKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTTask_UpdateBlackboard, MustRelocateKey));
	LightBurstCountKey.SelectedKeyName = TEXT("LightBurstCount");
	NextLightIsThrowKey.SelectedKeyName = TEXT("bNextLightIsThrow");
	MustRelocateKey.SelectedKeyName = TEXT("bMustRelocate");
}

void UCS_BTTask_UpdateBlackboard::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		LightBurstCountKey.ResolveSelectedKey(*BBAsset);
		NextLightIsThrowKey.ResolveSelectedKey(*BBAsset);
		MustRelocateKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		LightBurstCountKey.InvalidateResolvedKey();
		NextLightIsThrowKey.InvalidateResolvedKey();
		MustRelocateKey.InvalidateResolvedKey();
	}
}

EBTNodeResult::Type UCS_BTTask_UpdateBlackboard::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp)
	{
		return EBTNodeResult::Failed;
	}

	switch (Operation)
	{
	case ECS_EliteBlackboardOp::CountLight:
	{
		if (!LightBurstCountKey.IsSet() || !NextLightIsThrowKey.IsSet() || !MustRelocateKey.IsSet())
		{
			return EBTNodeResult::Failed;
		}

		const int32 BurstCount = BlackboardComp->GetValueAsInt(LightBurstCountKey.SelectedKeyName) + 1;
		BlackboardComp->SetValueAsInt(LightBurstCountKey.SelectedKeyName, BurstCount);
		BlackboardComp->SetValueAsBool(NextLightIsThrowKey.SelectedKeyName, !BlackboardComp->GetValueAsBool(NextLightIsThrowKey.SelectedKeyName));
		if (BurstCount >= BurstLimit)
		{
			BlackboardComp->SetValueAsBool(MustRelocateKey.SelectedKeyName, true);
		}
		break;
	}

	case ECS_EliteBlackboardOp::FinishRelocate:
		if (!LightBurstCountKey.IsSet() || !MustRelocateKey.IsSet())
		{
			return EBTNodeResult::Failed;
		}

		BlackboardComp->SetValueAsBool(MustRelocateKey.SelectedKeyName, false);
		BlackboardComp->SetValueAsInt(LightBurstCountKey.SelectedKeyName, 0);
		break;

	default:
		return EBTNodeResult::Failed;
	}

	return EBTNodeResult::Succeeded;
}
