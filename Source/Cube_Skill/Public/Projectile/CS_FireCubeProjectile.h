// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile/CS_CubeProjectile.h"
#include "CS_FireCubeProjectile.generated.h"

class ACS_EnemyCharacter;

/**
 * 火焰方块
 *
 * 特点：
 *   · 命中敌人后劈下「灼烧」持续伤害（DoT）
 *   · 用于测试火焰属性效果
 */
UCLASS()
class CUBE_SKILL_API ACS_FireCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_FireCubeProjectile();

	/** 命中敌人时施加灼烧 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

protected:
	/** 灼烧总伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube", meta = (ClampMin = "0.0"))
	float BurnTotalDamage = 30.f;

	/** 灼烧持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube", meta = (ClampMin = "0.1"))
	float BurnDuration = 3.f;

	/** 灼烧跳伤间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube", meta = (ClampMin = "0.1"))
	float BurnTickInterval = 0.5f;
};
