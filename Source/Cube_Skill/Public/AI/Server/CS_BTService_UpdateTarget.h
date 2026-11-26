#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "CS_BTService_UpdateTarget.generated.h"

UCLASS()
class CUBE_SKILL_API UCS_BTService_UpdateTarget : public UBTService
{
	GENERATED_BODY()

public:
	UCS_BTService_UpdateTarget();

	UPROPERTY(EditAnywhere, Category = "CS|Target")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "CS|Target")
	FBlackboardKeySelector DistanceKey;

	UPROPERTY(EditAnywhere, Category = "CS|Target")
	FBlackboardKeySelector HasLineOfSightKey;

protected:
	/** 编辑器里只保存了键名。运行时 IsSet 看的是编号，原生节点要自己解析。 */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
