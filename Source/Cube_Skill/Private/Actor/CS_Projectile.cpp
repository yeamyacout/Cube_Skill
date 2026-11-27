// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CS_Projectile.h"

#include "Characters/CS_PlayerCharacter.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"


ACS_Projectile::ACS_Projectile()
{
	PrimaryActorTick.bCanEverTick = false;
	
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovementComponent");
	
}

void ACS_Projectile::LaunchInDirection(const FVector& WorldDirection)
{
	const FVector Direction = WorldDirection.GetSafeNormal();
	if (Direction.IsNearlyZero() || !ProjectileMovementComponent)
	{
		return;
	}

	// 组件初始化时会把默认的本地 X 轴速度转到生成旋转上。这里改成瞄准方向，手上插槽的朝向就不再决定弹道。
	SetActorRotation(Direction.Rotation());

	const float Speed = ProjectileMovementComponent->InitialSpeed > 0.f
		? ProjectileMovementComponent->InitialSpeed
		: ProjectileMovementComponent->Velocity.Size();
	if (Speed <= 0.f)
	{
		return;
	}

	ProjectileMovementComponent->Velocity = Direction * Speed;
	ProjectileMovementComponent->UpdateComponentVelocity();
}

void ACS_Projectile::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	// 出生时会和发射者重叠。发射者不是这次要打的目标。
	if (!IsValid(OtherActor) || OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	ACS_PlayerCharacter* Player = Cast<ACS_PlayerCharacter>(OtherActor);
	if (!IsValid(Player))
	{
		return;
	}

	// 和近战同一条路径，玩家的 HealthComponent 在 OnTakeAnyDamage 里扣血。
	if (Damage > 0.f)
	{
		UGameplayStatics::ApplyDamage(Player, Damage, GetInstigatorController(), this, nullptr);
	}

	// 先在当前位置单独生成。销毁投射物时粒子不会跟着没掉。
	if (IsValid(ExplosionEffect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			this,
			ExplosionEffect,
			GetActorLocation(),
			GetActorRotation(),
			FVector(1.f),
			true,
			EPSCPoolMethod::None,
			true);
	}

	// 人还站在重叠体积里时，这个回调会反复进来。命中后销毁，保证只扣一次。
	Destroy();
}


void ACS_Projectile::BeginPlay()
{
	Super::BeginPlay();
	
}

