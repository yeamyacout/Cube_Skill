// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Projectile/CS_CubeProjectile.h"
#include "CS_NatureCubeProjectile.generated.h"

class ACS_EnemyCharacter;
class UNiagaraComponent;

/**
 * 自然方块
 *
 * 三套效果（对应设计文档的三个分支）：
 *   1. 发射神秘气液体 —— 方块飞行时沿途留下轨迹标记，轨迹存活一段时间后消失；
 *      敌人走进轨迹范围会被减速
 *   2. 命中敌人留下自然标记 —— 敌人身上保留自然属性，持续一段时间
 *   3. 命中敌人触发的负面状态 —— 中毒（持续伤害）+ 减速
 *
 * 所有时长、半径、倍率都暴露在蓝图，可直接调整。
 */
UCLASS()
class CUBE_SKILL_API ACS_NatureCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_NatureCubeProjectile();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 命中敌人时施加中毒、减速、自然标记 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

	// ========== 1. 飞行轨迹 ==========

	/** 在方块当前位置留下一个轨迹标记 */
	void SpawnTrailMark();

	/** 让范围内的敌人减速（轨迹残留效果） */
	void ApplyTrailSlow();

	// ========== 2. 敌人在身上的标记 ==========

	/** 对敌人施加中毒（持续伤害） */
	void ApplyPoison(ACS_EnemyCharacter* Enemy);

	/** 对敌人施加减速，到时自动恢复原速 */
	void ApplySlow(ACS_EnemyCharacter* Enemy, float Duration);

	/** 在敌人身上挂一个限时标记特效 */
	void AttachMarkEffect(ACS_EnemyCharacter* Enemy, UNiagaraSystem* Effect, float Duration);

	// ========== 轨迹参数 ==========

	/** 轨迹标记使用的特效（发射神秘气液体） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail")
	TObjectPtr<UNiagaraSystem> TrailFX;

	/** 轨迹标记存活时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail", meta = (ClampMin = "0.1", Units = "s"))
	float TrailLifetime = 5.f;

	/** 相邻两个轨迹标记的最小间距（cm），越小轨迹越连贯 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail", meta = (ClampMin = "1.0", Units = "cm"))
	float TrailSpacing = 80.f;

	/** 轨迹标记的缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail", meta = (ClampMin = "0.01"))
	float TrailFXScale = 1.f;

	/** 轨迹减速的生效半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail", meta = (ClampMin = "1.0", Units = "cm"))
	float TrailSlowRadius = 150.f;

	/** 轨迹减速的检测间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Trail", meta = (ClampMin = "0.05", Units = "s"))
	float TrailSlowCheckInterval = 0.5f;

	// ========== 负面状态参数 ==========

	/** 中毒总伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Poison", meta = (ClampMin = "0.0"))
	float PoisonTotalDamage = 25.f;

	/** 中毒持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Poison", meta = (ClampMin = "0.1", Units = "s"))
	float PoisonDuration = 3.f;

	/** 中毒跳伤间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Poison", meta = (ClampMin = "0.05", Units = "s"))
	float PoisonTickInterval = 0.5f;

	/** 中毒时在敌人身上挂的特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Poison")
	TObjectPtr<UNiagaraSystem> PoisonMarkFX;

	/** 减速比例（0~1，0.5 = 速度减半） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Slow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlowRatio = 0.5f;

	/** 命中后的减速持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Slow", meta = (ClampMin = "0.1", Units = "s"))
	float SlowDuration = 3.f;

	/** 减速时在敌人身上挂的特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|Slow")
	TObjectPtr<UNiagaraSystem> SlowMarkFX;

	// ========== 自然标记参数 ==========

	/** 命中后在敌人身上保留自然属性的时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|NatureMark", meta = (ClampMin = "0.1", Units = "s"))
	float NatureMarkDuration = 10.f;

	/** 自然标记使用的特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|NatureMark")
	TObjectPtr<UNiagaraSystem> NatureMarkFX;

	/** 敌人身上三种标记特效的统一缩放倍数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube|NatureMark", meta = (ClampMin = "0.01"))
	float MarkFXScale = 1.f;

private:
	/** 上次生成轨迹标记的位置，用于控制间距 */
	FVector LastTrailLocation = FVector::ZeroVector;

	/** 是否已经生成过第一个轨迹标记 */
	bool bTrailStarted = false;

	/** 轨迹减速检测的累计时间 */
	float TrailSlowElapsed = 0.f;

	/** 方块是否仍在飞行（回收后不再生成轨迹） */
	bool bTrailActive = true;
};
