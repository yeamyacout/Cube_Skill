#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CS_BTTask_PlayMontage.generated.h"

class UAnimMontage;

/** 每个敌人各自的播放进度。任务节点是共用的，不能把已播时间写在节点成员上。 */
struct FCS_PlayMontageMemory
{
	float ElapsedTime = 0.f;
};

UCLASS()
class CUBE_SKILL_API UCS_BTTask_PlayMontage : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCS_BTTask_PlayMontage();

	UPROPERTY(EditAnywhere, Category = "CS|Montage")
	TObjectPtr<UAnimMontage> Montage;

	/** 0 表示播完整段。大于 0 时播到这个秒数就停，持续施法用这个截断。 */
	UPROPERTY(EditAnywhere, Category = "CS|Montage", meta = (ClampMin = "0.0"))
	float PlayDuration = 0.f;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FCS_PlayMontageMemory); }
};
