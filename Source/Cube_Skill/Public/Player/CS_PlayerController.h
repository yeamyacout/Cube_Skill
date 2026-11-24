// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimMontage.h"
#include "CS_PlayerController.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class ACS_CubeProjectile;
class UCS_CubeRotationComponent;
class ACS_EnemyCharacter;
/**
 * 
 */
UCLASS()
class CUBE_SKILL_API ACS_PlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;
	
private:
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input")
	TArray<TObjectPtr<UInputMappingContext>> InputMappingContexts;

	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Move")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Move")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Move")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> PrimaryAction;

	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> FireCubeAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> LightCubeAction;
	
	
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> NatchCubeAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> BeginCubeAction;

	/** F 键：瞬移到雷电方块位置（仅雷电方块有效） */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Ability")
	TObjectPtr<UInputAction> TeleportAction;
	
	/**
	 * 左键发射的方块类
	 *
	 * 默认用 C++ 的 ACS_CubeProjectile。
	 * 如果想在编辑器里实时调方块大小/爆炸力度等参数而不用重新编译，
	 * 就创建一个它的蓝图子类（右键 > Blueprint Class > 搜索 CubeProjectile），
	 * 然后在 BP_PlayerController 里把这个属性指向那个蓝图类。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile")
	TSubclassOf<ACS_CubeProjectile> ProjectileClass;

	// ---------- 方块类型切换 ----------

	/**
	 * 可切换的方块类列表，索引 0/1/2 分别对应数字键 1/2/3
	 *
	 * 留空则一直使用 ProjectileClass。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile|Switch")
	TArray<TSubclassOf<ACS_CubeProjectile>> SwitchableProjectiles;

	/**
	 * 初始方块类（默认白色方块）
	 *
	 * 按 T 键在「初始方块」和「上次选中的特殊方块」之间切换。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile|Switch")
	TSubclassOf<ACS_CubeProjectile> DefaultProjectileClass;

	/** 当前选中的方块索引（-1 = 初始方块） */
	UPROPERTY(VisibleAnywhere, Category = "CS|Projectile|Switch")
	int32 CurrentProjectileIndex = -1;

	/** 上一次选中的特殊方块索引（按 T 回切用） */
	UPROPERTY(VisibleAnywhere, Category = "CS|Projectile|Switch")
	int32 LastSpecialProjectileIndex = 0;

	/** 当前是否处于初始方块状态 */
	UPROPERTY(VisibleAnywhere, Category = "CS|Projectile|Switch")
	bool bUsingDefaultProjectile = true;

public:
	/** 切换到指定索引的方块；越界会被忽略 */
	UFUNCTION(BlueprintCallable, Category = "CS|Projectile|Switch")
	void SelectProjectileType(int32 Index);

	/** 当前方块索引（-1 表示初始方块） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile|Switch")
	int32 GetCurrentProjectileIndex() const { return CurrentProjectileIndex; }

	/** 当前是否使用初始方块 */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile|Switch")
	bool IsUsingDefaultProjectile() const { return bUsingDefaultProjectile; }

private:
	/** 数字键 1/2/3 切换方块 */
	void SelectType1();
	void SelectType2();
	void SelectType3();

	/** T 键：在初始方块与上次特殊方块之间切换 */
	void ToggleDefaultProjectile();

	/** F 键：瞬移到雷电方块位置 */
	void Teleport();

	/** 取得当前应使用的方块类 */
	TSubclassOf<ACS_CubeProjectile> GetActiveProjectileClass() const;

/** 方块生成点距相机的距离（cm），需让方块脱离玩家身体 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile", meta = (ClampMin = "50.0"))
	float SpawnForwardOffset = 200.f;

/** 探测时额外延伸的距离（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile", meta = (ClampMin = "0.0"))
	float SpawnWallClearance = 110.f;

/** 生成点距相机的最小距离（cm） */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile", meta = (ClampMin = "10.0"))
	float SpawnMinDistance = 80.f;

