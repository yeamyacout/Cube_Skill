// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile/CS_CubeProjectile.h"
#include "CS_LightningCubeProjectile.generated.h"

class ACS_EnemyCharacter;

/**
 * 雷电方块
 *
 * 特点：
 *   · 无碰撞：飞行途中穿过一切物体
 *   · 球形范围伤害：范围内所有敌人受伤
 *   · 特殊技能：瞬移到方块位置，路径留下闪电造成伤害
 */
UCLASS()
class CUBE_SKILL_API ACS_LightningCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_LightningCubeProjectile();

protected:
	virtual void BeginPlay() override;

	/** 命中敌人时造成球形范围伤害 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

	/** 范围内敌人受到伤害 */
	void DamageEnemiesInRadius(const FVector& Center);

	/** 雷电伤害半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube", meta = (ClampMin = "1.0"))
	float LightningRadius = 300.f;

	/** 雷电伤害量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube", meta = (ClampMin = "0.0"))
	float LightningDamage = 20.f;

	/** 是否已经释放过雷电（避免每次检测都重复伤害） */
	bool bLightningDischarged = false;
};
