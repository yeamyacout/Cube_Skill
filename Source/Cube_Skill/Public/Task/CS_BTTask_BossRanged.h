// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Task/CS_BTTask_BossAttack.h"
#include "CS_BTTask_BossRanged.generated.h"

UCLASS(Blueprintable)
class CUBE_SKILL_API UCS_BTTask_BossRanged : public UCS_BTTask_BossAttack
{
	GENERATED_BODY()

public:
	UCS_BTTask_BossRanged();
};
