// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Blueprint/UserWidget.h"
#include "CS_HealthWidget.generated.h"

class UProgressBar;
class UCS_HealthComponent;

/**
 * 血条控件基类（C++ 版）
 *
 * 职责只有一个：订阅玩家 HealthComponent 的 OnHealthPercentChanged，
 * 拿到 0~1 的百分比直接推给 ProgressBar。不轮询、不 Cast、不每帧查组件。
 *
 * 与蓝图版本的区别（Unity 对照）：
 *   蓝图轮询版 = void Update() { bar.fillAmount = hp.Percent; }
 *   本类       = void OnEnable() { hp.OnHealthPercentChanged += SetBar; }
 *
 * 使用步骤：
 *   1. 新建 Widget Blueprint（或把现有 Widget_BP 的父类改成本类）
 *   2. 设计器里放一个 Progress Bar，把它的变量名改成 HealthBar
 *   3. 关卡蓝图 / PlayerController 里 Create Widget + Add to Viewport
 *
 * 注意：本类继承自 UUserWidget，而 UUserWidget 是 Abstract，
 *       所以不能直接用它建控件，必须有一个蓝图子类。
 */
UCLASS()
class CUBE_SKILL_API UCS_HealthWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 控件被创建时调用（≈ Unity 的 OnEnable） */
	virtual void NativeConstruct() override;

	/** 控件被销毁时调用（≈ Unity 的 OnDisable / OnDestroy） */
	virtual void NativeDestruct() override;

	/**
	 * 血量百分比变化回调
	 *
	 * 必须标 UFUNCTION()：UE 的动态委托靠反射找函数，
	 * 签名必须和 DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam 完全一致（一个 float）。
	 * Unity 里 += 可以随便绑方法/lambda，这里不行。
	 */
	UFUNCTION()
	void HandleHealthPercentChanged(float HealthPercent);

	/**
	 * 血条控件
	 *
	 * meta = (BindWidgetOptional)：
	 *   设计器里存在**同名**（HealthBar）的 ProgressBar 时自动绑定；
	 *   找不到也不报编译错误（Optional），只是这里为 nullptr。
	 * 相当于 Unity 的 [SerializeField] 拖引用，但 UE 是按变量名自动匹配。
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

private:
	/** 找到玩家身上的 HealthComponent 并订阅事件（找不到就等定时器重试） */
	void TryBindHealth();

	/** 定时器回调：角色迟迟没生成时反复尝试绑定 */
	void RetryBind();

	/** 用组件当前百分比立刻刷新一次（补初值） */
	void ForceRefresh();

	/** 退订，避免控件销毁后还挂在委托上 */
	void UnbindHealth();

	/** 真正去设置血条进度（做钳制） */
	void SetBarPercent(float Percent);

	/** 缓存的组件引用，解绑时要用 */
	UPROPERTY()
	TObjectPtr<UCS_HealthComponent> CachedHealth;

	/**
	 * 绑定重试定时器
	 *
	 * 为什么不用 NativeTick？
	 *   UUserWidget 的类声明上带 meta=(DisableNativeTick)，
	 *   蓝图子类因此默认被判为"不需要原生 Tick"——NativeTick 很可能根本不会被调用。
	 *   用定时器重试才可靠。
	 */
	FTimerHandle BindRetryTimer;

	/** 打开后在 Output Log 里打印绑定过程和每次更新，排查用 */
	UPROPERTY(EditAnywhere, Category = "CS|Debug")
	bool bLogDebug = false;
};
