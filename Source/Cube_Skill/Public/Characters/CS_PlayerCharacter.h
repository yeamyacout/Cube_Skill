// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "CS_BaseCharacter.h"
#include "CS_PlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USceneComponent;
class UCS_HealthComponent;
class UCS_CubeRotationComponent;

UCLASS()
class CUBE_SKILL_API ACS_PlayerCharacter : public ACS_BaseCharacter
{
	GENERATED_BODY()

public:
	ACS_PlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 翻转被触发时启动显示刷新（绑定在组件的 OnOrientationChanged 上） */
	UFUNCTION()
	void HandleFlipStarted();

	/** 定时器回调：写一次朝向，动画播完就停表 */
	void UpdateDisplayFromTimer();

	/** 把当前显示朝向写给显示部件 */
	void ApplyDisplayRotation();

	/** 没手动指定显示部件时，按 Tag / 唯一网格自动找 */
	void ResolveDisplayTarget();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Health")
	TObjectPtr<UCS_HealthComponent> HealthComponent;

	/** 方块翻转组件（BP 里 Add Component 添加，BeginPlay 自动按类型缓存） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CS|Cube")
	TObjectPtr<UCS_CubeRotationComponent> CubeRotation;

	/**
	 * 承接朝向的显示部件
	 *
	 * 留空时自动找：先找 Tag 为 CubeDisplay 的场景组件，再退到角色上唯一的 StaticMesh。
	 * 要精确控制就在蓝图里指过来（指 cube mesh，不要指负责展示倾斜的 Pivot）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube")
	TObjectPtr<USceneComponent> CubeDisplayMesh;

	/** 显示刷新间隔（秒），默认 1/60 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CS|Cube", meta = (ClampMin = "0.001"))
	float DisplayUpdateInterval = 0.016f;
	
private:
	UPROPERTY(VisibleAnywhere , Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 显示刷新用的定时器；动画结束即清空 */
	FTimerHandle DisplayTimerHandle;

	
};