/** 生成点与障碍之间必须保留的间隙（cm），需大于方块碰撞球半径 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnObstacleGap = 170.f;

/** 近距离敌人的判定距离（cm），命中时绕过物理碰撞直接生效 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Projectile", meta = (ClampMin = "0.0", Units = "cm"))
	float CloseRangeEnemyDistance = 300.f;

	/**
	 * 当前在场的方块（同一时间只允许存在一个）
	 *
	 * 用途：左键发射前检查是否已有方块在场。
	 * 用 TWeakObjectPtr 而不是裸指针，因为方块被回收时会自行销毁，
	 * 弱引用能自动感知"对象没了"，不会野指针崩溃。
	 */
	TWeakObjectPtr<ACS_CubeProjectile> ActiveProjectile;

	// ============================================================
	//  攻击动画相关
	// ============================================================

	/**
	 * 攻击蒙太奇
	 *
	 * 播放后，动画里的 UCS_AttackNotify 会在"投掷帧"通知控制器生成方块。
	 * 留空则左键不播动画，直接生成方块（保留旧的即时投掷行为）。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 是否正在播放攻击动画（用于阻止动画播完前重复触发） */
	bool bIsAttacking = false;

	/**
	 * 本次攻击是否已经生成过方块
	 * 防止动画循环播放或 Notify 重复触发导致一次攻击扔出多个
	 */
	bool bHasThrownThisAttack = false;

	/** 蒙太奇播放结束的回调（放在成员里，避免回调期间被 GC） */
	FOnMontageEnded AttackMontageEndedDelegate;

	/** 蒙太奇被中断时的处理 */
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
	/** 当前是否已经有方块在场（有则不允许再次发射） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	bool HasActiveProjectile() const;

	/** 获取当前在场的方块（可能为空） */
	UFUNCTION(BlueprintPure, Category = "CS|Projectile")
	ACS_CubeProjectile* GetActiveProjectile() const;

	/** 当前是否正在播放攻击动画 */
	UFUNCTION(BlueprintPure, Category = "CS|Attack")
	bool IsAttacking() const { return bIsAttacking; }

	/**
	 * 攻击动画播到"投掷帧"时由 UCS_AttackNotify 调用
	 *
	 * 这是真正生成方块的地方。左键只负责播动画，
	 * 方块由动画进度驱动生成，保证投掷动作与方块飞出对齐。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Attack")
	void OnAttackThrowNotify();

private:
	/** 真正生成并发射方块（相机前方） */
	void SpawnProjectile();

	/**
	 * 贴脸发射时要直接命中的敌人
	 *
	 * 发射前射线探测到前方很近处有敌人时记录下来，
	 * 方块生成后立即调用 HitEnemyImmediately 对它生效，
	 * 绕过物理碰撞（方块无法避免地与敌人重叠，碰撞检测不到）。
	 */
	TWeakObjectPtr<ACS_EnemyCharacter> PendingNearbyEnemy;

	void Jump();
	void Stopjump();
	void Move(const FInputActionValue &Value);
	void Look(const FInputActionValue &Value);
	void Primary();
	
	

public:
	// ============================================================
	//  方块翻转（E = 右翻，Q = 前翻）
	// ============================================================

	/** E：方块向右翻 90°；蓝图里可覆盖默认 IA */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Cube")
	TObjectPtr<UInputAction> FlipRightAction;

	/** Q：方块向前翻 90°；蓝图里可覆盖默认 IA */
	UPROPERTY(EditDefaultsOnly, Category = "CS|Input|Cube")
	TObjectPtr<UInputAction> FlipForwardAction;

	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void FlipRight();

	UFUNCTION(BlueprintCallable, Category = "CS|Cube")
	void FlipForward();

	/** 取得角色身上的方块翻转组件（未挂时返回空，按键被忽略） */
	UFUNCTION(BlueprintPure, Category = "CS|Cube")
	UCS_CubeRotationComponent* GetCubeRotation() const;
};


