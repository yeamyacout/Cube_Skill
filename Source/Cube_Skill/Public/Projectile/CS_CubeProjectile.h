// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_CubeProjectile.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;
class USphereComponent;
class ACS_EnemyCharacter;

/**
 * 状态变化事件签名
 * 用 DECLARE_DYNAMIC_MULTICAST_DELEGATE（无参数）才能暴露给蓝图绑定
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCSOnProjectileStateChanged);

/**
 * 抛射物的状态
 */
UENUM(BlueprintType)
enum class ECSProjectileState : uint8
{
	/** 飞行中 */
	Flying      UMETA(DisplayName = "飞行中"),

	/** 正在被收回（朝持有者飞回） */
	Returning   UMETA(DisplayName = "收回中"),

	/** 已回收完毕（已销毁） */
	Recalled    UMETA(DisplayName = "已回收")
};

/**
 * ACS_CubeProjectile - 立方体抛射物
 *
 * ============ 核心行为 ============
 *
 * 1. 碰到**敌人**：
 *    - 方块像炸弹一样，把周围所有敌人**以方块为中心向四周炸开**
 *    - 弹开距离较远，带向上的抛射弧度
 *    - 方块本身不受影响，继续飞行
 *
 * 2. 碰到**非敌人物体**（墙、地面、道具）：
 *    - 方块立即停止，并朝持有者方向飞回
 *
 * 3. 超过最大飞行距离（默认 150 米）：
 *    - 自动飞回持有者
 *
 * 4. 回收过程：
 *    - 飞回途中撞到的敌人同样会被炸开
 *    - 飞到持有者身边后**直接删除**
 *
 * ============ 设计说明 ============
 *
 * 为什么用"向四周炸开"而不是"沿方向推"：
 *   沿单一方向推时，敌人很容易被自己的速度、AI 寻路、物理挤压
 *   带偏成横向移动，效果不稳定。爆炸式弹开用的是瞬时冲量，
 *   方向由"方块 -> 敌人"的相对位置决定，几何上明确，不会被干扰。
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
	 * @param InOwner             拥有者（通常是发射者），回收时飞回它
	 * @param InInstigator        发起者，可为空
	 * @param ProjectileClass     要生成的类。传 nullptr 用 ACS_CubeProjectile 本身；
	 *                            传蓝图子类则用子类（可在蓝图里调所有参数）
	 * @return                    生成的抛射物；失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile",
		meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "InOwner,InInstigator,ProjectileClass"))
	static ACS_CubeProjectile* SpawnAndLaunch(
		UObject* WorldContextObject,
		const FTransform& SpawnTransform,
		const FVector& Direction,
		AActor* InOwner = nullptr,
		APawn* InInstigator = nullptr,
		TSubclassOf<ACS_CubeProjectile> ProjectileClass = nullptr);

	/** 让本抛射物朝指定方向发射 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void LaunchInDirection(const FVector& Direction);

	/**
	 * 开始回收：方块飞回目标
	 *
	 * @param TargetActor  飞向的目标；为空则回退到 Owner
	 * @param ReturnSpeed  回飞速度，<=0 则用默认值
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void StartRecall(AActor* TargetActor = nullptr, float ReturnSpeed = 0.f);

	/**
	 * 对指定位置触发一次爆炸式弹开
	 * 通常由内部在撞到敌人时调用，也可供外部复用
	 *
	 * @param Epicenter  爆炸中心（一般就是方块位置）
	 * @param Radius     影响半径，<=0 则用配置的 ExplosionRadius
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile")
	void ExplodeAt(const FVector& Epicenter, float Radius = 0.f);

	/** 当前状态 */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	ECSProjectileState GetProjectileState() const { return State; }

	/** 是否处于可被回收的状态 */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	bool CanBeRecalled() const;

	/** 当前飞行速度（不叫 GetVelocity，避免与 AActor 同名冲突） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	FVector GetFlightVelocity() const;

	/** 已飞行的距离（cm） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	float GetTraveledDistance() const { return TraveledDistance; }

	/** 状态变化时广播，方便外部（玩家、UI）响应 */
	UPROPERTY(BlueprintAssignable, Category = "CS|Projectile")
	FCSOnProjectileStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 碰撞回调
	 *   - 撞到敌人：触发爆炸弹开，方块继续飞
	 *   - 撞到其他：停下并开始回收
	 */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	/** 每帧检测周围敌人并炸开（兜底，防止某些接触方式收不到 Hit 回调） */
	void CheckForEnemyExplosion(float DeltaSeconds);

	/** 判断某个 Actor 是不是敌人 */
	static bool IsEnemyActor(const AActor* Actor);

	/**
	 * 给单个敌人施加"炸开"冲量
	 *
	 * @param Enemy      目标敌人
	 * @param Epicenter  爆炸中心
	 * @param Strength   冲量强度系数（0~1，距离越远越小）
	 */
	void BlastEnemy(ACS_EnemyCharacter* Enemy, const FVector& Epicenter, float Strength);

	/** 切换状态并广播 */
	void SetState(ECSProjectileState NewState);

	/**
	 * 应用待发射的速度
	 * 由 LaunchInDirection 与 BeginPlay 共同调用，
	 * 保证无论 SpawnActor 与 BeginPlay 的先后顺序如何，速度都能正确设置。
	 */
	void ApplyLaunchVelocity();

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

	// ---------- 飞行参数 ----------

	/** 初始飞行速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 2000.f;

	/** 重力影响系数：0 = 直线飞行，1 = 正常重力 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Movement", meta = (ClampMin = "0.0"))
	float GravityScale = 0.3f;

	/** 立方体边长（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Visual", meta = (ClampMin = "1.0"))
	float CubeSize = 150.f;

	// ---------- 爆炸弹开参数 ----------

	/**
	 * 爆炸影响半径（cm）
	 * 这个范围内的敌人都会被炸开。默认 400cm = 4 米
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Explosion", meta = (ClampMin = "1.0"))
	float ExplosionRadius = 400.f;

	/**
	 * 弹开力度（cm/s），决定敌人被炸飞的距离
	 * 数值越大飞得越远。默认 1800 约能把敌人弹飞 10+ 米
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Explosion", meta = (ClampMin = "0.0"))
	float BlastImpulse = 1800.f;

	/**
	 * 向上抛射比例（0~1）
	 * 0.6 表示水平方向 0.6 倍、向上 0.8 倍，形成明显的抛物线弹飞
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Explosion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlastUpwardRatio = 0.6f;

	/**
	 * 同一个敌人两次被炸的最小间隔（秒）
	 * 防止方块持续贴着敌人时每帧都炸，导致速度失控
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Explosion", meta = (ClampMin = "0.0"))
	float BlastCooldown = 0.35f;

	/**
	 * 被击飞期间锁定 AI 移动的最长时间（秒）
	 *
	 * 正常情况下敌人在落地时会自动解除锁定（见 ACS_EnemyCharacter::Landed），
	 * 这个值只是兜底：万一敌人卡在半空、掉出地图等异常情况，
	 * 超过这个时间也会强制恢复 AI，避免敌人永久僵住。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Explosion", meta = (ClampMin = "0.1"))
	float KnockbackLockDuration = 3.f;

	// ---------- 回收参数 ----------

	/** 最大飞行距离（cm），超过后自动回收。默认 5000 = 50 米 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "100.0"))
	float MaxTravelDistance = 5000.f;

	/** 回收时的飞行速度（cm/s） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.0"))
	float RecallSpeed = 2500.f;

	/** 回收时距离持有者多近算"接住"，到达后直接销毁（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "1.0"))
	float RecallAcceptRadius = 250.f;

	/**
	 * 回收过程的最长持续时间（秒）
	 *
	 * 注意：方块被墙挡住时会一直贴着墙等玩家过来，
	 * 所以这个值不能太短，否则玩家还没走到方块就消失了。
	 * 它只是最后的安全网——彻底被隔断（比如隔着厚墙）时才生效。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Recall", meta = (ClampMin = "0.5"))
	float RecallMaxDuration = 30.f;

	/** 是否忽略拥有者（避免出生瞬间撞到自己） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Projectile|Collision")
	bool bIgnoreOwner = true;

	// ---------- 运行时状态 ----------

	/** 当前状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	ECSProjectileState State = ECSProjectileState::Flying;

	/** 已飞行的总距离（cm），用于自动回收判定 */
	float TraveledDistance = 0.f;

	/** 上一帧的位置，用于计算飞行距离 */
	FVector LastFrameLocation = FVector::ZeroVector;

	/**
	 * 起飞宽限期（秒）
	 * 方块刚生成时可能与角色胶囊体/地面重叠，前几帧位移很小是正常的。
	 * 这个时间内的"位移小"不判定为卡住，避免刚发射就被误回收。
	 */
	float StuckGraceTime = 0.f;

	/** 期望的发射方向，BeginPlay 时复用，保证时序无关 */
	FVector PendingLaunchDirection = FVector::ForwardVector;

	/** 是否有待应用的发射（配合 PendingLaunchDirection） */
	bool bHasPendingLaunch = false;

	/** 回收开始后经过的时间（秒），用于超时保护 */
	float RecallElapsed = 0.f;

	/**
	 * 回收途中当前是否被墙体挡住（只读，供调试或手感调整）
	 * 被挡住时方块会沿墙滑动，仍然朝向玩家移动
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Projectile|State")
	bool bIsBlockedWhileReturning = false;

	/**
	 * 每个敌人上次被炸的时间（秒）
	 * key 用弱引用，避免敌人销毁后残留导致野指针
	 * 用于实现 BlastCooldown 冷却
	 */
	TMap<TWeakObjectPtr<ACS_EnemyCharacter>, float> LastBlastTimeMap;

	/** 回收目标（通常是持有者）。用弱引用防止目标先被销毁 */
	TWeakObjectPtr<AActor> RecallTarget;
};
