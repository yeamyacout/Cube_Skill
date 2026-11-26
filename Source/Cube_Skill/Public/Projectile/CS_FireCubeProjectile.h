// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile/CS_CubeProjectile.h"
#include "Actor/CS_FlameTongue.h"
#include "CS_FireCubeProjectile.generated.h"

class ACS_EnemyCharacter;
class ACS_FlameTongue;

/** 喷火开始（表现层可以在这里挂特效） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCSOnFireSprayStarted);

/** 每次火舌伤害（首次 / 后续刷新） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnFireHit, ACS_EnemyCharacter*, Enemy, bool, bFirstHit, float, Damage);

/** 火星挂点 / 刷新（表现层） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCSOnEmberAttached, ACS_EnemyCharacter*, Enemy, bool, bFirstHit);

/**
 * ACS_FireCubeProjectile - 火焰方块
 *
 * 与 FIRE_CUBE.md 一致的行为：
 *   · 固定占用「上下」面组，沿面组当前轴的双侧各生成一条火舌；
 *   · 不推敌、不进入模拟物理（两个基类开关固定关闭）；
 *   · 火舌每 DamageInterval 检测一次，范围内敌人结算火焰伤害；
 *   · 命中后广播火焰属性 / 火星事件（属性系统未接入，先留日志）；
 *   · 命中后的收回 / 表现完全沿用基类，本类不覆写。
 */
UCLASS()
class CUBE_SKILL_API ACS_FireCubeProjectile : public ACS_CubeProjectile
{
	GENERATED_BODY()

public:
	ACS_FireCubeProjectile();

	/** 当前火焰轴（世界空间单位向量，±X / ±Y / ±Z 之一） */
	UFUNCTION(BlueprintPure, Category = "CS|FireCube")
	FVector GetFireAxis() const;

	/** 旋转系统 / 控制器设置火焰轴；会同步更新已经生成的火舌 */
	UFUNCTION(BlueprintCallable, Category = "CS|FireCube")
	void SetFireAxisOverride(const FVector& InAxis);

	/** 任意一条火舌还在喷 */
	UFUNCTION(BlueprintPure, Category = "CS|FireCube")
	bool IsSpraying() const;

	UPROPERTY(BlueprintAssignable, Category = "CS|FireCube")
	FCSOnFireSprayStarted OnFireSprayStarted;

	UPROPERTY(BlueprintAssignable, Category = "CS|FireCube")
	FCSOnFireHit OnFireHit;

	UPROPERTY(BlueprintAssignable, Category = "CS|FireCube")
	FCSOnEmberAttached OnEmberAttached;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 基类状态变化：进入收回时停火 */
	UFUNCTION()
	void HandleStateChanged();

	/** 回收到玩家附近时开始检测，准备传送隐藏 */
	void StartRecallHideCheck();

	/** 停掉“到达玩家附近”的检测 */
	void StopRecallHideCheck();

	/** 检测是否已经到玩家附近 */
	UFUNCTION()
	void CheckRecallHide();

	/** 传送隐藏后延迟真正回收 */
	UFUNCTION()
	void FinishRecallAfterHide();

	/** 火舌命中回调：转发给蓝图事件 */
	UFUNCTION()
	void HandleFlameHit(ACS_EnemyCharacter* Enemy, bool bFirstHit, float Damage);

	/** 优先使用 BP 里挂好的火舌；不足时才 Spawn 兜底 */
	void AcquireFlameTongues();

	/** 收集挂在方块上的火舌（ChildActorComponent / 直接 Attach 都支持） */
	void FindAttachedFlameTongues(TArray<ACS_FlameTongue*>& OutTongues) const;

	/** 把整个方块旋转到当前翻转姿态（从玩家角色的旋转组件读显示 Quat） */
	void ApplyFlipRotation();

	/** Spawn 一条兜底火舌（挂载数量不足时用） */
	ACS_FlameTongue* SpawnFlameTongue(const FVector& Direction, float MuzzleOffset);

	/** 初始化 / 绑定 / 启动一条火舌；bApplyDirection=false 时保留它自己的朝向 */
	void SetupFlameTongue(ACS_FlameTongue* Tongue, const FVector& Direction, bool bApplyDirection);

	/** 停止喷火（保留 Actor，等方块销毁时一并销毁） */
	void StopSpray();

	/** 停火并销毁两条火舌 */
	void DestroyFlameTongues();

	/**
	 * 火焰属性 / 火星接入点
	 *
	 * 属性系统就绪后在这里调用 ApplyElement / 刷新时长；
	 * 当前只打日志，由 OnEmberAttached 通知表现层挂 / 刷新火星。
	 */
	void ApplyFireAttribute(ACS_EnemyCharacter* Enemy, bool bFirstHit);

	// ---------- 火舌配置（全部蓝图可调，数值由你调） ----------

	/** 火舌 Actor 类；留空则用 C++ 的 ACS_FlameTongue，建议在 BP_FireCube 里指向 BP_FlameTongue */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CS|FireCube|Flame")
	TSubclassOf<ACS_FlameTongue> FlameTongueClass;

	/** 两条火舌共用的参数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Flame")
	FCSFlameTongueSettings FlameSettings;

	/** 收回期间是否继续喷火（当前需求：飞回来时也要喷） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Flame")
	bool bSprayWhileReturning = true;

	// ---------- 回收到玩家身边时的隐藏 ----------

	/** 距离玩家多近时触发传送隐藏（cm，水平距离） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Recall", meta = (ClampMin = "1.0", Units = "cm"))
	float RecallHideTriggerRadius = 120.f;

	/** 传送隐藏时往地下多深（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Recall", meta = (ClampMin = "0.0", Units = "cm"))
	float RecallHideDepth = 100000.f;

	/** 传送后等多久再真正回收（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Recall", meta = (ClampMin = "0.0"))
	float RecallHideDelay = 0.5f;

	/** 距离检测间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Recall", meta = (ClampMin = "0.005"))
	float RecallHideCheckInterval = 0.02f;

	/** 是否已经开始“传送隐藏”流程 */
	bool bRecallHideStarted = false;

	FTimerHandle RecallHideCheckTimerHandle;
	FTimerHandle RecallHideTimerHandle;

	/**
	 * 火舌生成点离方块中心的距离（cm）
	 *
	 * 0 表示自动取 CubeSize / 2，让火舌从方块表面喷出。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|FireCube|Flame", meta = (ClampMin = "0.0", Units = "cm"))
	float FlameMuzzleOffset = 75.f;

	/** 运行时火焰轴覆盖（旋转系统 / 控制器调用） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireCube|Flame")
	FVector FireAxisOverride = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireCube|Flame")
	TObjectPtr<ACS_FlameTongue> FlameTongueA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|FireCube|Flame")
	TObjectPtr<ACS_FlameTongue> FlameTongueB;

	/** 这条火舌是不是本类 Spawn 出来的（挂载的销毁交给 ChildActorComponent） */
	bool bFlameTongueSpawnedA = false;
	bool bFlameTongueSpawnedB = false;
};
