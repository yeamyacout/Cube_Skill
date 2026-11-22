// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_LightningCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"


ACS_LightningCubeProjectile::ACS_LightningCubeProjectile()
{
	// 紫色：代表雷电
	CubeColor = FLinearColor(0.55f, 0.15f, 1.f, 1.f);


	// 雷电方块特征：无碰撞（飞行中穿过一切物体）
	bEnableCollisionDetection = false;

	// 不进入物理模拟
	bEnablePhysicsOnImpact = false;

	// 撞到敌人不推开，用范围伤害代替
	bEnableEnemyKnockback = false;
}


void ACS_LightningCubeProjectile::BeginPlay()
{
	Super::BeginPlay();
}


void ACS_LightningCubeProjectile::OnEnemyHitEffect(ACS_EnemyCharacter* Enemy)
{
	// 只在首次命中时释放雷电，避免同一飞行路径反复触发
	if (bLightningDischarged)
	{
		return;
	}

	bLightningDischarged = true;

	DamageEnemiesInRadius(GetActorLocation());
}


void ACS_LightningCubeProjectile::DamageEnemiesInRadius(const FVector& Center)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_LightningRadius), false, this);
	if (GetOwner())
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	const bool bFound = World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(LightningRadius),
		QueryParams);

	if (!bFound)
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACS_EnemyCharacter* Target = Cast<ACS_EnemyCharacter>(Overlap.GetActor());
		if (!Target)
		{
			continue;
		}

		Target->TakeDamage(LightningDamage, FDamageEvent(), nullptr, this);
	}
}
