// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile/CS_CubeProjectile.h"
#include "NiagaraSystem.h"
#include "CS_LightningCubeProjectile.generated.h"

class ACS_EnemyCharacter;
class UNiagaraComponent;

/**
 * 雷电方块
 *
 * 特点：
 *   · 无碰撞：飞行途中穿过一切物体
 *   · 球形范围伤害：范围内所有敌人受伤
 *   · 特殊技能（F 键）：玩家瞬移到方块位置，路径留下闪电并伤害沿途敌人
 *
 * 特效：
 *   · NS_Absorption_Yellow —— 发射后以方块为中心，回收时销毁
 *   · NS_Lightning_01b    —— 瞬移路径闪电，长度等于路径长度
 *   · NS_Lightning_01a    —— 命中范围内的敌人时，对该敌人生成
 */
UCLASS()
class CUBE_SKILL_API ACS_LightningCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_LightningCubeProjectile();

	/**
	 * 执行瞬移：把玩家移动到方块位置，路径生成闪电并伤害沿途敌人
	 *
	 * 由 PlayerController 在按下 F 键时调用（仅当方块是雷电方块）。
	 * @return 是否成功执行
	 */
	virtual bool ExecuteTeleport() override;

	/** 是否是雷电方块（供 PlayerController 判断 F 键是否生效） */
	virtual bool SupportsTeleport() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 命中敌人时造成球形范围伤害 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

	/** 范围内敌人受到伤害，并生成命中特效 */
	void DamageEnemiesInRadius(const FVector& Center);

	/** 在指定位置生成一次性特效 */
	void SpawnBurstEffect(UNiagaraSystem* System, const FVector& Location) const;

	/** 在敌人身上生成命中雷电特效（身体中心 + 头顶） */
	void SpawnEnemyHitEffects(ACS_EnemyCharacter* Enemy);

	// ---------- 伤害参数 ----------

	/** 雷电伤害半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "1.0"))
	float LightningRadius = 300.f;

	/** 范围伤害量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "0.0"))
	float LightningDamage = 20.f;

	/** 瞬移路径伤害量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "0.0"))
	float TeleportPathDamage = 40.f;

	/** 路径伤害的检测半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "1.0"))
	float TeleportPathRadius = 120.f;

	// ---------- 冲刺参数 ----------

	/**
	 * 冲刺的碰撞检测半径（cm）
	 *
	 * 用胶囊沿冲刺方向做扫掠，遇到墙就把目标点截断在墙前，
	 * 保证不会冲到墙里或冲出地图。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Dash", meta = (ClampMin = "10.0"))
	float DashProbeRadius = 40.f;

	// ---------- 命中特效（在蓝图里调） ----------

	/**
	 * 命中特效的雷电特效
	 *
	 * 默认 NS_EnemyHit_Lightning（从 NS_Lightning_01a 复制并调整过：
	 * SpawnRate 1→50、粒子寿命 1~1.5s→0.2~0.5s，所以能立即出现）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	TObjectPtr<UNiagaraSystem> EnemyHitFX;

	/** 命中特效大小 X 轴（1.0 = 原始大小） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleX = 1.5f;

	/** 命中特效大小 Y 轴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleY = 1.5f;

	/** 命中特效大小 Z 轴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleZ = 1.5f;

	/** 三个轴使用同一个缩放值（勾选后忽略 XYZ 分轴设置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	bool bEnemyHitFXUniformScale = true;

	/** 统一缩放值（仅在 bEnemyHitFXUniformScale 为 true 时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScale = 1.5f;

	/** 命中特效持续时间（秒），到时强制销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.05", Units = "s"))
	float EnemyHitFXDuration = 0.5f;

	/** 命中特效 Pitch（俯仰角，绕 Y 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXPitch = 0.f;

	/** 命中特效 Yaw（水平偏航角，绕 Z 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXYaw = 90.f;

	/** 命中特效 Roll（翻滚角，绕 X 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXRoll = 0.f;

	/** 命中特效相对敌人胶囊的位置偏移（Z 为向上），cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	FVector EnemyHitFXOffset = FVector(0.f, 0.f, 0.f);

	/** 命中特效是否自动根据胶囊高度抬高位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	bool bEnemyHitFXAutoHeight = true;

	/**
	 * 命中特效的粒子生成速率
	 *
	 * 原特效 NS_Lightning_01a 默认每秒只生成 1 个粒子，
	 * 看起来像没出现。调大这个值（如 20）能明显增强可见度。
	 * 设为 0 则不改动原值。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.0"))
	float EnemyHitFXSpawnRate = 0.f;

	/** 命中特效的粒子尺寸（会覆盖原特效的 Mesh Scale） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	bool bOverrideEnemyHitFXScale3D = false;

	/** 覆盖用的 Mesh Scale 值（仅在 bOverrideEnemyHitFXScale3D 为 true 时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	FVector EnemyHitFXMeshScale = FVector(3.f, 3.f, 2.f);

	// ---------- 飞行/路径特效 ----------

	/** 飞行期间跟随方块的光环特效（默认 NS_Absorption_Yellow） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight")
	TObjectPtr<UNiagaraSystem> FlightAuraFX;

	/** 瞬移路径闪电特效（默认 NS_Lightning_01b） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight")
	TObjectPtr<UNiagaraSystem> TeleportPathFX;

	/** 路径闪电特效持续时间（秒），到时销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight", meta = (ClampMin = "0.05", Units = "s"))
	float TeleportPathFXDuration = 2.f;

	/** 飞行期间的特效组件实例 */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> FlightAuraComponent;

	/** 是否已经释放过范围雷电（避免反复触发） */
	bool bLightningDischarged = false;

	/** 是否已经瞬移过（每次发射只能瞬移一次） */
	bool bTeleportUsed = false;
};
