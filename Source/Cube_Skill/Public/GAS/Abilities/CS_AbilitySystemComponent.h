// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CS_AbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CUBE_SKILL_API UCS_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	//服务端调用，给玩家赋予能力时触发 调用GiveAbility时触发
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
	//客户端调用，收到技能复制时触发，尝试激活技能
	virtual void OnRep_ActivateAbilities() override;
	
	
	UFUNCTION(BlueprintCallable,Category = "CS|Abilities")
	void SetAbilityLevel(TSubclassOf<UGameplayAbility> AbilityClass , int32 Level);
	
private:
	//判断并执行自动激活能力的逻辑，检查能力的标签是否包含"ActivateOnGiven"标签，如果包含则尝试激活该能力
	void HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec);
};
