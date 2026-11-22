// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_CubeProjectile.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;
class ACS_EnemyCharacter;

/** 状态变化事件 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCSOnProjectileStateChanged);

/** 方块状态 */
UENUM(BlueprintType)
enum class ECSProjectileState : uint8
{
	Flying      UMETA(DisplayName = "飞行中"),
	Blocked     UMETA(DisplayName = "已碰撞等待回收"),
	Returning   UMETA(DisplayName = "收回中"),
	Recalled    UMETA(DisplayName = "已回收")
};

/**
 * ACS_CubeProjectile - 立方体投掷物
 *
 * 飞行：ProjectileMovementComponent 直线推进，无重力。
 * 撞敌：球形 Overlap 主动检测（不依赖物理碰撞，远近都能命中），
 *       持续把敌人向后方推 5 米，0.5 秒后回收。
 * 撞物：转为真刚体，弹开 + 翻滚 + 落地滚动，0.5 秒后回收。
 * 回收：Homing 追踪玩家，穿过地形飞回，250cm 内销毁。
 */
UCLASS()
class CUBE_SKILL_API ACS_CubeProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACS_CubeProjectile();

	/** 生成并发射 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile",
		meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "InOwner,InInstigator,ProjectileClass"))
	static ACS_CubeProjectile* SpawnAndLaunch(
		UObject* WorldContextObject,
		const FTransform& SpawnTransform,
		const FVector& Direction,
		AActor* InOwner = nullptr,
		APawn* InInstigator = nullptr,
		TSubclassOf<ACS_CubeProjectile> ProjectileClass = nullptr);

	/** 朝指定方向发射 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void LaunchInDirection(const FVector& Direction);

	/** 立即命中敌人（近距离发射时绕过物理碰撞直接生效） */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void HitEnemyImmediately(ACS_EnemyCharacter* Enemy, const FVector& Direction);

	/** 开始回收 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void StartRecall(float ReturnSpeed = 0.f);

	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	ECSProjectileState GetProjectileState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	bool IsFlying() const { return bIsFlying; }

	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	FVector GetFlightVelocity() const;

	UPROPERTY(BlueprintAssignable, Category = "CS|Projectile")
	FCSOnProjectileStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;

	/** 物理碰撞回调（兜底，主检测用 Overlap） */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/** 出生保护期结束 */
	UFUNCTION()
	void ArmCollision();

	/** 球形检测周围敌人 */
	UFUNCTION()
	void CheckNearbyEnemies();

	/** 检测与玩家距离，超限则回收 */
	UFUNCTION()
	void CheckTravelDistance();

	/** 每 33ms 推动一次敌人 */
	UFUNCTION()
	void ApplyEnemyPush();

	/** 延迟结束，开始回收 */
	UFUNCTION()
	void HandleRecallDelayFinished();

	/** 检查是否已回到玩家身边 */
	void CheckRecallArrival();

	static bool IsEnemyActor(const AActor* Actor);
	void SetState(ECSProjectileState NewState);

	/** 重启移动组件（碰撞后 UpdatedComponent 会变 NULL，必须重绑） */
	void RestartMovement(const FVector& NewVelocity);

	/** 应用发射速度 */
	void ApplyLaunchVelocity();

	/** 命中敌人：推敌 + 停住 + 延迟回收 */
	void ProcessEnemyHit(ACS_EnemyCharacter* Enemy, const FVector& PushDir);

	/**
	 * 命中敌人时的额外效果钩子（子类覆写）
	 *
	 * 基类只负责推开敌人；火焰/雷电/自然等子类
	 * 可以覆写这个函数来施加各自的特殊效果
	 * （灼烧 DoT、溅射伤害、减速等）。
	 */
	virtual void OnEnemyHitEffect(ACS_EnemyCharacter* Enemy) {}

	/** 停止推动 */
	void StopEnemyPush();

	/** 转为真刚体 */
	void SwitchToPhysicsOnImpact(const FHitResult& Hit, bool bIsEnemyImpact = false);

	/** 从刚体恢复为移动组件模式（回收前必须调用） */
	void RestoreFromPhysics();

	// ---------- 组件 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// ---------- 飞行 ----------

	/** 初速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 2000.f;

	/** 重力系数，0 = 无重力直线飞行 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float GravityScale = 0.f;

	/**
	 * 立方体尺寸（cm）
	 *
	 * 视觉大小与碰撞大小都由这一个值控制：
	 *   网格缩放 = CubeSize / 100
	 *   碰撞球半径 = CubeSize
	 *
	 * 150 = 1.5 米见方。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Visual", meta = (ClampMin = "10.0", Units = "cm"))
	float CubeSize = 150.f;

	/** 方块颜色（默认白色；子类可覆写默认值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Visual")
	FLinearColor CubeColor = FLinearColor::White;

	/** 是否把 CubeColor 应用到网格材质 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Visual")
	bool bUseCubeColor = true;

	/** 设置网格相对缩放（同时同步碰撞球大小） */
	void SetMeshRelativeScale(const FVector& NewScale);

	// ---------- 回收 ----------

	/** 撞物体后延迟多久回收（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float RecallDelayAfterHit = 1.f;

	/** 回收飞行速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float RecallSpeed = 4000.f;

	/** Homing 追踪加速度，越大转向越快 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float RecallHomingAcceleration = 12000.f;

	/** 接到距离（cm），进入此范围即销毁 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "1.0"))
	float RecallAcceptRadius = 250.f;

	/**
	 * 最大飞行距离（米）
	 *
	 * 方块距离玩家超过这个值就立即回收。
	 * 注意这里是**米**，内部会换算成厘米。
	 *
	 * 35 米。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Flight|Range", meta = (ClampMin = "1.0", Units = "m"))
	float MaxTravelDistanceMeters = 35.f;

	/** 最大飞行距离（厘米），内部使用，由 MaxTravelDistanceMeters 换算 */
	float MaxTravelDistance = 3500.f;

