#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CS_BTTask_ChooseMoveGoal.generated.h"

/** 同一种选点任务用枚举区分，后退、侧移、接近、探头不用各写一个类。 */
UENUM(BlueprintType)
enum class ECS_EliteMoveGoal : uint8
{
	Retreat UMETA(DisplayName = "后退"),
	Relocate UMETA(DisplayName = "侧移"),
	Approach UMETA(DisplayName = "接近"),
	Peek UMETA(DisplayName = "探头"),
};

UCLASS()
class CUBE_SKILL_API UCS_BTTask_ChooseMoveGoal : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCS_BTTask_ChooseMoveGoal();

	UPROPERTY(EditAnywhere, Category = "CS|Goal")
	ECS_EliteMoveGoal Goal = ECS_EliteMoveGoal::Retreat;

	UPROPERTY(EditAnywhere, Category = "CS|Goal")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "CS|Goal")
	FBlackboardKeySelector MoveGoalKey;

	/** 从自己位置沿远离玩家的方向走出这么远。 */
	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float RetreatDistance = 450.f;

	/** 侧移点离玩家的水平距离，落在舒适输出圈里。 */
	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float RelocateDistance = 600.f;

	/** 接近时停在玩家周围这个距离，避免走到贴脸。 */
	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float ApproachDistance = 700.f;

	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float PeekRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "1"))
	int32 PeekAttempts = 8;

	/** 探头点必须落在这个距离带里，和输出距离一致，避免贴脸或站得太远。 */
	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float ComfortMin = 350.f;

	UPROPERTY(EditAnywhere, Category = "CS|Goal", meta = (ClampMin = "0.0"))
	float ComfortMax = 900.f;

	/** 编辑器里只保存了键名。运行时 IsSet 看的是编号，原生节点要自己解析。 */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
