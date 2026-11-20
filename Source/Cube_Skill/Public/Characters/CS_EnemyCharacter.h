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

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 开始"被击飞"状态
	 *
	 * 效果：
	 *   1. 立刻停止 AI 移动与当前寻路
	 *   2. 关闭移动到移动逻辑（让角色完全受物理/外力支配）
	 *   3. 由 Tick 监测落地，落地后自动恢复
	 *
	 * @param MaxKnockDuration  最长锁定时间（秒），防止卡在地下等异常情况永久锁定
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Combat")
	void StartKnockback(float MaxKnockDuration = 3.f);

	/** 结束击飞状态，恢复 AI 与移动控制 */
	UFUNCTION(BlueprintCallable, Category = "CS|Combat")
	void EndKnockback();

	/** 当前是否处于被击飞（不可移动）状态 */
	UFUNCTION(BlueprintPure, Category = "CS|Combat")
	bool IsBeingKnockedBack() const { return bIsKnockedBack; }

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
	
	
	void GiveStartupAbilities();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCS_AbilitySystemComponent> CS_AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly,Category = "CS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// ---------- 击飞状态 ----------

	/** 是否处于被击飞状态 */
	bool bIsKnockedBack = false;

	/** 击飞开始的时间戳（秒），用于超时保护 */
	float KnockbackStartTime = 0.f;

	/** 最长锁定时间（秒） */
	float KnockbackMaxDuration = 3.f;

	/** 被击飞前 AiController 是否处于暂停状态，用于恢复原状 */
	bool bWasAIPausedBeforeKnockback = false;
	
};
