#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "GameplayTagContainer.h"
#include "CS_BTService_UpdateCooldown.generated.h"

UCLASS()
class CUBE_SKILL_API UCS_BTService_UpdateCooldown : public UBTService
{
	GENERATED_BODY()

public:
	UCS_BTService_UpdateCooldown();

	UPROPERTY(EditAnywhere, Category = "CS|Cooldown")
	FBlackboardKeySelector LightReadyKey;

	UPROPERTY(EditAnywhere, Category = "CS|Cooldown")
	FBlackboardKeySelector HeavyReadyKey;

	/** 身上有这个标签表示轻击还在冷却。留空则视为已经好了。 */
	UPROPERTY(EditAnywhere, Category = "CS|Cooldown")
	FGameplayTag LightCooldownTag;

	/** 身上有这个标签表示重击还在冷却。留空则视为已经好了。 */
	UPROPERTY(EditAnywhere, Category = "CS|Cooldown")
	FGameplayTag HeavyCooldownTag;

protected:
	/** 编辑器里只保存了键名。运行时 IsSet 看的是编号，原生节点要自己解析。 */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
