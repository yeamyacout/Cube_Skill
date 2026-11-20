// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_CubeProjectile.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * 状态变化事件签名
 * 用 DECLARE_DYNAMIC_MULTICAST_DELEGATE（无参数）才能暴露给蓝图绑定
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCSOnProjectileStateChanged);

/**
 * 抛射物的状态
 *
 * 设计说明：
 *   方块在"被收回"之前必须一直存在于场景中，所以不设超时销毁。
 *   命中物体后不销毁，而是停下来等待回收。
 */
UENUM(BlueprintType)
enum class ECSProjectileState : uint8
{
	/** 飞行中 */
	Flying      UMETA(DisplayName = "飞行中"),

	/** 已命中物体，停在该处等待回收 */
	Stopped     UMETA(DisplayName = "已停下"),

	/** 正在被收回（回飞向持有者的过程中） */
	Returning   UMETA(DisplayName = "收回中"),

	/** 已回收完毕，通常随即销毁 */
	Recalled    UMETA(DisplayName = "已回收")
};

/**
 * ACS_CubeProjectile - 立方体抛射物
 *
 * 生命周期：
 *   生成 -> Flying（飞行）
 *        -> 命中物体 -> Stopped（停下，等待回收）
 *        -> 被收回 -> Returning -> Recalled（销毁）
 *
 * 关键设计：
 *   - 不设置 LifeSpan，方块会一直存在直到被收起
 *   - 命中后停止移动而非销毁，便于手动/自动回收
 *
 * 纯 C++ 实现，使用引擎内置立方体网格，无需蓝图即可工作。
 */
UCLASS()
class CUBE_SKILL_API ACS_CubeProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACS_CubeProjectile();

	/**
	 * 生成并发射抛射物的便捷入口
	 *
	 * @param WorldContextObject  任意对象（用于取 World）
	 * @param SpawnTransform      生成位置与朝向
	 * @param Direction           飞行方向（会自动归一化）
	 * @param InOwner             拥有者（伤害归属用），可为空
	 * @param InInstigator        发起者，可为空
	 * @return                    生成的抛射物；失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile",
		meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "InOwner,InInstigator"))
	static ACS_CubeProjectile* SpawnAndLaunch(
		UObject* WorldContextObject,
		const FTransform& SpawnTransform,
		const FVector& Direction,
		AActor* InOwner = nullptr,
		APawn* InInstigator = nullptr);

	/** 让本抛射物朝指定方向发射 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void LaunchInDirection(const FVector& Direction);

	/**
	 * 开始回收：方块飞回目标（通常是玩家）
	 *
	 * @param TargetActor  飞向的目标；为空则直接销毁
	 * @param ReturnSpeed  回飞速度，<=0 则用默认值
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void StartRecall(AActor* TargetActor, float ReturnSpeed = 0.f);

	/** 立即结束回收（被接住），销毁自身 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void FinishRecall();

	/** 当前状态 */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	ECSProjectileState GetProjectileState() const { return State; }

	/** 是否处于可被回收的状态（飞行中或已停下） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	bool CanBeRecalled() const;

	/** 当前飞行速度（不叫 GetVelocity，避免与 AActor 同名冲突） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	FVector GetFlightVelocity() const;

	/** 状态变化时广播，方便外部（玩家、UI）响应 */
	UPROPERTY(BlueprintAssignable, Category = "CS|Projectile")
	FCSOnProjectileStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;

	/** 碰撞回调：命中后停下，不销毁 */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/** 每帧检查是否飞回达到目标 */
	virtual void Tick(float DeltaSeconds) override;

	/** 切换状态并广播 */
	void SetState(ECSProjectileState NewState);

	// ---------- 组件 ----------

	/** 立方体网格组件（负责显示） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 碰撞球（负责命中检测） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** 飞行控制组件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ---------- 可调参数 ----------

	/** 初始飞行速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 2000.f;

	/** 重力影响系数：0 = 直线飞行，1 = 正常重力 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float GravityScale = 0.3f;

	/** 立方体边长（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Visual", meta = (ClampMin = "1.0"))
	float CubeSize = 30.f;

	/** 回收时的飞行速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float RecallSpeed = 2500.f;

	/** 回收时距离目标多近算"接住"（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "1.0"))
	float RecallAcceptRadius = 80.f;

	/** 是否忽略拥有者（避免出生瞬间撞到自己） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Collision")
	bool bIgnoreOwner = true;

	// ---------- 运行时状态 ----------

	/** 当前状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	ECSProjectileState State = ECSProjectileState::Flying;

	/** 回收目标（通常是玩家）。用弱引用防止目标先被销毁 */
	TWeakObjectPtr<AActor> RecallTarget;
};
