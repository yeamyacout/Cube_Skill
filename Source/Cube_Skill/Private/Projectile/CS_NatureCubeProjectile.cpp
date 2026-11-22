// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_NatureCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"


ACS_NatureCubeProjectile::ACS_NatureCubeProjectile()
{
	// 绿色：代表自然
	CubeColor = FLinearColor(0.15f, 0.9f, 0.2f, 1.f);

}


void ACS_NatureCubeProjectile::BeginPlay()
{
	Super::BeginPlay();
}


void ACS_NatureCubeProjectile::OnEnemyHitEffect(ACS_EnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TWeakObjectPtr<ACS_EnemyCharacter> WeakEnemy(Enemy);
	TWeakObjectPtr<ACS_NatureCubeProjectile> WeakSelf(this);

	// ---------- 中毒：持续伤害 ----------
	const int32 TickCount = FMath::Max(1, FMath::RoundToInt(PoisonDuration / PoisonTickInterval));
	const float DamagePerTick = PoisonTotalDamage / TickCount;

	for (int32 i = 1; i <= TickCount; ++i)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateWeakLambda(this, [WeakEnemy, WeakSelf, DamagePerTick]()
			{
				ACS_EnemyCharacter* Target = WeakEnemy.Get();
				ACS_NatureCubeProjectile* Self = WeakSelf.Get();
				if (!Target || !Self)
				{
					return;
				}

				Target->TakeDamage(DamagePerTick, FDamageEvent(), nullptr, Self);
			}),
			PoisonTickInterval * i,
			false);
	}

	// ---------- 减速：临时降低移动速度 ----------
	if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
	{
		const float OriginalSpeed = MoveComp->MaxWalkSpeed;
		MoveComp->MaxWalkSpeed = OriginalSpeed * (1.f - SlowRatio);

		FTimerHandle SlowHandle;
		World->GetTimerManager().SetTimer(
			SlowHandle,
			FTimerDelegate::CreateWeakLambda(this, [WeakEnemy, OriginalSpeed]()
			{
				ACS_EnemyCharacter* Target = WeakEnemy.Get();
				if (!Target)
				{
					return;
				}
				if (UCharacterMovementComponent* MC = Target->GetCharacterMovement())
				{
					MC->MaxWalkSpeed = OriginalSpeed;
				}
			}),
			SlowDuration,
			false);
	}
}
