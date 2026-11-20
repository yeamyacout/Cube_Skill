// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CS_BaseCharacter.h"
#include "CS_PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCS_HealthComponent;

UCLASS()
class CUBE_SKILL_API ACS_PlayerCharacter : public ACS_BaseCharacter
{
	GENERATED_BODY()

public:
	ACS_PlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Health")
	TObjectPtr<UCS_HealthComponent> HealthComponent;
	
private:
	UPROPERTY(VisibleAnywhere , Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	
};
