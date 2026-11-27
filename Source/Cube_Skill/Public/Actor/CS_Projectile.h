// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CS_Projectile.generated.h"

class UProjectileMovementComponent;
class UParticleSystem;

UCLASS()
class CUBE_SKILL_API ACS_Projectile : public AActor
{
	GENERATED_BODY()

public:
	ACS_Projectile();
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	/** 按世界方向发射。速度用组件上的 InitialSpeed，蓝图里调过的速度继续生效。 */
	void LaunchInDirection(const FVector& WorldDirection);
	
	
	UPROPERTY(EditAnywhere , BlueprintReadOnly , Category = "CS|Damage" , meta =(ExposeOnSpawn , ClampMin = "0.0"))
	float Damage{20.f};

	/** 命中时在原地播放。投射物马上销毁，特效不能挂在它身上，否则粒子会一起消失。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CS|Projectile")
	TObjectPtr<UParticleSystem> ExplosionEffect;
	
protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "CS|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent;

};
