// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/CS_FlameTongue.h"

#include "Characters/CS_EnemyCharacter.h"
#include "Characters/CS_PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCS_FlameTongue, Log, All);


ACS_FlameTongue::ACS_FlameTongue()
{
	// 判定体不需要 Tick：伤害靠 timer 驱动，少一份每帧开销
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 胶囊只作为「几何体 + 变换」使用：
	// 不开碰撞、不生成 overlap 事件，检测时直接把它的形状丢给 World 查询。
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->SetupAttachment(Root);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetGenerateOverlapEvents(false);
	Capsule->SetCapsuleSize(120.f, 130.f);
}


void ACS_FlameTongue::InitFlameTongue(AActor* InSource, AController* InInstigator, const FVector& InDirection,
	const FCSFlameTongueSettings& InSettings, bool bApplyDirection)
{
	SourceActor = InSource;
	InstigatorController = InInstigator;
	Settings = InSettings;

	// 生成者就是伤害归属的「主人」；方块被销毁时火舌也能感知
	SetOwner(InSource);

	Direction = InDirection.GetSafeNormal();
	if (bApplyDirection)
	{
		if (Direction.IsNearlyZero())
		{
			Direction = GetActorForwardVector();
		}

		// 独立生成的火舌：用方向覆盖 Actor 旋转
		SetActorRotation(FRotationMatrix::MakeFromX(Direction).Rotator());
	}
	else
	{
		// 挂在方块上的火舌：保留 BP 摆好的相对朝向，
		// 方向由整个方块的旋转决定，不再单独改这里
		Direction = GetActorForwardVector();
	}
}


void ACS_FlameTongue::SetDirection(const FVector& InDirection)
{
	Direction = InDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(FRotationMatrix::MakeFromX(Direction).Rotator());
}


void ACS_FlameTongue::StartFlame()
{
	if (bActive)
	{
		return;
	}

	bActive = true;
	SetActorHiddenInGame(false);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Interval = FMath::Max(0.02f, Settings.DamageInterval);
	World->GetTimerManager().SetTimer(
		DamageTimerHandle, this, &ACS_FlameTongue::DamageTick, Interval, true);

	// 立即检测一次：贴脸生成时不用等第一个间隔
	DamageTick();
}


void ACS_FlameTongue::StopFlame()
{
	bActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}

	SetActorHiddenInGame(true);
}


void ACS_FlameTongue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}


void ACS_FlameTongue::DamageTick()
{
	if (!bActive)
	{
		return;
	}

	// 生成者已经没了：火舌自动停，避免悬空 timer
	if (!SourceActor.IsValid())
	{
		StopFlame();
		return;
	}

	TSet<AActor*> TargetsThisTick;
	CollectTargets(TargetsThisTick);

	for (AActor* Target : TargetsThisTick)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		const TWeakObjectPtr<AActor> WeakTarget(Target);
		const bool bFirstHit = !HitTargets.Contains(WeakTarget);
		HitTargets.Add(WeakTarget);

		if (Settings.FlameDamage > 0.f)
		{
			// DamageCauser = 本 Actor（火焰），不是方块碰撞
			Target->TakeDamage(Settings.FlameDamage, FDamageEvent(), InstigatorController.Get(), this);
		}

		// 火焰属性 / 火星事件仍然只对敌人广播；玩家受伤不触发这套
		if (ACS_EnemyCharacter* Enemy = Cast<ACS_EnemyCharacter>(Target))
		{
			OnFlameHit.Broadcast(Enemy, bFirstHit, Settings.FlameDamage);
		}
	}
}


void ACS_FlameTongue::CollectTargets(TSet<AActor*>& OutTargets) const
{
	UWorld* World = GetWorld();
	if (!Capsule || !World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CS_FlameTongueOverlap), false, this);
	if (SourceActor.IsValid())
	{
		QueryParams.AddIgnoredActor(SourceActor.Get());
	}

	const float Radius = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius());
	const float HalfHeight = FMath::Max(1.f, Capsule->GetScaledCapsuleHalfHeight());
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	TArray<FOverlapResult> Overlaps;
	const bool bFound = World->OverlapMultiByObjectType(
		Overlaps,
		Capsule->GetComponentLocation(),
		Capsule->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn),
		Shape,
		QueryParams);

	if (!bFound)
	{
		return;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor)
		{
			continue;
		}

		if (Cast<ACS_EnemyCharacter>(Actor))
		{
			OutTargets.Add(Actor);
		}
		else if (Settings.bDamagePlayer && Cast<ACS_PlayerCharacter>(Actor))
		{
			OutTargets.Add(Actor);
		}
	}
}



