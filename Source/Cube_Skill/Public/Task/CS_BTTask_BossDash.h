// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Task/CS_BTTask_BossAttack.h"
#include "CS_BTTask_BossDash.generated.h"

UCLASS(Blueprintable)
class CUBE_SKILL_API UCS_BTTask_BossDash : public UCS_BTTask_BossAttack
{
	GENERATED_BODY()

public:
	UCS_BTTask_BossDash();

	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float DashAcceptRadius = 160.f;

protected:
	UPROPERTY(EditAnywhere, Category = "Boss|Blackboard")
	FBlackboardKeySelector TargetActorKey;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
