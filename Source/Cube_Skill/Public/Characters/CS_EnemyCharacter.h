// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CS_BaseCharacter.h"
#include "CS_EnemyCharacter.generated.h"

UCLASS()
class CUBE_SKILL_API ACS_EnemyCharacter : public ACS_BaseCharacter
{
	GENERATED_BODY()

public:
	ACS_EnemyCharacter();

protected:
	virtual void BeginPlay() override;
	
};