/** 回收超时（秒），防止被夹死永久滞留 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "1.0"))
	float RecallTimeout = 15.f;

	/** 回收已持续的时间（秒） */
	float RecallElapsed = 0.f;

	/** 回收开始的时间戳（世界时间），用于超时判定 */
	float RecallStartTime = 0.f;

	/** 距离检测间隔（秒），同时也是卡死兜底的检查频率 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.02"))
	float DistanceCheckInterval = 0.1f;

	// ---------- 卡死兜底 ----------

	/**
	 * 发射后多久开始接受"靠近回收"（秒）
	 *
	 * 方块发射后经过这段时间，玩家走近到一定距离内就会触发回收。
	 * 限制时间是为了避免刚发射时玩家本来就在旁边，方块立刻被召回。
	 *
	 * 2 秒。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float MinTimeBeforeCloseRecall = 2.f;

	/**
	 * 靠近回收的触发距离（cm）
	 *
	 * 发射超过 MinTimeBeforeCloseRecall 秒后，
	 * 方块与玩家距离小于这个值就强制回收。
	 *
	 * 100cm = 1 米。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0", Units = "cm"))
	float CloseProximityDistance = 100.f;

/** 卡死判定距离（cm）：离玩家很近 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0", Units = "cm"))
	float StuckTeleportRecoverDistance = 50.f;

/** 卡死判定速度（cm/s）：几乎不动 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float StuckSpeedThreshold = 50.f;

	// ---------- 碰撞 ----------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Collision")
	bool bIgnoreOwner = true;

	/** 出生保护时间（秒），期间忽略碰撞，避免出生即撞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Collision", meta = (ClampMin = "0.0"))
	float SpawnCollisionGraceTime = 0.2f;

	// ---------- 撞敌人 ----------

	/** 推动敌人的速度（cm/s），1000 × 0.5 秒 = 5 米 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "0.0"))
	float EnemyPushSpeed = 1000.f;

	/** 推动持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "0.0"))
	float EnemyPushDuration = 0.5f;

	/** 撞敌人后延迟回收（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "0.0"))
	float EnemyHitRecallDelay = 0.5f;

	/** 敌人 AI 锁定时长（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "0.0"))
	float EnemyKnockbackLockTime = 3.f;

	/** 敌人检测半径（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "1.0"))
	float EnemyDetectRadius = 220.f;

	/** 敌人检测间隔（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|EnemyHit", meta = (ClampMin = "0.01"))
	float EnemyDetectInterval = 0.03f;

	// ---------- 核心行为开关（蓝图可调） ----------

	/**
	 * 是否可以有碰撞
	 *
	 * 关闭后：方块不与任何物体发生物理碰撞，OnComponentHit 不再触发。
	 * 用途：做穿透类玩法（穿墙弹、穿透射击）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Switches")
	bool bEnableCollisionDetection = true;

	/**
	 * 是否可以弹开敌人
	 *
	 * 关闭后：撞到敌人不再推动对方，方块仍照常处理碰撞与回收。
	 * 用途：做"仅命中不击退"的变体方块。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Switches")
	bool bEnableEnemyKnockback = true;

	// ---------- 碰撞转物理 ----------

	/** 碰撞后是否转为真刚体（弹开 + 翻滚 + 滚动） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Physics")
	bool bEnablePhysicsOnImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Physics", meta = (ClampMin = "0.1"))
	float ImpactPhysicsMass = 15.f;

	/** 反弹系数 0~1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImpactRestitution = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Physics", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ImpactLinearDamping = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Physics", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float ImpactAngularDamping = 1.5f;

	// ---------- 旋转开关（蓝图可视化） ----------

	/** 撞墙/地面时是否让方块翻滚旋转 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Rotation")
	bool bEnableRotationOnSurfaceImpact = true;

	/** 撞敌人时是否让方块翻滚旋转 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Rotation")
	bool bEnableRotationOnEnemyImpact = true;

	/** 撞墙时的翻滚力度（0 = 不转） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Rotation", meta = (ClampMin = "0.0"))
	float SurfaceImpactAngularImpulse = 300.f;

	/** 撞敌人时的翻滚力度（0 = 不转） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Rotation", meta = (ClampMin = "0.0"))
	float EnemyImpactAngularImpulse = 300.f;

	// ---------- 运行时状态 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	ECSProjectileState State = ECSProjectileState::Flying;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	bool bIsFlying = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	bool bHasCollided = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	bool bIsReturning = false;

	/** 出生保护是否结束 */
	bool bCollisionArmed = false;

	/** 是否已转为刚体 */
	bool bSwitchedToPhysics = false;

	/** 是否正在推动敌人 */
	bool bIsPushingEnemy = false;

	/** 期望的发射方向 */
	FVector PendingLaunchDirection = FVector::ForwardVector;
	bool bHasPendingLaunch = false;

	/** 锁定的推力方向 */
	FVector LockedPushDirection = FVector::ZeroVector;

	/** 发射起点 */
	FVector LaunchOrigin = FVector::ZeroVector;
	bool bLaunchOriginValid = false;

	/** 发射时刻（世界时间秒），用于"发射 N 秒后才能靠近回收"的判定 */
	float LaunchStartTime = 0.f;

	/** 回收目标 */
	TWeakObjectPtr<AActor> RecallTarget;

	/** 正在被推的敌人 */
	TWeakObjectPtr<ACS_EnemyCharacter> PushedEnemy;

	/** 已命中过的敌人，避免重复推 */
	TSet<TWeakObjectPtr<ACS_EnemyCharacter>> AlreadyHitEnemies;

	/** 敌人原始移动参数，推动结束后还原 */
	float SavedEnemyMaxWalkSpeed = 600.f;
	float SavedEnemyBrakingDeceleration = 2048.f;

	// ---------- 定时器 ----------

	FTimerHandle SpawnGraceTimerHandle;      // 出生保护
	FTimerHandle DistanceCheckTimerHandle;   // 距离检测
	FTimerHandle EnemyDetectTimerHandle;     // 敌人检测
	FTimerHandle PushTimerHandle;            // 持续推动
	FTimerHandle PushStopTimerHandle;        // 停止推动
	FTimerHandle RecallDelayTimerHandle;     // 延迟回收
	FTimerHandle RecallCheckTimerHandle;     // 到达判定
};
