// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CS_BaseCharacter.h"
#include "CS_PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class CUBE_SKILL_API ACS_PlayerCharacter : public ACS_BaseCharacter
{
	GENERATED_BODY()

public:
	ACS_PlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:

private:
	UPROPERTY(VisibleAnywhere , Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
