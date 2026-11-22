// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile/CS_CubeProjectile.h"
#include "CS_NatureCubeProjectile.generated.h"

class ACS_EnemyCharacter;

/**
 * 自然方块
 *
 * 特点：
 *   · 飞行路径留下减速效果（神秘气体）
 *   · 命中敌人后施加「中毒」持续伤害 + 减速
 */
UCLASS()
class CUBE_SKILL_API ACS_NatureCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_NatureCubeProjectile();

protected:
	virtual void BeginPlay() override;

	/** 命中敌人时施毒 + 减速 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

	/** 中毒总伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube", meta = (ClampMin = "0.0"))
	float PoisonTotalDamage = 25.f;

	/** 中毒持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube", meta = (ClampMin = "0.1"))
	float PoisonDuration = 4.f;

	/** 中毒跳伤间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube", meta = (ClampMin = "0.1"))
	float PoisonTickInterval = 1.f;

	/** 减速比例（0~1，0.5 = 速度减半） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlowRatio = 0.5f;

	/** 减速持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|NatureCube", meta = (ClampMin = "0.1"))
	float SlowDuration = 3.f;
};
