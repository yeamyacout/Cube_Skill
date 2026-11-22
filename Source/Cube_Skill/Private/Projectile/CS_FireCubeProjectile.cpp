// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectile/CS_FireCubeProjectile.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "TimerManager.h"


ACS_FireCubeProjectile::ACS_FireCubeProjectile()
{
	// 红色：代表火焰
	CubeColor = FLinearColor(1.f, 0.15f, 0.05f, 1.f);

}


void ACS_FireCubeProjectile::OnEnemyHitEffect(ACS_EnemyCharacter* Enemy)
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

	// 灼烧：拆成多次跳伤，总伤害均分
	const int32 TickCount = FMath::Max(1, FMath::RoundToInt(BurnDuration / BurnTickInterval));
	const float DamagePerTick = BurnTotalDamage / TickCount;

	// 弱引用捕获，敌人或方块销毁后不再访问
	TWeakObjectPtr<ACS_EnemyCharacter> WeakEnemy(Enemy);
	TWeakObjectPtr<ACS_FireCubeProjectile> WeakSelf(this);

	for (int32 i = 1; i <= TickCount; ++i)
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateWeakLambda(this, [WeakEnemy, WeakSelf, DamagePerTick]()
			{
				ACS_EnemyCharacter* Target = WeakEnemy.Get();
				ACS_FireCubeProjectile* Self = WeakSelf.Get();
				if (!Target || !Self)
				{
					return;
				}

				Target->TakeDamage(DamagePerTick, FDamageEvent(), nullptr, Self);
			}),
			BurnTickInterval * i,
			false);
	}
}
