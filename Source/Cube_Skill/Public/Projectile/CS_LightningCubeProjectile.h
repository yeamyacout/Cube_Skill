// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Projectile/CS_CubeProjectile.h"
#include "CS_LightningCubeProjectile.generated.h"

class ACS_EnemyCharacter;
class UNiagaraComponent;

/**
 * 雷电方块
 *
 * 战斗机制：
 *   · 无碰撞，飞行途中穿过一切物体
 *   · 每 0.5 秒扫描一次周围敌人，范围内的每个敌人各受一次伤害并在其身上生成雷电特效
 *   · 回收途中继续保持扫描，方块会"电着回来"
 *   · 按 F 冲刺：锁定方块位置，玩家瞬移过去，路径留下闪电并伤害沿途敌人
 *
 * 三套特效（均可在蓝图替换）：
 *   · NS_Absorption_Yellow  飞行光环，挂在方块上随其移动
 *   · NS_Lightning_01b      冲刺路径闪电，长度 = 实际冲刺距离
 *   · NS_EnemyHit_Lightning 命中敌人时的特效，挂在敌人身上跟随
 */
UCLASS()
class CUBE_SKILL_API ACS_LightningCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_LightningCubeProjectile();

	/**
	 * 执行冲刺：玩家瞬移到方块位置，路径生成闪电并伤害沿途敌人
	 *
	 * 由 PlayerController 在按下 F 键时调用。
	 * 目标点会先做胶囊扫掠，撞墙则截断在墙前，防止穿墙或飞出地图。
	 * @return 是否成功执行（每次发射只能成功一次）
	 */
	virtual bool ExecuteTeleport() override;

	/** 标记为可冲刺方块，供 PlayerController 判断 F 键是否生效 */
	virtual bool SupportsTeleport() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 对单个敌人施加伤害并生成命中特效
	 *
	 * 基类 CheckNearbyEnemies 已按 EnemyDetectRadius 完成范围扫描，
	 * 每扫到一个敌人就会调用一次本函数，因此这里无需再做范围检测。
	 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) override;

	/** 把命中特效挂到敌人身上，并按配置设置大小、朝向、持续时间 */
	void SpawnEnemyHitEffects(ACS_EnemyCharacter* Enemy);

	// ---------- 伤害 ----------

	/**
	 * 单次判定的伤害量
	 *
	 * 每 EnemyDetectInterval 秒对 EnemyDetectRadius 内的每个敌人触发一次。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "0.0"))
	float LightningDamage = 20.f;

	/** 冲刺路径上敌人的伤害量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "0.0"))
	float TeleportPathDamage = 40.f;

	/** 冲刺路径的伤害判定半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Damage", meta = (ClampMin = "1.0"))
	float TeleportPathRadius = 120.f;

	/**
	 * 冲刺扫掠使用的碰撞半径（cm）
	 *
	 * 仅在无法取得玩家胶囊组件时作为兜底值。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|Dash", meta = (ClampMin = "10.0"))
	float DashProbeRadius = 40.f;

	// ---------- 命中特效 ----------

	/**
	 * 命中敌人时使用的特效
	 *
	 * 默认 NS_EnemyHit_Lightning（由 NS_Lightning_01a 复制并调过参数：
	 * 粒子生成速率 1→50、粒子寿命 1~1.5s→0.2~0.5s，因此挂上即出粒子）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	TObjectPtr<UNiagaraSystem> EnemyHitFX;

	/** 命中特效是否三轴等比缩放（关闭后使用下面的 XYZ 分轴值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	bool bEnemyHitFXUniformScale = true;

	/** 命中特效等比缩放值（1.0 = 原始大小） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScale = 1.5f;

	/** 命中特效 X 轴缩放（仅在关闭等比缩放时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleX = 1.5f;

	/** 命中特效 Y 轴缩放（仅在关闭等比缩放时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleY = 1.5f;

	/** 命中特效 Z 轴缩放（仅在关闭等比缩放时生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyHitFXScaleZ = 1.5f;

	/** 命中特效持续时长（秒），到时自动销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (ClampMin = "0.05", Units = "s"))
	float EnemyHitFXDuration = 0.5f;

	/** 命中特效俯仰角（绕 Y 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXPitch = 0.f;

	/** 命中特效偏航角（绕 Z 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXYaw = 90.f;

	/** 命中特效翻滚角（绕 X 轴），度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit", meta = (Units = "deg"))
	float EnemyHitFXRoll = 0.f;

	/** 命中特效相对敌人身体中心的位置偏移（Z 向上），cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	FVector EnemyHitFXOffset = FVector::ZeroVector;

	/** 是否按敌人胶囊高度自动上抬特效，避免埋进模型或地面 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|EnemyHit")
	bool bEnemyHitFXAutoHeight = true;

	// ---------- 飞行与路径特效 ----------

	/** 飞行期间跟随方块的光环特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight")
	TObjectPtr<UNiagaraSystem> FlightAuraFX;

	/** 冲刺路径闪电特效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight")
	TObjectPtr<UNiagaraSystem> TeleportPathFX;

	/** 冲刺路径闪电的持续时长（秒），到时自动销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|LightningCube|FX|Flight", meta = (ClampMin = "0.05", Units = "s"))
	float TeleportPathFXDuration = 2.f;

	/** 飞行光环的组件实例（由 BeginPlay 创建，EndPlay 清理） */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> FlightAuraComponent;

private:
	/** 本次发射是否已经冲刺过（每颗方块只允许冲刺一次） */
	bool bTeleportUsed = false;
};
