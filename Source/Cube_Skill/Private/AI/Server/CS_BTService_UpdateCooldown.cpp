#include "AI/Server/CS_BTService_UpdateCooldown.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

namespace
{
	bool IsCooldownReady(const UAbilitySystemComponent* AbilitySystem, FGameplayTag CooldownTag)
	{
		// 没填标签时当作已经好了，树可以先跑；填了之后身上带这个标签就是还在冷却。
		if (!CooldownTag.IsValid())
		{
			return true;
		}
		if (!AbilitySystem)
		{
			return false;
		}
		return !AbilitySystem->HasMatchingGameplayTag(CooldownTag);
	}
}

UCS_BTService_UpdateCooldown::UCS_BTService_UpdateCooldown()
{
	NodeName = TEXT("更新冷却");
	Interval = 0.5f;
	RandomDeviation = 0.1f;
	bCallTickOnSearchStart = true;
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	LightReadyKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTService_UpdateCooldown, LightReadyKey));
	HeavyReadyKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UCS_BTService_UpdateCooldown, HeavyReadyKey));
	LightReadyKey.SelectedKeyName = TEXT("bLightReady");
	HeavyReadyKey.SelectedKeyName = TEXT("bHeavyReady");
}

void UCS_BTService_UpdateCooldown::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (const UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		LightReadyKey.ResolveSelectedKey(*BBAsset);
		HeavyReadyKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		LightReadyKey.InvalidateResolvedKey();
		HeavyReadyKey.InvalidateResolvedKey();
	}
}

void UCS_BTService_UpdateCooldown::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BlackboardComp)
	{
		return;
	}

	const UAbilitySystemComponent* AbilitySystem = Pawn->FindComponentByClass<UAbilitySystemComponent>();
	if (LightReadyKey.IsSet())
	{
		BlackboardComp->SetValueAsBool(LightReadyKey.SelectedKeyName, IsCooldownReady(AbilitySystem, LightCooldownTag));
	}
	if (HeavyReadyKey.IsSet())
	{
		BlackboardComp->SetValueAsBool(HeavyReadyKey.SelectedKeyName, IsCooldownReady(AbilitySystem, HeavyCooldownTag));
	}
}
