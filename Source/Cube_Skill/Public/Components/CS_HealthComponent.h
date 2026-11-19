// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CS_HealthComponent.generated.h"

/**
 * 血量变化时广播的事件签名
 *
 * 用 DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams 声明"动态多播委托"：
 *   - DYNAMIC   : 带 UHT 反射信息，蓝图里才能绑定
 *   - MULTICAST : 允许多个监听者同时订阅（比如血条UI、音效、AI 各绑一份）
 *   - ThreeParams: 携带三个参数
 *
 * 参数说明：
 *   CurrentHealth - 变化后的当前血量
 *   MaxHealth     - 最大血量（方便UI直接算比例，不用额外查）
 *   Delta         - 变化量；负数代表扣血，正数代表回血
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCSOnHealthChanged, float, CurrentHealth, float, MaxHealth, float, Delta);

/**
 * 死亡时广播的事件签名
 *
 * 只带一个参数 DamageCauser（造成致命伤的 Actor）。
 * 常用于：播放死亡特效、给击杀者加分数、掉落物品、通知游戏模式。
 * 注意：可能为 nullptr（例如被环境伤害或直接 SetHealth 打到0）。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCSOnDeath, AActor*, DamageCauser);

/**
 * UCS_HealthComponent - 通用健康组件
 *
 * 挂在任意 Actor 上即可获得：
 *   - 血量管理与伤害结算
 *   - 血量变化 / 死亡 事件广播（蓝图可绑定）
 *   - 可选的自动回复
 *   - 可选的死亡自动销毁
 *
 * 典型用法（蓝图）：
 *   1. 在角色蓝图里 Add Component -> Health
 *   2. 在细节面板设置 MaxHealth 等参数
 *   3. 在 Event Graph 里绑定 OnDeath 事件处理死亡逻辑
 *   4. 任何地方调用 ApplyDamage 扣血
 *
 * 典型用法（C++）：
 *   UCS_HealthComponent* Health = Actor->FindComponentByClass<UCS_HealthComponent>();
 *   Health->ApplyDamage(25.f, DamageSource);
 *
 * 设计要点：
 *   - 出生时固定满血
 *   - 组件的 Tick 默认关闭，只有开启自动回复时才启用，避免无谓开销
 *   - 死亡状态有守卫，OnDeath 保证只广播一次
 *   - 死亡后不自动销毁 Actor，由外部监听 OnDeath 自行处理
 *   - 同时兼容 UE 标准的 ApplyDamage（自动绑定 Owner 的 OnTakeAnyDamage）
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CUBE_SKILL_API UCS_HealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 构造函数：设置 Tick 相关默认值 */
	UCS_HealthComponent();

	/**
	 * 游戏开始时调用
	 * 负责：初始化血量、绑定 Owner 的受击事件、决定是否需要 Tick、广播初始血量
	 */
	virtual void BeginPlay() override;

	/**
	 * 每帧更新
	 * 仅当开启了自动回复(bEnableRegen)才会执行到实际逻辑，否则组件 Tick 是关闭的。
	 * 逻辑：受击后等待 RegenDelay 秒，然后按 RegenPerSecond 持续回血直到满。
	 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================
	//  蓝图可调用的接口
	// ============================================================

	/**
	 * 造成伤害
	 *
	 * @param DamageAmount   伤害数值（<=0 会被忽略）
	 * @param DamageCauser   伤害来源 Actor，传给 OnDeath 事件用，可为空
	 * @return               实际扣除的血量（血量不足时会小于 DamageAmount；已死亡返回0）
	 *
	 * 说明：伤害会先把血量钳制到 [0, MaxHealth]，所以返回的是"真实掉血量"。
	 *       如果角色只剩 10 血却受到 100 伤害，返回 10 而不是 100。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Health")
	float ApplyDamage(float DamageAmount, AActor* DamageCauser = nullptr);

	/**
	 * 治疗回血
	 *
	 * @param HealAmount  回复量（<=0 会被忽略）
	 * @return            实际回复的血量（已满血或已死亡返回0）
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Health")
	float Heal(float HealAmount);

	/**
	 * 直接设置当前血量
	 * 会广播 OnHealthChanged；若设到 0 会触发死亡。
	 * 已死亡状态下调用无效（需先 ResetHealth）。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Health")
	void SetHealth(float NewHealth);

	/**
	 * 满血重置并清除死亡状态
	 * 常用于复活、关卡重开。会广播一次 OnHealthChanged。
	 */
	UFUNCTION(BlueprintCallable, Category = "CS|Health")
	void ResetHealth();

	/**
	 * 查询是否已死亡
	 * 用 BlueprintPure 表示它是"纯函数"——蓝图里不需要 exec 引脚，
	 * 可以直接连到分支节点上取结果。
	 */
	UFUNCTION(BlueprintPure, Category = "CS|Health")
	bool IsDead() const { return bIsDead; }

	/** 查询当前血量 */
	UFUNCTION(BlueprintPure, Category = "CS|Health")
	float GetHealth() const { return CurrentHealth; }

	/** 查询最大血量 */
	UFUNCTION(BlueprintPure, Category = "CS|Health")
	float GetMaxHealth() const { return MaxHealth; }

	/**
	 * 查询血量百分比（0.0 ~ 1.0）
	 * 专门给 UI 血条用，省得每次自己在蓝图里除一遍。
	 * 内部做了除零保护（MaxHealth 为 0 时返回 0）。
	 */
	UFUNCTION(BlueprintPure, Category = "CS|Health")
	float GetHealthPercent() const;

	// ============================================================
	//  事件（在蓝图里用"绑定事件"节点订阅）
	// ============================================================

	/**
	 * 血量变化时触发
	 * 触发时机：受伤、治疗、SetHealth、自动回复、开局初始化
	 * 用途：刷新血条、播放受击音效、触发屏幕红屏特效
	 */
	UPROPERTY(BlueprintAssignable, Category = "CS|Health")
	FCSOnHealthChanged OnHealthChanged;

	/**
	 * 死亡时触发（保证只触发一次）
	 * 用途：播放死亡动画、禁用输入、通知AI、掉落物品
	 * 注意：本组件不负责销毁 Actor，销毁逻辑请在此事件中自行处理。
	 */
	UPROPERTY(BlueprintAssignable, Category = "CS|Health")
	FCSOnDeath OnDeath;

