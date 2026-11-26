#include "AI/Server/CS_BTService_UpdateTarget.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Kismet/GameplayStatics.h"

UCS_BTService_UpdateTarget::UCS_BTService_UpdateTarget()
{
	NodeName = TEXT("更新目标");
	Interval = 0.5f;
	RandomDeviation = 0.1f;
	// 搜索一开始就写距离，避免第一拍距离还是 0，贴脸分支被误触发。
	bCallTickOnSearchStart = true;
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTService_UpdateTarget, TargetKey), AActor::StaticClass());
	DistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTService_UpdateTarget, DistanceKey));
	HasLineOfSightKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTService_UpdateTarget, HasLineOfSightKey));
	TargetKey.SelectedKeyName = TEXT("Target");
	DistanceKey.SelectedKeyName = TEXT("Distance");
	HasLineOfSightKey.SelectedKeyName = TEXT("bHasLOS");
}

void UCS_BTService_UpdateTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
		DistanceKey.ResolveSelectedKey(*BBAsset);
		HasLineOfSightKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		TargetKey.InvalidateResolvedKey();
		DistanceKey.InvalidateResolvedKey();
		HasLineOfSightKey.InvalidateResolvedKey();
	}
}

void UCS_BTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!Controller || !Pawn || !BlackboardComp)
	{
		return;
	}

	// 关卡只有一个玩家，直接取玩家，不依赖还没接上的知觉组件。
	APawn* Player = UGameplayStatics::GetPlayerPawn(Controller, 0);
	if (TargetKey.IsSet())
	{
		BlackboardComp->SetValueAsObject(TargetKey.SelectedKeyName, Player);
	}

	if (!IsValid(Player))
	{
		if (DistanceKey.IsSet())
		{
			BlackboardComp->SetValueAsFloat(DistanceKey.SelectedKeyName, 0.f);
		}
		if (HasLineOfSightKey.IsSet())
		{
			BlackboardComp->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, false);
		}
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	if (DistanceKey.IsSet())
	{
		BlackboardComp->SetValueAsFloat(DistanceKey.SelectedKeyName, FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation()));
	}
	if (HasLineOfSightKey.IsSet())
	{
		BlackboardComp->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, Controller->LineOfSightTo(Player));
	}

	Controller->SetFocus(Player, EAIFocusPriority::Gameplay);
}
