// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_FlameTongue.generated.h"

class UCapsuleComponent;
class USceneComponent;
class AController;
class ACS_EnemyCharacter;

/** 火舌命中事件 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnFlameTongueHit, ACS_EnemyCharacter*, Enemy, bool, bFirstHit, float, Damage);

/**
 * 火舌参数
 *
 * 单条火舌的完整配置。火焰方块生成两条时共用同一份。
 * 所有数值都暴露到蓝图，由你按手感调，不在代码里写死。
 */
USTRUCT(BlueprintType)
struct FCSFlameTongueSettings
{
	GENERATED_BODY()

	/** 伤害检测间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FlameTongue", meta = (ClampMin = "0.02"))
	float DamageInterval = 0.25f;

	/** 每次检测对范围内每个敌人结算的伤害 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FlameTongue", meta = (ClampMin = "0.0"))
	float FlameDamage = 10.f;

	/** 是否也伤害玩家（默认只打敌人） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FlameTongue")
	bool bDamagePlayer = false;
};

/**
 * ACS_FlameTongue - 单侧火舌
 *
 * 一个 Actor 只负责一条火舌：
 *   · 本地 +X 为喷射方向，喷口在 Actor 原点；
 *   · 胶囊只作几何体（NoCollision），每 DamageInterval 手动 Overlap 一次；
 *   · 伤害的 DamageCauser = 本 Actor（火焰），不是生成它的方块；
 *   · 可以挂在方块 / 武器 / 陷阱上复用，也可以独立摆放在场景里。
 *
 * 生成方（例如火焰方块）负责：
 *   · 生成时把 Actor 旋转到喷射方向；
 *   · 需要双侧时生成两条，方向相反；
 *   · 什么时候 StopFlame / Destroy。
 */
UCLASS()
class CUBE_SKILL_API ACS_FlameTongue : public AActor
{
	GENERATED_BODY()

public:
	ACS_FlameTongue();

	/**
	 * 初始化火舌
	 *
	 * @param InSource        生成者（方块 / 武器 / 陷阱），用于忽略自身和判断生命周期
	 * @param InInstigator    伤害归属的控制器，可为空
	 * @param InDirection     世界空间的喷射方向（bApplyDirection=false 时只作调试值）
	 * @param InSettings      火舌参数
	 * @param bApplyDirection 是否用 InDirection 覆盖 Actor 旋转。
	 *                        挂载在方块上的火舌传 false，保留 BP 摆好的相对朝向，
	 *                        让整个方块一起转，而不是单独改火舌方向。
	 */
	void InitFlameTongue(AActor* InSource, AController* InInstigator, const FVector& InDirection,
		const FCSFlameTongueSettings& InSettings, bool bApplyDirection = true);

	/** 修改喷射方向（旋转系统就绪后由控制器 / 方块调用） */
	UFUNCTION(BlueprintCallable, Category = "CS|FlameTongue")
	void SetDirection(const FVector& InDirection);

	/** 开始喷火：立即检测一次，之后每 DamageInterval 检测 */
	UFUNCTION(BlueprintCallable, Category = "CS|FlameTongue")
	void StartFlame();

	/** 停止喷火（保留 Actor，等生成方决定何时销毁） */
	UFUNCTION(BlueprintCallable, Category = "CS|FlameTongue")
	void StopFlame();

	UFUNCTION(BlueprintPure, Category = "CS|FlameTongue")
	bool IsFlameActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "CS|FlameTongue")
	FVector GetDirection() const { return Direction; }

	/** 每次检测、每个敌人各广播一次 */
	UPROPERTY(BlueprintAssignable, Category = "CS|FlameTongue")
	FCSOnFlameTongueHit OnFlameHit;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 每 DamageInterval 调用一次 */
	UFUNCTION()
	void DamageTick();

	/** 收集胶囊范围内的目标（敌人；开启 bDamagePlayer 时也包含玩家） */
	void CollectTargets(TSet<AActor*>& OutTargets) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FlameTongue|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FlameTongue|Components")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY()
	FCSFlameTongueSettings Settings;

	/** 生成者（方块 / 武器 / 陷阱） */
	TWeakObjectPtr<AActor> SourceActor;

	/** 伤害归属的控制器 */
	TWeakObjectPtr<AController> InstigatorController;

	/** 世界空间喷射方向 */
	FVector Direction = FVector::ForwardVector;

	/** 本次喷火已经打过的目标（只用于判断 bFirstHit） */
	TSet<TWeakObjectPtr<AActor>> HitTargets;

	/** 是否正在喷火 */
	bool bActive = false;

	FTimerHandle DamageTimerHandle;
};
