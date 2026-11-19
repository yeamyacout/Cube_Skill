// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CS_BaseCharacter.h"
#include "CS_EnemyCharacter.generated.h"

class UCS_AbilitySystemComponent;
class UAbilitySystemComponent;
class UGameplayAbility;

UCLASS()
class CUBE_SKILL_API ACS_EnemyCharacter : public ACS_BaseCharacter
{
	GENERATED_BODY()

public:
	ACS_EnemyCharacter();
	
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

protected:
	virtual void BeginPlay() override;
	
	
	void GiveStartupAbilities();
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCS_AbilitySystemComponent> CS_AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly,Category = "CS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
	

};
