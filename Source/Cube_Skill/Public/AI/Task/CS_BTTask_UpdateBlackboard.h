#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CS_BTTask_UpdateBlackboard.generated.h"

/** 轻击计数和换位结束都是瞬间改键，合成一个任务，用枚举选择这次改什么。 */
UENUM(BlueprintType)
enum class ECS_EliteBlackboardOp : uint8
{
	CountLight UMETA(DisplayName = "记一次轻击"),
	FinishRelocate UMETA(DisplayName = "换位结束"),
};

UCLASS()
class CUBE_SKILL_API UCS_BTTask_UpdateBlackboard : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCS_BTTask_UpdateBlackboard();

	UPROPERTY(EditAnywhere, Category = "CS|Blackboard")
	ECS_EliteBlackboardOp Operation = ECS_EliteBlackboardOp::CountLight;

	UPROPERTY(EditAnywhere, Category = "CS|Blackboard")
	FBlackboardKeySelector LightBurstCountKey;

	UPROPERTY(EditAnywhere, Category = "CS|Blackboard")
	FBlackboardKeySelector NextLightIsThrowKey;

	UPROPERTY(EditAnywhere, Category = "CS|Blackboard")
	FBlackboardKeySelector MustRelocateKey;

	/** 轻击累计到这个次数就要求先换位。 */
	UPROPERTY(EditAnywhere, Category = "CS|Blackboard", meta = (ClampMin = "1"))
	int32 BurstLimit = 3;

	/** 编辑器里只保存了键名。运行时 IsSet 看的是编号，原生节点要自己解析。 */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
