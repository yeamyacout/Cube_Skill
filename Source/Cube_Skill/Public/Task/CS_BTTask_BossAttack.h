// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CS_BTTask_BossAttack.generated.h"

class UAnimMontage;

struct FCS_BossAttackMemory
{
	float Elapsed = 0.f;
	float WaitDuration = 0.f;
};

UCLASS(Abstract)
class CUBE_SKILL_API UCS_BTTask_BossAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCS_BTTask_BossAttack();

	UPROPERTY(EditAnywhere, Category = "Boss")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float HoldTime = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float AttackLockDuration = 3.f;

protected:
	UPROPERTY(EditAnywhere, Category = "Boss|Blackboard")
	FBlackboardKeySelector AttackLockKey;

	UPROPERTY(EditAnywhere, Category = "Boss|Blackboard")
	FBlackboardKeySelector AttackReadyKey;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;

	void StopControlledMovement(UBehaviorTreeComponent& OwnerComp) const;
};
