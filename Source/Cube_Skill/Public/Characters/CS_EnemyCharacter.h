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
	void StartKnockback(float MaxKnockDuration = 3.f, bool bRequireLandingToEnd = true);

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

	/**
	 * 是否需要"落地"才解除锁定
	 *
	 * ============ 为什么需要这个开关 ============
	 * 原来的逻辑是：Tick 里检测到 IsMovingOnGround() 就解除锁定。
	 *
	 * 问题：方块把敌人**水平**推走时，敌人全程贴在地面上，
	 * IsMovingOnGround() 一直为 true，
	 * 于是 Tick 第一帧就调用了 EndKnockback()，
	 * AI 立刻恢复寻路把速度拉回去 —— 表现为"推不动"。
	 *
	 * 所以水平推的场景要传 false：只靠超时（KnockbackMaxDuration）解除，
	 * 让敌人安安静静被推完。
	 */
	bool bRequireLandingToEndKnockback = true;

	/** 击飞开始的时间戳（秒），用于超时保护 */
	float KnockbackStartTime = 0.f;

	/** 最长锁定时间（秒） */
	float KnockbackMaxDuration = 3.f;

	/** 被击飞前 AiController 是否处于暂停状态，用于恢复原状 */
	bool bWasAIPausedBeforeKnockback = false;

	/** 击飞前的重力缩放，用于恢复 */
	float SavedGravityScale = 1.f;

	/**
	 * 击飞前是否启用"控制器期望朝向"旋转
	 *
	 * 击飞期间会临时关闭，避免敌人一边被推一边转身，
	 * 转身时身体旋转会把推它的方块挤开。
	 */
	bool SavedbUseControllerDesiredRotation = false;

	/** 击飞前是否启用"朝移动方向旋转" */
	bool SavedbOrientRotationToMovement = true;

	// ---------- 推动期间临时改写、结束后要还原的移动参数 ----------

	/** 原始 MaxWalkSpeed（推动时会被临时抬高） */
	float SavedMaxWalkSpeed = 600.f;

	/** 原始地面摩擦（推动时清零） */
	float SavedGroundFriction = 8.f;

	/** 原始刹车减速度（推动时清零） */
	float SavedBrakingDecelerationWalking = 2048.f;

	/**
	 * 击飞期间的速度覆盖（实际是"每帧位移速度"）
	 *
	 * 由方块在推动时设置。敌人 Tick 里会每帧执行：
	 *     SetActorLocation(位置 + 本值 × DeltaTime, bSweep=true)
	 *
	 * 即**直接移动位置**，不经过 CharacterMovement 的速度系统。
	 * 这样 AI 控制器、行为树、MaxWalkSpeed 钳制都完全无法干扰。
	 */
	FVector KnockbackVelocityOverride = FVector::ZeroVector;

	/** 是否启用击飞速度覆盖 */
	bool bUseKnockbackVelocityOverride = false;

public:
	/**
	 * 设置/清除击飞期间的速度覆盖
	 * @param NewVelocity  要强制施加的速度；传 zero 并设 bEnable=false 可清除
	 * @param bEnable      是否启用
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Combat")
	void SetKnockbackVelocityOverride(const FVector& NewVelocity, bool bEnable);

	/** 是否正在被外力推动 */
	UFUNCTION(BlueprintPure, Category = "CS|Combat")
	bool HasKnockbackVelocityOverride() const { return bUseKnockbackVelocityOverride; }

private:

	/** 击飞起点的世界 Z 高度，用于计算抬升量 */
	float KnockbackStartHeight = 0.f;

	/**
	 * 击飞允许的最大抬升高度（cm）
	 *
	 * 需求：允许向上击飞，但高度不超过 2 米。
	 * 击飞期间每帧检查相对起点的高度，超过这个值就清零向上速度，
	 * 角色靠重力自然回落。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Combat|Knockback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float MaxKnockbackHeight = 200.f;

	/**
	 * 落地检测射线的长度（cm）
	 * 击飞期间每帧从胶囊体底部向下打一条短射线，
	 * 若脚下有地面且垂直速度不是向上，就认为已落地并解除锁定。
	 * 这样比只依赖 Landed 回调更可靠。
	 */
	float GroundCheckDistance = 20.f;
	
};