protected:
	// ============================================================
	//  可配置属性（在蓝图细节面板里可编辑）
	// ============================================================

	/**
	 * 最大血量
	 * ClampMin="1.0" 保证编辑器里填不小于 1，避免除零和逻辑异常。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** 是否启用自动回复（开启后组件才会 Tick） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Health|Regen")
	bool bEnableRegen = false;

	/** 每秒回复的血量（仅在 bEnableRegen 开启时可用） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Health|Regen",
		meta = (EditCondition = "bEnableRegen", ClampMin = "0.0"))
	float RegenPerSecond = 5.f;

	/**
	 * 受击后多久才开始回复（秒）
	 * 防止刚被打完立刻回血，让战斗有"喘息"节奏。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Health|Regen",
		meta = (EditCondition = "bEnableRegen", ClampMin = "0.0"))
	float RegenDelay = 3.f;

private:
	// ============================================================
	//  内部状态（蓝图只读，不能改）
	// ============================================================

	/**
	 * 当前血量
	 * AllowPrivateAccess="true" 的作用：虽然是 private，仍暴露给蓝图读取。
	 * 这样蓝图能直接连线读取，但无法写入——所有修改必须走 ApplyDamage/Heal/SetHealth，
	 * 保证事件一定会被广播。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 100.f;

	/** 是否已死亡（用作死亡逻辑的重入守卫，防止重复广播 OnDeath） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

	/** 距离上次受伤过去的时间（秒），用于判断自动回复的冷却何时结束 */
	float TimeSinceLastDamage = 0.f;

	// ============================================================
	//  内部函数
	// ============================================================

	/**
	 * 死亡处理
	 * 职责：置死亡标记 -> 关闭 Tick -> 广播 OnDeath -> 按配置销毁 Owner
	 * 通过 bIsDead 守卫保证只执行一次。
	 */
	void HandleDeath(AActor* DamageCauser);

	/**
	 * 统一的血量修改入口
	 * 负责钳制范围、判断是否真的变化、广播事件、必要时触发死亡。
	 * ApplyDamage/Heal/SetHealth/自动回复 最终都走到这里，保证行为一致。
	 *
	 * @param NewHealth     目标血量（会被钳制到 [0, MaxHealth]）
	 * @param DamageCauser  伤害来源，用于死亡事件
	 */
	void ChangeHealthInternal(float NewHealth, AActor* DamageCauser);

	/**
	 * Owner 受到 UE 标准伤害时的回调
	 *
	 * 在 BeginPlay 里绑定到 Owner->OnTakeAnyDamage，使得以下两种写法等价：
	 *   1) 直接调组件：HealthComp->ApplyDamage(25.f, Instigator)
	 *   2) 走引擎标准：UGameplayStatics::ApplyDamage(Actor, 25.f, ...)
	 *
	 * 这样其他系统（爆炸、陷阱、伤害区域）不用知道本组件的存在也能造成伤害。
	 * 参数签名由引擎的 FTakeAnyDamageSignature 规定，不可改动。
	 */
	UFUNCTION()
	void HandleOwnerTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
