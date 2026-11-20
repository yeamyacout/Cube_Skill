// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CS_PlayerController.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class ACS_CubeProjectile;
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
	
	void Jump();
	void Stopjump();
	void Move(const FInputActionValue &Value);
	void Look(const FInputActionValue &Value);
	void Primary();
	
	
};
