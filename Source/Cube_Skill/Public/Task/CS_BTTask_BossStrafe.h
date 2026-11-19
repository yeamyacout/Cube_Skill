// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CS_BTTask_BossStrafe.generated.h"

struct FCS_BossStrafeMemory
{
	float Elapsed = 0.f;
};

UCLASS(Blueprintable)
class CUBE_SKILL_API UCS_BTTask_BossStrafe : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UCS_BTTask_BossStrafe();

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float PreferredDistance = 450.f;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float TooFar = 550.f;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float TooClose = 350.f;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float StrafeStep = 280.f;

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float StepTime = 0.8f;

protected:
	UPROPERTY(EditAnywhere, Category = "Boss|Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Boss|Blackboard")
	FBlackboardKeySelector StrafeSignKey;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
};
